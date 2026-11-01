// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_state.hpp"

#include "deadline.hpp"

#include <climits>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

/** Validate the existing finite endpoint millisecond domain.
 * @param value Explicit positive interval.
 * @return Whether conversion is within the existing supported profile.
 */
bool validInterval(std::chrono::milliseconds value) noexcept {
    return value.count() > 0 && value.count() <= INT_MAX;
}

/** Recognize terminal phases without inspecting any external owner.
 * @param phase Current scheduler phase.
 * @return Whether all requested retirement actions have completed.
 */
bool terminalPhase(RecoveryPhase phase) noexcept {
    return phase == RecoveryPhase::CLOSED || phase == RecoveryPhase::EXHAUSTED || phase == RecoveryPhase::FAILED;
}

/** Construct a result without allocating or issuing an action.
 * @param status Exact bounded step outcome.
 * @param accepted Whether a matching observation was consumed.
 * @return Detached step result.
 */
RecoveryStep result(RecoveryStepStatus status, bool accepted = false) noexcept {
    return {status, {}, accepted, false};
}

/** Identify an action that may report a successful local output admission.
 * @param action Pending integration action.
 * @return True only for the three explicit output routes.
 */
bool outputAction(RecoveryAction action) noexcept {
    return action == RecoveryAction::NEGOTIATE || action == RecoveryAction::REPLAY ||
           action == RecoveryAction::HEARTBEAT;
}

} // namespace

RecoveryState::RecoveryState(RecoveryConfig config, const void *origin, std::uint64_t sequence) noexcept
    : config_(config), origin_(origin), sequence_(sequence) {
}

std::expected<RecoveryState, RecoveryConfigError> RecoveryState::create(RecoveryConfig config, const void *origin,
                                                                        std::uint64_t sequence) noexcept {
    const bool delayValid =
        config.maximumAttempts == 1 ? config.retryDelay.count() == 0 : validInterval(config.retryDelay);

    if (origin == nullptr || sequence == std::numeric_limits<std::uint64_t>::max() || config.maximumAttempts == 0 ||
        !delayValid || (config.families & ~RECOVERY_FAMILIES) != 0 ||
        (config.heartbeatInterval && !validInterval(*config.heartbeatInterval)) ||
        (config.protocolInactivity && !validInterval(*config.protocolInactivity))) {
        return std::unexpected(RecoveryConfigError::INVALID_CONFIGURATION);
    }

    return RecoveryState(config, origin, sequence);
}

void RecoveryState::requestStart() noexcept {
    if (!stopRequested_ && !terminalPhase(phase_)) {
        startRequested_ = true;
    }
}

void RecoveryState::requestStop() noexcept {
    stopRequested_ = true;
    retryDeadline_.reset();
    receiveDeadline_.reset();
    heartbeatDeadline_.reset();

    if (!terminalPhase(phase_)) {
        phase_ = RecoveryPhase::STOPPING;
    }
}

void RecoveryState::fail(RecoveryCause cause, bool retryable) noexcept {
    const bool alreadyRecovering = recovering_;

    if (cause == RecoveryCause::NONE) {
        cause = RecoveryCause::TRANSPORT;
    }

    if (firstCause_ == RecoveryCause::NONE) {
        firstCause_ = cause;
    } else if (secondaryCause_ == RecoveryCause::NONE && cause != firstCause_) {
        secondaryCause_ = cause;
    }

    currentCause_ = cause;
    retryable_ = alreadyRecovering ? retryable_ && retryable : retryable;
    recovering_ = true;

    if (!alreadyRecovering) {
        resetComplete_ = config_.families == 0 || attempts_ == 0;
    }

    retryDeadline_.reset();
    receiveDeadline_.reset();
    heartbeatDeadline_.reset();
}

bool RecoveryState::eligible() const noexcept {
    return attemptPresent_ && attemptStarted_ && facts_.transportEligible && facts_.authenticationEligible &&
           protocolObserved_ && !facts_.negotiationPending && !recovering_ && !stopRequested_;
}

void RecoveryState::rotate(RecoveryAction action) noexcept {
    if (outputAction(action)) {
        preferDelivery_ = true;

        if (action == RecoveryAction::REPLAY) {
            preferHeartbeat_ = true;
        } else if (action == RecoveryAction::HEARTBEAT) {
            preferHeartbeat_ = false;
        }
    } else if (action == RecoveryAction::DELIVER) {
        preferDelivery_ = false;
    }
}

RecoveryStep RecoveryState::observe(RecoveryClock::time_point now, const RecoveryObservation &observation,
                                    RecoveryStateWork &work) noexcept {
    if (observation.completion == RecoveryCompletion::NONE) {
        return result(RecoveryStepStatus::IDLE);
    }

    if (pending_.action == RecoveryAction::NONE || observation.ticket != pending_) {
        return result(RecoveryStepStatus::STALE);
    }

    const auto action = pending_.action;

    if (action == RecoveryAction::RETIRE_ALL) {
        if (observation.completion == RecoveryCompletion::DEFERRED) {
            return result(RecoveryStepStatus::IDLE, true);
        }

        if ((observation.completion != RecoveryCompletion::COMPLETED &&
             observation.completion != RecoveryCompletion::TERMINAL_FAILURE) ||
            !observation.privateDisposed || !observation.deliveryRetired) {
            return result(RecoveryStepStatus::INVALID_OBSERVATION);
        }

        pending_ = {};
        attemptPresent_ = false;
        attemptStarted_ = false;
        deliveryStopped_ = true;

        if (observation.completion == RecoveryCompletion::TERMINAL_FAILURE) {
            fail(observation.cause, false);
        }

        phase_ = RecoveryPhase::FAILED;

        return result(RecoveryStepStatus::TERMINAL, true);
    }

    if (observation.completion == RecoveryCompletion::FACTORY_ENTERED) {
        if (action != RecoveryAction::PREPARE_ATTEMPT || factoryEntered_ || stopRequested_ || recovering_) {
            return result(RecoveryStepStatus::INVALID_OBSERVATION);
        }

        if (work.transitions == 0) {
            return result(RecoveryStepStatus::YIELD);
        }

        --work.transitions;
        ++attempts_; // PREPARE is issued only while attempts_ < the finite uint32 maximumAttempts.
        factoryEntered_ = true;
        facts_ = {};
        protocolObserved_ = false;
        attemptStarted_ = false;
        abortRequested_ = false;
        recovering_ = false;
        retryable_ = false;
        resetComplete_ = true;
        resetFailed_ = false;
        currentCause_ = RecoveryCause::NONE;
        blockedReadiness_.reset();
        outputBlocked_ = false;
        preferDelivery_ = false;
        preferHeartbeat_ = false;
        retryDeadline_.reset();

        return {RecoveryStepStatus::WAITING, {}, true, true};
    }

    if (action == RecoveryAction::PREPARE_ATTEMPT && !factoryEntered_ &&
        observation.completion != RecoveryCompletion::DEFERRED) {
        return result(RecoveryStepStatus::INVALID_OBSERVATION);
    }

    if (observation.completion == RecoveryCompletion::DEFERRED) {
        if (action == RecoveryAction::PREPARE_ATTEMPT && factoryEntered_) {
            return result(RecoveryStepStatus::INVALID_OBSERVATION);
        }

        pending_ = {};
        rotate(action);

        if (outputAction(action) || action == RecoveryAction::DELIVER || action == RecoveryAction::POLL_ATTEMPT) {
            pollNeeded_ = true;
        }

        return result(RecoveryStepStatus::YIELD, true);
    }

    if (observation.completion == RecoveryCompletion::OUTPUT_BLOCKED) {
        if (!outputAction(action)) {
            return result(RecoveryStepStatus::INVALID_OBSERVATION);
        }

        outputBlocked_ = true;
        pollNeeded_ = true;
        pending_ = {};
        rotate(action);

        return result(RecoveryStepStatus::IDLE, true);
    }

    if (observation.completion == RecoveryCompletion::READINESS_BLOCKED) {
        if (action != RecoveryAction::REPLAY) {
            return result(RecoveryStepStatus::INVALID_OBSERVATION);
        }

        blockedReadiness_ = facts_.readinessVersion;
        pollNeeded_ = true;
        pending_ = {};
        rotate(action);

        return result(RecoveryStepStatus::IDLE, true);
    }

    const bool failed = observation.completion == RecoveryCompletion::RETRYABLE_FAILURE ||
                        observation.completion == RecoveryCompletion::TERMINAL_FAILURE;

    if (observation.completion != RecoveryCompletion::COMPLETED && !failed) {
        return result(RecoveryStepStatus::INVALID_OBSERVATION);
    }

    if ((action == RecoveryAction::DISPOSE_ATTEMPT && !observation.privateDisposed) ||
        (action == RecoveryAction::RESET_SOURCES && !failed && !observation.sourceResetComplete) ||
        (action == RecoveryAction::STOP_DELIVERY && !observation.deliveryRetired) ||
        (!failed && ((observation.facts.demandedFamilies & ~config_.families) != 0 ||
                     (observation.facts.convergedFamilies & ~observation.facts.demandedFamilies) != 0)) ||
        (observation.input != RecoveryInput::NONE && action != RecoveryAction::POLL_ATTEMPT) ||
        (observation.outputAdmitted && !outputAction(action)) ||
        (action == RecoveryAction::HEARTBEAT && !failed && !observation.outputAdmitted)) {
        return result(RecoveryStepStatus::INVALID_OBSERVATION);
    }

    pending_ = {};

    if (failed) {
        const auto cause = observation.cause == RecoveryCause::NONE
                               ? (action == RecoveryAction::PREPARE_ATTEMPT ? RecoveryCause::FACTORY_FAILURE
                                                                            : RecoveryCause::TRANSPORT)
                               : observation.cause;

        if (action == RecoveryAction::PREPARE_ATTEMPT) {
            factoryEntered_ = false;
            // The observed factory has returned through its own funded failure unwind.
            attemptPresent_ = false;
        } else if (action == RecoveryAction::DISPOSE_ATTEMPT) {
            attemptPresent_ = false;
            attemptStarted_ = false;
        } else if (action == RecoveryAction::RESET_SOURCES) {
            resetFailed_ = true;
        } else if (action == RecoveryAction::STOP_DELIVERY) {
            deliveryStopped_ = true;
        } else if (action == RecoveryAction::REQUEST_ABORT) {
            abortRequested_ = true;
        }

        facts_.terminal = facts_.terminal || observation.facts.terminal;
        const bool mayRetry = action != RecoveryAction::PREPARE_ATTEMPT && action != RecoveryAction::RESET_SOURCES &&
                              action != RecoveryAction::STOP_DELIVERY &&
                              observation.completion == RecoveryCompletion::RETRYABLE_FAILURE;
        fail(cause, mayRetry);

        return result(RecoveryStepStatus::IDLE, true);
    }

    switch (action) {
    case RecoveryAction::PREPARE_ATTEMPT:
        factoryEntered_ = false;
        attemptPresent_ = true;
        pollNeeded_ = true;
        break;
    case RecoveryAction::START_ATTEMPT:
        attemptStarted_ = true;
        pollNeeded_ = true;
        break;
    case RecoveryAction::POLL_ATTEMPT:
        pollNeeded_ = false;
        outputBlocked_ = false;
        break;
    case RecoveryAction::REQUEST_ABORT:
        abortRequested_ = true;
        pollNeeded_ = true;
        break;
    case RecoveryAction::DISPOSE_ATTEMPT:
        attemptPresent_ = false;
        attemptStarted_ = false;
        resetComplete_ = config_.families == 0;
        break;
    case RecoveryAction::RESET_SOURCES:
        resetComplete_ = true;
        break;
    case RecoveryAction::STOP_DELIVERY:
        deliveryStopped_ = true;
        break;
    default:
        pollNeeded_ = true;
        break;
    }

    rotate(action);

    if (action == RecoveryAction::START_ATTEMPT || action == RecoveryAction::POLL_ATTEMPT || outputAction(action) ||
        action == RecoveryAction::DELIVER) {
        facts_ = observation.facts;

        if (action == RecoveryAction::POLL_ATTEMPT && observation.input == RecoveryInput::PROTOCOL) {
            protocolObserved_ = true;
        }

        if (facts_.terminal && !recovering_ && !stopRequested_) {
            fail(RecoveryCause::TRANSPORT, true);
        }

        if (!recovering_ && !stopRequested_) {
            if (config_.protocolInactivity && facts_.transportEligible &&
                (!receiveDeadline_ || observation.input != RecoveryInput::NONE)) {
                receiveDeadline_ = deadlineAfter(now, *config_.protocolInactivity);

                if (!receiveDeadline_) {
                    fail(RecoveryCause::CLOCK_RANGE, false);
                }
            }

            if (config_.heartbeatInterval && eligible() && (!heartbeatDeadline_ || observation.outputAdmitted)) {
                heartbeatDeadline_ = deadlineAfter(now, *config_.heartbeatInterval);

                if (!heartbeatDeadline_) {
                    fail(RecoveryCause::CLOCK_RANGE, false);
                }
            }
        }
    }

    return result(RecoveryStepStatus::IDLE, true);
}

RecoveryAction RecoveryState::choose(RecoveryClock::time_point now) noexcept {
    if (stopRequested_ || recovering_) {
        if (attemptPresent_) {
            phase_ = RecoveryPhase::RETIRING;

            if (facts_.terminal) {
                return RecoveryAction::DISPOSE_ATTEMPT;
            }

            return abortRequested_ ? RecoveryAction::POLL_ATTEMPT : RecoveryAction::REQUEST_ABORT;
        }

        if (!stopRequested_ && !resetComplete_ && !resetFailed_) {
            phase_ = RecoveryPhase::RESETTING;

            return RecoveryAction::RESET_SOURCES;
        }

        if (stopRequested_ || !retryable_ || attempts_ >= config_.maximumAttempts) {
            phase_ = RecoveryPhase::STOPPING;

            if (!deliveryStopped_) {
                return RecoveryAction::STOP_DELIVERY;
            }

            phase_ = stopRequested_ ? RecoveryPhase::CLOSED
                     : retryable_   ? RecoveryPhase::EXHAUSTED
                                    : RecoveryPhase::FAILED;

            return RecoveryAction::NONE;
        }

        if (!retryDeadline_) {
            retryDeadline_ = deadlineAfter(now, config_.retryDelay);

            if (!retryDeadline_) {
                fail(RecoveryCause::CLOCK_RANGE, false);
                phase_ = RecoveryPhase::STOPPING;

                return RecoveryAction::STOP_DELIVERY;
            }
        }

        phase_ = RecoveryPhase::BACKOFF;

        if (now < *retryDeadline_) {
            return RecoveryAction::NONE;
        }

        // The previous epoch has actually retired; only the next factory-entry commit clears recovery.
        phase_ = RecoveryPhase::PREPARING;
        recovering_ = false;

        return RecoveryAction::PREPARE_ATTEMPT;
    }

    if (!startRequested_) {
        phase_ = RecoveryPhase::CONFIGURED;

        return RecoveryAction::NONE;
    }

    if (!attemptPresent_) {
        phase_ = RecoveryPhase::PREPARING;

        return RecoveryAction::PREPARE_ATTEMPT;
    }

    if (!attemptStarted_) {
        phase_ = RecoveryPhase::STARTING;

        return RecoveryAction::START_ATTEMPT;
    }

    const bool converged = (facts_.demandedFamilies & facts_.convergedFamilies) == facts_.demandedFamilies;
    phase_ = !eligible()                          ? RecoveryPhase::NEGOTIATING
             : facts_.replayPending || !converged ? RecoveryPhase::REPLAYING
                                                  : RecoveryPhase::ACTIVE;

    if (pollNeeded_) {
        return RecoveryAction::POLL_ATTEMPT;
    }

    if (facts_.transportEligible && facts_.negotiationPending && !outputBlocked_) {
        return RecoveryAction::NEGOTIATE;
    }

    if (eligible()) {
        if (facts_.deliveryPending && preferDelivery_) {
            return RecoveryAction::DELIVER;
        }

        const bool replayReady = facts_.replayPending && !outputBlocked_ &&
                                 (!blockedReadiness_ || *blockedReadiness_ != facts_.readinessVersion);
        const bool heartbeatReady = heartbeatDeadline_ && now >= *heartbeatDeadline_ && !outputBlocked_;

        if (heartbeatReady && (preferHeartbeat_ || !replayReady)) {
            return RecoveryAction::HEARTBEAT;
        }

        if (replayReady) {
            return RecoveryAction::REPLAY;
        }

        if (facts_.deliveryPending) {
            return RecoveryAction::DELIVER;
        }
    }

    pollNeeded_ = true;

    return RecoveryAction::NONE;
}

RecoveryStep RecoveryState::issue(RecoveryAction action, RecoveryStateWork &work) noexcept {
    if (work.transitions == 0) {
        return result(RecoveryStepStatus::YIELD);
    }

    if (sequence_ == std::numeric_limits<std::uint64_t>::max() - 1) {
        // Reserve the final identity for one continuing retirement job, never a new ordinary action.
        fail(RecoveryCause::IDENTITY_EXHAUSTED, false);
        action = RecoveryAction::RETIRE_ALL;
        phase_ = RecoveryPhase::RETIRING;
    }

    --work.transitions;
    ++sequence_;
    const auto attempt = action == RecoveryAction::PREPARE_ATTEMPT ? attempts_ + 1 : attempts_;
    pending_ = {origin_, sequence_, attempt, action};

    return {RecoveryStepStatus::ACTION, pending_, false, false};
}

RecoveryStep RecoveryState::step(RecoveryClock::time_point now, const RecoveryObservation &observation,
                                 RecoveryStateWork &work) noexcept {
    if (terminalPhase(phase_)) {
        return result(RecoveryStepStatus::TERMINAL);
    }

    if (work.observations == 0) {
        return result(RecoveryStepStatus::YIELD);
    }

    --work.observations;

    if (lastNow_ && now < *lastNow_) {
        if (!clockFailed_) {
            clockFailed_ = true;
            fail(RecoveryCause::CLOCK_BACKWARDS, false);
        }

        // Fatal cleanup does not need a progressing clock and must not reopen an already completed reset.
        now = *lastNow_;
    } else {
        lastNow_ = now;
    }

    if (!stopRequested_ && !recovering_ && receiveDeadline_ && now >= *receiveDeadline_) {
        fail(RecoveryCause::PROTOCOL_INACTIVITY, true);
    }

    auto observed = observe(now, observation, work);

    if (observed.factoryEntryAccepted || observed.status == RecoveryStepStatus::TERMINAL ||
        observed.status == RecoveryStepStatus::STALE || observed.status == RecoveryStepStatus::INVALID_OBSERVATION ||
        observed.status == RecoveryStepStatus::YIELD) {
        return observed;
    }

    if (pending_.action != RecoveryAction::NONE) {
        if (pending_.action == RecoveryAction::RETIRE_ALL) {
            if (work.transitions == 0) {
                return result(RecoveryStepStatus::YIELD, observed.observationAccepted);
            }

            --work.transitions;

            return {RecoveryStepStatus::CONTINUE, pending_, observed.observationAccepted, false};
        }

        return result(RecoveryStepStatus::WAITING, observed.observationAccepted);
    }

    const auto action = choose(now);

    if (action == RecoveryAction::NONE) {
        return result(terminalPhase(phase_) ? RecoveryStepStatus::TERMINAL : RecoveryStepStatus::IDLE,
                      observed.observationAccepted);
    }

    auto issued = issue(action, work);
    issued.observationAccepted = observed.observationAccepted;

    return issued;
}

RecoveryStateStatus RecoveryState::status() const noexcept {
    const bool due = heartbeatDeadline_ && lastNow_ && *lastNow_ >= *heartbeatDeadline_ && eligible();

    return {phase_,          attempts_,       firstCause_,       secondaryCause_, currentCause_, stopRequested_,
            attemptPresent_, factoryEntered_, protocolObserved_, eligible(),      due,           terminalPhase(phase_),
            pending_};
}

std::optional<RecoveryClock::time_point> RecoveryState::nextWake() const noexcept {
    if (terminalPhase(phase_) || (!startRequested_ && !stopRequested_)) {
        return std::nullopt;
    }

    auto wake = retryDeadline_;

    if (receiveDeadline_ && (!wake || *receiveDeadline_ < *wake)) {
        wake = receiveDeadline_;
    }

    if (eligible() && heartbeatDeadline_ && (!wake || *heartbeatDeadline_ < *wake)) {
        wake = heartbeatDeadline_;
    }

    return wake;
}

} // namespace org::ttldtor::qdcxx::transport::detail
