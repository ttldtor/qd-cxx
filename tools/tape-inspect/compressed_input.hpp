// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <span>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Explicit tool-owned envelope; no protocol inference or recursive decompression. */
enum class ContainerKind {
    NONE, ///< Pass-through bytes with the same explicit finite source bounds.
    GZIP, ///< All concatenated members, validated through physical EOF.
    ZIP   ///< Strict ZIP32 with one file and bounded empty directories.
};
/** Query compiled tool input support without allocation, source access or ambient configuration.
 * @param kind Explicit envelope choice; invalid enum values are unavailable.
 * @return True for NONE, and for GZIP/ZIP only when compression support was built.
 */
[[nodiscard]] bool containerAvailable(ContainerKind kind) noexcept;
/** Terminal source outcomes; malformed, unsupported and resource refusals remain distinct. */
enum class ContainerError {
    NONE,               ///< No error; completion requires a separate physicalComplete flag.
    INVALID_HEADER,     ///< Envelope header is malformed or has invalid reserved bits.
    INVALID_DATA,       ///< Deflate structure is malformed or cannot make progress.
    TRUNCATED,          ///< Physical EOF precedes a required header, payload or trailer byte.
    CHECKSUM,           ///< GZIP CRC/ISIZE or ZIP entry CRC differs from decoded bytes.
    INVALID_DIRECTORY,  ///< ZIP sizes, names, offsets, entry count or closure are inconsistent.
    UNSUPPORTED,        ///< Well-identified format choice lies outside the selected profile.
    COMPRESSED_LIMIT,   ///< One diagnostic byte exceeds the physical input allowance.
    INFLATED_LIMIT,     ///< Decoded output exceeds the logical input allowance.
    RATIO_LIMIT,        ///< Member expansion exceeds the configured ratio and slack.
    HEADER_LIMIT,       ///< Per-record or cumulative metadata byte allowance is exhausted.
    MEMBER_ENTRY_LIMIT, ///< A further GZIP member or ZIP entry would exceed its finite count.
    WORK_LIMIT,         ///< Irreversible byte work or actual inflater call allowance is exhausted.
    SOURCE_IO,          ///< Borrowed source failed; destruction still does not close it.
    ALLOCATION,         ///< Bounded allocation was refused or the allocator failed.
    INVALID_ARGUMENT,   ///< Empty output span or unrepresentable/invalid configuration.
    NOT_FINISHED        ///< finish was called before validated physical closure; it performs no I/O.
};

/** Finite source bounds. Every zero is literal, never an unlimited/default sentinel. */
struct ContainerLimits {
    std::uint64_t compressedBytes = 67108864; ///< Physical bytes; one diagnostic excess probe is permitted.
    std::uint64_t inflatedBytes = 67108864;   ///< Produced logical bytes before QTP consumption.
    std::uint64_t expansionRatio = 1000;      ///< Per-member output <= ratio * consumed payload + slack.
    std::uint64_t ratioSlack = 65536;         ///< Initial per-member expansion allowance in bytes.
    std::uint64_t gzipMembers = 1024;         ///< Includes empty members.
    std::uint64_t zipEntries = 64;            ///< Includes empty directories; exactly one file is required.
    std::uint64_t headerBytes = 262144;       ///< Maximum one envelope metadata record.
    std::uint64_t totalHeaderBytes = 4194304; ///< Cumulative scanned metadata bytes.
    std::uint64_t workBytes = 268435456;      ///< Physical + inflated + metadata scan/move charges.
    std::uint64_t inflateCalls = 262144;      ///< Actual calls, including zero-output work.
    std::size_t retainedBytes = 4194304;      ///< Scratch and retained ZIP metadata capacity.
    std::size_t inflaterBytes = 1048576;      ///< Outstanding zlib allocations, including allocation headers.
    std::size_t chunkBytes = 4096;            ///< Positive, representable input scratch size.
};

/** Distinct physical/read-ahead, consumed and logical coordinates; no inverse mapping is implied. */
struct ContainerCounters {
    std::uint64_t compressedPhysicalRead = 0; ///< May equal physical allowance + one observed excess byte.
    std::uint64_t compressedConsumed = 0;     ///< Compressed bytes actually consumed by envelope processing.
    std::uint64_t logicalProduced = 0;        ///< Bytes published to caller-owned output spans.
    std::uint64_t members = 0;                ///< Started GZIP members or ZIP entries.
    std::uint64_t headerBytes = 0;            ///< Cumulative scanned metadata bytes.
    std::uint64_t workBytes = 0;              ///< Cumulative irreversible byte work charge.
    std::uint64_t inflateCalls = 0;           ///< Cumulative irreversible inflater call charge.
};

/** One pull result. A produced prefix remains valid even when error is non-NONE. */
struct ContainerRead {
    std::size_t produced = 0;                    ///< Bytes placed in the supplied span, retained by the caller.
    bool physicalComplete = false;               ///< Integrity and actual physical EOF both validated.
    ContainerError error = ContainerError::NONE; ///< Sticky terminal error, never COMPLETE.
};

/** Bounded synchronous decoder borrowing its physical stream until destruction.
 * Owns decoder/scratch/metadata only. Does not close, drain, retain output spans or invoke callbacks.
 * The file owner must destroy this object before closing the physical stream and must combine
 * finish, parser/companion/segment closure, physical close and sink flush before reporting COMPLETE.
 */
class CompressedInput {
    struct State;
    std::istream &physical_;
    ContainerKind kind_;
    ContainerLimits limits_;
    std::unique_ptr<State> state_;
    ContainerError error_ = ContainerError::NONE;

    public:
    /** Prepare without allocation or input I/O; configuration is checked on first read.
     * @param physical Borrowed binary source; lifetime exceeds this adapter's lifetime.
     * @param kind Explicit envelope choice, including pass-through NONE.
     * @param limits Finite per-instance bounds; no ambient settings are consulted.
     */
    CompressedInput(std::istream &physical, ContainerKind kind, ContainerLimits limits = {}) noexcept;
    /** Release decoder/metadata without reading or closing the borrowed stream. */
    ~CompressedInput();
    CompressedInput(const CompressedInput &) = delete;
    CompressedInput &operator=(const CompressedInput &) = delete;
    /** Fill bounded caller-owned storage, retaining a produced prefix on a late terminal error.
     * @param output Nonempty writable span, borrowed only for this call; empty is INVALID_ARGUMENT.
     * @return Prefix count plus sticky error, or validated physical completion; no successful idle read.
     */
    [[nodiscard]] ContainerRead read(std::span<std::byte> output) noexcept;
    /** Check closure without I/O or state changes.
     * @return NONE only after validated physical completion, else sticky error or NOT_FINISHED.
     */
    [[nodiscard]] ContainerError finish() const noexcept;
    /** Snapshot coordinates without accessing the physical stream.
     * @return Owned counters, initially zero; physical and logical offsets remain distinct.
     */
    [[nodiscard]] ContainerCounters counters() const noexcept;
};
} // namespace org::ttldtor::qdcxx::tape_inspect
