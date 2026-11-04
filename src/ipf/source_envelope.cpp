// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "source_envelope.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace org::ttldtor::qdcxx::ipf::detail {
namespace {
/** Read an already bounded little-endian field.
 * @param bytes Complete bounded field.
 * @return Unsigned value of at most eight bytes.
 */
std::uint64_t little(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value = 0;

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        value |= std::uint64_t(std::to_integer<unsigned char>(bytes[i])) << (i * 8);
    }

    return value;
}

/** Check the JDK default UTF-8 name domain without producing text or a path.
 * @param bytes Complete bounded ZIP name.
 * @return True for well-formed scalar UTF-8.
 */
bool validName(std::span<const std::byte> bytes) noexcept {
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    unsigned remaining = 0;

    for (const auto byte : bytes) {
        const auto b = std::to_integer<unsigned char>(byte);

        if (remaining == 0) {
            if (b < 0x80) {
                continue;
            }

            if (b >= 0xc2 && b <= 0xdf) {
                remaining = 1;
                minimum = 0x80;
                value = b & 0x1f;
            } else if (b >= 0xe0 && b <= 0xef) {
                remaining = 2;
                minimum = 0x800;
                value = b & 0x0f;
            } else if (b >= 0xf0 && b <= 0xf4) {
                remaining = 3;
                minimum = 0x10000;
                value = b & 7;
            } else {
                return false;
            }
        } else {
            if ((b & 0xc0) != 0x80) {
                return false;
            }

            value = (value << 6) | (b & 0x3f);

            if (--remaining == 0 && (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))) {
                return false;
            }
        }
    }

    return remaining == 0;
}
} // namespace

SourceEnvelope::SourceEnvelope(const IpfEnvelopeLimits &limits, IpfTransferResult &result) noexcept
    : limits_(limits), result_(result) {
    stream_.zalloc = allocate;
    stream_.zfree = release;
    stream_.opaque = this;
}

SourceEnvelope::~SourceEnvelope() {
    if (initialized_) {
        inflateEnd(&stream_);
    }
}

bool SourceEnvelope::reject(IpfTransferError error, std::string_view stage) noexcept {
    failed_ = true;

    if (!result_.error) {
        result_.error = IpfTransferDiagnostic{error, stage};
    }

    return false;
}

bool SourceEnvelope::work(std::size_t bytes) noexcept {
    auto &used = result_.envelope.workBytes;

    if (used > limits_.workBytes || bytes > limits_.workBytes - used) {
        return reject(IpfTransferError::WORK_LIMIT, "envelope-work");
    }

    used += bytes;

    return true;
}

bool SourceEnvelope::entry() noexcept {
    if (result_.envelope.entries >= limits_.entries) {
        return reject(IpfTransferError::RESOURCE_LIMIT, "envelope-entries");
    }

    ++result_.envelope.entries;

    return true;
}

voidpf SourceEnvelope::allocate(voidpf opaque, uInt items, uInt size) noexcept {
    auto &self = *static_cast<SourceEnvelope *>(opaque);
    constexpr auto MAXIMUM = (std::numeric_limits<std::size_t>::max)();

    if (size != 0 && std::size_t(items) > (MAXIMUM - sizeof(Allocation)) / size) {
        self.reject(IpfTransferError::RESOURCE_LIMIT, "envelope-allocation-overflow");

        return nullptr;
    }

    const auto bytes = std::size_t(items) * size + sizeof(Allocation);

    if (self.allocated_ > self.limits_.inflaterBytes || bytes > self.limits_.inflaterBytes - self.allocated_) {
        self.reject(IpfTransferError::RESOURCE_LIMIT, "envelope-inflater-backing");

        return nullptr;
    }

    if (!self.work(bytes)) {
        return nullptr;
    }

    auto *block = static_cast<Allocation *>(std::calloc(1, bytes));

    if (block == nullptr) {
        self.reject(IpfTransferError::ALLOCATION_FAILURE, "envelope-allocation");

        return nullptr;
    }

    block->bytes = bytes;
    self.allocated_ += bytes;
    self.result_.envelope.peakInflaterBytes = (std::max)(self.result_.envelope.peakInflaterBytes, self.allocated_);

    return block + 1;
}

void SourceEnvelope::release(voidpf opaque, voidpf address) noexcept {
    if (address != nullptr) {
        auto &self = *static_cast<SourceEnvelope *>(opaque);
        auto *block = static_cast<Allocation *>(address) - 1;
        self.allocated_ -= block->bytes;
        std::free(block);
    }
}

bool SourceEnvelope::startInflater(bool gzip) noexcept {
    const auto window = gzip ? MAX_WBITS + 16 : -MAX_WBITS;
    const auto code = initialized_ ? inflateReset2(&stream_, window) : inflateInit2(&stream_, window);

    if (code != Z_OK || failed_) {
        if (!failed_) {
            reject(code == Z_MEM_ERROR ? IpfTransferError::ALLOCATION_FAILURE : IpfTransferError::CONTAINER_FAILURE,
                   "envelope-inflater-init");
        }

        return false;
    }

    initialized_ = true;

    if (gzip) {
        gzipHeader_ = {};
        gzipHeaderCounted_ = false;

        if (inflateGetHeader(&stream_, &gzipHeader_) != Z_OK) {
            return reject(IpfTransferError::CONTAINER_FAILURE, "gzip-header-state");
        }
    }

    return true;
}

void SourceEnvelope::select() noexcept {
    if (initialSize_ >= 2 && initial_[0] == std::byte{0x1f} && initial_[1] == std::byte{0x8b}) {
        result_.envelope.kind = IpfEnvelopeKind::GZIP;
        phase_ = Phase::GZIP;
    } else if (initialSize_ == 4 && little(initial_) == 0x04034b50) {
        result_.envelope.kind = IpfEnvelopeKind::ZIP;
        phase_ = Phase::ZIP_HEADER;
    } else {
        phase_ = Phase::PLAIN;
    }
}

bool SourceEnvelope::declareGzip() noexcept {
    if (phase_ != Phase::SNIFF || initialSize_ != 0) {
        return reject(IpfTransferError::HTTP_FAILURE, "http-coding-state");
    }

    result_.envelope.kind = IpfEnvelopeKind::GZIP;
    phase_ = Phase::GZIP;

    return true;
}

std::span<const std::byte> SourceEnvelope::output() const noexcept {
    return std::span(output_).first(outputSize_).subspan(outputPosition_);
}

void SourceEnvelope::consume(std::size_t bytes) noexcept {
    outputPosition_ += (std::min)(bytes, outputSize_ - outputPosition_);

    if (outputPosition_ == outputSize_) {
        outputPosition_ = outputSize_ = 0;
    }
}

bool SourceEnvelope::failed() const noexcept {
    return failed_;
}

bool SourceEnvelope::zipHeader() noexcept {
    const auto fixed = std::span<const std::byte>(header_);

    if (little(fixed.first(4)) != 0x04034b50) {
        return reject(IpfTransferError::CONTAINER_FAILURE, "zip-no-file-entry");
    }

    const auto flags = little(fixed.subspan(6, 2));
    method_ = static_cast<unsigned>(little(fixed.subspan(8, 2)));
    descriptorPresent_ = (flags & 8) != 0;

    if ((flags & 1) != 0 || (method_ != 0 && method_ != 8) || (descriptorPresent_ && method_ != 8)) {
        return reject(IpfTransferError::CONTAINER_FAILURE, "zip-entry-method");
    }

    nameSize_ = static_cast<std::size_t>(little(fixed.subspan(26, 2)));
    extraSize_ = static_cast<std::size_t>(little(fixed.subspan(28, 2)));
    const auto metadata = header_.size() + nameSize_ + extraSize_;
    auto &used = result_.envelope.metadataBytes;

    if (used > limits_.metadataBytes || metadata > limits_.metadataBytes - used) {
        return reject(IpfTransferError::RESOURCE_LIMIT, "zip-metadata");
    }

    if (!entry()) {
        return false;
    }

    used += metadata;
    expectedCrc_ = static_cast<std::uint32_t>(little(fixed.subspan(14, 4)));
    expectedInput_ = little(fixed.subspan(18, 4));
    expectedOutput_ = little(fixed.subspan(22, 4));
    entryInput_ = entryOutput_ = 0;
    crc_ = 0;
    metadataSize_ = 0;
    zip64_ = false;
    phase_ = Phase::ZIP_METADATA;

    return true;
}

bool SourceEnvelope::zipMetadata() noexcept {
    const auto bytes = std::span<const std::byte>(metadata_).first(nameSize_ + extraSize_);

    if (!work(bytes.size()) || !validName(bytes.first(nameSize_))) {
        if (!failed_) {
            reject(IpfTransferError::CONTAINER_FAILURE, "zip-name-utf8");
        }

        return false;
    }

    directory_ = nameSize_ != 0 && bytes[nameSize_ - 1] == std::byte{'/'};
    const auto extra = bytes.subspan(nameSize_);

    for (std::size_t offset = 0; offset + 4 <= extra.size();) {
        const auto tag = little(extra.subspan(offset, 2));
        const auto size = static_cast<std::size_t>(little(extra.subspan(offset + 2, 2)));
        offset += 4;

        if (size > extra.size() - offset) {
            break;
        }

        if (tag == 1) {
            if (zip64_ || size < 16) {
                return reject(IpfTransferError::CONTAINER_FAILURE, "zip64-extra");
            }

            zip64_ = true;

            if (!descriptorPresent_) {
                expectedOutput_ = little(extra.subspan(offset, 8));
                expectedInput_ = little(extra.subspan(offset + 8, 8));
            }
        }

        offset += size;
    }

    phase_ = method_ == 0 ? Phase::ZIP_STORED : Phase::ZIP_DEFLATE;

    if (method_ == 8 && !startInflater(false)) {
        return false;
    }

    return true;
}

bool SourceEnvelope::zipEnd() noexcept {
    if (crc_ != expectedCrc_ || entryInput_ != expectedInput_ || entryOutput_ != expectedOutput_) {
        return reject(IpfTransferError::CONTAINER_FAILURE, "zip-entry-integrity");
    }

    if (directory_) {
        phase_ = Phase::ZIP_HEADER;
        headerSize_ = 0;
    } else {
        phase_ = Phase::DISCARD_REMAINDER;
        result_.envelope.finished = true;
    }

    return true;
}

std::size_t SourceEnvelope::process(std::span<const std::byte> input) noexcept {
    auto &progress = result_.envelope;

    if (phase_ == Phase::DISCARD_REMAINDER) {
        return work(input.size()) ? input.size() : 0;
    }

    if (phase_ == Phase::PLAIN || phase_ == Phase::ZIP_STORED) {
        auto count = input.size();

        if (phase_ == Phase::ZIP_STORED) {
            if (entryInput_ == expectedInput_) {
                zipEnd();

                return 0;
            }

            count = static_cast<std::size_t>((std::min)(std::uint64_t(count), expectedInput_ - entryInput_));
        }

        count = (std::min)(count, limits_.outputBytes - (std::min)(limits_.outputBytes, progress.outputBytes));

        if (count == 0 && !input.empty()) {
            reject(IpfTransferError::RESOURCE_LIMIT, "envelope-output");

            return 0;
        }

        if (!work(count * 2)) {
            return 0;
        }

        if (phase_ == Phase::ZIP_STORED) {
            crc_ = static_cast<std::uint32_t>(
                crc32(crc_, reinterpret_cast<const Bytef *>(input.data()), static_cast<uInt>(count)));
            entryInput_ += count;
            entryOutput_ += count;
        }

        progress.outputBytes += count;

        if (phase_ == Phase::PLAIN || !directory_) {
            if (count != 0) {
                std::memcpy(output_.data(), input.data(), count);
            }

            outputSize_ = count;
        }

        return count;
    }

    if (phase_ == Phase::ZIP_HEADER) {
        const auto count = (std::min)(input.size(), header_.size() - headerSize_);

        if (!work(count)) {
            return 0;
        }

        if (count != 0) {
            std::memcpy(header_.data() + headerSize_, input.data(), count);
        }

        headerSize_ += count;

        if (headerSize_ == header_.size()) {
            zipHeader();
        }

        return count;
    }

    if (phase_ == Phase::ZIP_METADATA) {
        const auto count = (std::min)(input.size(), nameSize_ + extraSize_ - metadataSize_);

        if (!work(count)) {
            return 0;
        }

        if (count != 0) {
            std::memcpy(metadata_.data() + metadataSize_, input.data(), count);
        }

        metadataSize_ += count;

        if (metadataSize_ == nameSize_ + extraSize_) {
            zipMetadata();
        }

        return count;
    }

    if (phase_ == Phase::ZIP_DESCRIPTOR) {
        const auto wide = zip64_ || entryInput_ > UINT32_MAX || entryOutput_ > UINT32_MAX;
        const auto width = wide ? std::size_t(8) : std::size_t(4);
        const auto unsignedLength = 4 + 2 * width;
        const auto length = descriptorSize_ < unsignedLength ? unsignedLength : unsignedLength + 4;
        const auto count = (std::min)(input.size(), length - descriptorSize_);

        if (!work(count)) {
            return 0;
        }

        if (count != 0) {
            std::memcpy(descriptor_.data() + descriptorSize_, input.data(), count);
        }

        descriptorSize_ += count;

        if (descriptorSize_ < unsignedLength) {
            return count;
        }

        auto bytes = std::span<const std::byte>(descriptor_).first(descriptorSize_);
        // Test the unsigned form first using already known actual CRC/sizes. This also accepts
        // a legitimate unsigned CRC equal to the optional signature without stealing suffix bytes.
        const auto unsignedMatches = little(bytes.first(4)) == crc_ && little(bytes.subspan(4, width)) == entryInput_ &&
                                     little(bytes.subspan(4 + width, width)) == entryOutput_;

        if (unsignedMatches) {
            expectedCrc_ = crc_;
            expectedInput_ = entryInput_;
            expectedOutput_ = entryOutput_;
            zipEnd();
        } else if (little(bytes.first(4)) != 0x08074b50) {
            reject(IpfTransferError::CONTAINER_FAILURE, "zip-descriptor-integrity");
        } else if (descriptorSize_ == unsignedLength + 4) {
            bytes = bytes.subspan(4);
            expectedCrc_ = static_cast<std::uint32_t>(little(bytes.first(4)));
            expectedInput_ = little(bytes.subspan(4, width));
            expectedOutput_ = little(bytes.subspan(4 + width, width));
            zipEnd();
        }

        return count;
    }

    if (phase_ == Phase::GZIP_BETWEEN) {
        if (input.empty()) {
            return 0;
        }

        gzipCandidate_ = true;

        if (!startInflater(true)) {
            return 0;
        }

        phase_ = Phase::GZIP;
    }

    const auto gzip = phase_ == Phase::GZIP;

    if (!initialized_ && !startInflater(gzip)) {
        return 0;
    }

    const auto remaining = limits_.outputBytes - (std::min)(limits_.outputBytes, progress.outputBytes);
    const auto capacity = (std::min)(output_.size(), (std::max)(std::size_t(1), remaining));

    if (!work(input.size() + capacity * 2)) {
        return 0;
    }

    stream_.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(input.data()));
    stream_.avail_in = static_cast<uInt>(input.size());
    stream_.next_out = reinterpret_cast<Bytef *>(output_.data());
    stream_.avail_out = static_cast<uInt>(capacity);
    const auto code = inflate(&stream_, Z_NO_FLUSH);
    const auto consumed = input.size() - stream_.avail_in;
    const auto produced = capacity - stream_.avail_out;
    // zlib only borrows caller/window storage during this finite call.
    stream_.next_in = stream_.next_out = nullptr;
    stream_.avail_in = stream_.avail_out = 0;

    if (gzip && gzipHeader_.done == 1 && !gzipHeaderCounted_) {
        gzipHeaderCounted_ = true;

        if (!entry()) {
            return consumed;
        }
    }

    if (produced > remaining) {
        reject(IpfTransferError::RESOURCE_LIMIT, "envelope-output");

        return consumed;
    }

    progress.outputBytes += produced;

    if (!gzip) {
        entryInput_ += consumed;
        entryOutput_ += produced;
        crc_ = static_cast<std::uint32_t>(
            crc32(crc_, reinterpret_cast<const Bytef *>(output_.data()), static_cast<uInt>(produced)));
    }

    if (gzip || !directory_) {
        outputSize_ = produced;
    }

    if (code == Z_STREAM_END) {
        if (gzip) {
            phase_ = Phase::GZIP_BETWEEN;
        } else if (descriptorPresent_) {
            phase_ = Phase::ZIP_DESCRIPTOR;
            descriptorSize_ = 0;
        } else {
            zipEnd();
        }
    } else if (code != Z_OK && code != Z_BUF_ERROR) {
        if (gzip && gzipCandidate_ && gzipHeader_.done != 1 && code == Z_DATA_ERROR) {
            phase_ = Phase::DISCARD_REMAINDER;
        } else if (!failed_) {
            reject(code == Z_MEM_ERROR ? IpfTransferError::ALLOCATION_FAILURE : IpfTransferError::CONTAINER_FAILURE,
                   gzip ? "gzip-integrity" : "zip-deflate");
        }
    }

    return consumed;
}

std::size_t SourceEnvelope::feed(std::span<const std::byte> input) noexcept {
    if (failed_ || !output().empty()) {
        return 0;
    }

    auto &progress = result_.envelope;

    if (progress.calls >= limits_.calls) {
        reject(IpfTransferError::WORK_LIMIT, "envelope-calls");

        return 0;
    }

    ++progress.calls;

    if (phase_ == Phase::SNIFF) {
        const auto remaining = limits_.inputBytes - (std::min)(limits_.inputBytes, progress.inputBytes);
        const auto count = (std::min)({input.size(), initial_.size() - initialSize_, remaining});

        if (count == 0 && !input.empty()) {
            reject(IpfTransferError::RESOURCE_LIMIT, "envelope-input");

            return 0;
        }

        if (!work(count)) {
            return 0;
        }

        if (count != 0) {
            std::memcpy(initial_.data() + initialSize_, input.data(), count);
        }

        initialSize_ += count;
        progress.inputBytes += count;

        if (initialSize_ == initial_.size()) {
            select();
        }

        return count;
    }

    const auto staged = initialPosition_ < initialSize_;
    auto available = staged ? std::span<const std::byte>(initial_).first(initialSize_).subspan(initialPosition_)
                            : input.first((std::min)(input.size(), WINDOW_BYTES));

    if (!staged) {
        const auto remaining = limits_.inputBytes - (std::min)(limits_.inputBytes, progress.inputBytes);
        available = available.first((std::min)(available.size(), remaining));

        if (available.empty() && !input.empty()) {
            reject(IpfTransferError::RESOURCE_LIMIT, "envelope-input");

            return 0;
        }
    }

    const auto consumed = process(available);

    if (staged) {
        initialPosition_ += consumed;

        return 0;
    }

    progress.inputBytes += consumed;

    return consumed;
}

bool SourceEnvelope::finish() noexcept {
    if (!output().empty()) {
        return false;
    }

    if (failed_) {
        return true;
    }

    if (phase_ == Phase::SNIFF) {
        select();
    }

    const auto staged = initialPosition_ < initialSize_;
    const auto before = phase_;
    (void)feed({});

    if (!output().empty()) {
        return false;
    }

    if (failed_) {
        return true;
    }

    if (staged || before != phase_) {
        return false;
    }

    if (phase_ == Phase::PLAIN || phase_ == Phase::DISCARD_REMAINDER || phase_ == Phase::GZIP_BETWEEN ||
        (phase_ == Phase::GZIP && gzipCandidate_ && gzipHeader_.done != 1)) {
        result_.envelope.finished = true;

        return true;
    }

    reject(IpfTransferError::CONTAINER_FAILURE, "envelope-truncated");

    return true;
}

} // namespace org::ttldtor::qdcxx::ipf::detail
