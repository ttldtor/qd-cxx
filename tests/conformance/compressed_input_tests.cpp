// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/compressed_input.hpp"
#include <doctest/doctest.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;

/** Test compression with deterministic headers.
 * @param input Exact uncompressed bytes.
 * @param gzip Select gzip wrapper instead of raw deflate.
 * @return Compressed bytes; a test failure aborts invalid setup.
 */
std::string deflateBytes(const std::string &input, bool gzip) {
    z_stream stream{};
    REQUIRE(deflateInit2(&stream, 6, Z_DEFLATED, gzip ? 31 : -15, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    std::string output(compressBound(static_cast<uLong>(input.size())) + 64, '\0');
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    REQUIRE(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    output.resize(stream.total_out);
    REQUIRE(deflateEnd(&stream) == Z_OK);

    return output;
}

/** Append a little-endian test field.
 * @param bytes Mutable envelope.
 * @param value Exact field value.
 * @param width Field size in bytes.
 */
void le(std::string &bytes, std::uint32_t value, unsigned width) {
    for (unsigned index = 0; index < width; ++index) {
        bytes.push_back(static_cast<char>(value >> (8 * index)));
    }
}

/** One synthetic ZIP entry specification. */
struct ZipPart {
    std::string name;       ///< Opaque name, ending in slash for a directory.
    std::string payload;    ///< Uncompressed bytes.
    bool stored = false;    ///< Method0 rather than method8.
    bool descriptor = true; ///< Signed ZIP32 descriptor after deflate.
};

/** Generate a ZIP32 archive independently of the decoder.
 * @param parts Exact ordered entries, including optional empty directories.
 * @return Complete local records, central directory and EOCD.
 */
std::string zipBytes(const std::vector<ZipPart> &parts) {
    std::string local;
    std::string central;

    for (const auto &part : parts) {
        const auto compressed = part.stored ? part.payload : deflateBytes(part.payload, false);
        const auto crc = crc32_z(0, reinterpret_cast<const Bytef *>(part.payload.data()), part.payload.size());
        const auto offset = local.size();
        const auto flags = part.descriptor ? 8U : 0U;
        le(local, 0x04034b50, 4);
        le(local, 20, 2);
        le(local, flags, 2);
        le(local, part.stored ? 0 : 8, 2);
        le(local, 0, 4);
        le(local, part.descriptor ? 0 : crc, 4);
        le(local, part.descriptor ? 0 : static_cast<std::uint32_t>(compressed.size()), 4);
        le(local, part.descriptor ? 0 : static_cast<std::uint32_t>(part.payload.size()), 4);
        le(local, static_cast<std::uint32_t>(part.name.size()), 2);
        le(local, 0, 2);
        local += part.name;
        local += compressed;

        if (part.descriptor) {
            le(local, 0x08074b50, 4);
            le(local, crc, 4);
            le(local, static_cast<std::uint32_t>(compressed.size()), 4);
            le(local, static_cast<std::uint32_t>(part.payload.size()), 4);
        }

        le(central, 0x02014b50, 4);
        le(central, 20, 2);
        le(central, 20, 2);
        le(central, flags, 2);
        le(central, part.stored ? 0 : 8, 2);
        le(central, 0, 4);
        le(central, crc, 4);
        le(central, static_cast<std::uint32_t>(compressed.size()), 4);
        le(central, static_cast<std::uint32_t>(part.payload.size()), 4);
        le(central, static_cast<std::uint32_t>(part.name.size()), 2);
        le(central, 0, 2);
        le(central, 0, 2);
        le(central, 0, 2);
        le(central, 0, 2);
        le(central, 0, 4);
        le(central, static_cast<std::uint32_t>(offset), 4);
        central += part.name;
    }

    const auto offset = local.size();
    local += central;
    le(local, 0x06054b50, 4);
    le(local, 0, 2);
    le(local, 0, 2);
    le(local, static_cast<std::uint32_t>(parts.size()), 2);
    le(local, static_cast<std::uint32_t>(parts.size()), 2);
    le(local, static_cast<std::uint32_t>(central.size()), 4);
    le(local, static_cast<std::uint32_t>(offset), 4);
    le(local, 0, 2);

    return local;
}

/** Bounded short-read/failure stream; never owns an external process or file. */
class SourceBuffer : public std::streambuf {
    std::string bytes_;
    std::size_t position_ = 0;
    std::size_t chunk_;
    std::size_t failAt_;
    std::size_t calls_ = 0;

    protected:
    std::streamsize xsgetn(char *target, std::streamsize count) override {
        ++calls_;

        if (position_ >= failAt_) {
            throw std::runtime_error("injected source failure");
        }

        const auto size =
            std::min({static_cast<std::size_t>(count), chunk_, bytes_.size() - position_, failAt_ - position_});
        std::copy_n(bytes_.data() + position_, size, target);
        position_ += size;

        return static_cast<std::streamsize>(size);
    }

    int_type underflow() override {
        ++calls_;

        if (position_ >= failAt_) {
            throw std::runtime_error("injected source failure");
        }

        return position_ == bytes_.size() ? traits_type::eof() : traits_type::to_int_type(bytes_[position_]);
    }

    public:
    SourceBuffer(std::string bytes, std::size_t chunk, std::size_t failAt = std::numeric_limits<std::size_t>::max())
        : bytes_(std::move(bytes)), chunk_(chunk), failAt_(failAt) {
    }

    /** Count actual source operations, including zero-byte probes.
     * @return Total read/probe calls observed by this test source.
     */
    std::size_t ioCalls() const noexcept {
        return calls_;
    }
};

/** Captured terminal pull result. */
struct Result {
    std::string bytes;              ///< Completed produced prefix.
    ti::ContainerRead final;        ///< Terminal status.
    ti::ContainerCounters counters; ///< Final coordinate snapshot.
};

/** Drain only this test adapter with a finite loop guard.
 * @param source Exact physical bytes.
 * @param kind Explicit container.
 * @param limits Finite decoder bounds.
 * @param chunk Physical short-read bound.
 * @param outputChunk Caller output span capacity.
 * @param failAt Injected physical I/O failure boundary.
 * @return Owned bytes and terminal status; completion/finish are cross-checked.
 */
Result decode(const std::string &source, ti::ContainerKind kind, ti::ContainerLimits limits = {},
              std::size_t chunk = 4096, std::size_t outputChunk = 17,
              std::size_t failAt = std::numeric_limits<std::size_t>::max()) {
    SourceBuffer buffer(source, chunk, failAt);
    std::istream input(&buffer);
    ti::CompressedInput decoder(input, kind, limits);
    std::vector<std::byte> output(outputChunk);
    Result result;

    for (unsigned count = 0; count < 100000; ++count) {
        result.final = decoder.read(output);
        result.bytes.append(reinterpret_cast<const char *>(output.data()), result.final.produced);

        if (result.final.physicalComplete || result.final.error != ti::ContainerError::NONE) {
            CHECK(decoder.finish() == (result.final.physicalComplete ? ti::ContainerError::NONE : result.final.error));
            result.counters = decoder.counters();
            const auto beforeCalls = buffer.ioCalls();
            const auto terminal = decoder.read(output);
            CHECK(terminal.produced == 0);
            CHECK(terminal.error == result.final.error);
            CHECK(decoder.counters().compressedPhysicalRead == result.counters.compressedPhysicalRead);
            CHECK(buffer.ioCalls() == beforeCalls);

            return result;
        }

        REQUIRE(result.final.produced != 0);
    }

    FAIL("bounded decoder failed to terminate");

    return result;
}
} // namespace

TEST_CASE("compressed input gzip members and zip entries are chunk independent") {
    const std::string payload(20000, 'A');
    const auto gzip = deflateBytes(payload, true);
    const auto zip =
        zipBytes({{"directory/", "", false, true}, {"tape.qtp", payload, false, true}, {"tail/", "", true, false}});

    for (auto chunk : {1U, 7U, 4096U}) {
        for (auto output : {1U, 17U, 4096U}) {
            auto g = decode(deflateBytes("", true) + gzip + deflateBytes("tail", true), ti::ContainerKind::GZIP, {},
                            chunk, output);
            CHECK(g.final.physicalComplete);
            CHECK(g.bytes == payload + "tail");
            CHECK(g.counters.members == 3);
            auto z = decode(zip, ti::ContainerKind::ZIP, {}, chunk, output);
            CHECK(z.final.physicalComplete);
            CHECK(z.bytes == payload);
            CHECK(z.counters.members == 3);
        }
    }

    CHECK(decode(zipBytes({{"x", "stored", true, false}}), ti::ContainerKind::ZIP).bytes == "stored");
    CHECK(decode("plain", ti::ContainerKind::NONE).bytes == "plain");
}

TEST_CASE("late gzip and ZIP integrity failures retain the decoded prefix") {
    const auto validGzip = deflateBytes("complete prefix", true);
    const auto validZip = zipBytes({{"x", "complete prefix", false, true}});

    for (auto chunk : {1U, 4096U}) {
        auto crc = validGzip;
        crc[crc.size() - 8] ^= 1;
        auto size = validGzip;
        size[size.size() - 4] ^= 1;

        for (const auto &broken : {crc, size}) {
            auto result = decode(broken, ti::ContainerKind::GZIP, {}, chunk);
            CHECK(result.final.error == ti::ContainerError::CHECKSUM);
            CHECK(result.bytes == "complete prefix");
        }

        CHECK(decode(validGzip.substr(0, validGzip.size() - 1), ti::ContainerKind::GZIP, {}, chunk).final.error ==
              ti::ContainerError::TRUNCATED);
        CHECK_FALSE(decode(validGzip + "junk", ti::ContainerKind::GZIP, {}, chunk).final.physicalComplete);
        const auto central = validZip.find("PK\1\2");
        REQUIRE(central != std::string::npos);
        auto missing = decode(validZip.substr(0, central), ti::ContainerKind::ZIP, {}, chunk);
        CHECK(missing.final.error == ti::ContainerError::TRUNCATED);
        CHECK(missing.bytes == "complete prefix");
        CHECK_FALSE(decode(validZip + "junk", ti::ContainerKind::ZIP, {}, chunk).final.physicalComplete);
        CHECK_FALSE(
            decode(validZip.substr(0, validZip.size() - 1), ti::ContainerKind::ZIP, {}, chunk).final.physicalComplete);
        auto corrupt = validZip;
        corrupt[central - 12] ^= 1;
        auto result = decode(corrupt, ti::ContainerKind::ZIP, {}, chunk);
        CHECK(result.final.error == ti::ContainerError::CHECKSUM);
        CHECK(result.bytes == "complete prefix");
    }
}

TEST_CASE("unsupported ZIP choices cannot masquerade as a completed first entry") {
    CHECK(decode(zipBytes({}), ti::ContainerKind::ZIP).final.error == ti::ContainerError::INVALID_DIRECTORY);
    CHECK(decode(zipBytes({{"dir/", "", false, true}}), ti::ContainerKind::ZIP).final.error ==
          ti::ContainerError::INVALID_DIRECTORY);
    CHECK(decode(zipBytes({{"first", "one"}, {"second", "two"}}), ti::ContainerKind::ZIP).final.error ==
          ti::ContainerError::UNSUPPORTED);
    auto zip = zipBytes({{"x", "one"}});

    for (auto offset : {4U, 6U, 8U}) {
        auto changed = zip;
        changed[offset] = static_cast<char>(offset == 4 ? 45 : offset == 6 ? 1 : 99);
        CHECK(decode(changed, ti::ContainerKind::ZIP).final.error == ti::ContainerError::UNSUPPORTED);
    }

    auto changed = zip;
    changed[zip.find("PK\1\2") + 42] ^= 1;
    CHECK(decode(changed, ti::ContainerKind::ZIP).final.error == ti::ContainerError::INVALID_DIRECTORY);
    CHECK(decode(zipBytes({{"x", "stored", true, true}}), ti::ContainerKind::ZIP).final.error ==
          ti::ContainerError::UNSUPPORTED);
}

TEST_CASE("container source budgets are literal finite and exact EOF is permitted") {
    const auto gzip = deflateBytes("123456789", true);

    for (int delta : {-1, 0, 1}) {
        ti::ContainerLimits limits;
        limits.compressedBytes = gzip.size() + delta;
        auto result = decode(gzip, ti::ContainerKind::GZIP, limits, 1);
        CHECK(result.final.physicalComplete == (delta >= 0));
        CHECK(result.counters.compressedPhysicalRead <= limits.compressedBytes + 1);
        limits = {};
        limits.inflatedBytes = 9 + delta;
        result = decode(gzip, ti::ContainerKind::GZIP, limits, 1);
        CHECK(result.final.physicalComplete == (delta >= 0));
        CHECK(result.bytes.size() <= limits.inflatedBytes);
    }

    ti::ContainerLimits limits;
    limits.compressedBytes = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::COMPRESSED_LIMIT);
    limits = {};
    limits.inflatedBytes = 0;
    CHECK(decode(deflateBytes("", true), ti::ContainerKind::GZIP, limits).final.physicalComplete);
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::INFLATED_LIMIT);
    limits = {};
    limits.gzipMembers = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::MEMBER_ENTRY_LIMIT);
    limits = {};
    limits.zipEntries = 0;
    CHECK(decode(zipBytes({{"x", "y"}}), ti::ContainerKind::ZIP, limits).final.error ==
          ti::ContainerError::MEMBER_ENTRY_LIMIT);
    limits = {};
    limits.headerBytes = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::HEADER_LIMIT);
    limits = {};
    limits.workBytes = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::WORK_LIMIT);
    limits = {};
    limits.inflateCalls = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::WORK_LIMIT);
    limits = {};
    limits.inflaterBytes = 0;
    auto allocation = decode(gzip, ti::ContainerKind::GZIP, limits);
    CHECK(allocation.final.error == ti::ContainerError::ALLOCATION);
    CHECK(allocation.counters.compressedPhysicalRead == 0);
    limits = {};
    limits.retainedBytes = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::ALLOCATION);
    limits = {};
    limits.chunkBytes = std::numeric_limits<std::size_t>::max();
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::INVALID_ARGUMENT);
    limits = {};
    limits.compressedBytes = std::numeric_limits<std::uint64_t>::max();
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::INVALID_ARGUMENT);
    limits = {};
    limits.expansionRatio = 0;
    limits.ratioSlack = 0;
    CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.error == ti::ContainerError::RATIO_LIMIT);
}

TEST_CASE("borrowed input remains open and no work happens after terminal results") {
    std::istringstream physical(deflateBytes("payload", true));
    ti::CompressedInput input(physical, ti::ContainerKind::GZIP);
    CHECK(input.finish() == ti::ContainerError::NOT_FINISHED);
    CHECK(input.read({}).error == ti::ContainerError::INVALID_ARGUMENT);
    CHECK(input.counters().compressedPhysicalRead == 0);
    auto gzip = deflateBytes("payload", true);
    auto failure = decode(gzip, ti::ContainerKind::GZIP, {}, 1, 1, gzip.size() - 4);
    CHECK(failure.final.error == ti::ContainerError::SOURCE_IO);
    CHECK(failure.bytes == "payload");
    SourceBuffer buffer(gzip, 1);
    std::istream source(&buffer);
    std::size_t calls = 0;
    {
        ti::ContainerLimits limits;
        limits.chunkBytes = 1;
        ti::CompressedInput decoder(source, ti::ContainerKind::GZIP, limits);
        std::array<std::byte, 1> byte{};
        CHECK(decoder.read(byte).produced == 1);
        calls = buffer.ioCalls();
    }
    CHECK(buffer.ioCalls() == calls);
    CHECK(source.rdbuf()->sgetc() != std::char_traits<char>::eof());
}

TEST_CASE("exact work call header ratio and allocation boundaries remain finite") {
    const auto gzip = deflateBytes("123456789", true);
    const auto reference = decode(gzip, ti::ContainerKind::GZIP, {}, 1);
    REQUIRE(reference.final.physicalComplete);

    for (int delta : {-1, 0, 1}) {
        ti::ContainerLimits limits;
        limits.inflateCalls = reference.counters.inflateCalls + delta;
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.workBytes = reference.counters.workBytes + delta;
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.headerBytes = 10 + delta;
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.totalHeaderBytes = 10 + delta;
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.expansionRatio = 0;
        limits.ratioSlack = 9 + delta;
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
    }

    for (bool inflater : {false, true}) {
        std::size_t lower = 0;
        std::size_t upper = 1048576;

        while (lower < upper) {
            const auto middle = lower + (upper - lower) / 2;
            ti::ContainerLimits limits;

            if (inflater) {
                limits.inflaterBytes = middle;
            } else {
                limits.retainedBytes = middle;
            }

            const auto result = decode(gzip, ti::ContainerKind::GZIP, limits);

            if (result.final.physicalComplete) {
                upper = middle;
            } else {
                CHECK(result.final.error == ti::ContainerError::ALLOCATION);
                lower = middle + 1;
            }
        }

        REQUIRE(lower != 0);

        for (int delta : {-1, 0, 1}) {
            ti::ContainerLimits limits;

            if (inflater) {
                limits.inflaterBytes = lower + delta;
            } else {
                limits.retainedBytes = lower + delta;
            }

            CHECK(decode(gzip, ti::ContainerKind::GZIP, limits).final.physicalComplete == (delta >= 0));
        }
    }
}

TEST_CASE("every truncated container prefix fails without COMPLETE") {
    for (const auto &pair : {std::pair{deflateBytes("finite", true), ti::ContainerKind::GZIP},
                             std::pair{zipBytes({{"x", "finite"}}), ti::ContainerKind::ZIP}}) {
        for (std::size_t size = 0; size < pair.first.size(); ++size) {
            CAPTURE(size);
            CHECK_FALSE(decode(pair.first.substr(0, size), pair.second, {}, 1).final.physicalComplete);
        }
    }
}

TEST_CASE("GZIP optional metadata CRC and finite headers are validated") {
    const auto gzip = deflateBytes("optional metadata", true);
    auto header = gzip.substr(0, 10);
    header[3] = 0x1e;
    header += std::string("\3\0abcname\0comment\0", 18);
    const auto checksum = crc32_z(0, reinterpret_cast<const Bytef *>(header.data()), header.size());
    le(header, checksum & 0xffff, 2);
    const auto wrapped = header + gzip.substr(10);

    for (auto chunk : {1U, 4096U}) {
        auto result = decode(wrapped, ti::ContainerKind::GZIP, {}, chunk);
        REQUIRE(result.final.physicalComplete);
        CHECK(result.bytes == "optional metadata");

        for (int delta : {-1, 0, 1}) {
            ti::ContainerLimits limits;
            limits.headerBytes = header.size() + delta;
            CHECK(decode(wrapped, ti::ContainerKind::GZIP, limits, chunk).final.physicalComplete == (delta >= 0));
        }

        auto changed = wrapped;
        changed[header.size() - 1] ^= 1;
        CHECK(decode(changed, ti::ContainerKind::GZIP, {}, chunk).final.error == ti::ContainerError::INVALID_HEADER);
        changed = wrapped;
        changed[3] |= 0x20;
        CHECK(decode(changed, ti::ContainerKind::GZIP, {}, chunk).final.error == ti::ContainerError::INVALID_HEADER);
    }
}

TEST_CASE("large finite limits do not preallocate their maximum allowance") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    ti::ContainerLimits limits;
    limits.gzipMembers = maximum;
    limits.zipEntries = maximum;
    limits.headerBytes = maximum;
    limits.totalHeaderBytes = maximum;
    limits.workBytes = maximum;
    limits.inflateCalls = maximum;
    limits.expansionRatio = maximum;
    limits.ratioSlack = maximum;
    limits.retainedBytes = std::numeric_limits<std::size_t>::max();
    limits.inflaterBytes = std::numeric_limits<std::size_t>::max();
    CHECK(decode(deflateBytes("finite", true), ti::ContainerKind::GZIP, limits).final.physicalComplete);
    CHECK(decode(zipBytes({{"x", "finite"}}), ti::ContainerKind::ZIP, limits).final.physicalComplete);
}

TEST_CASE("ZIP exact work call and entry budgets accept their full allowance") {
    const auto zip = zipBytes({{"before/", ""}, {"x", "payload"}, {"after/", ""}});
    const auto reference = decode(zip, ti::ContainerKind::ZIP, {}, 1);
    REQUIRE(reference.final.physicalComplete);

    for (int delta : {-1, 0, 1}) {
        ti::ContainerLimits limits;
        limits.workBytes = reference.counters.workBytes + delta;
        CHECK(decode(zip, ti::ContainerKind::ZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.inflateCalls = reference.counters.inflateCalls + delta;
        CHECK(decode(zip, ti::ContainerKind::ZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.zipEntries = 3 + delta;
        CHECK(decode(zip, ti::ContainerKind::ZIP, limits, 1).final.physicalComplete == (delta >= 0));
        limits = {};
        limits.gzipMembers = 3 + delta;
        const auto gzip = deflateBytes("", true) + deflateBytes("payload", true) + deflateBytes("", true);
        CHECK(decode(gzip, ti::ContainerKind::GZIP, limits, 1).final.physicalComplete == (delta >= 0));
    }
}

TEST_CASE("ZIP64 extras unsigned descriptors sizes and directory payload are rejected explicitly") {
    auto zip = zipBytes({{"x", "payload"}});
    auto changed = zip;
    changed[28] = 4;
    changed.insert(31, std::string("\1\0\0\0", 4));
    CHECK(decode(changed, ti::ContainerKind::ZIP).final.error == ti::ContainerError::UNSUPPORTED);
    changed = zip;
    const auto descriptor = changed.find("PK\7\10");
    REQUIRE(descriptor != std::string::npos);
    changed.erase(descriptor, 4);
    CHECK(decode(changed, ti::ContainerKind::ZIP).final.error == ti::ContainerError::UNSUPPORTED);
    changed = zip;
    changed[descriptor + 8] ^= 1;
    CHECK(decode(changed, ti::ContainerKind::ZIP).final.error == ti::ContainerError::INVALID_DIRECTORY);
    CHECK(decode(zipBytes({{"dir/", "forbidden"}, {"x", "payload"}}), ti::ContainerKind::ZIP).final.error ==
          ti::ContainerError::INVALID_DIRECTORY);
    CHECK(decode(zip + zip, ti::ContainerKind::ZIP).final.error == ti::ContainerError::INVALID_DIRECTORY);
}

TEST_CASE("ZIP EOCD split and multidisk markers are unsupported rather than malformed") {
    const auto zip = zipBytes({{"x", "completed prefix"}});
    const auto end = zip.rfind("PK\5\6");
    REQUIRE(end != std::string::npos);

    for (auto fieldOffset : {4U, 6U}) {
        auto changed = zip;
        changed[end + fieldOffset] = 1;
        auto result = decode(changed, ti::ContainerKind::ZIP, {}, 1);
        CHECK(result.final.error == ti::ContainerError::UNSUPPORTED);
        CHECK(result.bytes == "completed prefix");
        CHECK_FALSE(result.final.physicalComplete);
    }
}

TEST_CASE("standard production Quote bytes survive both envelopes unchanged") {
    for (auto pin : {"baseline", "current"}) {
        const auto path = std::string(ORG_TTLDTOR_QDCXX_CONTAINER_FIXTURES) + "/../real-tape/producer/" + pin +
                          "/expected/quote-none.qtp";
        std::ifstream file(path, std::ios::binary);
        REQUIRE(file.good());
        const std::string wire{std::istreambuf_iterator<char>(file), {}};
        REQUIRE_FALSE(wire.empty());
        auto gzip = decode(deflateBytes(wire, true), ti::ContainerKind::GZIP, {}, 1, 1);
        CHECK(gzip.final.physicalComplete);
        CHECK(gzip.bytes == wire);
        auto zip = decode(zipBytes({{"Quote.qtp", wire}}), ti::ContainerKind::ZIP, {}, 1, 1);
        CHECK(zip.final.physicalComplete);
        CHECK(zip.bytes == wire);
    }
}

TEST_CASE("production baseline and current compressed files decode exact NONE payload") {
    for (auto pin : {"baseline", "current"}) {
        const auto prefix = std::string(ORG_TTLDTOR_QDCXX_CONTAINER_FIXTURES) + "/" + pin + "/";
        std::ifstream plain(prefix + "none.qtp", std::ios::binary);
        REQUIRE(plain.good());
        const std::string expected{std::istreambuf_iterator<char>(plain), {}};

        for (auto pair :
             {std::pair{"tape.qtp.gz", ti::ContainerKind::GZIP}, std::pair{"tape.qtp.zip", ti::ContainerKind::ZIP}}) {
            std::ifstream file(prefix + pair.first, std::ios::binary);
            REQUIRE(file.good());
            const std::string source{std::istreambuf_iterator<char>(file), {}};
            auto result = decode(source, pair.second, {}, 1, 1);
            CHECK(result.final.physicalComplete);
            CHECK(result.bytes == expected);
        }
    }
}
