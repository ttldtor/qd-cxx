// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "compressed_input.hpp"

#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
#    include <zlib.h>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
constexpr std::uint32_t ZIP_LOCAL = 0x04034b50;
constexpr std::uint32_t ZIP_CENTRAL = 0x02014b50;
constexpr std::uint32_t ZIP_END = 0x06054b50;
constexpr std::uint32_t ZIP_DESCRIPTOR = 0x08074b50;
constexpr std::uint16_t ZIP_DESCRIPTOR_FLAG = 8;
constexpr std::uint16_t ZIP_ALLOWED_FLAGS = 0x080e;
constexpr std::uint16_t ZIP_DEFLATE = 8;
constexpr std::uint16_t ZIP_STORED = 0;
constexpr std::uint16_t ZIP64_EXTRA = 1;
constexpr std::uint16_t ZIP32_VERSION = 20;
constexpr std::uint64_t ZIP32_SENTINEL = 0xffffffff;
constexpr int INFLATE_BLOCK_BOUNDARY = 128;
constexpr int INFLATE_FINAL_BLOCK = 64;
constexpr int GZIP_WINDOW_BITS = 31; ///< 15-bit deflate window plus the gzip wrapper selection.
constexpr int RAW_WINDOW_BITS = -15; ///< ZIP carries the wrapper; decode only raw deflate.

#endif

/** Overflow-safe allowance check.
 * @param used Irreversibly charged count.
 * @param count Proposed additional charge.
 * @param limit Finite configured capacity.
 * @return Whether the full charge fits without overflow.
 */
bool fits(std::uint64_t used, std::uint64_t count, std::uint64_t limit) noexcept {
    return used <= limit && count <= limit - used;
}
} // namespace

/** Private synchronous state; owning allocations never escape into protocol or caller storage. */
struct CompressedInput::State {
    /** Envelope progress; DONE requires physical EOF validation. */
    enum class Phase {
        START,    ///< Initial envelope or next GZIP member.
        PAYLOAD,  ///< Current payload and integrity validation.
        ZIP_NEXT, ///< Next local entry or final central directory.
        DONE      ///< Full physical closure; future reads perform no I/O.
    };

#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
    /** Owned local-entry facts retained until central-directory cross-check. */
    struct Entry {
        std::unique_ptr<unsigned char[]> name; ///< Exact owned opaque name bytes; never a path.
        std::size_t nameSize = 0;              ///< Name bytes, without an implicit terminator or text conversion.
        std::uint32_t crc = 0;                 ///< Actual decoded CRC32.
        std::uint64_t compressed = 0;          ///< Actual raw-deflate/stored input size.
        std::uint64_t inflated = 0;            ///< Actual decoded payload size.
        std::uint64_t offset = 0;              ///< Physical local-header signature offset.
        std::uint16_t method = 0;              ///< Admitted stored/deflate method.
        std::uint16_t flags = 0;               ///< Admitted ZIP general-purpose flags.
    };

    /** Aligned prefix charged together with each zlib allocation. */
    struct alignas(std::max_align_t) Allocation {
        std::size_t bytes; ///< Allocation size including this aligned prefix.
    };

#endif

    std::istream &physical;                                   ///< Borrowed until destruction; never closed here.
    ContainerKind kind;                                       ///< Explicit selected envelope.
    ContainerLimits limits;                                   ///< Instance-owned finite configuration.
    ContainerCounters counts;                                 ///< Irreversible observed/admitted coordinate charges.
    ContainerError error = ContainerError::NONE;              ///< First terminal failure.
    Phase phase = Phase::START;                               ///< Progress independent from QTP parsing.
    std::vector<unsigned char> input;                         ///< Owned bounded canonical chunk.
    std::size_t position = 0;                                 ///< Consumed scratch prefix.
    std::size_t available = 0;                                ///< Filled scratch length, possibly short at EOF/failure.
    bool physicalEof = false;                                 ///< Actual no-byte EOF probe completed.
    ContainerError pendingSourceError = ContainerError::NONE; ///< Deferred behind buffered input.
    std::size_t retained = 0;                                 ///< Charged scratch/entry-slot/name capacity.
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
    z_stream inflater{};                ///< Owned decoder; borrowed pointers cleared after each call.
    gz_header gzipHeader{};             ///< Nonretaining header-progress observation.
    bool initialized = false;           ///< Whether inflateEnd is required.
    std::size_t allocations = 0;        ///< Outstanding zlib bytes, including aligned headers.
    std::unique_ptr<Entry[]> entries;   ///< Bounded local facts for central validation.
    std::size_t entryCount = 0;         ///< Validated local entries retained so far.
    std::size_t entryCapacity = 0;      ///< Actual owned entry slots; grows only for observed entries.
    Entry current;                      ///< Current local facts, moved into entries after validation.
    std::uint32_t crc = 0;              ///< Current decoded CRC32.
    std::uint64_t payloadInput = 0;     ///< Decoder input, gzip wrapper adjusted for ratio.
    std::uint64_t payloadOutput = 0;    ///< Decoded output before publication.
    std::uint64_t localHeader = 0;      ///< Current header/EOCD metadata-record charge.
    std::uint64_t localCompressed = 0;  ///< Local ZIP size, zero if deferred.
    std::uint64_t localInflated = 0;    ///< Local ZIP output size, zero if deferred.
    std::uint32_t localCrc = 0;         ///< Local ZIP CRC, zero if deferred.
    bool directory = false;             ///< Current ZIP payload must be empty.
    bool fileSeen = false;              ///< A second file is outside the profile.
    bool gzipDeflateEnd = false;        ///< Last raw block reached before trailer.
    std::uint64_t gzipDeflateBytes = 0; ///< Raw size fixed at that boundary.

#endif

    /** Allocate bounded state before source I/O.
     * @param stream Borrowed physical source.
     * @param selected Explicit container kind.
     * @param bounds Validated finite capacities.
     */
    State(std::istream &stream, ContainerKind selected, ContainerLimits bounds)
        : physical(stream), kind(selected), limits(bounds), input(bounds.chunkBytes), retained(bounds.chunkBytes) {
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
        inflater.zalloc = allocate;
        inflater.zfree = release;
        inflater.opaque = this;

        if (kind != ContainerKind::NONE) {
            initialize(kind == ContainerKind::GZIP ? GZIP_WINDOW_BITS : RAW_WINDOW_BITS);
        }

#endif
    }

    /** Release allocations without draining or closing the borrowed source. */
    ~State() {
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB

        if (initialized) {
            inflateEnd(&inflater);
        }

#endif
    }

#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
    /** Allocate zeroed decoder memory with an aligned size header.
     * @param opaque Owning decoder state.
     * @param items Number of allocation elements.
     * @param size Element bytes.
     * @return Owned aligned memory, or null on overflow/capacity/allocation failure.
     */
    static voidpf allocate(voidpf opaque, uInt items, uInt size) noexcept {
        auto &self = *static_cast<State *>(opaque);
        const auto count = static_cast<std::size_t>(items);
        const auto unit = static_cast<std::size_t>(size);

        if (unit != 0 && count > (std::numeric_limits<std::size_t>::max() - sizeof(Allocation)) / unit) {
            return nullptr;
        }

        const auto bytes = count * unit + sizeof(Allocation);

        if (!fits(self.allocations, bytes, self.limits.inflaterBytes)) {
            return nullptr;
        }

        auto *block = static_cast<Allocation *>(std::calloc(1, bytes));

        if (block == nullptr) {
            return nullptr;
        }

        block->bytes = bytes;
        self.allocations += bytes;

        return block + 1;
    }

    /** Release one decoder allocation and its exact outstanding charge.
     * @param opaque Owning decoder state.
     * @param address Previously allocated memory, or null.
     */
    static void release(voidpf opaque, voidpf address) noexcept {
        if (address == nullptr) {
            return;
        }

        auto &self = *static_cast<State *>(opaque);
        auto *block = static_cast<Allocation *>(address) - 1;
        self.allocations -= block->bytes;
        std::free(block);
    }

#endif

    /** Preserve the first terminal outcome.
     * @param reason Typed failure, never NONE.
     * @return False for propagation through guarded decode operations.
     */
    bool fail(ContainerError reason) noexcept {
        if (error == ContainerError::NONE) {
            error = reason;
        }

        return false;
    }

    /** Debit actual byte work without refund or overflow.
     * @param bytes Newly observed byte work.
     * @return Whether the whole debit fits.
     */
    bool work(std::uint64_t bytes) noexcept {
        if (!fits(counts.workBytes, bytes, limits.workBytes)) {
            return fail(ContainerError::WORK_LIMIT);
        }

        counts.workBytes += bytes;

        return true;
    }

#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
    /** Reserve/debit one metadata slice before scanning it.
     * @param bytes Required scan bytes.
     * @param record Current metadata record's cumulative charge.
     * @return Whether per-record, cumulative and work allowances permit the slice.
     */
    bool header(std::uint64_t bytes, std::uint64_t &record) noexcept {
        if (!fits(record, bytes, limits.headerBytes) || !fits(counts.headerBytes, bytes, limits.totalHeaderBytes)) {
            return fail(ContainerError::HEADER_LIMIT);
        }

        if (!work(bytes)) {
            return false;
        }

        record += bytes;
        counts.headerBytes += bytes;

        return true;
    }

#endif

    /** Aggregate a bounded physical chunk; defer a late source failure behind its buffered prefix.
     * @return Whether buffered bytes are available; otherwise EOF or sticky failure is recorded.
     */
    bool fill() {
        if (position != available) {
            return true;
        }

        if (physicalEof || error != ContainerError::NONE) {
            return false;
        }

        if (pendingSourceError != ContainerError::NONE) {
            return fail(pendingSourceError);
        }

        const auto remaining = limits.compressedBytes - counts.compressedPhysicalRead;
        const auto target =
            static_cast<std::size_t>(std::min<std::uint64_t>(input.size(), remaining == 0 ? 1 : remaining));
        position = 0;
        available = 0;

        // Aggregate short physical reads into a canonical bounded input chunk. Preserve a buffered
        // prefix before surfacing a later source failure, without ever retrying after that failure.
        while (available < target && !physicalEof && pendingSourceError == ContainerError::NONE) {
            if (limits.workBytes == 0) {
                pendingSourceError = ContainerError::WORK_LIMIT;
                break;
            }

            const auto remainingWork = limits.workBytes - counts.workBytes;
            const auto request = static_cast<std::size_t>(
                std::min<std::uint64_t>(target - available, remainingWork == 0 ? 1 : remainingWork));

            std::streamsize count = 0;
            try {
                physical.read(reinterpret_cast<char *>(input.data() + available),
                              static_cast<std::streamsize>(request));
                count = physical.gcount();
            } catch (const std::ios_base::failure &) {
                count = physical.gcount();

                if (physical.bad() || !physical.eof()) {
                    pendingSourceError = ContainerError::SOURCE_IO;
                    break;
                }
            } catch (...) {
                pendingSourceError = ContainerError::SOURCE_IO;
                break;
            }

            if (physical.bad() || (physical.fail() && !physical.eof()) || count < 0) {
                pendingSourceError = ContainerError::SOURCE_IO;
                break;
            }

            const auto bytes = static_cast<std::size_t>(count);
            counts.compressedPhysicalRead += bytes;

            if (remaining == 0 && bytes != 0) {
                return fail(ContainerError::COMPRESSED_LIMIT);
            }

            if (!fits(counts.workBytes, bytes, limits.workBytes)) {
                pendingSourceError = ContainerError::WORK_LIMIT;
                break;
            }

            counts.workBytes += bytes;

            available += bytes;

            if (physical.eof()) {
                physical.clear(physical.rdstate() & ~(std::ios::eofbit | std::ios::failbit));

                if (bytes == 0) {
                    try {
                        physicalEof = physical.rdbuf()->sgetc() == std::char_traits<char>::eof();
                    } catch (...) {
                        pendingSourceError = ContainerError::SOURCE_IO;
                    }
                }
            }

            if (bytes == 0 && !physicalEof) {
                pendingSourceError = ContainerError::SOURCE_IO;
            }
        }

        if (available == 0 && pendingSourceError != ContainerError::NONE) {
            return fail(pendingSourceError);
        }

        return available != 0;
    }

#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
    /** Consume one required physical byte.
     * @param value Receives the byte only on success.
     * @return False on truncated source or an earlier resource/I/O failure.
     */
    bool byte(unsigned char &value) {
        if (!fill()) {
            if (error == ContainerError::NONE) {
                fail(ContainerError::TRUNCATED);
            }

            return false;
        }

        value = input[position++];
        ++counts.compressedConsumed;

        return true;
    }

    /** Read a checked little-endian metadata integer.
     * @param value Receives the parsed value.
     * @param width Format-fixed size, at most four bytes.
     * @param record Current metadata-record byte charge.
     * @return Whether the entire required field was available.
     */
    bool number(std::uint32_t &value, unsigned width, std::uint64_t &record) {
        if (!header(width, record)) {
            return false;
        }

        value = 0;

        for (unsigned index = 0; index < width; ++index) {
            unsigned char item = 0;

            if (!byte(item)) {
                return false;
            }

            value |= static_cast<std::uint32_t>(item) << (8 * index);
        }

        return true;
    }

    /** Scan opaque metadata without retaining its contents.
     * @param size Required bytes.
     * @param record Current metadata-record byte charge.
     * @return Whether the complete bounded slice was consumed.
     */
    bool skip(std::uint32_t size, std::uint64_t &record) {
        if (!header(size, record)) {
            return false;
        }

        for (std::uint32_t index = 0; index < size; ++index) {
            unsigned char ignored = 0;

            if (!byte(ignored)) {
                return false;
            }
        }

        return true;
    }

    /** Validate ZIP extra-field framing and reject ZIP64 before interpreting it.
     * @param bytes Exact extra-region size.
     * @param record Current metadata-record byte charge.
     * @return Whether all bounded opaque extra fields are within this profile.
     */
    bool extra(std::uint32_t bytes, std::uint64_t &record) {
        while (bytes != 0) {
            if (bytes < 4) {
                return fail(ContainerError::INVALID_HEADER);
            }

            std::uint32_t tag = 0;
            std::uint32_t length = 0;

            if (!number(tag, 2, record) || !number(length, 2, record)) {
                return false;
            }

            bytes -= 4;

            if (tag == ZIP64_EXTRA) {
                return fail(ContainerError::UNSUPPORTED);
            }

            if (length > bytes) {
                return fail(ContainerError::INVALID_HEADER);
            }

            if (!skip(length, record)) {
                return false;
            }

            bytes -= length;
        }

        return true;
    }

    /** Retain a local name or compare the central name without allocating another copy.
     * @param entry Retained local facts owning the exact name bytes.
     * @param size Required encoded byte length.
     * @param record Current metadata-record charge.
     * @param keep True for initial ownership; false for exact central comparison.
     * @return Whether the bounded name was read/compared successfully.
     */
    bool name(Entry &entry, std::uint32_t size, std::uint64_t &record, bool keep) {
        if (!header(size, record)) {
            return false;
        }

        if (keep) {
            if (!fits(retained, size, limits.retainedBytes)) {
                return fail(ContainerError::ALLOCATION);
            }

            retained += size;
            entry.name = size == 0 ? nullptr : std::make_unique<unsigned char[]>(size);
            entry.nameSize = size;
        }

        for (std::uint32_t index = 0; index < size; ++index) {
            unsigned char value = 0;

            if (!byte(value)) {
                return false;
            }

            if (keep) {
                entry.name[index] = value;
            } else if (index >= entry.nameSize || value != entry.name[index]) {
                return fail(ContainerError::INVALID_DIRECTORY);
            }
        }

        return keep || size == entry.nameSize || fail(ContainerError::INVALID_DIRECTORY);
    }

    /** Initialize/reset owned zlib state without retaining physical or caller output pointers.
     * @param window Explicit gzip or raw-deflate mode.
     * @return Whether bounded decoder allocation and initialization succeeded.
     */
    bool initialize(int window) {
        const auto result = initialized ? inflateReset2(&inflater, window) : inflateInit2(&inflater, window);

        if (result != Z_OK) {
            return fail(result == Z_MEM_ERROR ? ContainerError::ALLOCATION : ContainerError::INVALID_DATA);
        }

        initialized = true;
        payloadInput = 0;
        payloadOutput = 0;
        gzipDeflateEnd = false;
        gzipDeflateBytes = 0;
        crc = crc32(0, Z_NULL, 0);

        if (kind == ContainerKind::GZIP) {
            gzipHeader = {};
            localHeader = 0;

            if (inflateGetHeader(&inflater, &gzipHeader) != Z_OK) {
                return fail(ContainerError::INVALID_HEADER);
            }
        }

        return true;
    }

    /** Start the next bounded member, or authenticate EOF between completed members.
     * @return Whether the state transition succeeded.
     */
    bool beginGzip() {
        if (!fill()) {
            if (error != ContainerError::NONE) {
                return false;
            }

            if (counts.members == 0) {
                return fail(ContainerError::TRUNCATED);
            }

            phase = Phase::DONE;

            return true;
        }

        if (counts.members == limits.gzipMembers) {
            return fail(ContainerError::MEMBER_ENTRY_LIMIT);
        }

        ++counts.members;
        phase = Phase::PAYLOAD;

        return initialize(GZIP_WINDOW_BITS);
    }

    /** Grow entry storage only for an observed entry, bounding the old/new transient peak.
     * @return Whether another slot is available within retained and move-work allowances.
     */
    bool reserveEntry() {
        static_assert(std::is_nothrow_move_assignable_v<Entry>);

        if (entryCount < entryCapacity) {
            return true;
        }

        const auto maximum =
            std::min<std::uint64_t>(limits.zipEntries, std::numeric_limits<std::size_t>::max() / sizeof(Entry));
        const auto doubled = entryCapacity > maximum / 2 ? maximum : entryCapacity * 2;
        const auto capacity =
            static_cast<std::size_t>(std::min<std::uint64_t>(maximum, entryCapacity == 0 ? 1 : doubled));

        if (capacity <= entryCapacity || !fits(retained, capacity * sizeof(Entry), limits.retainedBytes)) {
            return fail(ContainerError::ALLOCATION);
        }

        if (!work(entryCount * sizeof(Entry))) {
            return false;
        }

        auto replacement = std::make_unique<Entry[]>(capacity);

        for (std::size_t index = 0; index < entryCount; ++index) {
            replacement[index] = std::move(entries[index]);
        }

        entries = std::move(replacement);
        retained += (capacity - entryCapacity) * sizeof(Entry);
        entryCapacity = capacity;

        return true;
    }

    /** Validate the next local entry before exposing its payload.
     * @param signature Already consumed local-header signature.
     * @return Whether the selected single-file profile admits the entry.
     */
    bool beginZip(std::uint32_t signature) {
        if (signature != ZIP_LOCAL) {
            return fail(ContainerError::INVALID_HEADER);
        }

        if (counts.members == limits.zipEntries) {
            return fail(ContainerError::MEMBER_ENTRY_LIMIT);
        }

        if (!reserveEntry()) {
            return false;
        }

        ++counts.members;
        current = {};
        current.offset = counts.compressedConsumed - 4;
        std::array<std::uint32_t, 10> fields{};
        constexpr std::array<unsigned, 10> WIDTHS{2, 2, 2, 2, 2, 4, 4, 4, 2, 2};

        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (!number(fields[index], WIDTHS[index], localHeader)) {
                return false;
            }
        }

        const auto [version, flags, method, modifiedTime, modifiedDate, expectedCrc, compressedSize, inflatedSize,
                    nameLength, extraLength] = fields;
        current.flags = static_cast<std::uint16_t>(flags);
        current.method = static_cast<std::uint16_t>(method);
        localCrc = expectedCrc;
        localCompressed = compressedSize;
        localInflated = inflatedSize;

        if (version > ZIP32_VERSION || (current.flags & ~ZIP_ALLOWED_FLAGS) != 0 ||
            (current.method != ZIP_STORED && current.method != ZIP_DEFLATE) || localCompressed == ZIP32_SENTINEL ||
            localInflated == ZIP32_SENTINEL ||
            (current.method == ZIP_STORED && (current.flags & ZIP_DESCRIPTOR_FLAG) != 0)) {
            return fail(ContainerError::UNSUPPORTED);
        }

        if (!name(current, nameLength, localHeader, true) || !extra(extraLength, localHeader)) {
            return false;
        }

        directory = current.nameSize != 0 && current.name[current.nameSize - 1] == '/';

        if (!directory) {
            if (fileSeen) {
                return fail(ContainerError::UNSUPPORTED);
            }

            fileSeen = true;
        }

        phase = Phase::PAYLOAD;
        payloadInput = 0;
        payloadOutput = 0;
        crc = crc32(0, Z_NULL, 0);

        if (current.method == ZIP_STORED) {
            if (localCompressed != localInflated) {
                return fail(ContainerError::INVALID_HEADER);
            }

            return true;
        }

        return initialize(RAW_WINDOW_BITS);
    }

    /** Validate a completed payload and its optional signed descriptor; retain bounded metadata.
     * @return Whether CRC, sizes and empty-directory policy match the decoded payload.
     */
    bool endZipEntry() {
        auto expectedCrc = localCrc;
        auto expectedInput = localCompressed;
        auto expectedOutput = localInflated;

        if ((current.flags & ZIP_DESCRIPTOR_FLAG) != 0) {
            std::uint32_t signature = 0;
            std::uint32_t compressed = 0;
            std::uint32_t inflated = 0;
            std::uint64_t descriptor = 0;

            if (!number(signature, 4, descriptor) || signature != ZIP_DESCRIPTOR) {
                return error != ContainerError::NONE ? false : fail(ContainerError::UNSUPPORTED);
            }

            if (!number(expectedCrc, 4, descriptor) || !number(compressed, 4, descriptor) ||
                !number(inflated, 4, descriptor)) {
                return false;
            }

            expectedInput = compressed;
            expectedOutput = inflated;

            if ((localCrc != 0 && localCrc != expectedCrc) ||
                (localCompressed != 0 && localCompressed != expectedInput) ||
                (localInflated != 0 && localInflated != expectedOutput)) {
                return fail(ContainerError::INVALID_DIRECTORY);
            }
        }

        if (expectedCrc != crc) {
            return fail(ContainerError::CHECKSUM);
        }

        if (expectedInput != payloadInput || expectedOutput != payloadOutput || (directory && payloadOutput != 0)) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        current.crc = crc;
        current.compressed = payloadInput;
        current.inflated = payloadOutput;
        entries[entryCount++] = std::move(current);
        phase = Phase::ZIP_NEXT;

        return true;
    }

    /** Cross-check every central entry, EOCD and physical EOF without seeking or extraction.
     * @param start Physical offset of the first central signature, already consumed.
     * @return Whether the entire single-disk archive closes successfully.
     */
    bool zipDirectory(std::uint64_t start) {
        if (!fileSeen) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        for (std::size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            auto &entry = entries[entryIndex];
            std::uint64_t record = 4;
            std::array<std::uint32_t, 16> fields{};
            constexpr std::array<unsigned, 16> WIDTHS{2, 2, 2, 2, 2, 2, 4, 4, 4, 2, 2, 2, 2, 2, 4, 4};

            for (std::size_t index = 0; index < fields.size(); ++index) {
                if (!number(fields[index], WIDTHS[index], record)) {
                    return false;
                }
            }

            const auto [creatorVersion, version, flags, method, modifiedTime, modifiedDate, expectedCrc, compressedSize,
                        inflatedSize, nameLength, extraLength, commentLength, disk, internalAttributes,
                        externalAttributes, localOffset] = fields;

            if (version > ZIP32_VERSION || disk != 0 || compressedSize == ZIP32_SENTINEL ||
                inflatedSize == ZIP32_SENTINEL || localOffset == ZIP32_SENTINEL) {
                return fail(ContainerError::UNSUPPORTED);
            }

            if (flags != entry.flags || method != entry.method || expectedCrc != entry.crc ||
                compressedSize != entry.compressed || inflatedSize != entry.inflated || localOffset != entry.offset ||
                !name(entry, nameLength, record, false) || !extra(extraLength, record) ||
                !skip(commentLength, record)) {
                return error != ContainerError::NONE ? false : fail(ContainerError::INVALID_DIRECTORY);
            }

            std::uint32_t signature = 0;
            localHeader = 0;

            if (!number(signature, 4, localHeader)) {
                return false;
            }

            const auto last = entryIndex + 1 == entryCount;

            if (signature != (last ? ZIP_END : ZIP_CENTRAL)) {
                return fail(ContainerError::INVALID_DIRECTORY);
            }
        }

        const auto centralSize = counts.compressedConsumed - 4 - start;
        std::array<std::uint32_t, 7> fields{};
        constexpr std::array<unsigned, 7> WIDTHS{2, 2, 2, 2, 4, 4, 2};

        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (!number(fields[index], WIDTHS[index], localHeader)) {
                return false;
            }
        }

        const auto [disk, centralDisk, diskEntries, totalEntries, expectedSize, expectedOffset, commentLength] = fields;

        if (disk != 0 || centralDisk != 0) {
            return fail(ContainerError::UNSUPPORTED);
        }

        if (diskEntries != entryCount || totalEntries != entryCount || expectedSize != centralSize ||
            expectedOffset != start) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        if (!skip(commentLength, localHeader)) {
            return false;
        }

        if (fill()) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        if (error != ContainerError::NONE) {
            return false;
        }

        phase = Phase::DONE;

        return true;
    }

    /** Dispatch the next ZIP signature without scanning payload bytes for it.
     * @return Whether a local-entry or final-directory transition succeeded.
     */
    bool zipNext() {
        std::uint32_t signature = 0;
        localHeader = 0;

        if (!number(signature, 4, localHeader)) {
            return false;
        }

        if (signature == ZIP_CENTRAL) {
            return zipDirectory(counts.compressedConsumed - 4);
        }

        if (signature == ZIP_END) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        return beginZip(signature);
    }

    /** Check expansion using division rather than an overflow-prone ratio multiplication.
     * @param inputBytes Consumed member deflate bytes, excluding wrapper metadata.
     * @param outputBytes Produced member payload bytes.
     * @return Whether ratio and initial slack permit the output.
     */
    bool ratio(std::uint64_t inputBytes, std::uint64_t outputBytes) noexcept {
        if (outputBytes <= limits.ratioSlack) {
            return true;
        }

        const auto required = outputBytes - limits.ratioSlack;

        if (limits.expansionRatio == 0 ||
            inputBytes < required / limits.expansionRatio + (required % limits.expansionRatio != 0 ? 1 : 0)) {
            return fail(ContainerError::RATIO_LIMIT);
        }

        return true;
    }

    /** Advance one bounded payload step, including zero-output trailer validation.
     * @param output Caller-owned scratch, retained only during this read call.
     * @param produced Published prefix length, updated before late integrity failure.
     * @return Whether decoding may continue without a terminal error.
     */
    bool payload(std::span<std::byte> output, std::size_t &produced) {
        const auto allowance = limits.inflatedBytes - counts.logicalProduced;

        if (limits.workBytes == 0) {
            return fail(ContainerError::WORK_LIMIT);
        }

        if (kind == ContainerKind::ZIP && current.method == ZIP_STORED && payloadInput == localCompressed) {
            return endZipEntry();
        }

        const auto hasInput = fill();

        if (!hasInput && error != ContainerError::NONE) {
            return false;
        }

        auto sourceSize = hasInput ? available - position : 0;
        unsigned char excess = 0;
        const auto remainingWork = limits.workBytes - counts.workBytes;
        auto capacity =
            static_cast<std::size_t>(std::min<std::uint64_t>({output.size() - produced, allowance, remainingWork}));
        capacity = std::min<std::size_t>(capacity, std::numeric_limits<unsigned int>::max());
        auto *destination = capacity == 0 ? &excess : reinterpret_cast<unsigned char *>(output.data() + produced);
        const auto reserved = capacity == 0 ? std::size_t{1} : capacity;

        if (kind == ContainerKind::GZIP && gzipHeader.done == 0) {
            sourceSize = std::min<std::size_t>(sourceSize, 1);

            if (!fits(localHeader, sourceSize, limits.headerBytes) ||
                !fits(counts.headerBytes, sourceSize, limits.totalHeaderBytes) ||
                !fits(counts.workBytes, sourceSize, limits.workBytes)) {
                return fail(!fits(counts.workBytes, sourceSize, limits.workBytes) ? ContainerError::WORK_LIMIT
                                                                                  : ContainerError::HEADER_LIMIT);
            }
        }

        if (kind == ContainerKind::ZIP && (current.flags & ZIP_DESCRIPTOR_FLAG) == 0) {
            sourceSize = static_cast<std::size_t>(std::min<std::uint64_t>(sourceSize, localCompressed - payloadInput));
        }

        std::size_t consumed = 0;
        std::size_t emitted = 0;
        int result = Z_OK;
        const auto gzipMetadata = kind == ContainerKind::GZIP && gzipHeader.done == 0;

        if (kind == ContainerKind::ZIP && current.method == ZIP_STORED) {
            consumed = std::min(sourceSize, reserved);
            emitted = consumed;
            std::memcpy(destination, input.data() + position, emitted);
        } else {
            if (counts.inflateCalls == limits.inflateCalls) {
                return fail(ContainerError::WORK_LIMIT);
            }

            ++counts.inflateCalls;
            inflater.next_in = input.data() + position;
            inflater.avail_in = static_cast<uInt>(sourceSize);
            inflater.next_out = destination;
            inflater.avail_out = static_cast<uInt>(reserved);
            result = inflate(&inflater, kind == ContainerKind::GZIP ? Z_BLOCK : Z_NO_FLUSH);
            consumed = sourceSize - inflater.avail_in;
            emitted = reserved - inflater.avail_out;
            inflater.next_in = Z_NULL;
            inflater.avail_in = 0;
            inflater.next_out = Z_NULL;
            inflater.avail_out = 0;
        }

        position += consumed;
        counts.compressedConsumed += consumed;
        payloadInput += consumed;
        payloadOutput += emitted;

        if (!work(emitted)) {
            return false;
        }

        if (gzipMetadata && !header(consumed, localHeader)) {
            return false;
        }

        if (emitted > allowance) {
            return fail(ContainerError::INFLATED_LIMIT);
        }

        if (directory && kind == ContainerKind::ZIP && emitted != 0) {
            return fail(ContainerError::INVALID_DIRECTORY);
        }

        if (kind == ContainerKind::GZIP && !gzipMetadata && !gzipDeflateEnd &&
            (inflater.data_type & (INFLATE_BLOCK_BOUNDARY | INFLATE_FINAL_BLOCK)) ==
                (INFLATE_BLOCK_BOUNDARY | INFLATE_FINAL_BLOCK)) {
            gzipDeflateEnd = true;
            gzipDeflateBytes = payloadInput - localHeader;
        }

        const auto ratioInput = kind == ContainerKind::GZIP
                                    ? (gzipDeflateEnd ? gzipDeflateBytes : payloadInput - localHeader)
                                    : payloadInput;

        if (!ratio(ratioInput, payloadOutput)) {
            return false;
        }

        crc = crc32_z(crc, destination, emitted);
        produced += emitted;
        counts.logicalProduced += emitted;

        if (result == Z_STREAM_END) {
            if (kind == ContainerKind::GZIP) {
                phase = Phase::START;

                return true;
            }

            return endZipEntry();
        }

        if (result != Z_OK && result != Z_BUF_ERROR) {
            if (result == Z_MEM_ERROR) {
                return fail(ContainerError::ALLOCATION);
            }

            if (kind == ContainerKind::GZIP && gzipHeader.done != 0 && inflater.msg != nullptr &&
                (std::strstr(inflater.msg, "incorrect data check") != nullptr ||
                 std::strstr(inflater.msg, "incorrect length check") != nullptr)) {
                return fail(ContainerError::CHECKSUM);
            }

            return fail(gzipMetadata ? ContainerError::INVALID_HEADER : ContainerError::INVALID_DATA);
        }

        if (consumed == 0 && emitted == 0) {
            if (result == Z_OK && kind == ContainerKind::GZIP && (inflater.data_type & INFLATE_BLOCK_BOUNDARY) != 0) {
                return true;
            }

            return fail(physicalEof ? ContainerError::TRUNCATED : ContainerError::INVALID_DATA);
        }

        return true;
    }

#endif

    /** Pull logical bytes while preserving the completed prefix on exceptions.
     * @param output Nonempty caller-owned output span.
     * @return Produced prefix and sticky terminal/completion state.
     */
    ContainerRead read(std::span<std::byte> output) {
        std::size_t produced = 0;

        try {
            while (produced < output.size() && phase != Phase::DONE && error == ContainerError::NONE) {
                if (kind == ContainerKind::NONE) {
                    if (!fill()) {
                        if (error == ContainerError::NONE) {
                            phase = Phase::DONE;
                        }

                        break;
                    }

                    const auto count = std::min<std::uint64_t>({available - position, output.size() - produced,
                                                                limits.inflatedBytes - counts.logicalProduced});

                    if (count == 0) {
                        fail(ContainerError::INFLATED_LIMIT);
                        break;
                    }

                    if (!work(count)) {
                        break;
                    }

                    std::memcpy(output.data() + produced, input.data() + position, static_cast<std::size_t>(count));
                    position += static_cast<std::size_t>(count);
                    produced += static_cast<std::size_t>(count);
                    counts.logicalProduced += count;
                    counts.compressedConsumed += count;
                }
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB
                else if (kind == ContainerKind::GZIP && phase == Phase::START) {
                    beginGzip();
                } else if (kind == ContainerKind::ZIP && (phase == Phase::START || phase == Phase::ZIP_NEXT)) {
                    zipNext();
                } else {
                    payload(output, produced);
                }
#endif
            }
        } catch (const std::bad_alloc &) {
            fail(ContainerError::ALLOCATION);
        } catch (...) {
            fail(ContainerError::SOURCE_IO);
        }

        return {produced, phase == Phase::DONE && error == ContainerError::NONE, error};
    }
};

bool containerAvailable(ContainerKind kind) noexcept {
    switch (kind) {
    case ContainerKind::NONE:
        return true;
    case ContainerKind::GZIP:
    case ContainerKind::ZIP:
#ifdef ORG_TTLDTOR_QDCXX_HAS_ZLIB

        return true;
#else

        return false;
#endif
    }

    return false;
}

CompressedInput::CompressedInput(std::istream &physical, ContainerKind kind, ContainerLimits limits) noexcept
    : physical_(physical), kind_(kind), limits_(limits) {
}

CompressedInput::~CompressedInput() = default;

ContainerRead CompressedInput::read(std::span<std::byte> output) noexcept {
    if (output.empty() && error_ == ContainerError::NONE) {
        error_ = ContainerError::INVALID_ARGUMENT;
    }

    if (error_ != ContainerError::NONE) {
        return {0, false, error_};
    }

    std::size_t produced = 0;
    try {
        if (!state_) {
            if (kind_ != ContainerKind::NONE && kind_ != ContainerKind::GZIP && kind_ != ContainerKind::ZIP) {
                error_ = ContainerError::INVALID_ARGUMENT;
            } else if (!containerAvailable(kind_)) {
                error_ = ContainerError::UNSUPPORTED;
            } else if (limits_.chunkBytes == 0 || limits_.chunkBytes > std::numeric_limits<unsigned int>::max() ||
                       limits_.chunkBytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
                       limits_.compressedBytes == std::numeric_limits<std::uint64_t>::max() ||
                       limits_.inflatedBytes == std::numeric_limits<std::uint64_t>::max()) {
                error_ = ContainerError::INVALID_ARGUMENT;
            } else if (limits_.chunkBytes > limits_.retainedBytes) {
                error_ = ContainerError::ALLOCATION;
            } else {
                state_ = std::make_unique<State>(physical_, kind_, limits_);
            }
        }

        if (error_ == ContainerError::NONE) {
            auto result = state_->read(output);
            produced = result.produced;
            error_ = result.error;

            return result;
        }
    } catch (const std::bad_alloc &) {
        error_ = ContainerError::ALLOCATION;
    } catch (...) {
        error_ = ContainerError::SOURCE_IO;
    }

    return {produced, false, error_};
}

ContainerError CompressedInput::finish() const noexcept {
    if (error_ != ContainerError::NONE) {
        return error_;
    }

    return state_ && state_->phase == State::Phase::DONE ? ContainerError::NONE : ContainerError::NOT_FINISHED;
}

ContainerCounters CompressedInput::counters() const noexcept {
    return state_ ? state_->counts : ContainerCounters{};
}
} // namespace org::ttldtor::qdcxx::tape_inspect
