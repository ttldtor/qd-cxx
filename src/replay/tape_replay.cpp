// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/replay/tape_replay.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace org::ttldtor::qdcxx::replay {
namespace {
/** One call's borrowed boundaries and independent owned scheduling/diagnostic state. */
class ReplaySession {
    const protocol::AdmittedTape &tape_;
    ReplayObserver &observer_;
    ReplayClock &clock_;
    ReplayCancellation &cancellation_;
    ReplayLimits limits_;
    ReplayPolicy policy_;
    ReplayResult result_;
    std::optional<std::int64_t> anchorMs_;
    std::int64_t anchorNs_ = 0;
    std::uint64_t highWaterMs_ = 0;
    std::optional<std::int64_t> lastNow_;

    /** Preserve the first outcome.
     * @param outcome Terminal category.
     * @param stage Static checkpoint.
     * @return False for convenient fail-closed control flow.
     */
    bool fail(ReplayOutcome outcome, std::string_view stage) noexcept {
        if (result_.outcome == ReplayOutcome::COMPLETED) {
            result_.outcome = outcome;
            result_.stage = stage;
        }

        return false;
    }

    /** Reserve one nonrefunded work unit and operation attempt.
     * @param counter Actual operation attempt count.
     * @param stage Static budget checkpoint.
     * @return True when the operation may begin.
     */
    bool charge(std::size_t &counter, std::string_view stage) noexcept {
        if (result_.counters.workUnits == limits_.workUnits) {
            return fail(ReplayOutcome::RESOURCE_LIMIT, stage);
        }

        ++result_.counters.workUnits;
        ++counter;

        return true;
    }

    /** Reserve independently bounded call and work attempts.
     * @param counter Call count.
     * @param maximum Mandatory finite count allowance.
     * @param stage Static checkpoint.
     * @return True before invoking user code, false on budget failure.
     */
    bool callCharge(std::size_t &counter, std::size_t maximum, std::string_view stage) noexcept {
        if (counter == maximum) {
            return fail(ReplayOutcome::RESOURCE_LIMIT, stage);
        }

        return charge(counter, stage);
    }

    /** Poll without permitting caller exceptions to escape or become storage failures.
     * @return True to continue, false on cancellation/error/work exhaustion.
     */
    bool poll() {
        if (!charge(result_.counters.cancellationPolls, "cancellation-work")) {
            return false;
        }

        try {
            if (cancellation_.isCancelled()) {
                return fail(ReplayOutcome::CANCELLED, "cancellation");
            }
        } catch (...) {
            return fail(ReplayOutcome::CANCELLATION_FAILURE, "cancellation");
        }

        return true;
    }

    /** Sample and validate a nonnegative nondecreasing monotonic clock.
     * @return Valid sample or absence after sticky failure.
     */
    std::optional<std::int64_t> now() {
        if (!callCharge(result_.counters.clockCalls, limits_.clockCalls, "clock-calls")) {
            return std::nullopt;
        }

        try {
            const auto sample = clock_.nowNs();

            if (!sample || *sample < 0 || (lastNow_ && *sample < *lastNow_)) {
                fail(ReplayOutcome::CLOCK_FAILURE, "clock-sample");

                return std::nullopt;
            }

            lastNow_ = *sample;

            return *sample;
        } catch (...) {
            fail(ReplayOutcome::CLOCK_FAILURE, "clock-sample");

            return std::nullopt;
        }
    }

    /** Schedule once for one successful nonempty timed frame.
     * @param timing Exact owned reader metadata.
     * @return True when delivery may begin; false on timing/clock/wait/cancellation failure.
     */
    bool schedule(const protocol::TapeTiming &timing) {
        if (policy_ == ReplayPolicy::IMMEDIATE || !timing.timestampMs) {
            return true;
        }

        if (!anchorMs_) {
            const auto sampled = now();

            if (!sampled) {
                return false;
            }

            anchorMs_ = *timing.timestampMs;
            anchorNs_ = *sampled;

            return true;
        }

        const auto timestamp = *timing.timestampMs;
        // Unsigned subtraction is the exact positive mathematical difference, including signed extremes.
        const auto elapsed =
            timestamp > *anchorMs_ ? static_cast<std::uint64_t>(timestamp) - static_cast<std::uint64_t>(*anchorMs_) : 0;
        constexpr std::uint64_t NS_PER_MS = 1000000;
        constexpr auto MAX_NS = std::numeric_limits<std::int64_t>::max();

        if (elapsed > limits_.maxRelativeMs || elapsed > static_cast<std::uint64_t>(MAX_NS) / NS_PER_MS) {
            return fail(ReplayOutcome::TIMING_RANGE, "relative-difference");
        }

        highWaterMs_ = std::max(highWaterMs_, elapsed);
        const auto elapsedNs = static_cast<std::int64_t>(highWaterMs_ * NS_PER_MS);

        if (elapsedNs > MAX_NS - anchorNs_) {
            return fail(ReplayOutcome::TIMING_RANGE, "relative-deadline");
        }

        const auto target = anchorNs_ + elapsedNs;
        auto sampled = now();

        if (!sampled) {
            return false;
        }

        while (*sampled < target) {
            const auto step = std::min(target - *sampled, limits_.maxWaitSliceNs);
            const auto deadline = *sampled + step; // Both sum operands are bounded by target <= INT64_MAX.

            if (!poll() || !callCharge(result_.counters.waitCalls, limits_.maxWaitCalls, "wait-calls")) {
                return false;
            }

            WaitResult waited;
            try {
                waited = clock_.waitUntilNs(deadline, cancellation_);
            } catch (...) {
                return fail(ReplayOutcome::CLOCK_FAILURE, "wait-exception");
            }

            if (waited == WaitResult::CANCELLED) {
                return fail(ReplayOutcome::CANCELLED, "wait-cancelled");
            }

            if (waited != WaitResult::READY) {
                return fail(ReplayOutcome::WAIT_FAILURE, "wait-failed");
            }

            if (!poll()) {
                return false;
            }

            sampled = now();

            if (!sampled) {
                return false;
            }

            if (*sampled < deadline) {
                return fail(ReplayOutcome::CLOCK_FAILURE, "wait-premature-ready");
            }
        }

        return true;
    }

    /** Resolve unique sender identity while charging every examined outgoing declaration.
     * @param localId Decoded local record identity.
     * @return Exact sender or absence after resource/identity failure.
     */
    std::optional<std::int32_t> sender(std::int32_t localId) {
        for (const auto &layout : tape_.layouts().records()) {
            if (!charge(result_.counters.layoutVisits, "layout-work")) {
                return std::nullopt;
            }

            if (layout.localRecordId == localId) {
                return layout.senderId;
            }
        }

        fail(ReplayOutcome::INPUT_FAILURE, "sender-identity");

        return std::nullopt;
    }

    /** Deliver a complete successful result with ephemeral synchronous views.
     * @param event Engine-owned reader result; no borrowed record outlives this operation.
     * @return True after all records, false on sticky failure.
     */
    bool deliver(const protocol::TapeFeedResult &event) {
        if (event.records.empty()) {
            return true; // Empty frames do not participate in any scheduling state or clock arithmetic.
        }

        const auto timing = event.timing.value_or(protocol::TapeTiming{});

        if (!schedule(timing)) {
            return false;
        }

        for (std::size_t index = 0; index < event.records.size(); ++index) {
            result_.recordIndex = index;

            if (!charge(result_.counters.recordAttempts, "record-work")) {
                return false;
            }

            if (result_.counters.deliveredRecords == limits_.deliveredRecords) {
                return fail(ReplayOutcome::RESOURCE_LIMIT, "delivered-records");
            }

            if (result_.counters.callbackCalls == limits_.callbackCalls) {
                return fail(ReplayOutcome::RESOURCE_LIMIT, "callback-calls");
            }

            const auto senderId = sender(event.records[index].localId);

            if (!senderId || !poll() ||
                !callCharge(result_.counters.callbackCalls, limits_.callbackCalls, "callback-calls")) {
                return false;
            }

            const ReplayRecordView view{event.records[index], timing, result_.frameIndex, index, *senderId};
            try {
                if (observer_.onRecord(view) != ObserverResult::OK) {
                    ++result_.counters.failedRecordCallbacks;

                    return fail(ReplayOutcome::OBSERVER_FAILURE, "observer-record");
                }
            } catch (...) {
                ++result_.counters.failedRecordCallbacks;

                return fail(ReplayOutcome::OBSERVER_FAILURE, "observer-record");
            }
            ++result_.counters.deliveredRecords;
        }

        return true;
    }

    /** Copy actual reader counters while the reader is alive.
     * @param reader Fresh independently bounded reader.
     */
    void capture(const protocol::TapeReader &reader) noexcept {
        auto &c = result_.counters;
        c.parsedBytes = reader.acceptedBytes();
        c.parsedFrames = reader.frameCount();
        c.parsedRecords = reader.processedRecords();
        c.parsedFields = reader.wireFields();
        c.constructionUnits = reader.constructionUnits();
        c.timingEvents = reader.timingEvents();
        c.fieldBytes = reader.fieldBytes();
        c.fieldUnits = reader.fieldUnits();
        c.ownedFieldUnits = reader.ownedFieldUnits();
    }

    /** Execute fresh parsing and completion, with pending frame destruction before return. */
    void execute() {
        if (!poll()) {
            return;
        }

        protocol::TapeReader reader(tape_.schema(), limits_.reader, protocol::TapeMode::RAW_MESSAGE_V1);
        const auto bytes = tape_.bytes();
        std::size_t position = 0;

        while (position < bytes.size()) {
            result_.recordIndex.reset();

            if (!poll() || !charge(result_.counters.frameAttempts, "frame-work")) {
                return;
            }

            const auto event = reader.feed(bytes.subspan(position));
            capture(reader);
            position += event.accepted;

            if (event.status == protocol::TapeStatus::FAILED) {
                result_.reader = reader.diagnostic();
                fail(ReplayOutcome::INPUT_FAILURE, "reader-feed");

                return;
            }

            if (event.status == protocol::TapeStatus::NEED_INPUT && event.accepted != 0) {
                continue; // Independent input bounds may stop in a prefix/body; obtain the reader's actual error next.
            }

            if (event.status != protocol::TapeStatus::EVENT || !event.frame) {
                fail(ReplayOutcome::INPUT_FAILURE, "reader-progress");

                return;
            }

            if (!deliver(event)) {
                return;
            }

            ++result_.frameIndex;
        }

        result_.recordIndex.reset();

        if (reader.finish() != protocol::TapeStatus::FINISHED) {
            capture(reader);
            result_.reader = reader.diagnostic();
            fail(ReplayOutcome::INPUT_FAILURE, "reader-finish");

            return;
        }

        capture(reader);
        result_.cleanEof = true;

        if (!poll() || !callCharge(result_.counters.callbackCalls, limits_.callbackCalls, "callback-calls")) {
            return;
        }

        try {
            if (observer_.finish() != ObserverResult::OK) {
                fail(ReplayOutcome::OBSERVER_FAILURE, "observer-finish");

                return;
            }
        } catch (...) {
            fail(ReplayOutcome::OBSERVER_FAILURE, "observer-finish");

            return;
        }
        result_.observerFinished = true;
    }

    public:
    /** Bind one synchronous session without allocating or retaining beyond run.
     * @param tape Borrowed admitted immutable source.
     * @param observer Borrowed observer.
     * @param clock Borrowed clock.
     * @param cancellation Borrowed cancellation.
     * @param limits Explicit per-session allowances.
     * @param policy Explicit scheduling policy.
     */
    ReplaySession(const protocol::AdmittedTape &tape, ReplayObserver &observer, ReplayClock &clock,
                  ReplayCancellation &cancellation, ReplayLimits limits, ReplayPolicy policy)
        : tape_(tape), observer_(observer), clock_(clock), cancellation_(cancellation), limits_(limits),
          policy_(policy) {
    }

    /** Complete synchronously and translate engine storage/configuration exceptions.
     * @return Owning counters/outcome after execute has destroyed its reader and pending values.
     */
    ReplayResult run() {
        if (!tape_.valid() || (policy_ != ReplayPolicy::IMMEDIATE && policy_ != ReplayPolicy::RELATIVE) ||
            (policy_ == ReplayPolicy::RELATIVE && limits_.maxWaitSliceNs <= 0)) {
            fail(ReplayOutcome::INVALID_CONFIGURATION, "configuration");

            return result_;
        }

        try {
            execute();
        } catch (const std::bad_alloc &) {
            fail(ReplayOutcome::ALLOCATION_FAILURE, "reader-allocation");
        } catch (const std::length_error &) {
            fail(ReplayOutcome::ALLOCATION_FAILURE, "reader-allocation");
        } catch (const std::invalid_argument &) {
            fail(ReplayOutcome::INPUT_FAILURE, "reader-configuration");
        }

        return result_;
    }
};
} // namespace

ReplayResult replayTape(const protocol::AdmittedTape &tape, ReplayObserver &observer, ReplayClock &clock,
                        ReplayCancellation &cancellation, ReplayLimits limits, ReplayPolicy policy) {
    return ReplaySession(tape, observer, clock, cancellation, limits, policy).run();
}
} // namespace org::ttldtor::qdcxx::replay
