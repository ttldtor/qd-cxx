// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_core.hpp"
#include "../client/subscription_receiver_observation.hpp"

#include "recovery_core_support.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

constexpr std::size_t FACTORY_SAMPLES = 3;    ///< Choose, explicit factory-entry permission and completion.
constexpr std::size_t TURN_FAMILY_VISITS = 9; ///< Three fixed family observations in each of three samples.
constexpr std::size_t SCHEMA_PEAK = 3;        ///< Immutable endpoint, composer and session coexist.

/** Construct a validated scheduler at its final stable address.
 * @param config Already validated preparation policy.
 * @param origin Final stable Core identity.
 * @return Nonallocating prepared state; failed validation would violate the private construction contract.
 */
RecoveryState scheduler(RecoveryConfig config, const void *origin) noexcept {
    auto result = RecoveryState::create(config, origin);

    if (!result) {
        std::terminate();
    }

    return std::move(*result);
}

/** Map the frozen private phases explicitly into the public inspection type.
 * @param phase Current actual state.
 * @return Corresponding detached public phase.
 */
org::ttldtor::qdcxx::transport::RecoveryPhase publicPhase(RecoveryPhase phase) noexcept {
    using Public = org::ttldtor::qdcxx::transport::RecoveryPhase;

    switch (phase) {
    case RecoveryPhase::SUSPENDED:
        return Public::SUSPENDED;
    case RecoveryPhase::CONFIGURED:
        return Public::CONFIGURED;
    case RecoveryPhase::PREPARING:
        return Public::PREPARING;
    case RecoveryPhase::STARTING:
        return Public::STARTING;
    case RecoveryPhase::NEGOTIATING:
        return Public::NEGOTIATING;
    case RecoveryPhase::REPLAYING:
        return Public::REPLAYING;
    case RecoveryPhase::ACTIVE:
        return Public::ACTIVE;
    case RecoveryPhase::RETIRING:
        return Public::RETIRING;
    case RecoveryPhase::RESETTING:
        return Public::RESETTING;
    case RecoveryPhase::BACKOFF:
        return Public::BACKOFF;
    case RecoveryPhase::STOPPING:
        return Public::STOPPING;
    case RecoveryPhase::CLOSED:
        return Public::CLOSED;
    case RecoveryPhase::EXHAUSTED:
        return Public::EXHAUSTED;
    case RecoveryPhase::FAILED:
        return Public::FAILED;
    }

    return Public::FAILED;
}

/** Run the actual pure state on caller-owned coordinates without retaining or refilling them.
 * @param state Stable actual production scheduler.
 * @param now Current selected monotonic sample.
 * @param observation Actual current action result.
 * @param work Mutable owner coordinates.
 * @return One actual scheduler result.
 */
RecoveryStep step(RecoveryState &state, RecoveryClock::time_point now, const RecoveryObservation &observation,
                  RecoveryWork &work) noexcept {
    RecoveryStateWork credits{work.operations, work.visits};
    const auto result = state.step(now, observation, credits);
    work.operations = credits.observations;
    work.visits = credits.transitions;

    return result;
}

/** Check and charge a fresh inner schema's complete preparation/rollback work before factory entry.
 * One optional postcheck pass is conservative for the opaque TLS-owned copy; no future turn is refilled.
 * @param capacity Admitted immutable three-schema capacity.
 * @param work Actual caller work; two scheduler samples/transitions must remain funded.
 * @return True on complete admission, false before any attempt is charged.
 */
bool schemaWork(RecoverySchemaCapacity capacity, RecoveryWork &work) {
    const RecoverySchemaCapacity one{capacity.records / SCHEMA_PEAK, capacity.fields / SCHEMA_PEAK,
                                     capacity.lookups / SCHEMA_PEAK, capacity.nameUnits / SCHEMA_PEAK};
    const auto nodes = recoverySum(one.records, recoverySum(one.fields, one.lookups));
    auto bytes = recoverySum(sizeof(core::LocalSchema), recoveryProduct(one.records, sizeof(core::LocalRecord)));
    bytes = recoverySum(bytes, recoveryProduct(one.fields, sizeof(core::LocalField)));
    bytes = recoverySum(bytes, recoveryProduct(one.lookups, sizeof(core::FieldLookup)));
    bytes = recoverySum(bytes, recoveryProduct(one.nameUnits, sizeof(char16_t)));
    const auto visits = recoveryProduct(5, nodes); // Validate, copy, measure, possible postcheck and rollback.
    const auto byteWork = recoveryProduct(2, bytes);

    if (work.operations < 3 || work.visits < recoverySum(visits, 2) || work.textUnits < one.nameUnits ||
        work.bytes < byteWork) {
        return false;
    }

    --work.operations;
    work.visits -= visits;
    work.textUnits -= one.nameUnits;
    work.bytes -= byteWork;

    return true;
}

/** Interpret only an engaged actual decoded event before any move/release.
 * @param kind Actual optional event's current kind.
 * @return Validated current-input marker.
 */
RecoveryInput inputKind(protocol::LiveEventKind kind) noexcept {
    switch (kind) {
    case protocol::LiveEventKind::EMPTY_HEARTBEAT:
        return RecoveryInput::EMPTY_HEARTBEAT;
    case protocol::LiveEventKind::HEARTBEAT:
        return RecoveryInput::HEARTBEAT;
    case protocol::LiveEventKind::PROTOCOL:
        return RecoveryInput::PROTOCOL;
    case protocol::LiveEventKind::DESCRIPTIONS:
        return RecoveryInput::DESCRIPTIONS;
    case protocol::LiveEventKind::DATA:
        return RecoveryInput::DATA;
    }

    return RecoveryInput::NONE;
}

/** Map one actual type10/15/20 data envelope to its sole destination family.
 * @param type Actual decoded data identity.
 * @return Fixed array index or no supported data family.
 */
std::optional<std::size_t> dataFamily(std::optional<std::int32_t> type) noexcept {
    if (type == 10)
        return 0;

    if (type == 15)
        return 1;

    if (type == 20)
        return 2;

    return std::nullopt;
}

/** Preserve actual queue evidence with a conservative recovery-level category.
 * @param failure Existing complete nested cause.
 * @return Detached static checkpoint.
 */
RecoveryDiagnostic queueFailure(mux::QueueFailure failure) noexcept {
    auto error = RecoveryError::INVALID_STATE;

    if (failure.error == mux::QueueError::WORK_LIMIT)
        error = RecoveryError::WORK_LIMIT;

    if (failure.error == mux::QueueError::RESOURCE_LIMIT)
        error = RecoveryError::RESOURCE_LIMIT;

    if (failure.error == mux::QueueError::ALLOCATION_FAILURE)
        error = RecoveryError::ALLOCATION_FAILURE;

    auto result = RecoveryDiagnostic{error, "recovery-queue"};
    result.queue = std::move(failure);

    return result;
}

} // namespace

RecoveryDiagnostic recoveryTransportDiagnostic(TcpDiagnostic diagnostic) noexcept {
    auto error = RecoveryError::TRANSPORT_FAILURE;

    switch (diagnostic.error) {
    case TcpError::NONE:
        error = RecoveryError::NONE;
        break;
    case TcpError::INVALID_STATE:
        error = RecoveryError::INVALID_STATE;
        break;
    case TcpError::WRONG_THREAD:
        error = RecoveryError::WRONG_THREAD;
        break;
    case TcpError::INVALID_CONFIGURATION:
    case TcpError::BOOTSTRAP_FAILURE:
        error = RecoveryError::INVALID_CONFIGURATION;
        break;
    case TcpError::RESOURCE_LIMIT:
        error = RecoveryError::RESOURCE_LIMIT;
        break;
    case TcpError::WORK_LIMIT:
        error = RecoveryError::WORK_LIMIT;
        break;
    case TcpError::ALLOCATION_FAILURE:
        error = RecoveryError::ALLOCATION_FAILURE;
        break;
    case TcpError::PROTOCOL_FAILURE:
        error = RecoveryError::PROTOCOL_FAILURE;
        break;
    case TcpError::UNSUPPORTED_DNS_NAME:
        error = RecoveryError::UNSUPPORTED_PROFILE;
        break;
    default:
        break;
    }

    return {error, diagnostic.stage, diagnostic};
}

RecoveryAttemptFailure recoveryTransportFailure(TcpDiagnostic diagnostic) noexcept {
    const bool retryable = diagnostic.error == TcpError::NONE || diagnostic.error == TcpError::DNS_FAILURE ||
                           diagnostic.error == TcpError::CONNECT_FAILURE ||
                           diagnostic.error == TcpError::READ_FAILURE || diagnostic.error == TcpError::WRITE_FAILURE ||
                           diagnostic.error == TcpError::DEADLINE;
    auto failure = recoveryTransportDiagnostic(diagnostic);

    if (diagnostic.error == TcpError::NONE) {
        failure = {RecoveryError::TRANSPORT_FAILURE, "recovery-peer-end"};
    }

    const auto cause =
        retryable                                          ? RecoveryCause::TRANSPORT
        : failure.error == RecoveryError::PROTOCOL_FAILURE ? RecoveryCause::PROTOCOL
        : failure.error == RecoveryError::RESOURCE_LIMIT || failure.error == RecoveryError::ALLOCATION_FAILURE
            ? RecoveryCause::RESOURCE
            : RecoveryCause::CONFIGURATION;

    return {failure, cause, retryable};
}

std::expected<RecoveryWork, RecoveryDiagnostic>
admitRecoveryOwner(const RecoveryConfiguration &configuration, std::size_t ownerBytes, RecoveryWork &work) noexcept {
    try {
        const auto protocolBytes = configuration.endpoint.settings().budgets.session.residentBytes;
        const RecoveryWork retirement{1, 2, 0, recoverySum(ownerBytes, protocolBytes)};
        const auto totalBytes = recoverySum(ownerBytes, retirement.bytes);
        recoveryDebit(2, work.operations); // Actual construction and prepaid final retirement.
        recoveryDebit(4, work.visits);     // Stable payload/control plus their final release.
        recoveryDebit(totalBytes, work.bytes);

        return retirement;
    } catch (const RecoveryActionRefusal &error) {
        return std::unexpected(error.diagnostic);
    }
}

RecoveryCore::RecoveryCore(RecoveryConfiguration &&configuration, RecoveryBackend &backend,
                           RecoveryWork retirement) noexcept
    : configuration_(std::move(configuration)), backend_(backend), state_(scheduler(configuration_.scheduler, this)),
      retirement_(retirement), thread_(std::this_thread::get_id()) {
}

RecoveryCore::~RecoveryCore() {
    cutoff();
    // Active public operations retain the enclosing Impl, so this fallback is outside its callbacks.
    (void)retireStatistics();

    if (event_) {
        const auto bytes = event_->releaseBytes();

        if (retirement_.visits == 0 || bytes > retirement_.bytes)
            std::terminate();

        --retirement_.visits;
        retirement_.bytes -= bytes;
        event_.reset();
    }
}

bool RecoveryCore::adopt(std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues) noexcept {
    return configuration_.adopt(std::move(blueprint), std::move(queues));
}

std::optional<RecoveryDiagnostic> RecoveryCore::rejection() const noexcept {
    if (thread_ != std::this_thread::get_id()) {
        return RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"};
    }

    if (executing_) {
        return RecoveryDiagnostic{RecoveryError::REENTRANT, "recovery-owner-busy"};
    }

    return std::nullopt;
}

std::expected<void, RecoveryDiagnostic> RecoveryCore::start() noexcept {
    if (const auto error = rejection())
        return std::unexpected(*error);

    if (cutoff_ || !configuration_.adopted || state_.status().terminal) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-start-state"});
    }

    state_.requestStart();

    return {};
}

void RecoveryCore::cutoff() noexcept {
    if (!cutoff_)
        nativeOperationsAtCutoff_ = backend_.facts().pendingOperations;

    cutoff_ = true;

    if (terminal_)
        return;

    drain_.abort();
    state_.requestStop();
    backend_.abort();
    abortIssued_ = true;
    stopQueues(mux::StopMode::ABORT);
}

void RecoveryCore::stopQueues(mux::StopMode mode) noexcept {
    for (std::size_t family = 0; family != 3; ++family) {
        if (auto &queue = configuration_.queue(family); queue && queue->state() != mux::QueueState::TERMINAL) {
            queue->requestStop(mode);
        }
    }
}

void RecoveryCore::sampleDrain(RecoveryClock::time_point now) noexcept {
    if (terminal_)
        return;

    const bool active = drain_.active();
    drain_.sample(now);

    if (active && !drain_.active())
        stopQueues(mux::StopMode::ABORT);
}

void RecoveryCore::observeShutdownCause() noexcept {
    switch (state_.status().currentCause) {
    case RecoveryCause::NONE:
        break;
    case RecoveryCause::TRANSPORT:
    case RecoveryCause::PROTOCOL_INACTIVITY:
        shutdownGap_ = shutdownGap_ || cutoff_ || finalRetirement_;
        break;
    default:
        shutdownFailed_ = true;

        if (drain_.active()) {
            drain_.abort();
            stopQueues(mux::StopMode::ABORT);
        }

        break;
    }
}

void RecoveryCore::observeRetiringBackend(bool afterFence) noexcept {
    const auto failure = backend_.failure();
    const auto observe = [&](RecoveryDiagnostic diagnostic) {
        const auto relevance = classifyRecoveryShutdown(diagnostic, afterFence);

        if (relevance == RecoveryShutdownFailure::NONE)
            return;

        record(diagnostic);
        shutdownGap_ = shutdownGap_ || relevance == RecoveryShutdownFailure::TRANSPORT_GAP;
        shutdownFailed_ = shutdownFailed_ || relevance == RecoveryShutdownFailure::FATAL;
    };

    observe(failure.diagnostic);

    if (failure.secondary)
        observe(*failure.secondary);

    if (shutdownFailed_) {
        drain_.abort();
        stopQueues(mux::StopMode::ABORT);
    }
}

void RecoveryCore::captureWire() noexcept {
    if (const auto actual = backend_.wireTerminal())
        wire_ = actual;

    if (backend_.facts().present) {
        retiredStatistics_.merge(backend_.statistics());
    }
}

ConnectionStatistics RecoveryCore::statistics() const noexcept {
    auto result = retiredStatistics_;
    const auto current = state_.status();
    result.connectionAttempts.value = current.attempts;

    if (backend_.facts().present) {
        const auto active = backend_.statistics();
        result.merge(active);
        result.activeConnections = active.activeConnections;
        result.attemptIdentity = current.attempts;

        if (active.sourceEpoch) {
            result.sourceEpoch = result.establishedConnections.value;
        }
    }

    return result;
}

std::expected<void, RecoveryDiagnostic>
RecoveryCore::requestAggregationPeriod(std::optional<std::chrono::milliseconds> period) noexcept {
    if (const auto refused = rejection())
        return std::unexpected(*refused);

    if (period && (period->count() < 0 || *period > MAXIMUM_REQUESTED_AGGREGATION_PERIOD))
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_CONFIGURATION, "recovery-aggregation-period"});

    if (!aggregationEnabled_ || requestedAggregation_ != period) {
        requestedAggregation_ = period;
        aggregationEnabled_ = true;
        aggregationPending_ = true;
    }

    return {};
}

std::optional<AggregationPeriodInfo> RecoveryCore::aggregationPeriodInfo() const noexcept {
    const auto actual = backend_.facts();

    if (!aggregationEnabled_ || cutoff_ || connectionFenced_ || !actual.present || !actual.connected ||
        !actual.authenticated || actual.terminal)
        return {};

    return aggregationInfo_;
}

ConnectionStatistics RecoveryCore::retireStatistics() noexcept {
    cutoff();

    if (backend_.facts().present) {
        retiredStatistics_.merge(backend_.retireStatistics());
    }

    return statistics();
}

void RecoveryCore::publishTerminal() noexcept {
    if (terminal_ || !state_.status().terminal || backend_.facts().present || event_)
        return;

    sampleDrain(RecoveryClock::now());
    observeShutdownCause();
    RecoveryTerminal result;
    result.outcome = shutdownFailed_                                           ? mux::TerminalOutcome::FAILED
                     : drain_.active() && !shutdownGap_ && !heldDataDiscarded_ ? mux::TerminalOutcome::DRAINED
                                                                               : mux::TerminalOutcome::ABORTED;
    result.deadlineExpired = drain_.expired();
    result.heldDataDiscarded = heldDataDiscarded_;
    result.wire = wire_;
    result.nativeOperationsAtCutoff = nativeOperationsAtCutoff_;

    for (std::size_t family = 0; family != result.queues.size(); ++family) {
        if (const auto &queue = configuration_.queue(family)) {
            result.queues[family] = queue->terminal();

            if (!result.queues[family])
                return;

            const auto &actual = *result.queues[family];

            if (actual.outcome == mux::TerminalOutcome::FAILED)
                result.outcome = mux::TerminalOutcome::FAILED;
            else if (result.outcome != mux::TerminalOutcome::FAILED &&
                     (actual.outcome == mux::TerminalOutcome::ABORTED || actual.hadGap))
                result.outcome = mux::TerminalOutcome::ABORTED;
        }
    }

    terminal_ = std::move(result);
}

std::expected<void, RecoveryDiagnostic> RecoveryCore::requestDisconnect(bool clear) noexcept {
    if (thread_ != std::this_thread::get_id()) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"});
    }

    if (!configuration_.adopted || cutoff_ || terminal_ || state_.status().terminal) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-disconnect-state"});
    }

    if (clear && !state_.sourceResetPending()) {
        // A completed cycle cannot lend its old global ACK to a later independent clear.
        resetRequested_.fill(false);
    }

    connectionFenced_ = true;

    for (std::size_t family = 0; family != 3; ++family) {
        if (auto &queue = configuration_.queue(family)) {
            queue->requestDiscardPendingSource();
        }
    }

    const bool resetAccepted = state_.sourceResetPending() && std::ranges::any_of(resetRequested_, [](bool accepted) {
                                   return accepted;
                               });
    state_.requestDisconnect(clear || resetAccepted);
    backend_.suspend();
    abortIssued_ = true;

    return {};
}

std::expected<void, RecoveryDiagnostic> RecoveryCore::requestReconnect() noexcept {
    if (thread_ != std::this_thread::get_id()) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"});
    }

    const auto current = state_.status();

    if (!configuration_.adopted || cutoff_ || terminal_ || current.terminal) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-reconnect-state"});
    }

    if (current.attempts >= configuration_.scheduler.maximumAttempts) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::RESOURCE_LIMIT, "recovery-attempt-limit"});
    }

    connectionFenced_ = true;

    for (std::size_t family = 0; family != 3; ++family) {
        if (auto &queue = configuration_.queue(family)) {
            queue->requestDiscardPendingSource();
        }
    }

    state_.requestReconnect();
    backend_.suspend();
    abortIssued_ = true;

    return {};
}

std::expected<void, RecoveryDiagnostic> RecoveryCore::requestStop() noexcept {
    if (thread_ != std::this_thread::get_id()) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"});
    }

    cutoff();

    return {};
}

std::expected<void, RecoveryDiagnostic> RecoveryCore::requestDrain(RecoveryClock::time_point deadline) noexcept {
    if (thread_ != std::this_thread::get_id()) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"});
    }

    const bool firstFence = !cutoff_;

    if (firstFence)
        nativeOperationsAtCutoff_ = backend_.facts().pendingOperations;

    cutoff_ = true;

    if (terminal_)
        return {};

    observeShutdownCause();

    if (firstFence && backend_.facts().present)
        observeRetiringBackend(false);

    drain_.request(deadline, RecoveryClock::now());

    if (shutdownFailed_)
        drain_.abort();

    state_.requestStop();
    backend_.abort();
    abortIssued_ = true;
    stopQueues(drain_.active() ? mux::StopMode::GRACEFUL : mux::StopMode::ABORT);

    return {};
}

void RecoveryCore::record(RecoveryDiagnostic diagnostic) noexcept {
    if (diagnostic.error == RecoveryError::NONE)
        return;

    if (first_.error == RecoveryError::NONE) {
        first_ = std::move(diagnostic);
    } else if (secondary_.error == RecoveryError::NONE &&
               (first_.error != diagnostic.error || first_.stage != diagnostic.stage ||
                first_.transport.error != diagnostic.transport.error ||
                first_.transport.nativeCode != diagnostic.transport.nativeCode ||
                first_.transport.protocolError != diagnostic.transport.protocolError)) {
        secondary_ = std::move(diagnostic);
    }
}

void RecoveryCore::recordModelCause() noexcept {
    observeShutdownCause();

    switch (state_.status().currentCause) {
    case RecoveryCause::PROTOCOL_INACTIVITY:
        record({RecoveryError::TRANSPORT_FAILURE, "recovery-protocol-inactivity"});
        break;
    case RecoveryCause::CLOCK_BACKWARDS:
        record({RecoveryError::IDENTITY_EXHAUSTED, "recovery-clock-backwards"});
        break;
    case RecoveryCause::CLOCK_RANGE:
        record({RecoveryError::IDENTITY_EXHAUSTED, "recovery-clock-range"});
        break;
    case RecoveryCause::IDENTITY_EXHAUSTED:
        record({RecoveryError::IDENTITY_EXHAUSTED, "recovery-action-identity"});
        break;
    default:
        break;
    }
}

RecoveryFacts RecoveryCore::facts() noexcept {
    const auto actual = backend_.facts();
    bool changed = actual.generation != peerGeneration_ || actual.authentication != authentication_;
    peerGeneration_ = actual.generation;
    authentication_ = actual.authentication;
    RecoveryFacts result;
    result.transportEligible = actual.connected && !cutoff_ && !connectionFenced_;
    result.authenticationEligible = actual.authenticated && !cutoff_ && !connectionFenced_;
    result.terminal = actual.terminal;
    result.negotiationPending = !descriptor_ || description_ < configuration_.blueprint.size();

    for (std::size_t family = 0; family != 3; ++family) {
        if (auto &queue = configuration_.queue(family)) {
            const auto revision = queue->revision();
            changed = changed || revision != desiredRevisions_[family];
            desiredRevisions_[family] = revision;

            if (revision != configuration_.replay[family].banks[0].revision)
                converged_[family] = false;

            const bool demanded = !queue->aggregateEntries().empty();

            if (demanded)
                result.demandedFamilies |= static_cast<std::uint8_t>(1U << family);

            if (demanded && converged_[family])
                result.convergedFamilies |= static_cast<std::uint8_t>(1U << family);

            result.replayPending = result.replayPending || !converged_[family];
            result.deliveryPending = true;
        }
    }

    if (changed && !readinessExhausted_) {
        if (readiness_ == std::numeric_limits<std::uint64_t>::max()) {
            readinessExhausted_ = true;
            record({RecoveryError::IDENTITY_EXHAUSTED, "recovery-readiness-identity"});
        } else {
            ++readiness_;
        }
    }

    result.readinessVersion = readiness_;

    return result;
}

bool RecoveryCore::releaseEvent(protocol::LiveWork &work) noexcept {
    if (!event_)
        return true;

    const auto release = event_->releaseBytes();

    if (cutoff_ || finalRetirement_) {
        if (retirement_.visits == 0 || retirement_.bytes < release)
            std::terminate();

        --retirement_.visits;
        retirement_.bytes -= release;
        heldDataDiscarded_ = heldDataDiscarded_ || event_->kind() == protocol::LiveEventKind::DATA;
        event_.reset();

        return true;
    }

    if (work.visits == 0 || work.bytes < release)
        return false;

    --work.visits;
    work.bytes -= release;
    heldDataDiscarded_ = heldDataDiscarded_ || (connectionFenced_ && event_->kind() == protocol::LiveEventKind::DATA);
    event_.reset();

    return true;
}

std::optional<RecoveryDiagnostic> RecoveryCore::controls(RecoveryTurnWork &work) {
    auto &queue = configuration_.queue(controlFamily_);
    auto &credits = work.queues[controlFamily_];
    controlFamily_ = (controlFamily_ + 1) % 3;

    if (!queue)
        return std::nullopt;

    // Offline desired commands and reset/control publication must not depend on auth/data eligibility.
    const auto callbacks = credits.callbacks;
    credits.callbacks = 0;
    const auto result = connectionFenced_ ? queue->progressRetainedCache(credits) : queue->progress(credits);
    credits.callbacks = callbacks; // No callback could consume a credit while this explicit gate was zero.

    if (!result && result.error().error != mux::QueueError::WORK_LIMIT &&
        result.error().error != mux::QueueError::BUSY) {
        return queueFailure(result.error());
    }

    return std::nullopt;
}

RecoveryObservation RecoveryCore::reset(RecoveryTicket ticket, RecoveryTurnWork &work) {
    auto result = recoveryResult(ticket, RecoveryCompletion::DEFERRED);

    if (callbackFence_ && callbackFence_->outstanding.load(std::memory_order_acquire) != 0) {
        callbackFence_->cancelQueuedRequested.store(true, std::memory_order_release);

        return result;
    }

    if (!releaseEvent(work.session))
        return result;

    for (std::size_t family = 0; family != 3; ++family) {
        auto &queue = configuration_.queue(family);

        if (!queue)
            continue;

        if (!resetRequested_[family]) {
            const auto requested = queue->resetSource(work.queues[family]);

            if (!requested) {
                if (requested.error().error == mux::QueueError::WORK_LIMIT)
                    return result;

                auto failure = queueFailure(requested.error());
                record(failure);
                result.completion = RecoveryCompletion::TERMINAL_FAILURE;
                result.cause = RecoveryCause::RESET_FAILURE;

                return result;
            }

            resetRequested_[family] = true;
        }

        const auto reset = queue->resetStatus();

        if (reset.failure) {
            record(queueFailure(*reset.failure));
            result.completion = RecoveryCompletion::TERMINAL_FAILURE;
            result.cause = RecoveryCause::RESET_FAILURE;

            return result;
        }

        if (reset.phase != mux::ResetPhase::NONE || !reset.globalAcknowledged) {
            const auto progressed = queue->progress(work.queues[family]);

            if (!progressed && progressed.error().error != mux::QueueError::WORK_LIMIT) {
                record(queueFailure(progressed.error()));
                result.completion = RecoveryCompletion::TERMINAL_FAILURE;
                result.cause = RecoveryCause::RESET_FAILURE;
            }

            return result;
        }
    }

    for (std::size_t family = 0; family != 3; ++family) {
        for (auto &bank : configuration_.replay[family].banks) {
            recoveryDebit(bank.size, work.recovery.visits);

            for (std::size_t index = 0; index != bank.size; ++index)
                bank.entries[index] = {};

            bank.size = 0;
            bank.units = 0;
            bank.revision = 0;
        }

        converged_[family] = false;
    }

    result.completion = RecoveryCompletion::COMPLETED;
    result.sourceResetComplete = true;

    return result;
}

RecoveryObservation RecoveryCore::execute(RecoveryTicket ticket, RecoveryTurnWork &work) {
    auto result = recoveryResult(ticket);
    const auto model = state_.status();
    const bool ordinary = ticket.action == RecoveryAction::PREPARE_ATTEMPT ||
                          ticket.action == RecoveryAction::START_ATTEMPT ||
                          ticket.action == RecoveryAction::NEGOTIATE || ticket.action == RecoveryAction::REPLAY ||
                          ticket.action == RecoveryAction::HEARTBEAT || ticket.action == RecoveryAction::DELIVER;

    if (cutoff_ && ticket.action == RecoveryAction::RESET_SOURCES)
        return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

    // Local retained projection after actual pause is independent of the retired native attempt's eligibility.
    const bool retainedCacheDelivery = ticket.action == RecoveryAction::DELIVER &&
                                       model.phase == RecoveryPhase::SUSPENDED && !backend_.facts().present && !event_;

    // A new PREPARE retains the previous cause until its actual entry commit; do not strand backoff here.
    if (ordinary &&
        (cutoff_ || (connectionFenced_ && ticket.action != RecoveryAction::PREPARE_ATTEMPT && !retainedCacheDelivery) ||
         (model.currentCause != RecoveryCause::NONE && ticket.action != RecoveryAction::PREPARE_ATTEMPT &&
          !retainedCacheDelivery))) {
        return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
    }

    if (ordinary && readinessExhausted_) {
        result.completion = RecoveryCompletion::TERMINAL_FAILURE;
        result.cause = RecoveryCause::IDENTITY_EXHAUSTED;

        return result;
    }

    const auto failure = [&](RecoveryDiagnostic diagnostic, RecoveryCause cause, bool retryable = false) {
        record(diagnostic);
        auto rejected = recoveryResult(ticket, retryable ? RecoveryCompletion::RETRYABLE_FAILURE
                                                         : RecoveryCompletion::TERMINAL_FAILURE);
        rejected.cause = cause;

        return rejected;
    };
    const auto output = [&](const std::expected<void, RecoveryDiagnostic> &admitted) {
        if (admitted) {
            auto accepted = recoveryResult(ticket);
            accepted.outputAdmitted = true;

            return accepted;
        }

        if (admitted.error().error == RecoveryError::WORK_LIMIT) {
            return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
        }

        if (recoveryOutputBlocked(admitted.error())) {
            return recoveryResult(ticket, RecoveryCompletion::OUTPUT_BLOCKED);
        }

        return failure(admitted.error(), RecoveryCause::RESOURCE);
    };
    try {
        switch (ticket.action) {
        case RecoveryAction::PREPARE_ATTEMPT: {
            if (backend_.facts().present || event_) {
                return failure({RecoveryError::INVALID_STATE, "recovery-attempt-overlap"},
                               RecoveryCause::CONFIGURATION);
            }

            if (!schemaWork(configuration_.capacity.schemaCopies, work.recovery)) {
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
            }

            auto entered = recoveryResult(ticket, RecoveryCompletion::FACTORY_ENTERED);

            if (!step(state_, RecoveryClock::now(), entered, work.recovery).factoryEntryAccepted) {
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
            }

            wire_.reset();
            std::optional<protocol::SubscriptionWriter> fresh;
            auto *writer = configuration_.firstWriter ? &*configuration_.firstWriter : nullptr;

            if (!writer) {
                auto composed = protocol::SubscriptionWriter::create(
                    configuration_.endpoint.schema(), configuration_.blueprint,
                    configuration_.endpoint.settings().budgets.layout,
                    configuration_.endpoint.settings().budgets.writer, work.writer, configuration_.options.selectors);

                if (!composed) {
                    auto diagnostic = RecoveryDiagnostic{
                        composed.error() == protocol::SubscriptionWriterError::WORK_LIMIT ? RecoveryError::WORK_LIMIT
                        : composed.error() == protocol::SubscriptionWriterError::ALLOCATION_FAILURE
                            ? RecoveryError::ALLOCATION_FAILURE
                            : RecoveryError::RESOURCE_LIMIT,
                        "recovery-attempt-writer"};
                    diagnostic.writer = composed.error();

                    return failure(diagnostic, diagnostic.error == RecoveryError::WORK_LIMIT
                                                   ? RecoveryCause::FACTORY_WORK_LIMIT
                                                   : RecoveryCause::FACTORY_FAILURE);
                }

                if (composed->storageBytes() > configuration_.options.limits.writerConfigurationBytes) {
                    return failure({RecoveryError::RESOURCE_LIMIT, "recovery-attempt-writer-adoption"},
                                   RecoveryCause::FACTORY_FAILURE);
                }

                fresh.emplace(std::move(*composed));
                writer = &*fresh;
            }

            const auto prepared = backend_.prepare(configuration_, std::move(*writer), work);

            if (!prepared) {
                return failure(prepared.error(), prepared.error().error == RecoveryError::WORK_LIMIT
                                                     ? RecoveryCause::FACTORY_WORK_LIMIT
                                                     : RecoveryCause::FACTORY_FAILURE);
            }

            connectionFenced_ = false;
            configuration_.firstWriter.reset();
            descriptor_ = false;
            aggregationInfo_.reset();
            aggregationPending_ = aggregationEnabled_;
            description_ = 0;
            resetRequested_ = {};
            converged_ = {};
            abortIssued_ = false;

            return result;
        }
        case RecoveryAction::START_ATTEMPT: {
            const auto started = backend_.start();

            if (started)
                return result;

            const auto classified = backend_.failure();

            return failure(started.error(), classified.cause, classified.retryable);
        }
        case RecoveryAction::POLL_ATTEMPT: {
            const auto observeAggregation = [&] {
                if (!aggregationEnabled_ || !event_ || event_->kind() != protocol::LiveEventKind::PROTOCOL)
                    return true;

                const auto snapshot = event_->snapshot();
                bool complete = false;
                const auto observed =
                    peerAggregationPeriodInfo(snapshot.protocol(),
                                              {configuration_.queue(0).has_value(), configuration_.queue(1).has_value(),
                                               configuration_.queue(2).has_value()},
                                              work.session, &complete);

                if (complete)
                    aggregationInfo_ = observed;

                return complete;
            };

            if (model.currentCause != RecoveryCause::NONE && !abortIssued_) {
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
            }

            if (event_ && event_->kind() != protocol::LiveEventKind::DATA) {
                if (!observeAggregation())
                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

                (void)releaseEvent(work.session);
            }

            if (event_ && event_->kind() == protocol::LiveEventKind::DATA && !cutoff_ && !connectionFenced_ &&
                model.currentCause == RecoveryCause::NONE) {
                const auto family = dataFamily(event_->messageType());

                if (!family || !configuration_.queue(*family)) {
                    return failure({RecoveryError::PROTOCOL_FAILURE, "recovery-unconfigured-data-family"},
                                   RecoveryCause::PROTOCOL);
                }

                const auto admitted = configuration_.queue(*family)->admit(std::move(*event_), work.queues[*family]);

                if (admitted) {
                    event_.reset(); // Only a moved-from handle remains; its epoch owner belongs to the queue.
                } else if (admitted.error().error != mux::QueueError::BUSY &&
                           admitted.error().error != mux::QueueError::WORK_LIMIT) {
                    return failure(queueFailure(admitted.error()), RecoveryCause::RESOURCE);
                }
            }

            if (aggregationEnabled_ && aggregationPending_ && descriptor_ && !cutoff_ && !connectionFenced_) {
                const auto actual = backend_.facts();

                if (actual.connected && actual.authenticated && !actual.terminal) {
                    const auto text = aggregationRequestText(requestedAggregation_);
                    const std::array<protocol::LivePropertyView, 1> values{
                        {{u"requestedAggregationPeriod", text.view()}}};
                    std::array<protocol::LiveMessageView, 3> receive{};
                    constexpr std::array<std::int32_t, 3> ids{10, 15, 20};
                    constexpr std::array<std::u16string_view, 3> names{u"TICKER_DATA", u"STREAM_DATA", u"HISTORY_DATA"};
                    std::size_t count = 0;

                    for (std::size_t family = 0; family != 3; ++family)
                        if (configuration_.queue(family))
                            receive[count++] = {ids[family], names[family], values};

                    const auto admitted = backend_.protocol({{}, {}, {receive.data(), count}}, work.session);

                    if (admitted)
                        aggregationPending_ = false;
                    else if (admitted.error().error != RecoveryError::WORK_LIMIT &&
                             !recoveryOutputBlocked(admitted.error()))
                        return failure(admitted.error(), RecoveryCause::RESOURCE);
                }
            }

            auto polled = backend_.poll(work, !event_ && !cutoff_ && !connectionFenced_);

            if (!polled) {
                if (polled.error().error == RecoveryError::WORK_LIMIT) {
                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
                }

                const auto classified = backend_.failure();

                if ((cutoff_ || connectionFenced_) && classifyRecoveryShutdown(classified.diagnostic, true) ==
                                                          RecoveryShutdownFailure::POST_FENCE_TRANSPORT) {
                    observeRetiringBackend(true);

                    return result;
                }

                return failure(polled.error(), classified.cause, classified.retryable);
            }

            if (polled->event) {
                // The zero-operation decode gate is source-bound: an engaged old event forbids another decode.
                if (event_)
                    std::terminate();

                if (polled->event->residentBytes() == 0) {
                    return failure({RecoveryError::INVALID_STATE, "recovery-empty-event-owner"},
                                   RecoveryCause::PROTOCOL);
                }

                result.input = inputKind(polled->event->kind());
                event_.emplace(std::move(*polled->event));
            }

            if (event_ && (event_->kind() != protocol::LiveEventKind::DATA || cutoff_ || connectionFenced_)) {
                if (!cutoff_ && !connectionFenced_ && !observeAggregation())
                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

                (void)releaseEvent(work.session);
            }

            const auto classified = backend_.failure();

            if (classified.diagnostic.error != RecoveryError::NONE &&
                (backend_.facts().terminal || !classified.retryable)) {
                if (!cutoff_ &&
                    ((!connectionFenced_ && model.currentCause == RecoveryCause::NONE) || !classified.retryable)) {
                    auto failed = failure(classified.diagnostic, classified.cause, classified.retryable);

                    if (classified.secondary)
                        record(*classified.secondary);

                    return failed;
                }

                if (cutoff_ || finalRetirement_)
                    observeRetiringBackend(true);
                else {
                    // A clean runtime is represented by synthetic peer-end; an explicit pause caused that end.
                    // Preserve genuine native failures and distinct secondary evidence instead of inventing failure.
                    const bool requestedCleanEnd = connectionFenced_ &&
                                                   classified.diagnostic.error == RecoveryError::TRANSPORT_FAILURE &&
                                                   classified.diagnostic.stage == "recovery-peer-end" &&
                                                   classified.diagnostic.transport.error == TcpError::NONE;

                    if (!requestedCleanEnd) {
                        record(classified.diagnostic);
                    }

                    if (classified.secondary)
                        record(*classified.secondary);
                }
            }

            return result;
        }
        case RecoveryAction::REQUEST_ABORT:
            backend_.abort();
            abortIssued_ = true;

            return result;
        case RecoveryAction::DISPOSE_ATTEMPT:

            if (!backend_.facts().terminal)
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

            if (cutoff_ || finalRetirement_)
                observeRetiringBackend(cutoff_);

            // A plain pause retains projected queue cache, never the old undecided protocol offer.
            if (connectionFenced_ && !releaseEvent(work.session)) {
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
            }

            captureWire();
            backend_.dispose();
            result.privateDisposed = !backend_.facts().present;

            return result;
        case RecoveryAction::RESET_SOURCES:
            return reset(ticket, work);
        case RecoveryAction::NEGOTIATE:

            if (!descriptor_) {
                std::array<protocol::LiveMessageView, 9> send{};
                std::array<protocol::LiveMessageView, 6> receive{};
                constexpr std::array<std::u16string_view, 3> commonNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL",
                                                                         u"DESCRIBE_RECORDS"};

                for (std::size_t index = 0; index != commonNames.size(); ++index) {
                    send[index] = {static_cast<std::int32_t>(index), commonNames[index], {}};
                    receive[index] = send[index];
                }

                constexpr std::array<std::int32_t, 3> adds{11, 16, 21};
                constexpr std::array<std::int32_t, 3> data{10, 15, 20};
                constexpr std::array<std::u16string_view, 3> addNames{
                    u"TICKER_ADD_SUBSCRIPTION", u"STREAM_ADD_SUBSCRIPTION", u"HISTORY_ADD_SUBSCRIPTION"};
                constexpr std::array<std::u16string_view, 3> removeNames{
                    u"TICKER_REMOVE_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION", u"HISTORY_REMOVE_SUBSCRIPTION"};
                constexpr std::array<std::u16string_view, 3> dataNames{u"TICKER_DATA", u"STREAM_DATA", u"HISTORY_DATA"};
                std::size_t sent = commonNames.size(), received = commonNames.size();
                const auto aggregation = aggregationRequestText(requestedAggregation_);
                const std::array<protocol::LivePropertyView, 1> aggregationProperties{
                    {{u"requestedAggregationPeriod", aggregation.view()}}};

                for (std::size_t family = 0; family != 3; ++family) {
                    if (!configuration_.queue(family))
                        continue;

                    send[sent++] = {adds[family], addNames[family], {}};
                    send[sent++] = {adds[family] + 1, removeNames[family], {}};
                    receive[received++] = {data[family], dataNames[family],
                                           aggregationEnabled_
                                               ? std::span<const protocol::LivePropertyView>{aggregationProperties}
                                               : std::span<const protocol::LivePropertyView>{}};
                }

                const std::array<protocol::LivePropertyView, 1> history{{{u"opt", u"hs"}}};
                const auto properties = configuration_.endpoint.settings().budgets.session.historySnapshot
                                            ? std::span<const protocol::LivePropertyView>{history}
                                            : std::span<const protocol::LivePropertyView>{};
                auto admitted = output(
                    backend_.protocol({properties, {send.data(), sent}, {receive.data(), received}}, work.session));

                if (admitted.outputAdmitted) {
                    descriptor_ = true;
                    aggregationPending_ = false;
                }

                return admitted;
            } else if (description_ < configuration_.blueprint.size()) {
                const auto id = configuration_.blueprint[description_].senderId;
                auto admitted = output(backend_.describe({&id, 1}, work.session));

                if (admitted.outputAdmitted)
                    ++description_;

                return admitted;
            }

            return result;
        case RecoveryAction::REPLAY:
            return replay(ticket, work);
        case RecoveryAction::HEARTBEAT:
            return output(backend_.heartbeat(work.session));
        case RecoveryAction::DELIVER: {
            for (std::size_t visited = 0; visited != 3; ++visited) {
                const auto family = family_;
                family_ = (family_ + 1) % 3;

                if (!configuration_.queue(family))
                    continue;

                const auto progressed = connectionFenced_
                                            ? configuration_.queue(family)->progressRetainedCache(work.queues[family])
                                            : configuration_.queue(family)->progress(work.queues[family]);

                if (cutoff_)
                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

                if (!progressed && progressed.error().error == mux::QueueError::WORK_LIMIT) {
                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
                }

                if (!progressed)
                    return failure(queueFailure(progressed.error()), RecoveryCause::RESOURCE);

                return result;
            }

            return result;
        }
        case RecoveryAction::STOP_DELIVERY:
        case RecoveryAction::RETIRE_ALL: {
            finalRetirement_ = true;

            if (ticket.action == RecoveryAction::RETIRE_ALL) {
                shutdownFailed_ = true;
                drain_.abort();
            }

            observeShutdownCause();
            backend_.abort();
            abortIssued_ = true;

            if (backend_.facts().present && !backend_.facts().terminal) {
                const auto progressed = backend_.poll(work, false);

                if (!progressed && progressed.error().error != RecoveryError::WORK_LIMIT) {
                    record(progressed.error());
                    const auto relevance = classifyRecoveryShutdown(progressed.error(), cutoff_);
                    shutdownGap_ = shutdownGap_ || relevance == RecoveryShutdownFailure::TRANSPORT_GAP;
                    shutdownFailed_ = shutdownFailed_ || relevance == RecoveryShutdownFailure::FATAL;
                    observeRetiringBackend(cutoff_);
                }

                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
            }

            if (backend_.facts().present)
                observeRetiringBackend(cutoff_);

            captureWire();
            backend_.dispose();
            (void)releaseEvent(work.session);

            if (backend_.facts().present || event_)
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

            for (std::size_t family = 0; family != 3; ++family) {
                auto &queue = configuration_.queue(family);

                if (!queue || queue->state() == mux::QueueState::TERMINAL) {
                    continue;
                }

                // Native disposal can consume the deadline; sample again before entering any delivery action.
                sampleDrain(RecoveryClock::now());
                queue->requestStop(drain_.active() ? mux::StopMode::GRACEFUL : mux::StopMode::ABORT);

                if (queue->state() != mux::QueueState::TERMINAL) {
                    const auto progressed = connectionFenced_ ? queue->progressRetainedCache(work.queues[family])
                                                              : queue->progress(work.queues[family]);

                    if (!progressed && progressed.error().error != mux::QueueError::WORK_LIMIT) {
                        record(queueFailure(progressed.error()));
                        shutdownFailed_ = true;
                        drain_.abort();
                    }

                    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
                }
            }

            result.privateDisposed = true;
            result.deliveryRetired = true;

            return result;
        }
        case RecoveryAction::NONE:
            return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
        }
    } catch (const RecoveryActionRefusal &error) {
        if (error.diagnostic.error == RecoveryError::WORK_LIMIT &&
            !(ticket.action == RecoveryAction::PREPARE_ATTEMPT && state_.status().factoryEntered)) {
            return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
        }

        return failure(error.diagnostic, ticket.action == RecoveryAction::PREPARE_ATTEMPT
                                             ? RecoveryCause::FACTORY_FAILURE
                                             : RecoveryCause::RESOURCE);
    } catch (const std::bad_alloc &) {
        return failure({RecoveryError::ALLOCATION_FAILURE, "recovery-action-allocation"}, RecoveryCause::RESOURCE);
    }

    return recoveryResult(ticket, RecoveryCompletion::DEFERRED);
}

std::expected<RecoveryStatus, RecoveryDiagnostic> RecoveryCore::poll(RecoveryTurnWork &work) {
    if (const auto error = rejection())
        return std::unexpected(*error);

    if (!configuration_.adopted) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-uncommitted"});
    }

    if (terminal_)
        return status();

    if (work.recovery.operations < FACTORY_SAMPLES + 1 || work.recovery.visits < TURN_FAMILY_VISITS + FACTORY_SAMPLES) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WORK_LIMIT, "recovery-state-work"});
    }

    struct Turn {
        bool &active;

        explicit Turn(bool &value) : active(value) {
            active = true;
        }

        ~Turn() {
            active = false;
        }
    } turn(executing_);

    sampleDrain(RecoveryClock::now());
    work.recovery.visits -= TURN_FAMILY_VISITS;
    auto action = step(state_, RecoveryClock::now(), {}, work.recovery);
    recordModelCause();

    if (action.status == RecoveryStepStatus::TERMINAL) {
        publishTerminal();

        return status();
    }

    sampleDrain(RecoveryClock::now());
    const auto controlError = controls(work);

    if (controlError)
        record(*controlError);

    if (action.status == RecoveryStepStatus::WAITING) {
        // A completion may have issued the next bounded ticket on the preceding public turn.
        action.action = state_.status().pending;
        action.status = RecoveryStepStatus::ACTION;
    }

    if (action.status != RecoveryStepStatus::ACTION && action.status != RecoveryStepStatus::CONTINUE)
        return status();

    auto observation = controlError ? recoveryResult(action.action, RecoveryCompletion::TERMINAL_FAILURE)
                                    : execute(action.action, work);

    if (controlError)
        observation.cause = RecoveryCause::RESET_FAILURE;

    observation.facts = facts();
    const auto completed = step(state_, RecoveryClock::now(), observation, work.recovery);
    recordModelCause();

    if (completed.status == RecoveryStepStatus::INVALID_OBSERVATION) {
        record({RecoveryError::INVALID_STATE, "recovery-action-proof"});
        shutdownFailed_ = true;
        cutoff();
    }

    sampleDrain(RecoveryClock::now());
    publishTerminal();

    return status();
}

RecoveryStatus RecoveryCore::status() const noexcept {
    const auto current = state_.status();
    RecoveryStatus result{publicPhase(current.phase), current.attempts, first_, secondary_};
    result.stopped = cutoff_;
    result.sourceResetPending = state_.sourceResetPending();
    result.terminal = terminal_;
    const std::array slots{&configuration_.queues.ticker, &configuration_.queues.stream,
                           &configuration_.queues.history};

    for (std::size_t family = 0; family != slots.size(); ++family) {
        if (const auto &queue = *slots[family]) {
            const bool demanded = !queue->aggregateEntries().empty();

            if (demanded)
                result.demandedFamilies |= static_cast<std::uint8_t>(1U << family);

            if (demanded && converged_[family] &&
                queue->revision() == configuration_.replay[family].banks[0].revision) {
                result.convergedFamilies |= static_cast<std::uint8_t>(1U << family);
            }
        }
    }

    return result;
}

RecoveryCapacity RecoveryCore::capacity() const noexcept {
    return configuration_.capacity;
}

std::expected<mux::MuxHandle, RecoveryReceiverFailure> RecoveryCore::openSubscriptionReceiver(
    core::SubscriptionFamily family, std::span<const client::PreparedSubscriptionBinding> bindings,
    const std::shared_ptr<client::SubscriptionConsumer> &consumer, client::ReceiverLimits receiverLimits,
    mux::ClientLimits clientLimits, client::ReceiverWork &receiverWork, mux::QueueWork &queueWork,
    const client::detail::ReceiverPlan **observationPlan) {
    if (const auto error = rejection()) {
        return std::unexpected(RecoveryReceiverFailure{*error});
    }

    if (!configuration_.adopted || cutoff_ || finalRetirement_ || terminal_ || state_.status().terminal) {
        return std::unexpected(
            RecoveryReceiverFailure{RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-receiver-state"}});
    }

    std::size_t slot;

    switch (family) {
    case core::SubscriptionFamily::TICKER:
        slot = 0;
        break;
    case core::SubscriptionFamily::STREAM:
        slot = 1;
        break;
    case core::SubscriptionFamily::HISTORY:
        slot = 2;
        break;
    default:
        return std::unexpected(RecoveryReceiverFailure{
            RecoveryDiagnostic{RecoveryError::INVALID_CONFIGURATION, "recovery-receiver-family"}});
    }

    auto &queue = configuration_.queue(slot);

    if (!queue) {
        return std::unexpected(
            RecoveryReceiverFailure{RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-receiver-queue"}});
    }

    struct Registration {
        bool &active;

        explicit Registration(bool &value) : active(value) {
            active = true;
        }

        ~Registration() {
            active = false;
        }
    } registration(executing_);

    auto opened = client::detail::openObservedSubscriptionReceiver(
        *queue, configuration_.endpoint.schema(), family, configuration_.options.selectors, bindings, consumer,
        receiverLimits, clientLimits, receiverWork, queueWork, observationPlan);

    if (!opened) {
        return std::unexpected(RecoveryReceiverFailure{std::move(opened.error())});
    }

    return std::move(*opened);
}

std::expected<const mux::MuxQueue *, RecoveryDiagnostic> RecoveryCore::historyObservationQueue() const {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    const auto &history = configuration_.queues.history;

    if (!history) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-history-queue"});
    }

    if (cutoff_ || connectionFenced_ || event_ || state_.status().phase != RecoveryPhase::ACTIVE) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-history-ineligible"});
    }

    // DEFERRED can preserve the scheduler's old ACTIVE phase after actual TLS authentication changes.
    // Use the read-only backend accessor, not facts(), which publishes mutable readiness observations.
    const auto actual = backend_.facts();

    if (!actual.present || !actual.connected || !actual.authenticated || actual.terminal ||
        actual.generation != peerGeneration_ || actual.authentication != authentication_) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-history-ineligible"});
    }

    return &*history;
}

std::expected<mux::TickerLatestObservation, RecoveryDiagnostic>
RecoveryCore::visitTickerLatest(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work,
                                void *context,
                                void (*visitor)(void *, const collectors::TickerValue &) noexcept) const {
    if (thread_ != std::this_thread::get_id())
        return std::unexpected(RecoveryDiagnostic{RecoveryError::WRONG_THREAD, "recovery-owner-thread"});

    // This reads only the committed collector cut. A decoded but uncommitted envelope
    // cannot change that cut, and a delivery callback remains under the owner's execution gate.
    const auto &ticker = configuration_.queues.ticker;
    const auto phase = state_.status().phase;

    const auto actual = backend_.facts();
    const bool active = phase == RecoveryPhase::ACTIVE && !connectionFenced_ && !state_.sourceResetPending() &&
                        actual.present && actual.connected && actual.authenticated && !actual.terminal &&
                        actual.generation == peerGeneration_ && actual.authentication == authentication_;
    // A disposed attempt owes a reset before the next attempt. While suspended the stable queue
    // remains readable under the same native absence fence used for retained delivery.
    const bool paused = phase == RecoveryPhase::SUSPENDED && !actual.present && !event_;

    if (!ticker || cutoff_ || (!active && !paused))
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "recovery-ticker-ineligible"});

    auto observed = ticker->visitTickerLatest(handle, key, work, context, visitor);

    if (!observed)
        return std::unexpected(queueFailure(observed.error()));

    return *observed;
}

std::expected<mux::QueueRangeStatus, RecoveryDiagnostic>
RecoveryCore::historyRangeStatus(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work) const {
    auto history = historyObservationQueue();

    if (!history)
        return std::unexpected(history.error());

    auto observed = (*history)->rangeStatus(handle, key, work);

    if (!observed) {
        return std::unexpected(queueFailure(observed.error()));
    }

    return std::move(*observed);
}

std::expected<std::vector<mux::QueueRangeObservation>, RecoveryDiagnostic>
RecoveryCore::historyRangeSnapshot(const mux::MuxHandle &handle, mux::QueueWork &work) const {
    auto history = historyObservationQueue();

    if (!history)
        return std::unexpected(history.error());

    auto observed = (*history)->rangeSnapshot(handle, work);

    if (!observed)
        return std::unexpected(queueFailure(observed.error()));

    return std::move(*observed);
}

std::expected<collectors::HistoryRetainedRange, RecoveryDiagnostic>
RecoveryCore::retainedHistoryRange(const mux::MuxHandle &handle, const collectors::HistoryRetainedQuery &query,
                                   mux::QueueWork &work) const {
    const auto history = historyObservationQueue();

    if (!history)
        return std::unexpected(history.error());

    auto observed = (*history)->retainedHistoryRange(handle, query, work);

    if (!observed)
        return std::unexpected(queueFailure(observed.error()));

    return std::move(*observed);
}

std::optional<RecoveryClock::time_point> RecoveryCore::nextWake() const noexcept {
    if (terminal_)
        return std::nullopt;

    const auto scheduled = state_.nextWake();
    const auto native = backend_.nextWake();
    auto earliest = drain_.nextWake();

    for (const auto hint : {scheduled, native}) {
        if (hint && (!earliest || *hint < *earliest))
            earliest = hint;
    }

    return earliest;
}

} // namespace org::ttldtor::qdcxx::transport::detail
