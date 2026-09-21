// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace qd::protocol {

/** Mandatory operational storage limits, independent of payload syntax. */
struct FrameLimits {
    std::size_t maximumBodyBytes;    ///< Largest accepted type-plus-payload length; zero is allowed.
    std::size_t maximumPendingBytes; ///< Must accommodate maximumBodyBytes plus nine prefix bytes.
};

/** Distinguishes a missing type field from an explicit type zero. */
enum class FrameKind {
    EMPTY_HEARTBEAT, ///< Zero body, without a type field or payload.
    MESSAGE          ///< Nonempty body with a complete in-range type and opaque payload.
};

/** Owned extraction event, independent of Java payload callbacks. */
struct Frame {
    FrameKind kind;                   ///< Envelope kind; not a heartbeat callback claim.
    std::optional<std::int32_t> type; ///< Absent only for EMPTY_HEARTBEAT.
    std::vector<std::byte> payload;   ///< Owned bytes, valid after further feeds or reader destruction.
    std::uint64_t startOffset;        ///< Inclusive length-prefix position since reset.
    std::uint64_t endOffset;          ///< Exclusive body end since reset.
};

/** Feed/finish lifecycle outcomes. */
enum class FrameStatus {
    NEED_INPUT,  ///< No complete frame; may be at a clean boundary. Empty feed is not EOF.
    FRAME_READY, ///< Exactly one owning frame returned; resubmit any unaccepted input suffix.
    FAILED,      ///< Terminal error; further feeds accept nothing until reset.
    FINISHED     ///< Clean explicit end of input; further feeds accept nothing until reset.
};

/** First terminal failure; payload syntax is deliberately not classified. */
enum class FrameError {
    NONE,               ///< No failure.
    INVALID_LENGTH,     ///< Negative signed compact length.
    BODY_LIMIT,         ///< Nonnegative length exceeds the configured body budget.
    TRUNCATED_TYPE,     ///< Complete nonempty body cannot supply a complete compact type.
    INVALID_TYPE,       ///< Type lies outside 0..2147483647.
    TRUNCATED_INPUT,    ///< Explicit finish encountered an incomplete prefix or body.
    ALLOCATION_FAILURE, ///< Owning payload allocation failed; retained input remains inspectable.
    OFFSET_OVERFLOW     ///< Cumulative accepted position would exceed uint64_t.
};

/** One bounded delivery with exact per-call input accounting. */
struct FeedResult {
    std::size_t accepted;       ///< Input bytes copied this call; excludes the unaccepted suffix.
    FrameStatus status;         ///< Whether input, delivery, terminal failure or EOF occurred.
    std::optional<Frame> frame; ///< Owned frame only on FRAME_READY.
};

/** Single-threaded, non-reentrant incremental frame extractor; no payload or transport policy.
 * Holds one fixed byte allocation and no output queue. Copying and moving are disabled.
 */
class FrameReader {
    FrameLimits limits_;
    std::size_t storageBytes_;
    std::unique_ptr<std::byte[]> storage_;
    std::size_t pendingBytes_{};
    std::size_t prefixBytes_{};
    std::size_t bodyBytes_{};
    std::uint64_t totalAccepted_{};
    FrameStatus status_{FrameStatus::NEED_INPUT};
    FrameError error_{FrameError::NONE};

    public:
    /** Validate budgets and allocate body+9 byte elements once.
     * @param limits Finite budgets; incompatible/overflowing values throw invalid_argument.
     * Allocation failure propagates from construction; no input has been accepted.
     */
    explicit FrameReader(FrameLimits limits);

    /** Accept input up to one frame or the first error; never borrow an unaccepted suffix.
     * Input must not alias pending(). Payload allocation failure becomes FAILED, not an exception.
     * @param input Caller-owned bytes borrowed only for this call; empty does not signal EOF.
     * @return Exact accepted count, lifecycle status, and at most one owning frame.
     */
    [[nodiscard]] FeedResult feed(std::span<const std::byte> input);

    /** End input explicitly; repeated calls preserve the terminal result.
     * @return FINISHED at a clean boundary, FAILED on pending input or an earlier failure.
     */
    FrameStatus finish() noexcept;

    /** Clear only this framer's state and counters, retaining allocation and configuration. */
    void reset() noexcept;

    /** Inspect bytes retained for the current incomplete or failed frame.
     * @return Borrowed prefix-plus-body span, invalidated by a mutating call or destruction.
     */
    [[nodiscard]] std::span<const std::byte> pending() const noexcept;

    /** Inspect the first failure without altering state.
     * @return NONE before failure; otherwise the immutable first error until reset.
     */
    [[nodiscard]] FrameError error() const noexcept;

    /** Locate the first failed frame, independent of Java diagnostic consumption.
     * @return Frame's inclusive start since reset; interpret only after FAILED.
     */
    [[nodiscard]] std::uint64_t errorOffset() const noexcept;

    /** Inspect cumulative accepted input, including retained bytes.
     * @return Byte count since reset, never wrapping.
     */
    [[nodiscard]] std::uint64_t totalAccepted() const noexcept;

    /** Inspect the fixed internal byte-element allocation (excluding allocator overhead).
     * @return maximumBodyBytes+9, no larger than maximumPendingBytes.
     */
    [[nodiscard]] std::size_t storageBytes() const noexcept;

    /** Copying a reader is forbidden to avoid duplicating live stream state.
     * @param other Reader whose state must remain uniquely owned.
     */
    FrameReader(const FrameReader &other) = delete;

    /** Copy assignment is forbidden for live stream state.
     * @param other Reader which cannot be assigned.
     * @return No result: this operation is deleted.
     */
    FrameReader &operator=(const FrameReader &other) = delete;

    /** Moving is forbidden so no usable reader can lose its configured storage.
     * @param other Reader which cannot be moved.
     */
    FrameReader(FrameReader &&other) = delete;

    /** Move assignment is forbidden so configured storage remains attached to its reader.
     * @param other Reader which cannot be moved.
     * @return No result: this operation is deleted.
     */
    FrameReader &operator=(FrameReader &&other) = delete;
};

} // namespace qd::protocol
