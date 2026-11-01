// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: deterministic observations exercise the production recovery transition model.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "recovery_state.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
namespace t = org::ttldtor::qdcxx::transport::detail;
using namespace std::chrono_literals;
using Time = t::RecoveryClock::time_point;

/** Select a synthetic point in the real monotonic clock's duration domain.
 * @param value Explicit fake-clock milliseconds, including negative values.
 * @return Time point; no clock is sampled.
 */
Time at(std::chrono::milliseconds value) {
    return Time{std::chrono::duration_cast<t::RecoveryClock::duration>(value)};
}

/** Supply the existing finite retry policy with separately enabled scheduler intervals.
 * @param attempts Whole lifetime factory count.
 * @return Explicit finite configuration; no transport or SDK defaults are changed.
 */
t::RecoveryConfig config(std::uint32_t attempts = 1) {
    t::RecoveryConfig result;
    result.maximumAttempts = attempts;
    result.retryDelay = attempts == 1 ? 0ms : 10ms;

    return result;
}

/** Build a valid fixed state value with a stable fixture identity.
 * @param settings Explicit scheduler policy.
 * @param origin Stable fixture address.
 * @param sequence Last used action identity, for ordinary boundary construction tests.
 * @return Prepared model; no factory or native action has run.
 */
t::RecoveryState makeState(t::RecoveryConfig settings, const void *origin, std::uint64_t sequence) {
    auto made = t::RecoveryState::create(settings, origin, sequence);
    REQUIRE(made);

    return std::move(*made);
}

/** Test-side caller driving the actual model; observations never execute a fake SDK implementation. */
struct Driver {
    char origin{};            ///< Stable identity anchor; this fixture never moves.
    t::RecoveryState state;   ///< Exact production transition model.
    Time now{};               ///< Explicit deterministic time, never a global clock override.
    t::RecoveryFacts facts{}; ///< Current bounded integration observations.

    /** Construct a stable fixture.
     * @param settings Explicit finite scheduling policy.
     * @param sequence Initial checked action sequence.
     */
    explicit Driver(t::RecoveryConfig settings = config(), std::uint64_t sequence = 0)
        : state(makeState(settings, &origin, sequence)) {
    }

    /** Explicitly create one fresh test-caller turn.
     * @param observation One real model input, not retained by the model.
     * @param transitions Finite caller-selected action credits for this turn.
     * @return Pure model result.
     */
    t::RecoveryStep step(t::RecoveryObservation observation = {}, std::size_t transitions = 16) {
        t::RecoveryStateWork work{1, transitions};

        return state.step(now, observation, work);
    }

    /** Report the matching current action using explicit facts/proofs.
     * @param completion Exact action outcome.
     * @param input Validated current poll input kind, or NONE.
     * @param cause Bounded failure classification.
     * @param output Whether a real local output was admitted.
     * @param disposed Whether private SDK disposal actually returned.
     * @param reset Whether the real source reset/global barrier completed.
     * @param retired Whether owned delivery/control retirement completed.
     * @return Actual model result after this caller turn.
     */
    t::RecoveryStep finish(t::RecoveryCompletion completion = t::RecoveryCompletion::COMPLETED,
                           t::RecoveryInput input = t::RecoveryInput::NONE,
                           t::RecoveryCause cause = t::RecoveryCause::NONE, bool output = false, bool disposed = false,
                           bool reset = false, bool retired = false) {
        return step({state.status().pending, completion, facts, input, output, disposed, reset, retired, cause});
    }

    /** Require the exact currently outstanding action.
     * @param action Expected one action, not an eventual condition.
     */
    void pending(t::RecoveryAction action) const {
        REQUIRE(state.status().pending.action == action);
    }

    /** Admit and finish the pending fresh factory, then finish start.
     * Entry permission is checked before reporting factory completion; nothing is silently retried.
     */
    void createAttempt() {
        pending(t::RecoveryAction::PREPARE_ATTEMPT);
        const auto entered = finish(t::RecoveryCompletion::FACTORY_ENTERED);
        REQUIRE(entered.factoryEntryAccepted);
        facts = {};
        REQUIRE(finish().observationAccepted);
        pending(t::RecoveryAction::START_ATTEMPT);
        REQUIRE(finish().observationAccepted);
        pending(t::RecoveryAction::POLL_ATTEMPT);
    }

    /** Reach a current transport/protocol/auth observation through actual model actions.
     * @param input First validated poll event; NONE proves no protocol receipt.
     * @param authenticated Actual independent authentication eligibility.
     */
    void connect(t::RecoveryInput input = t::RecoveryInput::PROTOCOL, bool authenticated = true) {
        state.requestStart();
        REQUIRE(step().status == t::RecoveryStepStatus::ACTION);
        createAttempt();
        facts.transportEligible = true;
        facts.authenticationEligible = authenticated;
        REQUIRE(finish(t::RecoveryCompletion::COMPLETED, input).observationAccepted);
    }

    /** Begin one retryable transport failure and observe its actual native terminal.
     * @param terminal Whether terminal was already observed with the first failure.
     */
    void loseTransport(bool terminal = false) {
        if (state.status().pending.action == t::RecoveryAction::NONE) {
            REQUIRE(step().status == t::RecoveryStepStatus::ACTION);
        }

        pending(t::RecoveryAction::POLL_ATTEMPT);
        facts.terminal = terminal;
        REQUIRE(finish(t::RecoveryCompletion::RETRYABLE_FAILURE, t::RecoveryInput::NONE, t::RecoveryCause::TRANSPORT)
                    .observationAccepted);

        if (!terminal) {
            pending(t::RecoveryAction::REQUEST_ABORT);
            REQUIRE(finish().observationAccepted);
            pending(t::RecoveryAction::POLL_ATTEMPT);
            facts.terminal = true;
            REQUIRE(finish().observationAccepted);
        }

        pending(t::RecoveryAction::DISPOSE_ATTEMPT);
    }

    /** Complete real private-disposal and source-reset proof observations, separately.
     * @return Model outcome after the source barrier.
     */
    t::RecoveryStep retireAndReset() {
        pending(t::RecoveryAction::DISPOSE_ATTEMPT);
        REQUIRE(finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, true)
                    .observationAccepted);
        pending(t::RecoveryAction::RESET_SOURCES);

        return finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, false,
                      true);
    }
};

static_assert(!std::is_copy_constructible_v<t::RecoveryState>);
static_assert(!std::is_move_assignable_v<t::RecoveryState>);
static_assert(std::is_nothrow_move_constructible_v<t::RecoveryState>);
static_assert(noexcept(std::declval<t::RecoveryState &>().step(std::declval<Time>(),
                                                               std::declval<const t::RecoveryObservation &>(),
                                                               std::declval<t::RecoveryStateWork &>())));
static_assert(noexcept(std::declval<const t::RecoveryState &>().status()));
static_assert(noexcept(std::declval<const t::RecoveryState &>().nextWake()));
} // namespace

TEST_CASE("recovery state validates finite policy and preserves explicit single-attempt default") {
    char origin{};
    CHECK(t::RecoveryState::create(config(), &origin));
    CHECK_FALSE(t::RecoveryState::create(config(), nullptr));

    for (const auto attempts : {0U, 1U, 2U}) {
        auto selected = config(attempts);
        selected.retryDelay = 0ms;
        CHECK(t::RecoveryState::create(selected, &origin).has_value() == (attempts == 1));
    }

    auto selected = config();
    selected.families = 0x08;
    CHECK_FALSE(t::RecoveryState::create(selected, &origin));
    selected = config();
    selected.heartbeatInterval = 0ms;
    CHECK_FALSE(t::RecoveryState::create(selected, &origin));
    selected = config();
    selected.protocolInactivity = -1ms;
    CHECK_FALSE(t::RecoveryState::create(selected, &origin));
    selected = config();
    selected.heartbeatInterval = std::chrono::milliseconds{static_cast<std::int64_t>(INT32_MAX) + 1};
    CHECK_FALSE(t::RecoveryState::create(selected, &origin));
    CHECK_FALSE(t::RecoveryState::create(config(), &origin, std::numeric_limits<std::uint64_t>::max()));

    Driver value;
    CHECK(value.step().status == t::RecoveryStepStatus::IDLE);
    CHECK(value.state.status().attempts == 0);
    CHECK_FALSE(value.state.nextWake());
}

TEST_CASE("recovery factory entry is explicit once and pure work refusal consumes no attempt") {
    Driver value(config(3));
    value.state.requestStart();
    t::RecoveryStateWork none{0, 20};
    CHECK(value.state.step(value.now, {}, none).status == t::RecoveryStepStatus::YIELD);
    CHECK(none.transitions == 20);
    CHECK(value.step({}, 0).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.state.status().attempts == 0);
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    const auto old = value.state.status().pending;
    CHECK(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.state.status().attempts == 0);
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    CHECK(value.state.status().pending.sequence != old.sequence);

    auto entered = t::RecoveryObservation{value.state.status().pending, t::RecoveryCompletion::FACTORY_ENTERED};
    CHECK(value.step(entered, 0).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.state.status().attempts == 0);
    CHECK(value.step(entered).factoryEntryAccepted);
    CHECK(value.state.status().attempts == 1);
    CHECK_FALSE(value.step(entered).factoryEntryAccepted);
    CHECK(value.state.status().attempts == 1);
}

TEST_CASE("recovery entered factory failure including late work rejection is terminal without retry refund") {
    for (const auto cause : {t::RecoveryCause::FACTORY_FAILURE, t::RecoveryCause::FACTORY_WORK_LIMIT}) {
        Driver value(config(3));
        value.state.requestStart();
        REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
        REQUIRE(value.finish(t::RecoveryCompletion::FACTORY_ENTERED).factoryEntryAccepted);
        REQUIRE(
            value.finish(t::RecoveryCompletion::RETRYABLE_FAILURE, t::RecoveryInput::NONE, cause).observationAccepted);
        CHECK(value.state.status().attempts == 1);
        CHECK_FALSE(value.state.status().attemptPresent);
        value.pending(t::RecoveryAction::RESET_SOURCES);
        REQUIRE(value
                    .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false,
                            false, true)
                    .observationAccepted);
        value.pending(t::RecoveryAction::STOP_DELIVERY);
        CHECK(value
                  .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false,
                          false, false, true)
                  .status == t::RecoveryStepStatus::TERMINAL);
        CHECK(value.state.status().phase == t::RecoveryPhase::FAILED);
        CHECK(value.state.status().currentCause == cause);
        CHECK(value.state.status().attempts == 1);
    }
}

TEST_CASE("recovery empty demand still requires actual protocol and independent authentication eligibility") {
    auto selected = config();
    selected.heartbeatInterval = 5ms;
    Driver value(selected);
    value.connect(t::RecoveryInput::NONE, false);
    CHECK_FALSE(value.state.status().eligible);
    CHECK(value.state.status().phase == t::RecoveryPhase::NEGOTIATING);
    value.now = at(10ms);
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).observationAccepted);
    CHECK(value.state.status().protocolObserved);
    CHECK_FALSE(value.state.status().eligible);
    CHECK_FALSE(value.state.status().heartbeatDue);
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    value.facts.authenticationEligible = true;
    REQUIRE(value.finish().observationAccepted);
    CHECK(value.state.status().eligible);
    CHECK(value.state.status().phase == t::RecoveryPhase::ACTIVE);
    CHECK(value.state.nextWake() == at(15ms));
}

TEST_CASE("recovery protocol inactivity begins during negotiation and only real validated poll kinds refresh it") {
    const std::array kinds{t::RecoveryInput::EMPTY_HEARTBEAT, t::RecoveryInput::HEARTBEAT, t::RecoveryInput::PROTOCOL,
                           t::RecoveryInput::DESCRIPTIONS, t::RecoveryInput::DATA};

    for (const auto kind : kinds) {
        auto selected = config();
        selected.protocolInactivity = 10ms;
        Driver value(selected);
        value.connect();
        value.now = at(9ms);
        REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
        REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, kind).observationAccepted);
        CHECK(value.state.nextWake() == at(19ms));
        value.now = at(10ms);
        CHECK(value.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
        CHECK(value.state.status().firstCause == t::RecoveryCause::NONE);
    }

    auto selected = config();
    selected.protocolInactivity = 10ms;
    Driver silent(selected);
    silent.connect(t::RecoveryInput::NONE, false);
    CHECK(silent.state.nextWake() == at(10ms));
    silent.now = at(9ms);
    REQUIRE(silent.step().status == t::RecoveryStepStatus::ACTION);
    REQUIRE(silent.finish().observationAccepted); // Default/no owning event proves no traffic.
    silent.now = at(10ms);
    CHECK(silent.step().action.action == t::RecoveryAction::REQUEST_ABORT);
    CHECK(silent.state.status().currentCause == t::RecoveryCause::PROTOCOL_INACTIVITY);
}

TEST_CASE("recovery deadline wins same-sample traffic and cancellation wins retry replay and heartbeat") {
    auto selected = config(2);
    selected.protocolInactivity = 10ms;
    selected.heartbeatInterval = 10ms;
    Driver value(selected);
    value.connect();
    value.now = at(9ms);
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    value.now = at(10ms);
    CHECK(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::DATA).action.action ==
          t::RecoveryAction::REQUEST_ABORT);
    CHECK(value.state.status().firstCause == t::RecoveryCause::PROTOCOL_INACTIVITY);

    Driver stopped(selected);
    stopped.connect();
    REQUIRE(stopped.step().status == t::RecoveryStepStatus::ACTION);
    stopped.now = at(10ms);
    stopped.state.requestStop();
    stopped.facts.replayPending = true;
    stopped.facts.deliveryPending = true;
    CHECK(stopped.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).action.action ==
          t::RecoveryAction::REQUEST_ABORT);
    CHECK(stopped.state.status().stopRequested);
    CHECK(stopped.state.status().firstCause == t::RecoveryCause::NONE);
    CHECK_FALSE(stopped.state.status().eligible);
}

TEST_CASE("recovery heartbeat uses local output admission idle and retains one due action across slot work rejection") {
    auto selected = config();
    selected.heartbeatInterval = 10ms;
    Driver value(selected);
    value.connect();
    value.now = at(10ms);
    REQUIRE(value.step({}, 0).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.state.status().heartbeatDue);
    CHECK(value.state.nextWake() == at(10ms));
    REQUIRE(value.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::DATA).action.action ==
            t::RecoveryAction::HEARTBEAT);
    const auto first = value.state.status().pending;
    REQUIRE(value.finish(t::RecoveryCompletion::OUTPUT_BLOCKED).action.action == t::RecoveryAction::POLL_ATTEMPT);
    CHECK(value.state.status().heartbeatDue);
    REQUIRE(value.finish().action.action == t::RecoveryAction::HEARTBEAT);
    CHECK(value.state.status().pending.sequence != first.sequence);
    CHECK(value.finish().status == t::RecoveryStepStatus::INVALID_OBSERVATION); // No actual admission proof.
    CHECK(value.state.status().heartbeatDue);
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, true)
                .observationAccepted);
    CHECK_FALSE(value.state.status().heartbeatDue);
    CHECK(value.state.nextWake() == at(20ms));
}

TEST_CASE("recovery intended replay gates rearm separately on readiness version and sole-output progress") {
    Driver value;
    value.state.requestStart();
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    value.createAttempt();
    value.facts.transportEligible = true;
    value.facts.authenticationEligible = true;
    value.facts.demandedFamilies = 0x07;
    value.facts.replayPending = true;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).action.action ==
            t::RecoveryAction::REPLAY);
    REQUIRE(value.finish(t::RecoveryCompletion::READINESS_BLOCKED).action.action == t::RecoveryAction::POLL_ATTEMPT);
    CHECK(value.finish().status == t::RecoveryStepStatus::IDLE);
    REQUIRE(value.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
    ++value.facts.readinessVersion;
    REQUIRE(value.finish().action.action == t::RecoveryAction::REPLAY);
    REQUIRE(value.finish(t::RecoveryCompletion::OUTPUT_BLOCKED).action.action == t::RecoveryAction::POLL_ATTEMPT);
    REQUIRE(value.finish().action.action == t::RecoveryAction::REPLAY);
    value.facts.replayPending = false;
    value.facts.convergedFamilies = value.facts.demandedFamilies;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, true)
                .observationAccepted);
    CHECK(value.state.status().phase == t::RecoveryPhase::ACTIVE);
}

TEST_CASE("recovery successor waits for terminal actual private disposal and real source reset then fixed backoff") {
    Driver value(config(2));
    value.connect();
    value.loseTransport();
    CHECK(value.state.status().attemptPresent);
    CHECK(value.finish().status == t::RecoveryStepStatus::INVALID_OBSERVATION);
    CHECK(value.state.status().attemptPresent);
    REQUIRE(value.retireAndReset().observationAccepted);
    CHECK_FALSE(value.state.status().attemptPresent);
    CHECK(value.state.status().phase == t::RecoveryPhase::BACKOFF);
    CHECK(value.state.nextWake() == at(10ms));
    value.now = at(9ms);
    CHECK(value.step().status == t::RecoveryStepStatus::IDLE);
    value.now = at(10ms);
    REQUIRE(value.step().action.action == t::RecoveryAction::PREPARE_ATTEMPT);
    CHECK(value.state.status().attempts == 1);
    value.createAttempt();
    CHECK(value.state.status().attempts == 2);
    value.facts.transportEligible = true;
    value.facts.authenticationEligible = true;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).observationAccepted);
    value.loseTransport(true);
    REQUIRE(value.retireAndReset().observationAccepted);
    value.pending(t::RecoveryAction::STOP_DELIVERY);
    CHECK(value
              .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, false,
                      false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().phase == t::RecoveryPhase::EXHAUSTED);
    CHECK(value.state.status().attempts == 2);
    CHECK(value.state.status().firstCause == t::RecoveryCause::TRANSPORT);
}

TEST_CASE("recovery stop during backoff or continuing reset never waits for downstream ACK or starts a factory") {
    for (const bool inReset : {false, true}) {
        Driver value(config(2));
        value.connect();
        value.loseTransport(true);

        if (inReset) {
            REQUIRE(value
                        .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false,
                                true)
                        .observationAccepted);
            value.pending(t::RecoveryAction::RESET_SOURCES);
            value.state.requestStop();
            CHECK(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::YIELD);
        } else {
            REQUIRE(value.retireAndReset().observationAccepted);
            value.state.requestStop();
        }

        CHECK(value.step({}, 0).status == t::RecoveryStepStatus::YIELD);
        REQUIRE(value.step().action.action == t::RecoveryAction::STOP_DELIVERY);
        CHECK(value.finish().status == t::RecoveryStepStatus::INVALID_OBSERVATION);
        CHECK(value
                  .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false,
                          false, false, true)
                  .status == t::RecoveryStepStatus::TERMINAL);
        CHECK(value.state.status().phase == t::RecoveryPhase::CLOSED);
        CHECK(value.state.status().attempts == 1);
        CHECK_FALSE(value.state.nextWake());
    }
}

TEST_CASE("recovery ignores foreign old action tickets and stop invalidates post-callback continuation") {
    Driver first(config(2));
    first.connect();
    REQUIRE(first.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
    const auto old = first.state.status().pending;
    Driver replacement(config(2));
    replacement.connect();
    REQUIRE(replacement.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
    CHECK(replacement.step({old, t::RecoveryCompletion::COMPLETED}).status == t::RecoveryStepStatus::STALE);
    CHECK(replacement.state.status().attempts == 1);
    CHECK(replacement.state.status().pending.origin != old.origin);
    first.state.requestStop();
    CHECK(first.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::DATA).action.action ==
          t::RecoveryAction::REQUEST_ABORT);
    CHECK_FALSE(first.state.status().eligible);
    CHECK(replacement.state.status().eligible);
}

TEST_CASE("recovery regressing clock is fatal once and cannot repeatedly reopen cleanup barriers") {
    Driver value(config(3));
    value.connect();
    value.now = at(-1ms);
    REQUIRE(value.step().action.action == t::RecoveryAction::REQUEST_ABORT);
    REQUIRE(value.finish().action.action == t::RecoveryAction::POLL_ATTEMPT);
    value.facts.terminal = true;
    REQUIRE(value.finish().action.action == t::RecoveryAction::DISPOSE_ATTEMPT);
    REQUIRE(value.retireAndReset().observationAccepted);
    value.pending(t::RecoveryAction::STOP_DELIVERY);
    CHECK(value
              .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, false,
                      false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().phase == t::RecoveryPhase::FAILED);
    CHECK(value.state.status().firstCause == t::RecoveryCause::CLOCK_BACKWARDS);
    CHECK(value.state.status().attempts == 1);
}

TEST_CASE("recovery checked liveness and retry deadline arithmetic fails before monotonic wrap") {
    auto selected = config();
    selected.protocolInactivity = 10ms;
    Driver input(selected);
    input.now = Time::max() - std::chrono::duration_cast<t::RecoveryClock::duration>(1ms);
    input.connect();
    input.pending(t::RecoveryAction::REQUEST_ABORT);
    CHECK(input.state.status().firstCause == t::RecoveryCause::CLOCK_RANGE);

    Driver retry(config(2));
    retry.connect();
    retry.loseTransport(true);
    retry.now = Time::max() - std::chrono::duration_cast<t::RecoveryClock::duration>(1ms);
    REQUIRE(retry.retireAndReset().observationAccepted);
    retry.pending(t::RecoveryAction::STOP_DELIVERY);
    CHECK(retry.state.status().currentCause == t::RecoveryCause::CLOCK_RANGE);
    CHECK(retry.state.status().firstCause == t::RecoveryCause::TRANSPORT);
    CHECK(retry.state.status().secondaryCause == t::RecoveryCause::CLOCK_RANGE);
}

TEST_CASE("recovery reserves final ticket identity for continuing fatal retirement without reuse") {
    Driver value(config(), std::numeric_limits<std::uint64_t>::max() - 1);
    value.state.requestStart();
    const auto issued = value.step();
    REQUIRE(issued.action.action == t::RecoveryAction::RETIRE_ALL);
    CHECK(issued.action.sequence == std::numeric_limits<std::uint64_t>::max());
    CHECK(value.state.status().attempts == 0);
    CHECK(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::CONTINUE);
    CHECK(value.state.status().pending == issued.action);
    CHECK(value.step({}, 0).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.step().status == t::RecoveryStepStatus::CONTINUE);
    CHECK(value.finish().status == t::RecoveryStepStatus::INVALID_OBSERVATION);
    CHECK(value
              .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, true,
                      false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().phase == t::RecoveryPhase::FAILED);
    CHECK(value.state.status().currentCause == t::RecoveryCause::IDENTITY_EXHAUSTED);
}

TEST_CASE("recovery cutoff before factory-entry commit cannot charge or create an attempt") {
    Driver value(config(3));
    value.state.requestStart();
    REQUIRE(value.step().action.action == t::RecoveryAction::PREPARE_ATTEMPT);
    const auto old = value.state.status().pending;
    value.state.requestStop();
    const auto rejected = value.finish(t::RecoveryCompletion::FACTORY_ENTERED);
    CHECK(rejected.status == t::RecoveryStepStatus::INVALID_OBSERVATION);
    CHECK_FALSE(rejected.factoryEntryAccepted);
    CHECK(value.state.status().attempts == 0);
    REQUIRE(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::YIELD);
    CHECK(value.step({old, t::RecoveryCompletion::COMPLETED}).status == t::RecoveryStepStatus::STALE);
    REQUIRE(value.step().action.action == t::RecoveryAction::STOP_DELIVERY);
    CHECK(value
              .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, false,
                      false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().attempts == 0);
}

TEST_CASE("recovery old same-owner epoch completion cannot revive timers or readiness in a new attempt") {
    auto selected = config(2);
    selected.heartbeatInterval = 5ms;
    selected.protocolInactivity = 20ms;
    Driver value(selected);
    value.connect();
    REQUIRE(value.step().action.action == t::RecoveryAction::POLL_ATTEMPT);
    const auto old = value.state.status().pending;
    value.loseTransport(true);
    REQUIRE(value.retireAndReset().observationAccepted);
    value.now = at(10ms);
    REQUIRE(value.step().action.action == t::RecoveryAction::PREPARE_ATTEMPT);
    value.createAttempt();
    const auto current = value.state.status().pending;
    CHECK(current.attempt == 2);
    CHECK(current.origin == old.origin);
    CHECK_FALSE(value.state.status().protocolObserved);
    const t::RecoveryFacts staleFacts{true, true, false, true, true, false, 1, 0, 99};
    CHECK(value.step({old, t::RecoveryCompletion::COMPLETED, staleFacts, t::RecoveryInput::PROTOCOL}).status ==
          t::RecoveryStepStatus::STALE);
    CHECK(value.state.status().pending == current);
    CHECK_FALSE(value.state.status().eligible);
    CHECK_FALSE(value.state.status().protocolObserved);
    CHECK_FALSE(value.state.nextWake());
    value.facts.transportEligible = true;
    value.facts.authenticationEligible = true;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).observationAccepted);
    CHECK(value.state.nextWake() == at(15ms));
}

TEST_CASE("recovery failed reset does not fabricate a commit retry or lose first cleanup causes") {
    Driver value(config(3));
    value.connect();
    value.loseTransport(true);
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, true)
                .observationAccepted);
    value.pending(t::RecoveryAction::RESET_SOURCES);
    REQUIRE(
        value.finish(t::RecoveryCompletion::TERMINAL_FAILURE, t::RecoveryInput::NONE, t::RecoveryCause::RESET_FAILURE)
            .observationAccepted);
    value.pending(t::RecoveryAction::STOP_DELIVERY);
    CHECK(value.state.status().firstCause == t::RecoveryCause::TRANSPORT);
    CHECK(value.state.status().secondaryCause == t::RecoveryCause::RESET_FAILURE);
    CHECK(value
              .finish(t::RecoveryCompletion::TERMINAL_FAILURE, t::RecoveryInput::NONE, t::RecoveryCause::RESOURCE,
                      false, false, false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().phase == t::RecoveryPhase::FAILED);
    CHECK(value.state.status().firstCause == t::RecoveryCause::TRANSPORT);
    CHECK(value.state.status().secondaryCause == t::RecoveryCause::RESET_FAILURE);
    CHECK(value.state.status().currentCause == t::RecoveryCause::RESOURCE);
    CHECK(value.state.status().attempts == 1);
}

TEST_CASE("recovery combined blocked replay and delivery receive bounded alternating opportunities") {
    for (const auto blocked : {t::RecoveryCompletion::OUTPUT_BLOCKED, t::RecoveryCompletion::DEFERRED}) {
        Driver value;
        value.state.requestStart();
        REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
        value.createAttempt();
        value.facts.transportEligible = true;
        value.facts.authenticationEligible = true;
        value.facts.demandedFamilies = 1;
        value.facts.replayPending = true;
        value.facts.deliveryPending = true;
        REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).observationAccepted);
        unsigned replay = 0;
        unsigned delivery = 0;
        unsigned sinceDelivery = 0;
        unsigned longestDeliveryWait = 0;

        for (unsigned action = 0; action < 24; ++action) {
            if (value.state.status().pending.action == t::RecoveryAction::NONE) {
                REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
            }

            ++sinceDelivery;
            longestDeliveryWait = (std::max)(longestDeliveryWait, sinceDelivery);

            switch (value.state.status().pending.action) {
            case t::RecoveryAction::REPLAY:
                ++replay;
                CHECK(value.finish(blocked).observationAccepted);
                break;
            case t::RecoveryAction::DELIVER:
                ++delivery;
                sinceDelivery = 0;
                CHECK(value.finish().observationAccepted);
                break;
            case t::RecoveryAction::POLL_ATTEMPT:
                CHECK(value.finish().observationAccepted);
                break;
            default:
                FAIL("Only the three bounded active action classes belong to this trace");
            }
        }

        CHECK(replay > 0);
        CHECK(delivery > 0);
        CHECK(longestDeliveryWait <= 4); // One poll between each output/delivery opportunity, independent of admission.
        CHECK(value.state.status().attempts == 1);
    }
}

TEST_CASE("recovery combined always-due heartbeat replay and delivery cannot starve an eligible class") {
    auto selected = config();
    selected.heartbeatInterval = 1ms;
    Driver value(selected);
    value.state.requestStart();
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    value.createAttempt();
    value.facts.transportEligible = true;
    value.facts.authenticationEligible = true;
    value.facts.demandedFamilies = 1;
    value.facts.replayPending = true;
    value.facts.deliveryPending = true;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).observationAccepted);
    unsigned replay = 0;
    unsigned heartbeat = 0;
    unsigned delivery = 0;
    auto previousOutput = t::RecoveryAction::NONE;

    for (unsigned action = 0; action < 48; ++action) {
        if (value.state.status().pending.action == t::RecoveryAction::NONE) {
            REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
        }

        const auto kind = value.state.status().pending.action;

        if (kind == t::RecoveryAction::POLL_ATTEMPT) {
            // Native cadence may exceed the heartbeat interval; unconditional heartbeat priority is insufficient.
            value.now += 2ms;
            CHECK(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::DATA).observationAccepted);
        } else if (kind == t::RecoveryAction::DELIVER) {
            ++delivery;
            CHECK(value.finish().observationAccepted);
        } else {
            REQUIRE((kind == t::RecoveryAction::REPLAY || kind == t::RecoveryAction::HEARTBEAT));
            CHECK(kind != previousOutput);
            previousOutput = kind;
            replay += kind == t::RecoveryAction::REPLAY;
            heartbeat += kind == t::RecoveryAction::HEARTBEAT;
            CHECK(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, true)
                      .observationAccepted);
        }
    }

    CHECK(replay > 0);
    CHECK(heartbeat > 0);
    CHECK(delivery > 0);
    CHECK(value.state.status().attempts == 1);
}

TEST_CASE("recovery combined backpressure never overrides reset or stop control priority") {
    Driver value(config(2));
    value.state.requestStart();
    REQUIRE(value.step().status == t::RecoveryStepStatus::ACTION);
    value.createAttempt();
    value.facts.transportEligible = true;
    value.facts.authenticationEligible = true;
    value.facts.replayPending = true;
    value.facts.deliveryPending = true;
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::PROTOCOL).action.action ==
            t::RecoveryAction::REPLAY);
    REQUIRE(value.finish(t::RecoveryCompletion::OUTPUT_BLOCKED).action.action == t::RecoveryAction::POLL_ATTEMPT);
    value.facts.terminal = true;
    REQUIRE(value.finish(t::RecoveryCompletion::RETRYABLE_FAILURE, t::RecoveryInput::NONE, t::RecoveryCause::TRANSPORT)
                .action.action == t::RecoveryAction::DISPOSE_ATTEMPT);
    REQUIRE(value.finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, true)
                .action.action == t::RecoveryAction::RESET_SOURCES);
    CHECK(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::YIELD);
    REQUIRE(value.step().action.action == t::RecoveryAction::RESET_SOURCES);
    value.state.requestStop();
    CHECK(value.finish(t::RecoveryCompletion::DEFERRED).status == t::RecoveryStepStatus::YIELD);
    REQUIRE(value.step().action.action == t::RecoveryAction::STOP_DELIVERY);
    CHECK(value
              .finish(t::RecoveryCompletion::COMPLETED, t::RecoveryInput::NONE, t::RecoveryCause::NONE, false, false,
                      false, true)
              .status == t::RecoveryStepStatus::TERMINAL);
    CHECK(value.state.status().phase == t::RecoveryPhase::CLOSED);
    CHECK(value.state.status().attempts == 1);
}
