// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/protocol/tape_admission.hpp"

namespace org::ttldtor::qdcxx::replay {
/** Explicit scheduling policy; business fields never control delivery time. */
enum class ReplayPolicy {
    IMMEDIATE, ///< No clock sampling or waiting.
    RELATIVE   ///< Relative nonempty-frame timing with bounded slices.
};
/** Terminal synchronous replay outcome. */
enum class ReplayOutcome {
    COMPLETED,            ///< Clean parser EOF and successful observer completion.
    CANCELLED,            ///< Pending deliveries discarded without observer completion.
    INPUT_FAILURE,        ///< Fresh reader failed; no failed-result prefix delivered.
    OBSERVER_FAILURE,     ///< Record/completion callback failed or threw.
    CLOCK_FAILURE,        ///< Invalid clock sample or unexpected clock/wait exception.
    WAIT_FAILURE,         ///< Wait explicitly failed.
    TIMING_RANGE,         ///< Relative difference or deadline exceeds representable bounds.
    CANCELLATION_FAILURE, ///< Cancellation poll threw.
    RESOURCE_LIMIT,       ///< Mandatory finite replay allowance exceeded.
    ALLOCATION_FAILURE,   ///< Engine-owned allocation failed.
    INVALID_CONFIGURATION ///< Inactive token, invalid policy or nonpositive relative slice.
};
/** Synchronous observer callback result. */
enum class ObserverResult {
    OK,    ///< Callback succeeded.
    FAILED ///< Callback failed; external effects are not rolled back.
};
/** Explicit clock sampling error. */
enum class ClockError {
    FAILED ///< Caller could not obtain a monotonic sample.
};
/** Synchronous wait result. */
enum class WaitResult {
    READY,     ///< Supplied deadline reached; engine verifies a clock sample.
    CANCELLED, ///< Cancellation wins even when the deadline was reached.
    FAILED     ///< Caller could not complete the wait.
};

/** Prompt caller-owned cancellation boundary, borrowed only during replayTape. */
class ReplayCancellation {
    public:
    /** Destroy the caller-owned callback object; replay never takes ownership. */
    virtual ~ReplayCancellation() = default;
    /** Poll promptly on the replay thread without throwing in valid use.
     * @return True stops this invocation without draining pending deliveries.
     */
    [[nodiscard]] virtual bool isCancelled() = 0;
};

/** Caller-owned monotonic clock/wait boundary; no implicit wall clock or background work. */
class ReplayClock {
    public:
    /** Destroy the caller-owned callback object. */
    virtual ~ReplayClock() = default;
    /** Sample nonnegative monotonic nanoseconds.
     * @return Finite sample or explicit clock error; unexpected exceptions are caught.
     */
    [[nodiscard]] virtual std::expected<std::int64_t, ClockError> nowNs() = 0;
    /** Wait promptly within the requested bounded slice; no detached work may survive.
     * @param deadline Nonnegative finite monotonic nanosecond deadline.
     * @param cancellation Borrowed prompt cancellation boundary, valid for this call only.
     * @return READY, CANCELLED or FAILED; READY is checked against a fresh sample.
     */
    [[nodiscard]] virtual WaitResult waitUntilNs(std::int64_t deadline, ReplayCancellation &cancellation) = 0;
};

/** Borrowed record reference valid only until onRecord returns; copy retained data before returning. */
struct ReplayRecordView {
    const protocol::StreamDataRecord &record; ///< Borrowed engine-owned record, never retained by replay.
    protocol::TapeTiming timing;              ///< Owned exact raw timestamp and saturated Java cursor sequence.
    std::size_t frameIndex;                   ///< Zero-based envelope position, including header/descriptions/markers.
    std::size_t recordIndex;                  ///< Zero-based within-frame record position.
    std::int32_t senderId;                    ///< Exact outgoing sender identity, distinct from local record ID.
};

/** Caller-owned synchronous output boundary; failure side effects are never retried or rolled back. */
class ReplayObserver {
    public:
    /** Destroy the caller-owned callback object. */
    virtual ~ReplayObserver() = default;
    /** Receive one complete successful-frame record.
     * @param view Borrowed record and owned timing; record reference expires on return.
     * @return OK or FAILED; unexpected exceptions become OBSERVER_FAILURE.
     */
    [[nodiscard]] virtual ObserverResult onRecord(const ReplayRecordView &view) = 0;
    /** Flush/complete output once after actual clean EOF and final cancellation poll.
     * @return OK or FAILED; never called on earlier failure or cancellation.
     */
    [[nodiscard]] virtual ObserverResult finish() = 0;
};

/** Required finite per-invocation bounds; no unlimited sentinel or ambient configuration. */
struct ReplayLimits {
    protocol::TapeLimits reader;  ///< Independent fresh-reader parse/storage budgets.
    std::size_t deliveredRecords; ///< Successful onRecord count; checked before each attempted delivery.
    std::size_t callbackCalls;    ///< All onRecord/finish attempts, including failures.
    std::size_t workUnits;        ///< Frame/record/layout visits and poll/clock/wait/callback attempts, one each.
    std::uint64_t maxRelativeMs;  ///< Maximum positive elapsed milliseconds from the first timed nonempty frame.
    std::int64_t maxWaitSliceNs;  ///< Positive relative wait slice; ignored by IMMEDIATE.
    std::size_t maxWaitCalls;     ///< All wait attempts, including failure/cancellation.
    std::size_t clockCalls;       ///< All nowNs attempts, including errors/exceptions.
};

/** Actual nonrefunded replay work and independent reader counters. */
struct ReplayCounters {
    std::size_t workUnits = 0;         ///< Sum of admitted frame/record/layout/poll/clock/wait/callback operations.
    std::size_t frameAttempts = 0;     ///< Feed attempts, including partial feeds and a failed current envelope.
    std::size_t recordAttempts = 0;    ///< Complete successful-frame records considered for delivery.
    std::size_t layoutVisits = 0;      ///< Outgoing layout elements examined in reverse local-ID lookup.
    std::size_t cancellationPolls = 0; ///< Engine-issued isCancelled attempts; nested caller wait polls are excluded.
    std::size_t clockCalls = 0;        ///< nowNs attempts.
    std::size_t waitCalls = 0;         ///< waitUntilNs attempts.
    std::size_t callbackCalls = 0;     ///< onRecord and finish attempts.
    std::size_t failedRecordCallbacks = 0; ///< Failed or throwing onRecord attempts.
    std::size_t deliveredRecords = 0;      ///< Successful onRecord callbacks only.
    std::uint64_t parsedBytes = 0;         ///< Fresh reader accepted file bytes.
    std::size_t parsedFrames = 0;          ///< Fresh reader complete envelopes, including failing frames.
    std::size_t parsedRecords = 0;         ///< Fresh reader charged record attempts, including failed prefixes.
    std::size_t parsedFields = 0;          ///< Fresh reader charged wire fields.
    std::size_t constructionUnits = 0;     ///< Fresh reader cumulative reconstruction work.
    std::size_t timingEvents = 0;          ///< Fresh reader RAW/heartbeat metadata attempts.
    std::size_t fieldBytes = 0;            ///< Fresh reader cumulative string-byte work.
    std::size_t fieldUnits = 0;            ///< Fresh reader cumulative string-unit work.
    std::size_t ownedFieldUnits = 0;       ///< Fresh reader cumulative published string units.
};

/** Owning terminal result; diagnostic text always references static storage. */
struct ReplayResult {
    ReplayOutcome outcome = ReplayOutcome::COMPLETED; ///< Terminal category; returned only after synchronous cleanup.
    std::string_view stage = "none";                  ///< First static checkpoint, never caller/input text.
    std::size_t frameIndex = 0;                     ///< Current zero-based envelope or exhausted envelope count at EOF.
    std::optional<std::size_t> recordIndex;         ///< Current within-frame record, absent outside record work.
    std::optional<protocol::TapeDiagnostic> reader; ///< Exact copied fresh-reader diagnostic on INPUT_FAILURE.
    ReplayCounters counters;                        ///< Actual charges retained after failures.
    bool cleanEof = false;         ///< Actual fresh-reader finish succeeded, even if final cancellation then occurs.
    bool observerFinished = false; ///< Observer finish returned OK after clean EOF and final cancellation poll.
};

/** Replay one immutable admitted recording synchronously, with no retained callback/view or background work.
 * All borrowed objects must outlive the call; admitted storage stays immutable. Callback methods may update their
 * internal state. External concurrent/reentrant mutation, movement, replacement or destruction is unsupported.
 * @param tape Active immutable admitted source; moved-from tokens are refused before access.
 * @param observer Borrowed prompt synchronous output boundary.
 * @param clock Borrowed monotonic/wait boundary, never called for IMMEDIATE.
 * @param cancellation Borrowed prompt poll boundary.
 * @param limits Mandatory finite per-session bounds, independent of admission.
 * @param policy Explicit IMMEDIATE or RELATIVE policy.
 * @return Owning first outcome/counters after all engine-owned pending storage is destroyed.
 */
[[nodiscard]] ReplayResult replayTape(const protocol::AdmittedTape &tape, ReplayObserver &observer, ReplayClock &clock,
                                      ReplayCancellation &cancellation, ReplayLimits limits, ReplayPolicy policy);
} // namespace org::ttldtor::qdcxx::replay
