// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile_url_reader.hpp"

#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace org::ttldtor::qdcxx::ipf::detail {

/** Incremental single-layer source envelope; no source or expanded-response buffering.
 * Output views borrow this nonmovable operation member until consume(); caller input never escapes feed().
 * One turn presents at most 4096 bytes to zlib and produces at most 4096 bytes. ZIP metadata uses fixed
 * backing for the two unsigned-16-bit local name/extra fields; names are never filesystem paths.
 */
class SourceEnvelope {
    static constexpr std::size_t WINDOW_BYTES = 4096;            ///< Maximum input/output per finite decoder turn.
    static constexpr std::size_t ZIP_METADATA_BYTES = 2 * 65535; ///< Two format-defined uint16 length fields.
    /** Current finite envelope step. */
    enum class Phase {
        SNIFF,            ///< Initial four-byte Java-compatible magic probe.
        PLAIN,            ///< Identity byte copy.
        GZIP,             ///< Current gzip member/header/trailer via zlib.
        GZIP_BETWEEN,     ///< Valid member end; next candidate header or physical EOF.
        ZIP_HEADER,       ///< Fixed local-entry header.
        ZIP_METADATA,     ///< Bounded name and extra fields.
        ZIP_STORED,       ///< Known-length stored payload.
        ZIP_DEFLATE,      ///< Raw deflate payload, followed by integrity/optional descriptor.
        ZIP_DESCRIPTOR,   ///< Optional-signature ZIP32/ZIP64 descriptor.
        DISCARD_REMAINDER ///< First ZIP file or gzip trailing junk; still account source bytes.
    };

    /** Aligned zlib allocation prefix, charged together with its requested payload. */
    struct alignas(std::max_align_t) Allocation {
        std::size_t bytes; ///< Whole allocation, including this prefix.
    };

    const IpfEnvelopeLimits &limits_;              ///< Stable operation-owned finite limits.
    IpfTransferResult &result_;                    ///< Stable detached result, first-cause destination.
    Phase phase_ = Phase::SNIFF;                   ///< Committed state; errors never rearm.
    bool failed_ = false;                          ///< Independent envelope failure, even after a native first cause.
    bool initialized_ = false;                     ///< inflateEnd is required.
    bool gzipCandidate_ = false;                   ///< Later member header errors are ignored as in the JDK stream.
    bool gzipHeaderCounted_ = false;               ///< Current complete header has consumed an entry credit.
    z_stream stream_{};                            ///< Private zlib state; no borrowed pointers survive a call.
    gz_header gzipHeader_{};                       ///< Nonretaining optional-header progress.
    std::size_t allocated_ = 0;                    ///< Outstanding zlib requested bytes with aligned prefixes.
    std::array<std::byte, 4> initial_{};           ///< Owned detection prefix.
    std::size_t initialSize_ = 0;                  ///< Collected detection bytes.
    std::size_t initialPosition_ = 0;              ///< Detection bytes subsequently processed.
    std::array<std::byte, WINDOW_BYTES> output_{}; ///< Single owned decoded window.
    std::size_t outputSize_ = 0;                   ///< Published window end.
    std::size_t outputPosition_ = 0;               ///< Explicitly consumed decoded prefix.
    std::array<std::byte, 30> header_{};           ///< Fixed ZIP local header.
    std::size_t headerSize_ = 0;                   ///< Initialized ZIP fixed bytes.
    std::array<std::byte, ZIP_METADATA_BYTES> metadata_{}; ///< One local name/extra slot.
    std::size_t metadataSize_ = 0;                         ///< Collected local metadata bytes.
    std::size_t nameSize_ = 0;                             ///< Exact name bytes, never interpreted as a path.
    std::size_t extraSize_ = 0;                            ///< Exact extra bytes.
    std::array<std::byte, 24> descriptor_{};               ///< Maximum signed ZIP64 descriptor.
    std::size_t descriptorSize_ = 0;                       ///< Collected descriptor bytes.
    bool directory_ = false;                               ///< Last name byte was '/'.
    bool zip64_ = false; ///< Valid local ZIP64 extra selects64-bit descriptor sizes, even for small payload.
    bool descriptorPresent_ = false;   ///< General-purpose flag bit3.
    unsigned method_ = 0;              ///< Stored or raw deflate.
    std::uint32_t expectedCrc_ = 0;    ///< Local/descriptor CRC.
    std::uint64_t expectedInput_ = 0;  ///< Local/descriptor compressed size.
    std::uint64_t expectedOutput_ = 0; ///< Local/descriptor inflated size.
    std::uint64_t entryInput_ = 0;     ///< Actual encoded payload, excluding metadata.
    std::uint64_t entryOutput_ = 0;    ///< Actual decoded current entry.
    std::uint32_t crc_ = 0;            ///< Current entry actual CRC32.

    /** Keep the earliest cause, and independently seal the decoder.
     * @param error Detached category.
     * @param stage Static checkpoint.
     * @return Always false.
     */
    bool reject(IpfTransferError error, std::string_view stage) noexcept;
    /** Debit nonrefundable byte visits.
     * @param bytes Whole next charge.
     * @return False after finite work refusal.
     */
    bool work(std::size_t bytes) noexcept;
    /** Debit a complete member/local-entry credit.
     * @return False after finite entry refusal.
     */
    bool entry() noexcept;
    /** Allocate bounded zeroed zlib storage.
     * @param opaque Stable decoder.
     * @param items Element count.
     * @param size Element bytes.
     * @return Aligned allocation or null after a detached refusal.
     */
    static voidpf allocate(voidpf opaque, uInt items, uInt size) noexcept;
    /** Release exact matching zlib storage.
     * @param opaque Stable decoder.
     * @param address Owned allocation or null.
     */
    static void release(voidpf opaque, voidpf address) noexcept;
    /** Initialize/reset one inflater in the same accounting domain.
     * @param gzip True for gzip framing, false for raw ZIP deflate.
     * @return False on resource/allocation/decoder failure.
     */
    bool startInflater(bool gzip) noexcept;
    /** Select Java magic after bounded staging, or plain on short physical EOF. */
    void select() noexcept;
    /** Parse admitted ZIP fixed header and reserve metadata before any name scan.
     * @return True for supported bounded local entry.
     */
    bool zipHeader() noexcept;
    /** Validate one complete bounded name/extra and select its payload.
     * @return True for supported entry.
     */
    bool zipMetadata() noexcept;
    /** Validate current entry integrity and select the next directory/file state.
     * @return False on integrity mismatch.
     */
    bool zipEnd() noexcept;
    /** Process at most one borrowed input prefix.
     * @param input Borrowed prefix, at most WINDOW_BYTES.
     * @return Exact consumed bytes; no pointer is retained.
     */
    std::size_t process(std::span<const std::byte> input) noexcept;

    public:
    /** Bind immutable limits and stable detached result; no allocation or source progress.
     * @param limits Stable finite envelope ceilings.
     * @param result Stable enclosing operation result.
     */
    SourceEnvelope(const IpfEnvelopeLimits &limits, IpfTransferResult &result) noexcept;
    /** Release only private inflater storage; never performs source/native progress. */
    ~SourceEnvelope();
    SourceEnvelope(const SourceEnvelope &) = delete;
    SourceEnvelope &operator=(const SourceEnvelope &) = delete;
    /** Select one gzip envelope from an ordinary HTTP Content-Encoding:gzip field.
     * @return False if payload was already admitted or a conflicting selection occurred.
     */
    bool declareGzip() noexcept;
    /** Perform one finite progress turn, only when no decoded output remains.
     * @param input Borrowed physical/body prefix.
     * @return Exact accepted source prefix, including discarded trailing bytes.
     */
    std::size_t feed(std::span<const std::byte> input) noexcept;
    /** Inspect currently owned verified/decompressed output, including a prefix before later integrity failure.
     * @return Borrowed contiguous prefix, valid until consume/feed/destruction.
     */
    std::span<const std::byte> output() const noexcept;
    /** Explicitly retire at most a delivered output prefix.
     * @param bytes Prefix byte count, clamped to available output.
     */
    void consume(std::size_t bytes) noexcept;
    /** Deliver physical/local or HTTP message EOF, separately from parser COMPLETE.
     * @return True when terminal; false if owned detection/output still needs a finite turn.
     */
    bool finish() noexcept;
    /** Inspect decoder-local terminal failure.
     * @return True after any envelope failure, independently of earlier transport cause.
     */
    bool failed() const noexcept;
};

} // namespace org::ttldtor::qdcxx::ipf::detail
