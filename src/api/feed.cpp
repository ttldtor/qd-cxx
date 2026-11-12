// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"
#include "../core/delivery_statistics.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/api/time_series_subscription.hpp"

#include "../transport/tcp_recovery_factory.hpp"
#include "endpoint_configuration.hpp"
#include "event_registry_defaults.hpp"
#include "feed_internal.hpp"
#include "history_observation.hpp"
#include "receiving_profile.hpp"
#include "subscription_desired.hpp"
#include "subscription_observer.hpp"
#include "system_dns.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <exception>
#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {
constexpr unsigned char FIRST_PRINTABLE_ASCII = 0x20; ///< Exact selector minimum byte.
constexpr unsigned char LAST_PRINTABLE_ASCII = 0x7e;  ///< Exact selector maximum byte.

/** Throw a detached synchronous rejection without formatting caller or peer data.
 * @param code High-level category.
 * @param stage Static library-owned checkpoint.
 */
[[noreturn]] void reject(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** Acquire the owned constructor's lease while its parameter still retains process ownership.
 * @param bootstrap Newly created owner, kept alive through every delegated-constructor failure.
 * @return Counted lease or the existing typed bootstrap refusal; no allocation or SDK initialization.
 */
transport::detail::BootstrapLease acquireOwnedBootstrap(transport::RuntimeBootstrap &bootstrap) {
    auto lease = transport::detail::BootstrapLease::acquireForRuntime(bootstrap);

    if (!lease) {
        throw Error({ErrorCode::BOOTSTRAP_FAILURE, lease.error().stage, lease.error().nativeCode,
                     static_cast<std::uint32_t>(lease.error().error)});
    }

    return std::move(*lease);
}

/** Translate only allocation/capacity exceptions at one public producer boundary.
 * @tparam Action Callable with the required producer operation.
 * @param action Operation; existing Error remains unchanged.
 * @return Operation's original result.
 */
template <class Action> decltype(auto) producerAction(Action &&action) {
    try {
        return std::forward<Action>(action)();
    } catch (const std::bad_alloc &) {
        reject(ErrorCode::ALLOCATION_FAILURE, "api-allocation");
    } catch (const std::length_error &) {
        reject(ErrorCode::RESOURCE_LIMIT, "api-capacity");
    }
}

/** Coalesce a wake without retaining or accessing the native runtime.
 * @param weak Detached runtime control, allowed to expire after shutdown.
 */
void wake(const std::weak_ptr<detail::RuntimeControl> &weak) noexcept {
    if (const auto control = weak.lock()) {
        {
            std::lock_guard lock(control->mutex);
            ++control->wakeSequence;
        }

        control->condition.notify_one();
    }
}

/** Create one finite pure pool only after explicit opt-in; caller holds callbackPolicyMutex. */
std::shared_ptr<detail::CallbackSlotPool> callbackPool(detail::RuntimeControl &control, const RuntimeOptions &limits) {
    if (control.callbackPool)
        return control.callbackPool;

    const auto slots = limits.maximumEndpoints * limits.maximumSubscriptionsPerEndpoint;
    const auto storage = detail::CallbackSlotPool::storageBytes(slots);

    if (storage >= limits.maximumCallbackBytes)
        reject(ErrorCode::RESOURCE_LIMIT, "callback-pool-budget");

    const auto packets = std::min(limits.maximumCustomCallbackSubscriptions,
                                  (limits.maximumCallbackBytes - storage) / detail::CallbackSlotPool::PACKET_BYTES);

    if (!packets)
        reject(ErrorCode::RESOURCE_LIMIT, "callback-packet-budget");

    control.callbackPool = std::make_shared<detail::CallbackSlotPool>(slots, packets);

    return control.callbackPool;
}

/** Caller owns policy, endpoint and subscription locks; no application reference is released. */
void reserveCallbackPolicy(detail::SubscriptionPort &port, const std::shared_ptr<detail::CallbackSlotPool> &pool,
                           bool custom) {
    if (!port.callbackPool && custom) {
        const auto slot = pool->assign();
        port.callbackPool = pool;
        port.callbackSlot = slot;
    }

    if (port.callbackPool)
        port.callbackPool->reserve(port.callbackSlot, custom);
}

/** Reserve the first absolute drain deadline before publishing its monotonic cutoff.
 * A listener may block the SDK worker; that delay must not restart the admitted close allowance.
 * @param port Detached endpoint whose immutable drain policy remains available.
 */
void requestEndpointClose(const std::shared_ptr<detail::EndpointPort> &port) noexcept {
    const auto deadline = std::chrono::steady_clock::now() + port->options.drainTimeout;
    std::chrono::steady_clock::duration::rep expected = 0;
    port->closeDeadline.compare_exchange_strong(expected, deadline.time_since_epoch().count(),
                                                std::memory_order_acq_rel);
    port->closeRequested.store(true, std::memory_order_release);
}

/** Release a charged callable slot only after its application captures have been destroyed.
 * @param ledger Detached callback reservation and notification routes.
 */
void releaseListenerReservation(const std::shared_ptr<detail::ListenerLedger> &ledger) noexcept {
    ledger->count.fetch_sub(1, std::memory_order_acq_rel);

    if (const auto endpoint = ledger->endpoint.lock()) {
        {
            // Pair with the inactive main wait's predicate-to-wait interval; no callable is destroyed here.
            std::lock_guard lock(endpoint->mutex);
        }

        endpoint->retiredCondition.notify_all();
    }

    wake(ledger->control);
}

/** Invoke one complete state-observer selection without a control lock.
 * @tparam Port Detached endpoint or subscription port.
 * @tparam State Its public lifecycle enum.
 * @param port Detached owner, kept alive through callbacks and exception observation.
 * @param listeners Immutable selection made atomically with the transition.
 * @param oldState Actual preceding observation.
 * @param newState Actual newly published observation.
 */
template <class Port, class State>
void notifyState(const std::shared_ptr<Port> &port, const std::shared_ptr<const detail::ListenerSnapshot> &listeners,
                 State oldState, State newState) noexcept {
    if (!listeners) {
        return;
    }

    for (const auto &entry : *listeners) {
        try {
            std::get<std::function<void(State, State)>>(entry.callback->function)(oldState, newState);
        } catch (...) {
            std::lock_guard lock(port->mutex);

            if (port->stateListeners.diagnostic.code == ErrorCode::NONE) {
                port->stateListeners.diagnostic = {ErrorCode::CALLBACK_FAILURE, "state-change-listener"};
            }
        }
    }
}

/** Reserve and publish one actual endpoint transition under its mutex, after caller admission.
 * @param port Detached endpoint with one free preallocated ring slot.
 * @param next Newly published state; the current state differs and is not absorbing.
 */
void queueStateChange(detail::EndpointPort &port, EndpointState next) noexcept {
    if (port.stateChangeCount == port.stateChanges.size()) {
        std::terminate();
    }

    const auto tail = (port.stateChangeHead + port.stateChangeCount) % port.stateChanges.size();
    auto &entry = port.stateChanges[tail];
    entry.previous = port.state;
    entry.current = next;
    entry.listeners = port.stateListeners.snapshot;
    ++port.stateChangeCount;
    port.state = next;
}

/** Dispatch a finite allowance of complete endpoint selections, keeping executing leases charged.
 * Only the claimed execution domain enters this helper; reentrant producers can only append.
 * @param port Detached endpoint, kept alive through selection/capture destruction.
 * @param allowance Fixed entry allowance for this call; producer refills do not extend it.
 * @return True when no notification remains after this allowance.
 */
bool dispatchStateChanges(const std::shared_ptr<detail::EndpointPort> &port, std::size_t allowance) noexcept {
    for (std::size_t dispatched = 0; dispatched < allowance; ++dispatched) {
        detail::EndpointStateChange selected;
        {
            std::lock_guard lock(port->mutex);

            if (port->stateChangeCount == 0 || port->stateChangeDispatching) {
                return port->stateChangeCount == 0;
            }

            port->stateChangeDispatching = true;
            selected = std::move(port->stateChanges[port->stateChangeHead]);
        }

        notifyState(port, selected.listeners, selected.previous, selected.current);
        // This notification's lease, including any final application destructors, ends outside every lock.
        selected.listeners.reset();
        {
            std::lock_guard lock(port->mutex);
            port->stateChangeHead = (port->stateChangeHead + 1) % port->stateChanges.size();
            --port->stateChangeCount;
            port->stateChangeDispatching = false;
        }

        port->retiredCondition.notify_all();
    }

    std::lock_guard lock(port->mutex);

    return port->stateChangeCount == 0;
}

/** Publish a distinct owning-domain observation without bypassing producer publication order.
 * @tparam Port Detached endpoint or subscription port.
 * @tparam State Its public lifecycle enum.
 * @param port Owner with serialized native/claimed creator dispatch.
 * @param next Actual new state; CLOSED/FAILED are absorbing.
 * @param revision Endpoint control revision captured before native progress; absent for final retirement.
 */
template <class Port, class State>
void observeState(const std::shared_ptr<Port> &port, State next,
                  std::optional<std::uint64_t> revision = std::nullopt) noexcept {
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    State previous;
    {
        std::lock_guard lock(port->mutex);
        previous = port->state;

        if (previous == next || previous == State::CLOSED || previous == State::FAILED) {
            return;
        }

        if constexpr (std::is_same_v<Port, detail::EndpointPort>) {
            if (port->stateChangeCount != 0 || (revision && *revision != port->connectionRevision)) {
                return;
            }

            queueStateChange(*port, next);
        } else {
            port->state = next;
            listeners = port->stateListeners.snapshot;
        }
    }

    port->stateCondition.notify_all();

    if constexpr (std::is_same_v<Port, detail::EndpointPort>) {
        // Pending debt has its separate per-turn allowance; this one new native observation is bounded by poll work.
        static_cast<void>(dispatchStateChanges(port, 1));
    } else {
        notifyState(port, listeners, previous, next);
    }
}

/** Drain bounded producer publication debt before native/final observations, including every failure path.
 * @param port Endpoint whose pending transitions already own complete selected snapshots.
 * @return True if all earlier published selections have retired; producer refills cannot extend this call.
 */
bool notifyConnecting(const std::shared_ptr<detail::EndpointPort> &port) noexcept {
    return dispatchStateChanges(port, port->limits.maximumPendingStateChangesPerEndpoint);
}

/** Producer controls; physical intent can coalesce without losing complete published observation selections. */
enum class ConnectionRequest { DISCONNECT, CLEAR, RECONNECT };

/** Admit a nonterminal lifecycle control atomically, without I/O or callable release under locks.
 * @param port Detached endpoint with fixed notification storage.
 * @param request Explicit pause, clear or restart intent.
 */
void requestConnectionChange(const std::shared_ptr<detail::EndpointPort> &port, ConnectionRequest request) {
    producerAction([&] {
        const bool restart = request == ConnectionRequest::RECONNECT;
        const bool clear = request == ConnectionRequest::CLEAR;
        const auto next = restart ? EndpointState::CONNECTING : EndpointState::NOT_CONNECTED;
        {
            std::lock_guard lock(port->mutex);

            if (port->state == EndpointState::CLOSED || port->state == EndpointState::FAILED ||
                (restart && (!port->connectRequested || !port->connectionIntent)) ||
                (!restart && !clear && !port->connectionIntent)) {
                return;
            }

            if (port->closeRequested.load(std::memory_order_acquire)) {
                reject(ErrorCode::CLOSED, "endpoint-closed");
            }

            if (port->connectionRevision == std::numeric_limits<std::uint64_t>::max() ||
                (port->state != next && port->stateChangeCount == port->stateChanges.size())) {
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-control-admission");
            }

            ++port->connectionRevision;
            port->connectionIntent = restart;

            if (clear) {
                port->clearRevision = port->connectionRevision;
            }

            if (port->state != next) {
                queueStateChange(*port, next);
            }
        }

        port->stateCondition.notify_all();
        wake(port->control);
    });
}

/** Record detached failure and admission cutoff without invoking application code.
 * @param port Detached subscription. @param diagnostic Static/numeric failure.
 * @return False only after CLOSED admission.
 */
bool recordSubscriptionFailure(const std::shared_ptr<detail::SubscriptionPort> &port, Diagnostic diagnostic) noexcept {
    std::lock_guard lock(port->mutex);

    if (port->state == SubscriptionState::CLOSED)
        return false;

    if (port->diagnostic.code == ErrorCode::NONE)
        port->diagnostic = diagnostic;

    port->closeRequested.store(true, std::memory_order_release);

    return true;
}

/** Preserve the existing immediate failure publication on the SDK worker.
 * @param port Detached subscription. @param diagnostic Static/numeric failure.
 */
void failSubscription(const std::shared_ptr<detail::SubscriptionPort> &port, Diagnostic diagnostic) noexcept {
    if (recordSubscriptionFailure(port, diagnostic))
        observeState(port, SubscriptionState::FAILED);
}

/** Host event execution requests failure without dispatching the separate state-observer channel.
 * @param port Detached subscription. @param diagnostic Static/numeric failure, with no application text.
 */
void requestSubscriptionFailure(const std::shared_ptr<detail::SubscriptionPort> &port, Diagnostic diagnostic) noexcept {
    if (recordSubscriptionFailure(port, diagnostic))
        wake(port->control);
}

/** Publish requested failure from worker progress or existing owner retirement.
 * @param port Detached subscription; no callback packet or control lock is held by this caller.
 */
void publishRequestedSubscriptionFailure(const std::shared_ptr<detail::SubscriptionPort> &port) noexcept {
    bool requested;
    {
        std::lock_guard lock(port->mutex);
        requested = port->diagnostic.code != ErrorCode::NONE;
    }

    if (requested)
        observeState(port, SubscriptionState::FAILED);
}

/** Preserve an endpoint's first bounded asynchronous failure.
 * @param port Detached producer state.
 * @param diagnostic Static/numeric failure snapshot.
 */
void failEndpoint(const std::shared_ptr<detail::EndpointPort> &port, Diagnostic diagnostic) noexcept {
    port->closeRequested.store(true, std::memory_order_release);
    notifyConnecting(port);
    {
        std::lock_guard lock(port->mutex);

        if (port->state == EndpointState::CLOSED) {
            return;
        }

        if (port->diagnostic.code == ErrorCode::NONE) {
            port->diagnostic = diagnostic;
        }
    }

    observeState(port, EndpointState::FAILED);
}

/** Allocation-free stack node for creator-thread capture-retirement reentry checks. */
struct RetirementFrame {
    detail::EndpointPort *port; ///< Endpoint currently releasing application captures on this thread.
    RetirementFrame *previous;  ///< Earlier nested cleanup, including A-to-B-to-A cycles.
};

thread_local RetirementFrame *retirementFrame = nullptr; ///< Current thread's active cleanup chain.

/** Track every final accepted callable destruction, including producer-side listener removal. */
class RetirementGuard final {
    RetirementFrame frame_;

    public:
    /** Push one allocation-free nested destruction scope.
     * @param port Endpoint whose accepted callable is being destroyed on this thread.
     */
    explicit RetirementGuard(detail::EndpointPort *port) noexcept : frame_{port, retirementFrame} {
        retirementFrame = &frame_;
    }

    /** Restore the enclosing scope after all application destructors return. */
    ~RetirementGuard() {
        retirementFrame = frame_.previous;
    }
};

/** Inspect the complete nested cleanup chain without allocation or locking.
 * @param port Detached endpoint considered for a blocking await.
 * @return True when that await would wait for its own enclosing callable destruction.
 */
bool retirementReentered(const detail::EndpointPort *port) noexcept {
    for (auto *frame = retirementFrame; frame; frame = frame->previous) {
        if (frame->port == port) {
            return true;
        }
    }

    return false;
}

/** Wait on a detached port's current state without selecting observers or performing lifecycle work.
 * @tparam Port EndpointPort or SubscriptionPort, retaining immutable creator affinity.
 * @tparam State The matching public lifecycle enum; its contiguous range ends with FAILED.
 * @param port Strong ownership lasts through cancellation callback deregistration.
 * @param target Requested current state, not a remembered transition.
 * @param deadline Absolute steady-clock deadline, or max for no deadline.
 * @param stop Application-owned cancellation token.
 * @return One state/diagnostic snapshot with target, terminal, cancellation, deadline precedence.
 */
template <class Port, class State>
StateWaitResult<State> awaitCurrentState(std::shared_ptr<Port> port, State target,
                                         std::chrono::steady_clock::time_point deadline, std::stop_token stop) {
    if (std::this_thread::get_id() != port->owner) {
        reject(ErrorCode::WRONG_THREAD, "state-await-thread");
    }

    if (retirementFrame || detail::subscriptionObserverCallbackActive()) {
        reject(ErrorCode::INVALID_STATE, "state-await-retirement-reentry");
    }

    if (target < State{} || target > State::FAILED) {
        reject(ErrorCode::INVALID_ARGUMENT, "state-await-target");
    }

    // Registration may invoke inline for a pre-cancelled token. It must precede locking the port.
    // This object is destroyed after the later unique_lock, so deregistration cannot join under that lock.
    std::stop_callback cancellation(stop, [port] {
        {
            // Synchronize with the predicate-to-wait interval, even though stop_requested is atomic.
            std::lock_guard lock(port->mutex);
        }

        port->stateCondition.notify_all();
    });
    std::unique_lock lock(port->mutex);

    for (;;) {
        if (port->state == target) {
            return {WaitStatus::REACHED, port->state, port->diagnostic};
        }

        if (port->state == State::CLOSED || port->state == State::FAILED) {
            return {WaitStatus::TERMINAL, port->state, port->diagnostic};
        }

        // Sample time before the monotonic stop flag: an expired result then has a point at which stop was false.
        const auto observedAt = std::chrono::steady_clock::now();

        if (stop.stop_requested()) {
            return {WaitStatus::CANCELLED, port->state, port->diagnostic};
        }

        if (observedAt >= deadline) {
            return {WaitStatus::DEADLINE_EXPIRED, port->state, port->diagnostic};
        }

        if (deadline == std::chrono::steady_clock::time_point::max()) {
            port->stateCondition.wait(lock);
        } else {
            port->stateCondition.wait_until(lock, deadline);
        }
    }
}

/** Retire accepted snapshots outside all locks, then publish actual callback quiescence.
 * @param port Subscription whose worker registration and executing callbacks have already retired.
 * @return True only after accepted and producer-held removed callable captures have all retired.
 */
bool retireSubscription(const std::shared_ptr<detail::SubscriptionPort> &port) noexcept {
    port->closeRequested.store(true, std::memory_order_release);
    publishRequestedSubscriptionFailure(port);
    std::shared_ptr<detail::CallbackSlotPool> callbackPool;
    std::size_t callbackSlot;
    {
        std::lock_guard lock(port->mutex);
        callbackPool = port->callbackPool;
        callbackSlot = port->callbackSlot;
    }

    if (callbackPool) {
        callbackPool->cancel(callbackSlot, {ErrorCode::CALLBACK_CANCELLED, "callback-close"});

        if (!callbackPool->snapshot(callbackSlot).quiescent)
            return false;
    }

    detail::closeSubscriptionObservers(port);
    observeState(port, SubscriptionState::CLOSING);
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    std::shared_ptr<CallbackExecutor> eventExecutor;
    {
        std::lock_guard lock(port->mutex);
        listeners = std::move(port->listeners);
        eventExecutor = std::move(port->eventExecutor);
    }
    eventExecutor.reset();
    listeners.reset();

    if (port->listenerLedger->count.load(std::memory_order_acquire) != 0 ||
        port->observers.ledger->count.load(std::memory_order_acquire) != 0) {
        return false;
    }

    observeState(port, SubscriptionState::CLOSED);
    {
        std::lock_guard lock(port->mutex);
        listeners = std::move(port->stateListeners.snapshot);
    }

    listeners.reset();

    if (port->stateListeners.ledger->count.load(std::memory_order_acquire) != 0) {
        return false;
    }

    {
        // Publish under the wait predicate's mutex so notification cannot fall between its check and wait.
        std::lock_guard lock(port->mutex);

        if (port->callbackPool) {
            const auto customRevision = port->callbackPool->snapshot(port->callbackSlot).retirementRevision;
            port->callbackSynchronousRetirementRevision =
                customRevision > UINT64_MAX - port->callbackSynchronousRetirementRevision
                    ? UINT64_MAX
                    : customRevision + port->callbackSynchronousRetirementRevision;
            port->callbackPool->release(port->callbackSlot);
            port->callbackPool.reset();
            port->callbackSlot = detail::CallbackSlotPool::NO_SLOT;
        }

        port->statistics->update([](auto &s) {
            s.activeSelectors = 0;
            s.activeDesiredRevision.reset();
            s.sourceEpoch.reset();
            s.final = true;
        });
        port->history.final = true;
        port->history.state = HistoryObservationState::RETIRED;

        for (auto &range : port->history.ranges)
            range.status.completeDeliveredRange = false;

        port->retired.store(true, std::memory_order_release);
    }

    return true;
}

/** Retire all producer snapshots after the endpoint's native/receiver owners are gone.
 * @param port Detached endpoint.
 * @param result Actual detached local outcome.
 * @return True only after every native, selected and producer-held callable capture has retired.
 */
bool retireEndpoint(const std::shared_ptr<detail::EndpointPort> &port, CloseResult result) noexcept {
    if (port->retired.load(std::memory_order_acquire)) {
        return true;
    }

    port->closeRequested.store(true, std::memory_order_release);
    bool expected = false;
    port->retirementClaimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);

    if (port->retirementOnMain.load(std::memory_order_acquire) && std::this_thread::get_id() != port->owner) {
        return false;
    }

    RetirementGuard guard(port.get());
    notifyConnecting(port);
    observeState(port, EndpointState::CLOSING);

    bool complete = true;
    {
        std::lock_guard lock(port->mutex);

        if (!port->retirementResultPrepared) {
            port->terminal = result;
            port->retirementResultPrepared = true;
        }
    }

    for (std::size_t index = 0; index < port->subscriptions.size(); ++index) {
        std::shared_ptr<detail::SubscriptionPort> subscription;
        {
            std::lock_guard lock(port->mutex);
            subscription = port->subscriptions[index];
        }

        if (subscription && !subscription->retired.load(std::memory_order_acquire)) {
            complete = retireSubscription(subscription) && complete;
        }
    }

    if (!complete) {
        return false;
    }

    {
        std::lock_guard lock(port->mutex);
        port->terminal.diagnostic = port->diagnostic;
    }

    std::shared_ptr<CallbackExecutor> eventExecutor;
    {
        std::lock_guard lock(port->mutex);
        eventExecutor = std::move(port->eventExecutor);
    }
    eventExecutor.reset();
    observeState(port,
                 port->terminal.diagnostic.code == ErrorCode::NONE ? EndpointState::CLOSED : EndpointState::FAILED);
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    {
        std::lock_guard lock(port->mutex);
        listeners = std::move(port->stateListeners.snapshot);
    }

    listeners.reset();

    if (port->stateListeners.ledger->count.load(std::memory_order_acquire) != 0) {
        return false;
    }

    {
        // Publish under the wait predicate's mutex so notification cannot fall between its check and wait.
        std::lock_guard lock(port->mutex);
        port->statistics.final = true;
        port->statistics.observedAt = std::chrono::steady_clock::now();
        port->retired.store(true, std::memory_order_release);
    }
    port->retiredCondition.notify_all();
    wake(port->control);

    return true;
}

/** Observe producer-held callable leases after accepted snapshots have been cleared.
 * @param port Detached closed endpoint, protected by its mutex for the slot scan.
 * @return True only when all independently charged callable boxes have been destroyed.
 */
bool capturesRetired(const detail::EndpointPort &port) noexcept {
    if (!port.stateListeners.snapshot && port.stateListeners.ledger->count.load(std::memory_order_acquire) != 0) {
        return false;
    }

    return std::ranges::all_of(port.subscriptions, [](const auto &subscription) {
        if (!subscription) {
            return true;
        }

        std::lock_guard lock(subscription->mutex);

        return subscription->listenerLedger->count.load(std::memory_order_acquire) == 0 &&
               subscription->observers.ledger->count.load(std::memory_order_acquire) == 0 &&
               (subscription->stateListeners.snapshot ||
                subscription->stateListeners.ledger->count.load(std::memory_order_acquire) == 0);
    });
}

/** Finish an already claimed inactive creator-thread cleanup without starting a worker.
 * @param port Detached endpoint; runtime control gate published MAIN ownership before calling.
 */
void finishInactiveRetirement(const std::shared_ptr<detail::EndpointPort> &port) {
    while (!retireEndpoint(port, CloseResult{true})) {
        std::unique_lock lock(port->mutex);
        port->retiredCondition.wait(lock, [&] {
            return capturesRetired(*port);
        });
    }
}

/** Translate original recovery evidence without retaining its source owner.
 * @param value Existing static/numeric recovery failure.
 * @return Detached high-level diagnostic preserving original native/nested categories.
 */
Diagnostic recoveryDiagnostic(const transport::RecoveryDiagnostic &value) noexcept {
    auto code = ErrorCode::TRANSPORT_FAILURE;

    switch (value.error) {
    case transport::RecoveryError::NONE:
        code = ErrorCode::NONE;
        break;
    case transport::RecoveryError::RESOURCE_LIMIT:
        code = ErrorCode::RESOURCE_LIMIT;
        break;
    case transport::RecoveryError::WORK_LIMIT:
        code = ErrorCode::WORK_LIMIT;
        break;
    case transport::RecoveryError::ALLOCATION_FAILURE:
        code = ErrorCode::ALLOCATION_FAILURE;
        break;
    case transport::RecoveryError::PROTOCOL_FAILURE:
    case transport::RecoveryError::AUTHENTICATION:
        code = ErrorCode::PROTOCOL_FAILURE;
        break;
    case transport::RecoveryError::UNSUPPORTED_PROFILE:
    case transport::RecoveryError::INVALID_CONFIGURATION:
        code = ErrorCode::UNSUPPORTED_PROFILE;
        break;
    default:
        break;
    }

    return {code, value.stage, value.transport.nativeCode, static_cast<std::uint32_t>(value.error)};
}

/** Classify a real queue refusal without weakening its failure to an endless retry.
 * @param error Original queue category.
 * @param stage Static API checkpoint.
 * @return Detached category and original enum ordinal.
 */
Diagnostic queueDiagnostic(mux::QueueError error, std::string_view stage) noexcept {
    const auto code = error == mux::QueueError::WORK_LIMIT           ? ErrorCode::WORK_LIMIT
                      : error == mux::QueueError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                     : ErrorCode::RESOURCE_LIMIT;

    return {code, stage, 0, static_cast<std::uint32_t>(error)};
}

/** Reject producer mutations after either logical or enclosing endpoint cutoff.
 * @param port Detached subscription, protected by its mutex during mutation.
 */
void requireOpen(const detail::SubscriptionPort &port) {
    const auto endpoint = port.endpoint.lock();

    if (port.closeRequested.load(std::memory_order_acquire) || port.retired.load(std::memory_order_acquire) ||
        !endpoint || endpoint->closeRequested.load(std::memory_order_acquire) ||
        endpoint->retired.load(std::memory_order_acquire)) {
        reject(ErrorCode::CLOSED, "subscription-closed");
    }
}

/** Prepare a detached state-observer reservation before publishing its owner.
 * @param endpoint Enclosing endpoint whose wait includes final capture retirement.
 * @param maximum Explicit positive count of accepted and still selected/retiring observers.
 * @return Empty state-observer selection and its independent detached ledger.
 */
detail::StateListeners makeStateListeners(const std::shared_ptr<detail::EndpointPort> &endpoint, std::size_t maximum) {
    detail::StateListeners result;
    result.ledger = std::make_shared<detail::ListenerLedger>();
    result.ledger->maximum = maximum;
    result.ledger->control = endpoint->control;
    result.ledger->endpoint = endpoint;
    result.snapshot = std::make_shared<const detail::ListenerSnapshot>();

    return result;
}

/** Check state-observer admission while the enclosing port mutex is held.
 * @tparam Port Detached endpoint or subscription port.
 * @param port Owner whose close cutoff also fences ordinary observer admission.
 */
template <class Port> void requireStateOpen(const Port &port) {
    if constexpr (std::is_same_v<Port, detail::SubscriptionPort>) {
        requireOpen(port);
    } else if (port.closeRequested.load(std::memory_order_acquire) || port.retired.load(std::memory_order_acquire)) {
        reject(ErrorCode::CLOSED, "endpoint-closed");
    }
}

/** Add one bounded advisory observer without copying or destroying application functors under locks.
 * @tparam Port Detached endpoint or subscription port.
 * @tparam Function Exact public state-observer callable type.
 * @param port Owner of the accepted snapshot.
 * @param listener Owning nonempty callable, moved into one charged shared box.
 * @return Non-reused identity; throws Error for invalid input, closed admission or exhausted capacity.
 */
template <class Port, class Function>
std::uint64_t addStateListener(const std::shared_ptr<Port> &port, Function listener) {
    return producerAction([&]() -> std::uint64_t {
        if (!listener) {
            reject(ErrorCode::INVALID_ARGUMENT, "state-change-listener");
        }

        const auto ledger = port->stateListeners.ledger;
        {
            std::lock_guard lock(port->mutex);
            requireStateOpen(*port);
            auto count = ledger->count.load(std::memory_order_acquire);

            do {
                if (count >= ledger->maximum) {
                    reject(ErrorCode::RESOURCE_LIMIT, "state-change-listener-count");
                }
            } while (!ledger->count.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel));
        }

        std::shared_ptr<detail::ListenerBox> callback;

        try {
            callback = std::make_shared<detail::ListenerBox>(ledger, std::move(listener));
        } catch (...) {
            releaseListenerReservation(ledger);
            throw;
        }

        std::shared_ptr<const detail::ListenerSnapshot> previous;
        std::uint64_t id = 0;
        {
            std::lock_guard lock(port->mutex);
            requireStateOpen(*port);
            auto &state = port->stateListeners;

            if (state.nextListener == std::numeric_limits<std::uint64_t>::max()) {
                reject(ErrorCode::RESOURCE_LIMIT, "state-change-listener-identity");
            }

            auto candidate = std::make_shared<detail::ListenerSnapshot>(*state.snapshot);
            id = state.nextListener;
            candidate->push_back({id, callback});
            previous = std::move(state.snapshot);
            state.snapshot = std::move(candidate);
            ++state.nextListener;
        }

        previous.reset();

        return id;
    });
}

/** Remove an advisory observer without revoking an already selected invocation.
 * @tparam Port Detached endpoint or subscription port.
 * @param port Owner of the accepted snapshot, possibly already closed.
 * @param id Non-reused state-observer identity; absence is an idempotent no-op.
 */
template <class Port> void removeStateListener(const std::shared_ptr<Port> &port, std::uint64_t id) {
    producerAction([&] {
        std::shared_ptr<const detail::ListenerSnapshot> previous;
        {
            std::lock_guard lock(port->mutex);
            auto &snapshot = port->stateListeners.snapshot;

            if (!snapshot) {
                return;
            }

            const auto found = std::ranges::find(*snapshot, id, &detail::ListenerEntry::id);

            if (found == snapshot->end()) {
                return;
            }

            auto candidate = std::make_shared<detail::ListenerSnapshot>(*snapshot);
            candidate->erase(candidate->begin() + (found - snapshot->begin()));
            previous = std::move(snapshot);
            snapshot = std::move(candidate);
        }

        previous.reset();
    });
}

/** Validate and own one exact canonical ASCII selector before publication.
 * @param symbol Caller text, borrowed only for validation/copy.
 * @param maximumUnits Explicit runtime selector ceiling.
 * @return Owned exact UTF16 spelling, without case folding or normalization.
 */
std::u16string exactSymbol(std::string_view symbol, std::size_t maximumUnits) {
    if (symbol.empty() || symbol.size() > maximumUnits || symbol.front() == '*' ||
        !std::ranges::all_of(symbol, [](unsigned char value) {
            return value >= FIRST_PRINTABLE_ASCII && value <= LAST_PRINTABLE_ASCII;
        })) {
        reject(ErrorCode::INVALID_ARGUMENT, "subscription-symbol");
    }

    return {symbol.begin(), symbol.end()};
}

/** Accepted desired-state mutation kind; there is no producer command FIFO. */
enum class SymbolMutation { ADD, REMOVE, REPLACE };

/** Validate a typed value before copying caller-owned text or publishing any desired state.
 * @param port Detached logical owner with immutable selector bounds.
 * @param selector Caller-owned value, borrowed only during this call.
 * @param mutation Removal does not repeat regional representability checks.
 */
void validateSelector(const detail::SubscriptionPort &port, const SubscriptionSelector &selector,
                      SymbolMutation mutation) {
    const auto endpoint = port.endpoint.lock();

    if (selector.kind() == SubscriptionSymbolKind::TIME_SERIES &&
        (port.timeSeries || !endpoint || endpoint->options.contract != DeliveryContract::HISTORY ||
         !std::ranges::all_of(port.eventTypes, isTimeSeriesEventType))) {
        reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series");
    }

    if (port.timeSeries && selector.kind() != SubscriptionSymbolKind::PLAIN) {
        reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-nested-symbol");
    }

    if (selector.kind() == SubscriptionSymbolKind::ALL && !endpoint) {
        reject(ErrorCode::CLOSED, "subscription-closed");
    }

    if (selector.kind() == SubscriptionSymbolKind::ALL) {
        if (!endpoint || !endpoint->options.enableWildcard || endpoint->options.contract != DeliveryContract::STREAM) {
            reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-wildcard");
        }

        return;
    }

    const auto text = selector.eventSymbol();

    if (text.empty() || text.size() > port.maximumSymbolUnits || text.front() == u'*' ||
        !std::ranges::all_of(text, [](char16_t unit) {
            return unit >= FIRST_PRINTABLE_ASCII && unit <= LAST_PRINTABLE_ASCII;
        })) {
        reject(ErrorCode::INVALID_ARGUMENT, "subscription-symbol");
    }

    if (mutation != SymbolMutation::REMOVE && endpoint) {
        if (port.candleRoutes) {
            if (selector.sourceId().value_or(0) > 0)
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-candle-source");

            (void)detail::resolveReceivingCandle(*port.candleRoutes, text);
        }

        detail::validateReceivingSymbol(endpoint->options.eventRegistry ? &*endpoint->options.eventRegistry : nullptr,
                                        port.eventSelection, text);
    }
}

/** Transform a complete bounded desired set, preserving typed identity and effective attributes.
 * @tparam Input Borrowed string view or selector type.
 * @tparam Parse Validating conversion into an owned selector.
 * @param port Detached producer state.
 * @param inputs Borrowed inputs; each conversion validates before copying its text.
 * @param mutation Complete desired transformation.
 * @param parse Conversion invoked only while caller input is alive.
 */
template <class Input, class Parse>
void mutateDesired(const std::shared_ptr<detail::SubscriptionPort> &port, std::span<const Input> inputs,
                   SymbolMutation mutation, Parse parse) {
    producerAction([&] {
        std::vector<SubscriptionSelector> parsed;
        parsed.reserve(port->maximumSymbols);

        for (const auto &input : inputs) {
            auto symbol = parse(input);
            validateSelector(*port, symbol, mutation);
            const auto found = std::ranges::find(parsed, symbol);

            if (found != parsed.end()) {
                *found = std::move(symbol);
            } else {
                if (parsed.size() >= port->maximumSymbols) {
                    reject(ErrorCode::RESOURCE_LIMIT, "subscription-symbol-count");
                }

                parsed.push_back(std::move(symbol));
            }
        }

        {
            detail::SubscriptionObserverAdmission admission(port->changes);
            std::lock_guard lock(port->mutex);
            requireOpen(*port);
            std::vector<SubscriptionSelector> candidate;
            candidate.reserve(port->maximumSymbols);

            if (mutation != SymbolMutation::REPLACE) {
                candidate.insert(candidate.end(), port->symbols.begin(), port->symbols.end());
            }

            for (auto &symbol : parsed) {
                if (port->timeSeries) {
                    symbol = SubscriptionSelector::timeSeries(std::u16string(symbol.eventSymbol()), *port->fromTime);
                }

                const auto found = std::ranges::find(candidate, symbol);

                if (mutation == SymbolMutation::REMOVE) {
                    if (found != candidate.end()) {
                        candidate.erase(found);
                    }
                } else if (found == candidate.end()) {
                    if (candidate.size() >= port->maximumSymbols) {
                        reject(ErrorCode::RESOURCE_LIMIT, "subscription-symbol-count");
                    }

                    candidate.push_back(std::move(symbol));
                } else {
                    *found = std::move(symbol);
                }
            }

            detail::commitSubscriptionDesiredLocked(*port, std::move(candidate), port->fromTime);
        }

        wake(port->control);
        detail::dispatchSubscriptionChanges(port->changes);
    });
}

/** Preserve the original exact string API over plain typed identities.
 * @param port Detached producer state.
 * @param inputs Borrowed exact ASCII spellings.
 * @param mutation Complete desired transformation.
 */
void mutateSymbols(const std::shared_ptr<detail::SubscriptionPort> &port, std::span<const std::string_view> inputs,
                   SymbolMutation mutation) {
    mutateDesired(port, inputs, mutation, [&](std::string_view input) {
        return SubscriptionSelector::plain(exactSymbol(input, port->maximumSymbolUnits));
    });
}

/** Validate bounded typed input before any text copy; no caller decorator is retained.
 * @param port Detached producer state.
 * @param inputs Borrowed selector batch, including duplicates.
 * @param mutation Complete desired transformation.
 */
void mutateSelectors(const std::shared_ptr<detail::SubscriptionPort> &port,
                     std::span<const SubscriptionSelector> inputs, SymbolMutation mutation) {
    if (inputs.size() > port->maximumSymbols) {
        reject(ErrorCode::RESOURCE_LIMIT, "subscription-selector-arguments");
    }

    mutateDesired(port, inputs, mutation, [&](const SubscriptionSelector &input) {
        validateSelector(*port, input, mutation);

        return input;
    });
}

/** Convert a bounded unique canonical object set before entering the existing atomic producer path.
 * @param port Detached producer owner with immutable limits.
 * @param inputs Borrowed immutable Candle values; no input view survives this call.
 * @param mutation Complete desired transformation.
 */
void mutateCandleSymbols(const std::shared_ptr<detail::SubscriptionPort> &port,
                         std::span<const events::CandleSymbol> inputs, SymbolMutation mutation) {
    producerAction([&] {
        std::vector<std::string> owned;
        owned.reserve(std::min(inputs.size(), port->maximumSymbols));

        for (const auto &input : inputs) {
            auto text = input.getSymbol();

            if (!text || text->empty() || text->size() > port->maximumSymbolUnits || text->front() == u'*' ||
                !std::ranges::all_of(*text, [](char16_t unit) {
                    return unit >= FIRST_PRINTABLE_ASCII && unit <= LAST_PRINTABLE_ASCII;
                })) {
                reject(ErrorCode::INVALID_ARGUMENT, "subscription-symbol");
            }

            if (port->candleRoutes && events::CandleSymbol::valueOf(text).getPeriod() != input.getPeriod())
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-candle-period");

            bool duplicate = std::ranges::any_of(owned, [&](const auto &other) {
                return std::equal(text->begin(), text->end(), other.begin(), other.end(),
                                  [](char16_t left, unsigned char right) {
                                      return left == right;
                                  });
            });

            if (duplicate) {
                continue;
            }

            if (owned.size() >= port->maximumSymbols) {
                reject(ErrorCode::RESOURCE_LIMIT, "subscription-symbol-count");
            }

            auto &result = owned.emplace_back();
            result.reserve(text->size());

            for (char16_t unit : *text) {
                result.push_back(static_cast<char>(unit));
            }
        }

        std::vector<std::string_view> views;
        views.reserve(owned.size());

        for (const auto &text : owned) {
            views.push_back(text);
        }

        mutateSymbols(port, views, mutation);
    });
}

} // namespace

Error::Error(Diagnostic diagnostic) : std::runtime_error(diagnostic.stage.data()), diagnostic_(diagnostic) {
}

Diagnostic Error::diagnostic() const noexcept {
    return diagnostic_;
}

namespace detail {

ListenerBox::~ListenerBox() {
    const auto endpoint = ledger->endpoint.lock();
    RetirementGuard guard(endpoint.get());
    function.emplace<0>();
    releaseListenerReservation(ledger);
}
} // namespace detail

template <class Event>
Subscription<Event>::Subscription(std::shared_ptr<detail::SubscriptionPort> port) noexcept : port_(std::move(port)) {
}

template <class Event> SubscriptionSnapshot Subscription<Event>::getSnapshot() const {
    return producerAction([&] {
        std::lock_guard lock(port_->mutex);

        return detail::subscriptionSnapshotLocked(*port_);
    });
}

template <class Event> std::int64_t Subscription<Event>::getFromTime() const {
    std::lock_guard lock(port_->mutex);

    if (!port_->timeSeries)
        reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-mode");

    return *port_->fromTime;
}

template <class Event> void Subscription<Event>::setFromTime(std::int64_t fromTime) {
    producerAction([&] {
        {
            detail::SubscriptionObserverAdmission admission(port_->changes);
            std::lock_guard lock(port_->mutex);
            requireOpen(*port_);
            detail::replaceSubscriptionFromTimeLocked(*port_, fromTime);
        }
        wake(port_->control);
        detail::dispatchSubscriptionChanges(port_->changes);
    });
}

template <class Event> SubscriptionStatistics Subscription<Event>::getStatistics() const noexcept {
    return port_->statistics->snapshot();
}

template <class Event>
std::optional<std::chrono::milliseconds> Subscription<Event>::getAggregationPeriod() const noexcept {
    std::lock_guard lock(port_->statistics->mutex);

    return port_->statistics->subscriptionAggregationPeriod;
}

template <class Event> void Subscription<Event>::setAggregationPeriod(std::optional<std::chrono::milliseconds> period) {
    if (period && (period->count() < 0 || *period > MAXIMUM_LOCAL_AGGREGATION_PERIOD))
        reject(ErrorCode::INVALID_ARGUMENT, "subscription-local-aggregation-period");

    {
        std::lock_guard lock(port_->mutex);
        requireOpen(*port_);
        std::lock_guard statisticsLock(port_->statistics->mutex);
        port_->statistics->subscriptionAggregationPeriod = period;
    }
    wake(port_->control);
}

template <class Event>
std::optional<std::chrono::milliseconds> Subscription<Event>::getAggregationPeriodInfo() const noexcept {
    std::lock_guard lock(port_->statistics->mutex);

    return port_->statistics->deliveredAggregationPeriod;
}

template <class Event> HistorySubscriptionSnapshot Subscription<Event>::getHistorySnapshot() const {
    return producerAction([&] {
        return detail::copyHistorySnapshot(port_);
    });
}

template <class Event> std::vector<EventType> Subscription<Event>::getEventTypes() const {
    return producerAction([&] {
        return port_->eventTypes;
    });
}

template <class Event> bool Subscription<Event>::containsEventType(EventType type) const noexcept {
    return std::ranges::find(port_->eventTypes, type) != port_->eventTypes.end();
}

template <class Event>
typename Subscription<Event>::ChangeListenerId Subscription<Event>::addChangeListener(ChangeListener listener) {
    return producerAction([&] {
        return detail::addSubscriptionObserver(port_, std::move(listener));
    });
}

template <class Event> void Subscription<Event>::removeChangeListener(ChangeListenerId id) {
    producerAction([&] {
        detail::removeSubscriptionObserver(port_, id);
    });
}

template <class Event> Diagnostic Subscription<Event>::getChangeDiagnostic() const noexcept {
    std::lock_guard lock(port_->changes->mutex);

    return port_->observers.diagnostic;
}

template <class Event>
typename Subscription<Event>::StateChangeListenerId
Subscription<Event>::addStateChangeListener(StateChangeListener listener) {
    return addStateListener(port_, std::move(listener));
}

template <class Event> void Subscription<Event>::removeStateChangeListener(StateChangeListenerId id) {
    removeStateListener(port_, id);
}

template <class Event> Diagnostic Subscription<Event>::getStateChangeDiagnostic() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->stateListeners.diagnostic;
}

template <class Event> Subscription<Event>::~Subscription() {
    close();
}

template <class Event>
typename Subscription<Event>::ListenerId Subscription<Event>::addEventListener(Listener listener) {
    return producerAction([&]() -> ListenerId {
        if (!listener) {
            reject(ErrorCode::INVALID_ARGUMENT, "subscription-listener");
        }

        auto ledger = port_->listenerLedger;
        {
            std::lock_guard lock(port_->mutex);
            requireOpen(*port_);
            auto count = ledger->count.load(std::memory_order_acquire);

            do {
                if (count >= ledger->maximum) {
                    reject(ErrorCode::RESOURCE_LIMIT, "subscription-listener-count");
                }
            } while (!ledger->count.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel));
        }

        std::shared_ptr<detail::ListenerBox> callback;

        try {
            callback = std::make_shared<detail::ListenerBox>(ledger, std::move(listener));
        } catch (...) {
            releaseListenerReservation(ledger);
            throw;
        }

        std::shared_ptr<const detail::ListenerSnapshot> previous;
        ListenerId id = 0;
        {
            std::lock_guard lock(port_->mutex);
            requireOpen(*port_);

            if (port_->nextListener == std::numeric_limits<ListenerId>::max()) {
                reject(ErrorCode::RESOURCE_LIMIT, "subscription-listener-identity");
            }

            auto candidate = std::make_shared<detail::ListenerSnapshot>(*port_->listeners);
            id = port_->nextListener;
            candidate->push_back({id, callback});
            previous = std::move(port_->listeners);
            port_->listeners = std::move(candidate);
            ++port_->nextListener;
        }

        previous.reset();
        wake(port_->control);

        return id;
    });
}

template <class Event> void Subscription<Event>::removeEventListener(ListenerId id) {
    producerAction([&] {
        std::shared_ptr<const detail::ListenerSnapshot> previous;
        {
            std::lock_guard lock(port_->mutex);
            requireOpen(*port_);
            const auto found = std::ranges::find(*port_->listeners, id, &detail::ListenerEntry::id);

            if (found == port_->listeners->end()) {
                return;
            }

            auto candidate = std::make_shared<detail::ListenerSnapshot>(*port_->listeners);
            candidate->erase(candidate->begin() + (found - port_->listeners->begin()));
            previous = std::move(port_->listeners);
            port_->listeners = std::move(candidate);
        }

        previous.reset();
        wake(port_->control);
    });
}

template <class Event> void Subscription<Event>::addSymbols(std::string_view symbol) {
    mutateSymbols(port_, std::span(&symbol, 1), SymbolMutation::ADD);
}

template <class Event> void Subscription<Event>::addSymbols(std::initializer_list<std::string_view> symbols) {
    mutateSymbols(port_, std::span(symbols.begin(), symbols.size()), SymbolMutation::ADD);
}

template <class Event> void Subscription<Event>::removeSymbols(std::string_view symbol) {
    mutateSymbols(port_, std::span(&symbol, 1), SymbolMutation::REMOVE);
}

template <class Event> void Subscription<Event>::setSymbols(std::initializer_list<std::string_view> symbols) {
    mutateSymbols(port_, std::span(symbols.begin(), symbols.size()), SymbolMutation::REPLACE);
}

template <class Event> void Subscription<Event>::addSymbols(const events::CandleSymbol &symbol) {
    mutateCandleSymbols(port_, std::span(&symbol, 1), SymbolMutation::ADD);
}

template <class Event> void Subscription<Event>::addCandleSymbols(std::span<const events::CandleSymbol> symbols) {
    mutateCandleSymbols(port_, symbols, SymbolMutation::ADD);
}

template <class Event> void Subscription<Event>::removeSymbols(const events::CandleSymbol &symbol) {
    mutateCandleSymbols(port_, std::span(&symbol, 1), SymbolMutation::REMOVE);
}

template <class Event> void Subscription<Event>::setCandleSymbols(std::span<const events::CandleSymbol> symbols) {
    mutateCandleSymbols(port_, symbols, SymbolMutation::REPLACE);
}

template <class Event> void Subscription<Event>::addSelectors(std::span<const SubscriptionSelector> selectors) {
    mutateSelectors(port_, selectors, SymbolMutation::ADD);
}

template <class Event> void Subscription<Event>::setSelectors(std::span<const SubscriptionSelector> selectors) {
    mutateSelectors(port_, selectors, SymbolMutation::REPLACE);
}

template <class Event> void Subscription<Event>::removeSelector(const SubscriptionSelector &selector) {
    mutateSelectors(port_, std::span(&selector, 1), SymbolMutation::REMOVE);
}

template <class Event> void Subscription<Event>::clearSymbols() {
    mutateSymbols(port_, {}, SymbolMutation::REPLACE);
}

template <class Event> void Subscription<Event>::close() noexcept {
    if (port_) {
        port_->closeRequested.store(true, std::memory_order_release);
        detail::closeSubscriptionObservers(port_, false);
        wake(port_->control);
    }
}

template <class Event> SubscriptionState Subscription<Event>::getState() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->state;
}

template <class Event>
StateWaitResult<SubscriptionState> Subscription<Event>::awaitState(SubscriptionState target,
                                                                   std::chrono::steady_clock::time_point deadline,
                                                                   std::stop_token stop) const {
    return awaitCurrentState(port_, target, deadline, stop);
}

template <class Event> Diagnostic Subscription<Event>::getDiagnostic() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->diagnostic;
}

std::shared_ptr<detail::SubscriptionPort> Feed::createSubscriptionPort(EventType type) {
    return createSubscriptionPort(std::span(&type, 1), false);
}

std::shared_ptr<Subscription<RegisteredEvent>> Feed::createSubscription(std::span<const EventType> types) {
    return producerAction([&] {
        auto subscription = std::shared_ptr<Subscription<RegisteredEvent>>(new Subscription<RegisteredEvent>({}));
        subscription->port_ = createSubscriptionPort(types, true);

        return subscription;
    });
}

std::shared_ptr<Subscription<RegisteredEvent>> Feed::createSubscription(std::initializer_list<EventType> types) {
    return createSubscription(std::span(types.begin(), types.size()));
}

std::shared_ptr<Subscription<RegisteredEvent>> Feed::createTimeSeriesSubscription(std::span<const EventType> types) {
    return producerAction([&] {
        auto subscription = std::shared_ptr<Subscription<RegisteredEvent>>(new Subscription<RegisteredEvent>({}));
        subscription->port_ = createSubscriptionPort(types, true, true);

        return subscription;
    });
}

std::shared_ptr<Subscription<RegisteredEvent>>
Feed::createTimeSeriesSubscription(std::initializer_list<EventType> types) {
    return createTimeSeriesSubscription(std::span(types.begin(), types.size()));
}

std::shared_ptr<detail::SubscriptionPort> Feed::createSubscriptionPort(std::span<const EventType> types,
                                                                       bool registeredDelivery, bool timeSeries) {
    return producerAction([&] {
        if (types.size() > port_->limits.maximumEventTypeArgumentsPerSubscription) {
            reject(ErrorCode::RESOURCE_LIMIT, "subscription-event-type-arguments");
        }

        if (types.empty()) {
            reject(ErrorCode::INVALID_ARGUMENT, "subscription-event-types");
        }

        bool quote = false;
        bool profile = false;
        bool trade = false;
        bool tradeETH = false;
        bool summary = false;
        bool textMessage = false;
        bool textConfiguration = false;
        bool message = false;
        bool configuration = false;
        bool order = false;
        bool marketMaker = false;
        bool spreadOrder = false;
        bool analyticOrder = false;
        bool otcMarketsOrder = false;
        bool orderImbalance = false;
        bool series = false;
        bool greeks = false;
        bool underlying = false;
        bool theoPrice = false;
        bool candle = false;
        bool timeAndSale = false;
        bool optionSale = false;

        for (const auto type : types) {
            switch (type) {
            case EventType::QUOTE:
                quote = true;
                break;
            case EventType::PROFILE:
                profile = true;
                break;
            case EventType::TRADE:
                trade = true;
                break;
            case EventType::TRADE_ETH:
                tradeETH = true;
                break;
            case EventType::MESSAGE:
                message = true;
                break;
            case EventType::CONFIGURATION:
                configuration = true;
                break;
            case EventType::SUMMARY:
                summary = true;
                break;
            case EventType::TEXT_MESSAGE:
                textMessage = true;
                break;
            case EventType::TEXT_CONFIGURATION:
                textConfiguration = true;
                break;
            case EventType::ORDER:
                order = true;
                break;
            case EventType::MARKET_MAKER:
                marketMaker = true;
                break;
            case EventType::SPREAD_ORDER:
                spreadOrder = true;
                break;
            case EventType::ANALYTIC_ORDER:
                analyticOrder = true;
                break;
            case EventType::OTC_MARKETS_ORDER:
                otcMarketsOrder = true;
                break;
            case EventType::SERIES:
                series = true;
                break;
            case EventType::GREEKS:
                greeks = true;
                break;
            case EventType::UNDERLYING:
                underlying = true;
                break;
            case EventType::THEO_PRICE:
                theoPrice = true;
                break;
            case EventType::CANDLE:
                candle = true;
                break;
            case EventType::TIME_AND_SALE:
                timeAndSale = true;
                break;
            case EventType::OPTION_SALE:
                optionSale = true;
                break;
            case EventType::ORDER_IMBALANCE:
                orderImbalance = true;
                break;
            default:
                reject(ErrorCode::INVALID_ARGUMENT, "subscription-event-types");
            }
        }

        const auto selection = static_cast<detail::EventSelection>(
            (quote ? 1U : 0U) | (profile ? 2U : 0U) | (trade ? 4U : 0U) | (tradeETH ? 8U : 0U) | (summary ? 16U : 0U) |
            (textMessage ? 32U : 0U) | (textConfiguration ? 64U : 0U) | (message ? 128U : 0U) |
            (configuration ? 256U : 0U) | (order ? 512U : 0U) | (marketMaker ? 1024U : 0U) |
            (spreadOrder ? 1U << 11 : 0U) | (analyticOrder ? 1U << 12 : 0U) | (otcMarketsOrder ? 1U << 13 : 0U) |
            (orderImbalance ? 1U << 14 : 0U) | (series ? 1U << 15 : 0U) | (greeks ? 1U << 16 : 0U) |
            (underlying ? 1U << 17 : 0U) | (theoPrice ? 1U << 18 : 0U) | (candle ? 1U << 19 : 0U) |
            (timeAndSale ? 1U << 20 : 0U) | (optionSale ? 1U << 21 : 0U));
        const auto delivery = registeredDelivery  ? detail::EventDelivery::REGISTERED
                              : quote             ? detail::EventDelivery::QUOTE
                              : profile           ? detail::EventDelivery::PROFILE
                              : trade             ? detail::EventDelivery::TRADE
                              : tradeETH          ? detail::EventDelivery::TRADE_ETH
                              : summary           ? detail::EventDelivery::SUMMARY
                              : textMessage       ? detail::EventDelivery::TEXT_MESSAGE
                              : textConfiguration ? detail::EventDelivery::TEXT_CONFIGURATION
                              : message           ? detail::EventDelivery::MESSAGE
                              : marketMaker       ? detail::EventDelivery::MARKET_MAKER
                              : spreadOrder       ? detail::EventDelivery::SPREAD_ORDER
                              : analyticOrder     ? detail::EventDelivery::ANALYTIC_ORDER
                              : otcMarketsOrder   ? detail::EventDelivery::OTC_MARKETS_ORDER
                              : orderImbalance    ? detail::EventDelivery::ORDER_IMBALANCE
                              : series            ? detail::EventDelivery::SERIES
                              : greeks            ? detail::EventDelivery::GREEKS
                              : underlying        ? detail::EventDelivery::UNDERLYING
                              : theoPrice         ? detail::EventDelivery::THEO_PRICE
                              : candle            ? detail::EventDelivery::CANDLE
                              : timeAndSale       ? detail::EventDelivery::TIME_AND_SALE
                              : optionSale        ? detail::EventDelivery::OPTION_SALE
                              : order             ? detail::EventDelivery::ORDER
                                                  : detail::EventDelivery::CONFIGURATION;
        const auto control = port_->control.lock();

        if (!control)
            reject(ErrorCode::CLOSED, "runtime-closed");

        std::unique_lock policyGate(control->callbackPolicyMutex);
        auto subscription = std::make_shared<detail::SubscriptionPort>();
        subscription->statistics = std::make_shared<core::detail::DeliveryStatisticsState>();
        subscription->statistics->endpointAggregationPeriod = port_->options.aggregationPeriod;
        subscription->statistics->value.ownerIdentity = std::make_shared<const std::byte>();
        subscription->statistics->value.observedAt = std::chrono::steady_clock::now();
        subscription->history.ownerIdentity = subscription->statistics->value.ownerIdentity;
        subscription->control = port_->control;
        subscription->endpoint = port_;
        subscription->owner = port_->owner;
        subscription->eventTypes.assign(types.begin(), types.end());
        std::ranges::sort(subscription->eventTypes);
        const auto uniqueTypes = std::ranges::unique(subscription->eventTypes);
        subscription->eventTypes.erase(uniqueTypes.begin(), uniqueTypes.end());
        subscription->eventSelection = selection;
        subscription->eventDelivery = delivery;
        subscription->timeSeries = timeSeries;

        if (timeSeries)
            subscription->fromTime = TIME_SERIES_FROM_TIME_DISABLED;

        subscription->maximumSymbols = port_->limits.maximumSymbolsPerSubscription;
        subscription->maximumSymbolUnits = port_->limits.maximumSymbolUnits;
        subscription->listenerLedger = std::make_shared<detail::ListenerLedger>();
        subscription->listenerLedger->maximum = port_->limits.maximumListenersPerSubscription;
        subscription->listenerLedger->control = port_->control;
        subscription->listenerLedger->endpoint = port_;
        subscription->listeners = std::make_shared<const detail::ListenerSnapshot>();
        subscription->stateListeners = makeStateListeners(port_, port_->limits.maximumStateListenersPerSubscription);
        detail::initializeSubscriptionObservers(subscription, port_->limits);
        {
            std::lock_guard lock(port_->mutex);

            if (port_->closeRequested.load(std::memory_order_acquire) ||
                port_->retired.load(std::memory_order_acquire)) {
                reject(ErrorCode::CLOSED, "endpoint-closed");
            }

            if (timeSeries && (port_->options.contract != DeliveryContract::HISTORY ||
                               !std::ranges::all_of(subscription->eventTypes, isTimeSeriesEventType))) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-event-type");
            }

            // Business capability is explicit; an indexed layout alone does not make a TimeSeriesEvent.
            constexpr auto historyTypes = static_cast<unsigned>(detail::EventSelection::ORDER) |
                                          static_cast<unsigned>(detail::EventSelection::MARKET_MAKER) |
                                          static_cast<unsigned>(detail::EventSelection::SPREAD_ORDER) |
                                          static_cast<unsigned>(detail::EventSelection::ANALYTIC_ORDER) |
                                          static_cast<unsigned>(detail::EventSelection::OTC_MARKETS_ORDER) |
                                          static_cast<unsigned>(detail::EventSelection::SERIES) |
                                          static_cast<unsigned>(detail::EventSelection::GREEKS) |
                                          static_cast<unsigned>(detail::EventSelection::UNDERLYING) |
                                          static_cast<unsigned>(detail::EventSelection::THEO_PRICE) |
                                          static_cast<unsigned>(detail::EventSelection::CANDLE) |
                                          static_cast<unsigned>(detail::EventSelection::TIME_AND_SALE) |
                                          static_cast<unsigned>(detail::EventSelection::OPTION_SALE);

            if (port_->options.contract == DeliveryContract::HISTORY &&
                (static_cast<unsigned>(selection) & ~historyTypes)) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-history-event-type");
            }

            if ((order && port_->options.contract == DeliveryContract::STREAM) ||
                ((marketMaker || spreadOrder || analyticOrder || otcMarketsOrder) &&
                 port_->options.contract != DeliveryContract::HISTORY)) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "order-requires-history");
            }

            if (optionSale && port_->options.contract != DeliveryContract::HISTORY) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "option-sale-requires-history");
            }

            if (series && port_->options.contract != DeliveryContract::HISTORY) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "series-requires-history");
            }

            if ((greeks || underlying || theoPrice) && port_->options.contract == DeliveryContract::STREAM) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, greeks       ? "greeks-requires-ticker-or-history"
                                                       : underlying ? "underlying-requires-ticker-or-history"
                                                                    : "theo-price-requires-ticker-or-history");
            }

            if (candle && port_->options.contract == DeliveryContract::STREAM)
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-candle-contract");

            if (timeAndSale && port_->options.contract == DeliveryContract::TICKER)
                reject(ErrorCode::UNSUPPORTED_PROFILE, "time-and-sale-requires-stream-or-history");

            if (orderImbalance && port_->options.contract != DeliveryContract::TICKER) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "order-imbalance-requires-ticker");
            }

            if (message && port_->options.contract != DeliveryContract::STREAM) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "message-stream-contract");
            }

            if (textMessage && port_->options.contract != DeliveryContract::STREAM) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "text-message-requires-stream");
            }

            bool registeredQuote = !port_->options.eventRegistry;
            bool registeredProfile = !port_->options.eventRegistry;
            bool registeredTrade = !port_->options.eventRegistry;
            bool registeredTradeETH = !port_->options.eventRegistry;
            bool registeredSummary = !port_->options.eventRegistry;
            bool registeredTextMessage = !port_->options.eventRegistry;
            bool registeredTextConfiguration = !port_->options.eventRegistry;
            bool registeredMessage = !port_->options.eventRegistry;
            bool registeredConfiguration = !port_->options.eventRegistry;
            bool registeredOrder = !port_->options.eventRegistry && port_->options.contract != DeliveryContract::STREAM;
            bool registeredMarketMaker =
                !port_->options.eventRegistry && port_->options.contract == DeliveryContract::HISTORY;

            bool registeredSpreadOrder = registeredMarketMaker;
            bool registeredAnalyticOrder = registeredMarketMaker;
            bool registeredOtcMarketsOrder = registeredMarketMaker;
            bool registeredSeries = registeredMarketMaker;
            bool registeredGreeks = !port_->options.eventRegistry;
            bool registeredUnderlying = registeredGreeks;
            bool registeredTheoPrice = registeredGreeks;
            bool registeredTimeAndSale = !port_->options.eventRegistry;
            bool registeredOptionSale = registeredMarketMaker;
            bool registeredOrderImbalance =
                !port_->options.eventRegistry && port_->options.contract == DeliveryContract::TICKER;

            if (port_->options.eventRegistry) {
                for (const auto &registration : port_->options.eventRegistry->registrations()) {
                    if (!detail::receivingRouteSupportsContract(registration, port_->options.contract)) {
                        continue;
                    }

                    registeredMarketMaker = registeredMarketMaker || registration.type == EventType::MARKET_MAKER;
                    registeredSpreadOrder = registeredSpreadOrder || registration.type == EventType::SPREAD_ORDER;
                    registeredAnalyticOrder = registeredAnalyticOrder || registration.type == EventType::ANALYTIC_ORDER;
                    registeredOtcMarketsOrder =
                        registeredOtcMarketsOrder || registration.type == EventType::OTC_MARKETS_ORDER;
                    registeredOrderImbalance =
                        registeredOrderImbalance || registration.type == EventType::ORDER_IMBALANCE;
                    registeredSeries = registeredSeries || registration.type == EventType::SERIES;
                    registeredGreeks = registeredGreeks || registration.type == EventType::GREEKS;
                    registeredUnderlying = registeredUnderlying || registration.type == EventType::UNDERLYING;
                    registeredTheoPrice = registeredTheoPrice || registration.type == EventType::THEO_PRICE;
                    registeredTimeAndSale = registeredTimeAndSale || registration.type == EventType::TIME_AND_SALE;
                    registeredOptionSale = registeredOptionSale || registration.type == EventType::OPTION_SALE;
                    registeredMessage = registeredMessage || registration.type == EventType::MESSAGE;
                    registeredConfiguration = registeredConfiguration || registration.type == EventType::CONFIGURATION;
                    registeredOrder = registeredOrder || registration.type == EventType::ORDER;
                    registeredQuote = registeredQuote || registration.type == EventType::QUOTE;
                    registeredProfile = registeredProfile || registration.type == EventType::PROFILE;
                    registeredTrade = registeredTrade || registration.type == EventType::TRADE;
                    registeredTradeETH = registeredTradeETH || registration.type == EventType::TRADE_ETH;
                    registeredSummary = registeredSummary || registration.type == EventType::SUMMARY;
                    registeredTextMessage = registeredTextMessage || registration.type == EventType::TEXT_MESSAGE;
                    registeredTextConfiguration =
                        registeredTextConfiguration || registration.type == EventType::TEXT_CONFIGURATION;
                }
            }

            const auto missingQuote = quote && !registeredQuote;
            const auto missingProfile = profile && !registeredProfile;

            if (missingQuote || missingProfile || (trade && !registeredTrade) || (tradeETH && !registeredTradeETH) ||
                (summary && !registeredSummary) || (textMessage && !registeredTextMessage) ||
                (textConfiguration && !registeredTextConfiguration) || (message && !registeredMessage) ||
                (configuration && !registeredConfiguration) || (order && !registeredOrder) ||
                (marketMaker && !registeredMarketMaker) || (spreadOrder && !registeredSpreadOrder) ||
                (analyticOrder && !registeredAnalyticOrder) || (otcMarketsOrder && !registeredOtcMarketsOrder) ||
                (orderImbalance && !registeredOrderImbalance) || (series && !registeredSeries) ||
                (greeks && !registeredGreeks) || (underlying && !registeredUnderlying) ||
                (theoPrice && !registeredTheoPrice) || (timeAndSale && !registeredTimeAndSale) ||
                (optionSale && !registeredOptionSale)) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-unregistered-event");
            }

            if (candle) {
                subscription->candleRoutes =
                    detail::prepareReceivingCandleRoutes(port_->options, subscription->maximumSymbolUnits);

                if (!subscription->candleRoutes)
                    reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-unregistered-event");
            }

            const auto free = std::ranges::find_if(port_->subscriptions, [](const auto &item) {
                return !item || item->retired.load(std::memory_order_acquire);
            });

            if (free == port_->subscriptions.end()) {
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-subscription-count");
            }

            if (port_->eventExecutor) {
                auto pool = callbackPool(*control, port_->limits);

                if (!pool->canReserve(1))
                    reject(ErrorCode::RESOURCE_LIMIT, "callback-packet-budget");

                reserveCallbackPolicy(*subscription, pool, true);
            }

            *free = subscription;
        }
        policyGate.unlock();
        wake(port_->control);

        return subscription;
    });
}

template <class Event> void Subscription<Event>::setEventExecutor(std::shared_ptr<CallbackExecutor> executor) {
    producerAction([&] {
        const auto control = port_->control.lock();
        const auto endpoint = port_->endpoint.lock();

        if (!control || !endpoint)
            reject(ErrorCode::CLOSED, "callback-policy-closed");

        std::shared_ptr<CallbackExecutor> previous;
        {
            std::lock_guard gate(control->callbackPolicyMutex);
            std::lock_guard endpointLock(endpoint->mutex);
            std::lock_guard lock(port_->mutex);

            if (port_->closeRequested.load(std::memory_order_acquire) ||
                endpoint->closeRequested.load(std::memory_order_acquire))
                reject(ErrorCode::CLOSED, "callback-policy-closed");

            const bool custom = executor || endpoint->eventExecutor;
            auto pool = custom ? callbackPool(*control, endpoint->limits) : port_->callbackPool;

            if (custom && (!port_->callbackPool || !pool->reserved(port_->callbackSlot)) && !pool->canReserve(1))
                reject(ErrorCode::RESOURCE_LIMIT, "callback-packet-budget");

            reserveCallbackPolicy(*port_, pool, custom);
            previous = std::exchange(port_->eventExecutor, std::move(executor));
        }
        previous.reset();
    });
}

template <class Event> std::shared_ptr<CallbackExecutor> Subscription<Event>::getEventExecutor() const {
    std::lock_guard lock(port_->mutex);

    return port_->eventExecutor;
}

template <class Event> Diagnostic Subscription<Event>::getCallbackDispatchDiagnostic() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->callbackDispatchDiagnostic;
}

void Endpoint::setEventExecutor(std::shared_ptr<CallbackExecutor> executor) {
    producerAction([&] {
        const auto control = port_->control.lock();

        if (!control)
            reject(ErrorCode::CLOSED, "callback-policy-closed");

        std::shared_ptr<CallbackExecutor> previous;
        {
            std::lock_guard gate(control->callbackPolicyMutex);
            std::lock_guard endpointLock(port_->mutex);

            if (port_->closeRequested.load(std::memory_order_acquire))
                reject(ErrorCode::CLOSED, "callback-policy-closed");

            auto pool = executor ? callbackPool(*control, port_->limits) : control->callbackPool;
            std::size_t additional = 0;

            for (const auto &sub : port_->subscriptions) {
                if (!sub || sub->retired.load(std::memory_order_acquire))
                    continue;

                std::lock_guard lock(sub->mutex);

                if (executor && !sub->eventExecutor && (!sub->callbackPool || !pool->reserved(sub->callbackSlot)))
                    ++additional;
            }

            if (executor && !pool->canReserve(additional))
                reject(ErrorCode::RESOURCE_LIMIT, "callback-packet-budget");

            for (const auto &sub : port_->subscriptions) {
                if (!sub || sub->retired.load(std::memory_order_acquire))
                    continue;

                std::lock_guard lock(sub->mutex);

                if (!sub->eventExecutor)
                    reserveCallbackPolicy(*sub, pool, executor != nullptr);
            }

            previous = std::exchange(port_->eventExecutor, std::move(executor));
        }
        previous.reset();
    });
}

std::shared_ptr<CallbackExecutor> Endpoint::getEventExecutor() const {
    std::lock_guard lock(port_->mutex);

    return port_->eventExecutor;
}

template class Subscription<events::Quote>;
template class Subscription<events::Profile>;
template class Subscription<events::Trade>;
template class Subscription<events::TradeETH>;
template class Subscription<events::Summary>;
template class Subscription<events::TextMessage>;
template class Subscription<events::TextConfiguration>;
template class Subscription<events::Message>;
template class Subscription<events::Configuration>;
template class Subscription<events::Order>;
template class Subscription<events::MarketMaker>;
template class Subscription<events::SpreadOrder>;
template class Subscription<events::AnalyticOrder>;
template class Subscription<events::OtcMarketsOrder>;
template class Subscription<events::OrderImbalance>;
template class Subscription<events::Series>;
template class Subscription<events::Greeks>;
template class Subscription<events::Underlying>;
template class Subscription<events::TheoPrice>;
template class Subscription<events::Candle>;
template class Subscription<events::TimeAndSale>;
template class Subscription<events::OptionSale>;
template class Subscription<RegisteredEvent>;

Feed::Feed(std::shared_ptr<detail::EndpointPort> port) : port_(std::move(port)) {
}

Endpoint::Endpoint(std::shared_ptr<detail::EndpointPort> port) : port_(std::move(port)), feed_(new Feed(port_)) {
}

Endpoint::StateChangeListenerId Endpoint::addStateChangeListener(StateChangeListener listener) {
    return addStateListener(port_, std::move(listener));
}

void Endpoint::removeStateChangeListener(StateChangeListenerId id) {
    removeStateListener(port_, id);
}

Diagnostic Endpoint::getStateChangeDiagnostic() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->stateListeners.diagnostic;
}

Endpoint::~Endpoint() {
    close();
}

std::shared_ptr<Feed> Endpoint::getFeed() const noexcept {
    return feed_;
}

FeedFilter Endpoint::createFilter(std::span<const FeedFilterCategory> categories,
                                  std::shared_ptr<FeedFilterContext> context) const {
    return producerAction([&] {
        std::optional<EventRegistry> registry;
        DeliveryContract contract;
        {
            std::lock_guard lock(port_->mutex);
            registry = port_->options.eventRegistry;
            contract = port_->options.contract;
        }

        if (!registry) {
            auto prepared = detail::defaultReceivingRegistry(contract);

            if (!prepared) {
                auto code = ErrorCode::UNSUPPORTED_PROFILE;

                if (const auto *own = std::get_if<EventRegistryError>(&prepared.error().cause)) {
                    code = *own == EventRegistryError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                           : *own == EventRegistryError::WORK_LIMIT       ? ErrorCode::WORK_LIMIT
                           : *own == EventRegistryError::RESOURCE_LIMIT   ? ErrorCode::RESOURCE_LIMIT
                                                                          : ErrorCode::INVALID_ARGUMENT;
                }

                throw Error({code, "endpoint-filter-registry"});
            }

            registry = std::move(*prepared);
        }

        FeedFilter::Builder builder(std::move(*registry), contract);
        builder.withResourceContext(std::move(context));

        for (const auto &category : categories) {
            builder.withCategory(category.name, category.expression);
        }

        return builder.build();
    });
}

void Endpoint::connect(std::string_view address) {
    producerAction([&] {
        auto parsed = detail::parseEndpointAddress(address);
        const auto control = port_->control.lock();

        if (!control) {
            reject(ErrorCode::CLOSED, "runtime-closed");
        }

        {
            // Pins worker creation and dispatch lifetime; all fallible preparation precedes publication.
            std::lock_guard gate(control->mutex);

            if (!control->runtime || control->stopping) {
                reject(ErrorCode::CLOSED, "runtime-closed");
            }

            std::lock_guard lock(port_->mutex);

            if (port_->closeRequested.load(std::memory_order_acquire) ||
                port_->retired.load(std::memory_order_acquire) || port_->state == EndpointState::FAILED) {
                reject(ErrorCode::INVALID_STATE, "endpoint-connect-state");
            }

            if (port_->connectRequested) {
                if (port_->address != parsed.spelling) {
                    reject(ErrorCode::INVALID_STATE, "endpoint-address-fixed");
                }

                if (port_->connectionIntent) {
                    return;
                }
            }

            if (port_->connectionRevision == std::numeric_limits<std::uint64_t>::max()) {
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-control-identity");
            }

            if (port_->state != EndpointState::CONNECTING && port_->stateChangeCount == port_->stateChanges.size()) {
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-state-notification-count");
            }

            control->runtime->startWorker();

            if (!port_->connectRequested) {
                port_->address = std::move(parsed.spelling);
                port_->host = std::move(parsed.host);
                port_->port = parsed.port;
                port_->connectRequested = true;
            }

            ++port_->connectionRevision;
            port_->connectionIntent = true;

            if (port_->state != EndpointState::CONNECTING) {
                queueStateChange(*port_, EndpointState::CONNECTING);
            }

            ++control->wakeSequence;
        }

        port_->stateCondition.notify_all();
        control->condition.notify_one();
    });
}

std::optional<std::chrono::milliseconds> Endpoint::getRequestedAggregationPeriod() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->requestedAggregationPeriod;
}

void Endpoint::setRequestedAggregationPeriod(std::optional<std::chrono::milliseconds> period) {
    producerAction([&] {
        if (period && (period->count() < 0 || *period > transport::MAXIMUM_REQUESTED_AGGREGATION_PERIOD))
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-aggregation-period");

        {
            std::lock_guard lock(port_->mutex);

            if (port_->requestedAggregationPeriod == period)
                return;

            if (port_->requestedAggregationRevision == std::numeric_limits<std::uint64_t>::max())
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-aggregation-revision");

            port_->requestedAggregationPeriod = period;
            ++port_->requestedAggregationRevision;
        }

        if (const auto control = port_->control.lock()) {
            {
                std::lock_guard gate(control->mutex);
                ++control->wakeSequence;
            }
            control->condition.notify_one();
        }
    });
}

std::optional<transport::AggregationPeriodInfo> Endpoint::getAggregationPeriodInfo() const noexcept {
    std::lock_guard lock(port_->mutex);

    if (!port_->connectionIntent || port_->closeRequested.load(std::memory_order_acquire) ||
        port_->retired.load(std::memory_order_acquire) || port_->state == EndpointState::FAILED ||
        port_->aggregationSourceRevision != port_->connectionRevision)
        return {};

    return port_->aggregationInfo;
}

void Endpoint::connect() {
    producerAction([&] {
        const auto configuration = port_->configuration->snapshot();

        if (!configuration.configuredAddress) {
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address-not-configured");
        }

        connect(*configuration.configuredAddress);
    });
}

EndpointConfigurationSnapshot Endpoint::getEffectiveConfiguration() const {
    return producerAction([&] {
        auto snapshot = port_->configuration->snapshot();
        std::lock_guard lock(port_->mutex);

        if (port_->connectRequested) {
            snapshot.acceptedAddress = port_->address;
        }

        snapshot.activationRequested =
            port_->connectionIntent && !port_->closeRequested.load(std::memory_order_acquire) &&
            !port_->retired.load(std::memory_order_acquire) && port_->state != EndpointState::FAILED;
        snapshot.connectionRevision = port_->connectionRevision;
        snapshot.requestedAggregationRevision = port_->requestedAggregationRevision;

        if (port_->requestedAggregationRevision != 0) {
            for (auto &property : snapshot.properties) {
                if (property.key == "dxfeed.requestedAggregationPeriod") {
                    property.requestedValue = detail::aggregationPropertyValue(port_->requestedAggregationPeriod);
                    property.effectiveValue = property.requestedValue;
                    property.origin = EndpointPropertyOrigin::RUNTIME_UPDATE;
                }
            }
        }

        return snapshot;
    });
}

void Endpoint::disconnect() {
    requestConnectionChange(port_, ConnectionRequest::DISCONNECT);
}

void Endpoint::disconnectAndClear() {
    requestConnectionChange(port_, ConnectionRequest::CLEAR);
}

void Endpoint::reconnect() {
    requestConnectionChange(port_, ConnectionRequest::RECONNECT);
}

void Endpoint::close() noexcept {
    requestEndpointClose(port_);
    wake(port_->control);
}

CloseResult Endpoint::closeAndAwaitTermination() {
    const auto control = port_->control.lock();

    if (std::this_thread::get_id() != port_->owner) {
        reject(ErrorCode::WRONG_THREAD, "endpoint-await-thread");
    }

    if (retirementReentered(port_.get()) || detail::subscriptionObserverCallbackActive()) {
        reject(ErrorCode::INVALID_STATE, "endpoint-retirement-reentry");
    }

    close();

    if (control) {
        std::unique_lock gate(control->mutex);

        if (control->runtime && !control->runtime->worker.joinable()) {
            port_->retirementOnMain.store(true, std::memory_order_release);
            bool expected = false;
            port_->retirementClaimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
            gate.unlock();
            finishInactiveRetirement(port_);
        }
    }

    std::unique_lock lock(port_->mutex);
    port_->retiredCondition.wait(lock, [&] {
        return port_->retired.load(std::memory_order_acquire);
    });

    return port_->terminal;
}

EndpointState Endpoint::getState() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->state;
}

EndpointStatistics Endpoint::getStatistics() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->statistics;
}

StateWaitResult<EndpointState>
Endpoint::awaitState(EndpointState target, std::chrono::steady_clock::time_point deadline, std::stop_token stop) const {
    return awaitCurrentState(port_, target, deadline, stop);
}

Diagnostic Endpoint::getDiagnostic() const noexcept {
    std::lock_guard lock(port_->mutex);

    return port_->diagnostic;
}

namespace detail {
namespace {
/** Finite fair owner actions before a cadence wait, identical for normal operation and local drain.
 * All polls in this burst share one unchanged RecoveryTurnWork seed; no exhausted coordinate is refilled.
 */
constexpr std::size_t MAXIMUM_RECOVERY_POLLS_PER_TURN = RECEIVING_OWNER_POLLS_PER_TURN;

/** Receiver adapter projects the entire typed batch before selecting any application callback. */
class TypedConsumer final : public client::SubscriptionConsumer {
    std::shared_ptr<SubscriptionPort> port_; ///< Detached state; callback capture cycles end at runtime retirement.
    client::ReceiverProjectionWork projectionWork_; ///< Finite whole-callback seed, never shared with queue work.

    enum class DispatchOutcome { DISCARDED, DELIVERED, CALLBACK_FAILED };

    /** Finalize selected events after all adapter-local callback captures and callback scopes have retired. */
    struct DispatchCompletion {
        std::shared_ptr<core::detail::DeliveryStatisticsState> state;
        std::size_t events;
        DispatchOutcome outcome = DispatchOutcome::DISCARDED;
        bool active = true;
        std::shared_ptr<SubscriptionPort> synchronousPort;

        void finish() noexcept {
            if (!std::exchange(active, false))
                return;

            state->update([&](auto &s) {
                s.inFlightEvents -= events;
                auto &counter = outcome == DispatchOutcome::DELIVERED         ? s.deliveredEvents
                                : outcome == DispatchOutcome::CALLBACK_FAILED ? s.callbackFailedEvents
                                                                              : s.discardedEvents;
                counter.add(events);
            });

            if (synchronousPort) {
                std::lock_guard lock(synchronousPort->mutex);
                synchronousPort->callbackSynchronousExecuting = false;

                if (synchronousPort->callbackSynchronousRetirementRevision != UINT64_MAX)
                    ++synchronousPort->callbackSynchronousRetirementRevision;
            }
        }

        ~DispatchCompletion() {
            finish();
        }
    };

    template <class Event>
    static std::vector<Event> project(const client::SubscriptionBatch &batch,
                                      client::ReceiverProjectionWork projectionWork) {
        std::vector<Event> projectedEvents;
        projectedEvents.reserve(batch.eventCount());
        auto work = projectionWork;

        for (std::size_t index = 0; index < batch.records().size(); ++index) {
            auto group = batch.projectAll(index, work);

            if (!group) {
                throw Error({ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});
            }

            for (std::size_t output = 0; output < group->size; ++output) {
                auto &projected = group->values[output];

                if constexpr (std::is_same_v<Event, RegisteredEvent>) {
                    std::visit(
                        [&](auto &&value) {
                            using Value = std::remove_cvref_t<decltype(value)>;

                            if constexpr (std::is_same_v<Value, std::monostate>) {
                                throw Error({ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});
                            } else {
                                projectedEvents.emplace_back(std::move(value));
                            }
                        },
                        std::move(projected.value));
                } else {
                    if (!std::holds_alternative<Event>(projected.value)) {
                        throw Error({ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});
                    }

                    projectedEvents.push_back(std::get<Event>(std::move(projected.value)));
                }
            }
        }

        return projectedEvents;
    }

    template <class Event>
    static mux::BatchAction invoke(const std::shared_ptr<SubscriptionPort> &port_,
                                   const std::vector<Event> &projectedEvents,
                                   const std::shared_ptr<const ListenerSnapshot> &listeners,
                                   DispatchCompletion &completion, bool asynchronous = false) {
        // One selection lasts through the whole underlying batch. Reentrant mutation affects a later batch.
        bool complete = true;

        bool began = false;

        for (const auto &listener : *listeners) {
            if (port_->closeRequested.load(std::memory_order_acquire)) {
                complete = false;
                break;
            }

            if (!began) {
                std::lock_guard lock(port_->statistics->mutex);
                port_->statistics->deliveredAggregationPeriod = port_->statistics->selectedAggregationPeriod;

                if (!port_->statistics->selectedAggregationSnapshotBypass)
                    port_->statistics->nextAggregationDelivery =
                        std::chrono::steady_clock::now() + port_->statistics->selectedAggregationPeriod;

                began = true;
            }

            port_->statistics->update([](auto &s) {
                s.callbackCalls.add(1);
            });
            const auto started = std::chrono::steady_clock::now();
            bool failed = false;
            try {
                std::get<typename Subscription<Event>::Listener>(listener.callback->function)(projectedEvents);
            } catch (...) {
                failed = true;
            }
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                    .count();
            port_->statistics->update([&](auto &s) {
                if (failed)
                    s.callbackFailures.add(1);

                const auto duration = static_cast<std::uint64_t>(std::max<decltype(elapsed)>(0, elapsed));

                if (s.callbackDurationTotal.availability == core::StatisticAvailability::NO_SAMPLE)
                    s.callbackDurationTotal.availability = core::StatisticAvailability::AVAILABLE;

                s.callbackDurationTotal.add(duration);

                if (s.callbackDurationMaximum.availability == core::StatisticAvailability::NO_SAMPLE)
                    s.callbackDurationMaximum.availability = core::StatisticAvailability::AVAILABLE;

                s.callbackDurationMaximum.value = std::max(s.callbackDurationMaximum.value, duration);
                s.callbackDurationSamples.add(1);
            });

            if (failed) {
                completion.outcome = DispatchOutcome::CALLBACK_FAILED;

                if (asynchronous)
                    requestSubscriptionFailure(port_, {ErrorCode::CALLBACK_FAILURE, "subscription-listener"});
                else
                    failSubscription(port_, {ErrorCode::CALLBACK_FAILURE, "subscription-listener"});

                return mux::BatchAction::PAUSE;
            }
        }

        if (complete)
            completion.outcome = DispatchOutcome::DELIVERED;

        return mux::BatchAction::CONTINUE;
    }

    /** One HISTORY invocation claim, distinct from task execution and close admission. */
    enum class HistoryBegin { CURRENT, OBSOLETE, CLOSED };

    /** @param port Detached owner; its private delivery intent shares the command-publication mutex. */
    static void requireHistoryReplay(SubscriptionPort &port) noexcept {
        std::lock_guard lock(port.statistics->mutex);
        port.statistics->historyReplayRequired = true;
    }

    /** Claim one complete HISTORY invocation against accepted state, outside every pool lock.
     * @param port Detached subscription; the mutex defines BEGIN before any user instruction.
     * @param revision Immutable actually applied selection, absent for unchanged TICKER/STREAM semantics.
     * @return CURRENT permits invocation; OBSOLETE requests replay; CLOSED preserves the close fence.
     */
    static HistoryBegin claimHistoryBegin(SubscriptionPort &port, std::optional<std::uint64_t> revision) {
        if (!revision)
            return HistoryBegin::CURRENT;

        std::lock_guard lock(port.mutex);

        if (port.closeRequested.load(std::memory_order_acquire))
            return HistoryBegin::CLOSED;

        if (port.revision != *revision) {
            requireHistoryReplay(port);

            return HistoryBegin::OBSOLETE;
        }

        return HistoryBegin::CURRENT;
    }

    template <class Event> struct Invocation final : CallbackInvocation {
        std::shared_ptr<SubscriptionPort> port;
        std::shared_ptr<const ListenerSnapshot> listeners;
        std::shared_ptr<CallbackExecutor> executor;
        const client::SubscriptionBatch *batch;
        client::ReceiverProjectionWork projectionWork;
        std::vector<Event> events;
        DispatchCompletion completion;
        const std::optional<std::uint64_t> historyRevision;
        bool obsolete = false;

        Invocation(std::shared_ptr<SubscriptionPort> port, std::shared_ptr<const ListenerSnapshot> listeners,
                   std::shared_ptr<CallbackExecutor> executor, const client::SubscriptionBatch &batch,
                   client::ReceiverProjectionWork work, std::optional<std::uint64_t> historyRevision)
            : port(std::move(port)), listeners(std::move(listeners)), executor(std::move(executor)), batch(&batch),
              projectionWork(work), completion{this->port->statistics, batch.eventCount()},
              historyRevision(historyRevision) {
        }

        void prepare() override {
            events = project<Event>(*batch, projectionWork);
            batch = nullptr;
        }

        CallbackPostResult post(CallbackTask task) override {
            return executor->post(std::move(task));
        }

        void execute() noexcept override {
            const auto begin = claimHistoryBegin(*port, historyRevision);
            obsolete = begin == HistoryBegin::OBSOLETE;

            if (begin == HistoryBegin::CURRENT)
                static_cast<void>(invoke<Event>(port, events, listeners, completion, true));
        }

        void finish(Diagnostic diagnostic) noexcept override {
            if (obsolete && diagnostic.code == ErrorCode::NONE)
                diagnostic = {ErrorCode::CALLBACK_CANCELLED, "callback-history-revision"};

            if (historyRevision && diagnostic.code == ErrorCode::CALLBACK_CANCELLED &&
                diagnostic.stage == "callback-history-revision")
                requireHistoryReplay(*port);

            if (diagnostic.code != ErrorCode::NONE) {
                {
                    std::lock_guard lock(port->mutex);
                    port->callbackDispatchDiagnostic = diagnostic;
                }

                if (diagnostic.code == ErrorCode::CALLBACK_CANCELLED) {
                    if (auto endpoint = port->endpoint.lock()) {
                        std::lock_guard lock(endpoint->mutex);
                        endpoint->terminal.hadGap = endpoint->terminal.hadGap || completion.events != 0;
                    }
                } else {
                    requestSubscriptionFailure(port, diagnostic);
                }
            }

            listeners.reset();
            executor.reset();
            events.clear();
            completion.finish();
        }
    };

    template <class Event>
    mux::BatchAction deliver(const client::SubscriptionBatch &batch, std::shared_ptr<const ListenerSnapshot> &listeners,
                             std::shared_ptr<CallbackExecutor> &executor, DispatchCompletion &completion,
                             std::optional<std::uint64_t> historyRevision) {
        if (executor) {
            auto packet = std::make_unique<Invocation<Event>>(port_, std::move(listeners), std::move(executor), batch,
                                                              projectionWork_, historyRevision);
            completion.active = false; // The packet now owns the exact selected statistics completion.
            port_->callbackPool->submit(port_->callbackSlot, std::move(packet));

            return mux::BatchAction::PAUSE;
        }

        auto values = project<Event>(batch, projectionWork_);

        if (claimHistoryBegin(*port_, historyRevision) != HistoryBegin::CURRENT)
            return mux::BatchAction::CONTINUE;

        return invoke<Event>(port_, values, listeners, completion);
    }

    public:
    /** Retain only detached state and an explicit complete callback projection profile.
     * @param port Detached logical subscription.
     * @param projectionWork Finite independently replenished callback-boundary seed.
     */
    TypedConsumer(std::shared_ptr<SubscriptionPort> port, client::ReceiverProjectionWork projectionWork)
        : SubscriptionConsumer(port->statistics), port_(std::move(port)), projectionWork_(projectionWork) {
    }

    /** Invoke ready owning values with no control mutex held.
     * @param batch Existing callback-scoped generic/projection view.
     * @return Queue action; application exceptions fail only this logical subscription.
     */
    mux::BatchAction onBatch(const client::SubscriptionBatch &batch) override {
        std::optional<SubscriptionDataCallbackScope> callbackScope;
        DispatchCompletion completion{port_->statistics, batch.eventCount()};
        {
            std::lock_guard lock(port_->mutex);
            port_->callbackSynchronousExecuting = true;
            completion.synchronousPort = port_;
        }
        const auto endpoint = port_->endpoint.lock();

        if (port_->closeRequested.load(std::memory_order_acquire) || !endpoint) {
            return mux::BatchAction::PAUSE;
        }

        std::shared_ptr<const ListenerSnapshot> listeners;
        std::optional<std::uint64_t> historyRevision;
        std::shared_ptr<CallbackExecutor> executor;
        bool customPrepared = false;
        Diagnostic preparationDiagnostic;
        {
            // Endpoint-to-subscription order makes the whole selection atomic with producer source/clear fences.
            std::lock_guard endpointLock(endpoint->mutex);

            const bool currentSource =
                endpoint->connectionIntent && endpoint->activeDeliveryRevision == endpoint->connectionRevision;
            const bool retainedCache =
                !endpoint->connectionIntent && endpoint->pausedReplayFence == endpoint->connectionRevision;

            if ((!currentSource && !retainedCache) || endpoint->clearRevision > endpoint->clearedRevision) {
                endpoint->terminal.hadGap = endpoint->terminal.hadGap || !batch.records().empty();

                // Consume an obsolete offer without permanently pausing this still-live stable registration.
                return mux::BatchAction::CONTINUE;
            }

            std::lock_guard lock(port_->mutex);

            if (endpoint->options.contract == DeliveryContract::HISTORY &&
                batch.appliedDesiredRevision() != port_->revision) {
                if (!port_->closeRequested.load(std::memory_order_acquire) && port_->listeners &&
                    !port_->listeners->empty() && !batch.records().empty())
                    requireHistoryReplay(*port_);

                return mux::BatchAction::CONTINUE;
            }

            if (endpoint->options.contract == DeliveryContract::HISTORY)
                historyRevision = batch.appliedDesiredRevision();

            listeners = port_->listeners;
            executor = port_->eventExecutor ? port_->eventExecutor : endpoint->eventExecutor;
            const auto customRevision =
                port_->callbackPool ? port_->callbackPool->snapshot(port_->callbackSlot).retirementRevision : 0;

            if (customRevision >= UINT64_MAX - port_->callbackSynchronousRetirementRevision) {
                preparationDiagnostic = {ErrorCode::RESOURCE_LIMIT, "callback-retirement-revision"};
            } else if (executor && listeners && !listeners->empty()) {
                try {
                    port_->callbackPool->begin(port_->callbackSlot,
                                               {port_->changes, &endpoint->callbackFence, port_->control});
                    // Transfer one selected completion without a false-quiescent intermediate cut.
                    port_->callbackSynchronousExecuting = false;
                    completion.synchronousPort.reset();
                    customPrepared = true;
                    port_->callbackHistoryRevision = historyRevision;
                } catch (const Error &error) {
                    preparationDiagnostic = error.diagnostic();
                }
            }
        }

        if (!customPrepared) {
            callbackScope.emplace(port_->changes);
        }

        if (preparationDiagnostic.code != ErrorCode::NONE) {
            failSubscription(port_, preparationDiagnostic);

            return mux::BatchAction::PAUSE;
        }

        if (!listeners || listeners->empty()) {
            return mux::BatchAction::CONTINUE;
        }

        try {
            switch (port_->eventDelivery) {
            case EventDelivery::QUOTE:
                return deliver<events::Quote>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::PROFILE:
                return deliver<events::Profile>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::TRADE:
                return deliver<events::Trade>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::TRADE_ETH:
                return deliver<events::TradeETH>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::MESSAGE:
                return deliver<events::Message>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::CONFIGURATION:
                return deliver<events::Configuration>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::SUMMARY:
                return deliver<events::Summary>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::MARKET_MAKER:
                return deliver<events::MarketMaker>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::SPREAD_ORDER:
                return deliver<events::SpreadOrder>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::ANALYTIC_ORDER:
                return deliver<events::AnalyticOrder>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::OTC_MARKETS_ORDER:
                return deliver<events::OtcMarketsOrder>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::SERIES:
                return deliver<events::Series>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::GREEKS:
                return deliver<events::Greeks>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::UNDERLYING:
                return deliver<events::Underlying>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::THEO_PRICE:
                return deliver<events::TheoPrice>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::CANDLE:
                return deliver<events::Candle>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::TIME_AND_SALE:
                return deliver<events::TimeAndSale>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::OPTION_SALE:
                return deliver<events::OptionSale>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::ORDER_IMBALANCE:
                return deliver<events::OrderImbalance>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::ORDER:
                return deliver<events::Order>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::TEXT_MESSAGE:
                return deliver<events::TextMessage>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::TEXT_CONFIGURATION:
                return deliver<events::TextConfiguration>(batch, listeners, executor, completion, historyRevision);
            case EventDelivery::REGISTERED:
                return deliver<RegisteredEvent>(batch, listeners, executor, completion, historyRevision);
            default:
                throw Error({ErrorCode::UNSUPPORTED_PROFILE, "subscription-event-type"});
            }
        } catch (const std::bad_alloc &) {
            failSubscription(port_, {ErrorCode::ALLOCATION_FAILURE, "subscription-projection"});
        } catch (...) {
            failSubscription(port_, {ErrorCode::RESOURCE_LIMIT, "subscription-projection"});
        }

        if (customPrepared && completion.active) {
            callbackScope.emplace(port_->changes);
            listeners.reset();
            executor.reset();
            completion.finish();
            port_->callbackPool->abortPreparation(port_->callbackSlot);
        }

        return mux::BatchAction::PAUSE;
    }
};

/** Actual logical registration; created, mutated and destroyed only on the SDK worker. */
struct NativeSubscription {
    std::shared_ptr<SubscriptionPort> port; ///< Detached producer state kept alive through callback return.
    std::shared_ptr<client::SubscriptionConsumer> consumer; ///< Original complete receiver callable.
    mux::MuxHandle handle;               ///< Existing registration, not an ad hoc filtering callback.
    mux::MuxCommandTicket ticket;        ///< At most one pending real desired replacement.
    std::uint64_t submittedRevision = 0; ///< Frontend revision associated with the actual ticket.
    bool hasSubmitted = false;           ///< Distinguishes initial empty desire from an unsubmitted registration.
    bool stopping = false;               ///< A reserved registration cutoff was already forwarded.
};

/** Worker-local native endpoint. Member order keeps preparation alive beyond its recovery owners. */
struct NativeEndpoint {
    std::shared_ptr<EndpointPort> port; ///< Detached producer state, independent of public facade destruction.
    ReceivingProfile profile;           ///< Complete immutable configuration and fixed work seeds.
    std::optional<transport::TcpRecovery> recovery; ///< Sole native/protocol/queue owner on this worker.
    std::vector<std::unique_ptr<NativeSubscription>> subscriptions; ///< Fixed native registration slots.
    bool draining = false;                                          ///< Actual admitted-prefix drain requested once.
    std::uint64_t appliedRevision = 0;            ///< Latest physical control captured/applied on this worker.
    std::uint64_t appliedAggregationRevision = 0; ///< Latest intent copied into the fixed recovery slot.
    bool aggregationConfigured = false;           ///< Initial default/reset is also an actual protocol property.
    std::uint64_t appliedClearRevision = 0;       ///< Sticky clear included in that physical control.
    std::uint32_t actualAttempts = 0; ///< Last actual factory-entry count, never inferred from desired state.

    /** Adopt nonnative immutable preparation on the worker.
     * @param port Detached endpoint route/policy.
     * @param profile Complete receiving preparation.
     */
    NativeEndpoint(std::shared_ptr<EndpointPort> port, ReceivingProfile profile)
        : port(std::move(port)), profile(std::move(profile)),
          subscriptions(this->port->limits.maximumSubscriptionsPerEndpoint) {
    }
};

/** Create all low-level owners on the actual execution thread, then request explicit start.
 * @param bootstrap Main-owned lifetime, borrowed through all worker-native retirement.
 * @param port Detached route and immutable public settings.
 * @return Complete native endpoint or detached Error; no native object escapes the worker.
 */
std::unique_ptr<NativeEndpoint> activate(const transport::detail::BootstrapLease &bootstrap,
                                         const std::shared_ptr<EndpointPort> &port) {
    auto profile = prepareReceivingProfile(port->limits, port->options, port->host, port->port);
    auto native = std::make_unique<NativeEndpoint>(port, std::move(profile));
    auto queueWork = native->profile.queueWork;
    auto queue = createReceivingQueue(native->profile, queueWork);

    if (!queue) {
        throw Error(queueDiagnostic(queue.error().error, "endpoint-queue"));
    }

    transport::RecoveryQueues queues;

    if (native->profile.family == core::SubscriptionFamily::TICKER) {
        queues.ticker.emplace(std::move(*queue));
    } else if (native->profile.family == core::SubscriptionFamily::HISTORY) {
        queues.history.emplace(std::move(*queue));
    } else {
        queues.stream.emplace(std::move(*queue));
    }

    auto work = native->profile.preparationWork;
    auto recovery = transport::detail::TcpRecoveryFactory::create(
        bootstrap, native->profile.endpoint, std::move(native->profile.blueprint), std::move(queues),
        native->profile.recoveryOptions, work);

    if (!recovery) {
        throw Error(recoveryDiagnostic(recovery.error()));
    }

    native->recovery.emplace(std::move(*recovery));
    transport::detail::TcpRecoveryFactory::setCallbackRetirementFence(*native->recovery, &port->callbackFence);
    const auto started = native->recovery->start();

    if (!started) {
        throw Error(recoveryDiagnostic(started.error()));
    }

    return native;
}

/** Register one actual independent existing receiver on the adopted family queue.
 * @param endpoint Sole worker-owned native endpoint.
 * @param port Detached logical producer state.
 * @return Actual registration, empty during a continuing source reset, or detached Error for permanent refusal.
 */
std::unique_ptr<NativeSubscription> registerSubscription(NativeEndpoint &endpoint,
                                                         const std::shared_ptr<SubscriptionPort> &port) {
    const auto &plan = receivingEventPlan(endpoint.profile, port->eventSelection);

    if (plan.bindingCount == 0) {
        throw Error({ErrorCode::UNSUPPORTED_PROFILE, "subscription-unregistered-event"});
    }

    auto result = std::make_unique<NativeSubscription>();
    result->port = port;
    result->consumer = std::make_shared<TypedConsumer>(port, plan.projectionWork);
    auto receiverWork = plan.work;
    auto queueWork = endpoint.profile.queueWork;
    std::array<client::PreparedSubscriptionBinding, MAXIMUM_RECEIVING_BINDINGS> selectedBindings{};

    if (!copyReceivingBindings(endpoint.profile, plan, selectedBindings, receiverWork)) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-binding-copy"});
    }

    auto handle = endpoint.recovery->openSubscriptionReceiver(
        endpoint.profile.family, std::span(selectedBindings).first(plan.bindingCount), result->consumer, plan.limits,
        endpoint.profile.clientLimits, receiverWork, queueWork);

    if (!handle) {
        if (const auto *failure = std::get_if<transport::RecoveryDiagnostic>(&handle.error())) {
            throw Error(recoveryDiagnostic(*failure));
        }

        const auto &failure = std::get<client::ReceiverFailure>(handle.error());

        if (const auto *queue = std::get_if<mux::QueueFailure>(&failure.cause)) {
            if (queue->error == mux::QueueError::BUSY ||
                (queue->error == mux::QueueError::CLOSING &&
                 endpoint.recovery->status().phase == transport::RecoveryPhase::RESETTING)) {
                // A real reset temporarily fences registration; retry once on a later bounded owner turn.
                return {};
            }

            throw Error(queueDiagnostic(queue->error, "subscription-registration"));
        }

        if (const auto *mapping = std::get_if<events::MappingError>(&failure.cause)) {
            throw Error(
                {ErrorCode::UNSUPPORTED_PROFILE, "subscription-mapping", 0, static_cast<std::uint32_t>(*mapping)});
        }

        const auto receiver = std::get<client::ReceiverError>(failure.cause);
        const auto code = receiver == client::ReceiverError::WORK_LIMIT           ? ErrorCode::WORK_LIMIT
                          : receiver == client::ReceiverError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                                  : ErrorCode::RESOURCE_LIMIT;
        throw Error({code, "subscription-plan", 0, static_cast<std::uint32_t>(receiver)});
    }

    result->handle = std::move(*handle);
    port->statistics->update([&](auto &s) {
        s.queueCapacityEvents =
            endpoint.profile.clientLimits.dataItems +
            (endpoint.profile.family == core::SubscriptionFamily::TICKER ? endpoint.profile.tickerLimits.cacheKeys
             : endpoint.profile.family == core::SubscriptionFamily::HISTORY
                 ? endpoint.profile.historyLimits.points + endpoint.profile.historyLimits.pendingItems
                 : 0);
    });
    observeState(port, SubscriptionState::ACTIVE);

    return result;
}

/** Observe actual replacement completion before admitting the next coalesced desired snapshot.
 * @param endpoint Worker-owned immutable profile and exact schema origin.
 * @param subscription Existing independent receiver registration.
 * @param work Remaining desired submission coordinates for this whole bounded endpoint turn.
 */
void applyDesired(NativeEndpoint &endpoint, NativeSubscription &subscription, mux::QueueSubmitWork &work) {
    if (endpoint.profile.family == core::SubscriptionFamily::HISTORY &&
        !historyCallbackRetirement(*subscription.port).quiescent)
        return;

    if (subscription.ticket.id()) {
        const auto completed = subscription.ticket.result();

        if (!completed) {
            return;
        }

        subscription.ticket = {};

        if (completed->status != mux::CommandStatus::APPLIED) {
            if (!subscription.port->closeRequested.load(std::memory_order_acquire) &&
                !endpoint.port->closeRequested.load(std::memory_order_acquire)) {
                failSubscription(subscription.port,
                                 {ErrorCode::RESOURCE_LIMIT, "subscription-desired-result", 0,
                                  completed->error ? static_cast<std::uint32_t>(*completed->error) : 0});
            }

            return;
        }
    }

    if (work.operations == 0) {
        return;
    }

    const auto &plan = receivingEventPlan(endpoint.profile, subscription.port->eventSelection);
    std::vector<ReceivingSymbol> symbols;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock(subscription.port->mutex);
        revision = subscription.port->revision;

        if (subscription.hasSubmitted && revision == subscription.submittedRevision) {
            return;
        }

        if (!prepayReceivingRouting(endpoint.profile, work)) {
            return;
        }

        if (subscription.port->symbols.size() > endpoint.profile.routing.symbols) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-routing-symbols"});
        }

        symbols.reserve(subscription.port->symbols.size());

        for (const auto &symbol : subscription.port->symbols) {
            symbols.push_back({std::u16string(symbol.eventSymbol()),
                               {},
                               0,
                               {},
                               symbol.kind() == SubscriptionSymbolKind::ALL ? core::SubscriptionSelectorKind::ALL
                                                                            : core::SubscriptionSelectorKind::EXACT,
                               symbol.sourceId(),
                               symbol.fromTime(),
                               {}});
        }
    }

    std::vector<core::SubscriptionRequest> requests;

    const auto physicalRecords = endpoint.profile.physicalRouteCount;

    if (!physicalRecords || symbols.size() > endpoint.profile.clientLimits.commandEntries / physicalRecords) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-route-entries"});
    }

    requests.reserve(symbols.size() * physicalRecords);
    prepareReceivingSymbols(endpoint.profile, plan, symbols);

    for (std::size_t offset = 0; offset < plan.bindingCount; ++offset) {
        const auto index = plan.bindingIndices[offset];
        const auto &binding = endpoint.profile.bindings[index];
        const auto context = endpoint.profile.schema.recordContext(binding.localId);

        for (const auto &symbol : symbols) {
            const auto projections = receivingProjectionMask(endpoint.profile, index, symbol);

            if (!projections) {
                continue;
            }

            const auto &physical = receivingPhysicalSymbol(endpoint.profile, index, symbol);
            appendReceivingDesiredRequest(requests, context, physical, symbol.selector, endpoint.profile.family,
                                          symbol.fromTime, endpoint.profile.bindingTimeEncodings[index], projections);
        }
    }

    {
        std::lock_guard lock(subscription.port->statistics->mutex);
        subscription.port->statistics->submittedSelectors = symbols.size();
        subscription.port->statistics->submittedDesiredRevision = revision;
    }
    auto accepted = subscription.handle.replace(requests, work);

    if (!accepted) {
        failSubscription(subscription.port, queueDiagnostic(accepted.error(), "subscription-desired-submit"));

        return;
    }

    subscription.ticket = std::move(*accepted);
    subscription.submittedRevision = revision;
    subscription.hasSubmitted = true;
}

/** Process each finite logical slot once, outside any application callback.
 * @param endpoint Sole worker-owned native endpoint.
 * @param submitWork Remaining submission coordinates, never refilled inside the bounded owner turn.
 */
void progressSubscriptions(NativeEndpoint &endpoint, mux::QueueSubmitWork &submitWork) {
    for (std::size_t index = 0; index < endpoint.subscriptions.size(); ++index) {
        std::shared_ptr<SubscriptionPort> port;
        {
            std::lock_guard lock(endpoint.port->mutex);
            port = endpoint.port->subscriptions[index];
        }

        auto &native = endpoint.subscriptions[index];

        if (!port || port->retired.load(std::memory_order_acquire)) {
            continue;
        }

        publishRequestedSubscriptionFailure(port);
        std::shared_ptr<CallbackSlotPool> callbackPool;
        std::size_t callbackSlot;
        bool obsoleteHistory = false;
        {
            std::lock_guard lock(port->mutex);
            callbackPool = port->callbackPool;
            callbackSlot = port->callbackSlot;
            obsoleteHistory = endpoint.profile.family == core::SubscriptionFamily::HISTORY &&
                              port->callbackHistoryRevision && *port->callbackHistoryRevision != port->revision;
        }

        if (callbackPool) {
            if (port->closeRequested.load(std::memory_order_acquire) ||
                endpoint.port->closeRequested.load(std::memory_order_acquire))
                callbackPool->cancel(callbackSlot, {ErrorCode::CALLBACK_CANCELLED, "callback-close"});
            else if (obsoleteHistory)
                // This worker is the sole packet publisher for the fixed slot: no newer packet can intervene.
                callbackPool->cancel(callbackSlot, {ErrorCode::CALLBACK_CANCELLED, "callback-history-revision"});

            if (native && callbackPool->snapshot(callbackSlot).quiescent)
                static_cast<void>(native->handle.requestResume());
        }

        if (!native && !port->closeRequested.load(std::memory_order_acquire) &&
            !endpoint.port->closeRequested.load(std::memory_order_acquire)) {
            try {
                native = registerSubscription(endpoint, port);
            } catch (const Error &failure) {
                failSubscription(port, failure.diagnostic());
            } catch (const std::bad_alloc &) {
                failSubscription(port, {ErrorCode::ALLOCATION_FAILURE, "subscription-registration"});
            } catch (...) {
                failSubscription(port, {ErrorCode::RESOURCE_LIMIT, "subscription-registration"});
            }
        }

        if (!native) {
            if (port->closeRequested.load(std::memory_order_acquire) ||
                endpoint.port->closeRequested.load(std::memory_order_acquire)) {
                retireSubscription(port);
            }

            continue;
        }

        const auto terminal =
            native->handle.state() == mux::QueueClientState::TERMINAL ? native->handle.terminal() : std::nullopt;

        if (terminal && terminal->primary && terminal->primary->error == mux::QueueError::RESOURCE_LIMIT) {
            failSubscription(port, queueDiagnostic(terminal->primary->error, "subscription-delivery-capacity"));
        }

        if (port->closeRequested.load(std::memory_order_acquire)) {
            if (terminal) {
                native.reset();
                retireSubscription(port);
            } else if (!native->stopping) {
                native->handle.requestStop(mux::StopMode::ABORT);
                native->stopping = true;
            }

            continue;
        }

        if (const auto notice = native->handle.resetNotice()) {
            {
                std::lock_guard lock(endpoint.port->mutex);
                endpoint.port->terminal.hadGap = true;
            }

            native->handle.acknowledgeReset(notice->sequence);
        }

        if (!endpoint.port->closeRequested.load(std::memory_order_acquire)) {
            try {
                applyDesired(endpoint, *native, submitWork);
            } catch (const Error &failure) {
                failSubscription(port, failure.diagnostic());
            } catch (const std::bad_alloc &) {
                failSubscription(port, {ErrorCode::ALLOCATION_FAILURE, "subscription-desired-copy"});
            } catch (...) {
                failSubscription(port, {ErrorCode::RESOURCE_LIMIT, "subscription-desired-copy"});
            }
        }
    }
}

/** Reduce actual terminal evidence without erasing earlier reset/gap observations.
 * @param endpoint Detached producer endpoint.
 * @param status Last actual recovery observation.
 * @return Actual local admitted-prefix result, never peer acknowledgement.
 */
CloseResult closeResult(const std::shared_ptr<EndpointPort> &endpoint, const transport::RecoveryStatus &status) {
    CloseResult result;
    {
        std::lock_guard lock(endpoint->mutex);
        result.hadGap = endpoint->terminal.hadGap;
    }

    if (status.terminal) {
        result.deadlineExpired = status.terminal->deadlineExpired;
        result.hadGap = result.hadGap || status.terminal->heldDataDiscarded;

        for (const auto &queue : status.terminal->queues) {
            if (queue) {
                result.hadGap =
                    result.hadGap || queue->hadGap || queue->unknownUpstreamLoss || queue->unknownEligibility;
            }
        }

        result.drained =
            status.terminal->outcome == mux::TerminalOutcome::DRAINED && !result.deadlineExpired && !result.hadGap;
    }

    return result;
}

/** Actual endpoint result after one poll from its finite shared-work burst. */
/** Cancel not-started selected work without requiring executor progress. */
void cancelEndpointCallbacks(const std::shared_ptr<EndpointPort> &endpoint, std::string_view stage) noexcept {
    for (std::size_t index = 0; index < endpoint->subscriptions.size(); ++index) {
        std::shared_ptr<SubscriptionPort> port;
        {
            std::lock_guard lock(endpoint->mutex);
            port = endpoint->subscriptions[index];
        }

        if (!port)
            continue;

        std::shared_ptr<CallbackSlotPool> pool;
        std::size_t slot;
        {
            std::lock_guard lock(port->mutex);
            pool = port->callbackPool;
            slot = port->callbackSlot;
        }

        if (pool)
            pool->cancel(slot, {ErrorCode::CALLBACK_CANCELLED, stage});
    }
}

enum class EndpointProgress {
    PENDING,       ///< More lower-level work may remain; no idle acknowledgement is inferred.
    RETIRED,       ///< Native retirement completed or a visible fixed-profile refusal requires fallback disposal.
    TURN_EXHAUSTED ///< A spent shared turn yielded; the next scheduled turn may admit more work.
};

/** Give one endpoint one native/queue/control poll from an unchanged finite owner-turn allowance.
 * @param endpoint Sole worker-owned endpoint.
 * @param work Remaining actual recovery/native/queue coordinates, shared across this whole burst.
 * @param submitWork Remaining desired submission coordinates, shared across this whole burst.
 * @param firstPoll True only before the fresh seed has admitted any preceding poll in this turn.
 * @return Actual retirement, ordinary shared-turn exhaustion, or continuing progress requirement.
 */
EndpointProgress progressEndpoint(NativeEndpoint &endpoint, transport::RecoveryTurnWork &work,
                                  mux::QueueSubmitWork &submitWork, bool firstPoll) {
    std::uint64_t revision;
    std::uint64_t clearRevision;
    bool online;
    {
        std::lock_guard lock(endpoint.port->mutex);

        if (endpoint.port->stateChangeCount != 0) {
            return EndpointProgress::TURN_EXHAUSTED;
        }

        revision = endpoint.port->connectionRevision;
        clearRevision = endpoint.port->clearRevision;
        online = endpoint.port->connectionIntent;
    }

    if (!endpoint.port->closeRequested.load(std::memory_order_acquire) && endpoint.appliedRevision != revision) {
        cancelEndpointCallbacks(endpoint.port, "callback-source-change");
        const bool clear = clearRevision > endpoint.appliedClearRevision;
        const auto disconnected = endpoint.recovery->requestDisconnect(clear);

        if (!disconnected) {
            failEndpoint(endpoint.port, recoveryDiagnostic(disconnected.error()));

            return EndpointProgress::RETIRED;
        }

        if (online) {
            const auto restarted = endpoint.recovery->requestReconnect();

            if (!restarted) {
                failEndpoint(endpoint.port, recoveryDiagnostic(restarted.error()));

                return EndpointProgress::RETIRED;
            }
        }

        endpoint.appliedRevision = revision;

        if (clear) {
            endpoint.appliedClearRevision = clearRevision;
        }
    }

    std::optional<std::chrono::milliseconds> aggregationPeriod;
    std::uint64_t aggregationRevision = 0;
    {
        std::lock_guard lock(endpoint.port->mutex);
        aggregationPeriod = endpoint.port->requestedAggregationPeriod;
        aggregationRevision = endpoint.port->requestedAggregationRevision;
    }

    if (!endpoint.port->closeRequested.load(std::memory_order_acquire) &&
        (!endpoint.aggregationConfigured || endpoint.appliedAggregationRevision != aggregationRevision)) {
        const auto accepted =
            transport::detail::TcpRecoveryFactory::requestAggregationPeriod(*endpoint.recovery, aggregationPeriod);

        if (!accepted) {
            failEndpoint(endpoint.port, recoveryDiagnostic(accepted.error()));

            return EndpointProgress::RETIRED;
        }

        endpoint.appliedAggregationRevision = aggregationRevision;
        endpoint.aggregationConfigured = true;
    }

    progressSubscriptions(endpoint, submitWork);

    if (endpoint.port->closeRequested.load(std::memory_order_acquire) && !endpoint.draining) {
        const auto ticks = endpoint.port->closeDeadline.load(std::memory_order_acquire);
        const auto deadline = ticks == 0
                                  ? std::chrono::steady_clock::now() + endpoint.port->options.drainTimeout
                                  : std::chrono::steady_clock::time_point{std::chrono::steady_clock::duration{ticks}};
        const auto requested = endpoint.recovery->requestDrain(deadline);
        endpoint.draining = true;

        if (!requested) {
            failEndpoint(endpoint.port, recoveryDiagnostic(requested.error()));
            static_cast<void>(endpoint.recovery->requestStop());
        }

        observeState(endpoint.port, EndpointState::CLOSING);
    }

    const auto polled = endpoint.recovery->poll(work);

    if (endpoint.port->callbackFence.cancelQueuedRequested.exchange(false, std::memory_order_acq_rel))
        cancelEndpointCallbacks(endpoint.port, "callback-source-reset");

    const auto statistics = endpoint.recovery->statistics();

    for (const auto &subscription : endpoint.subscriptions) {
        if (subscription)
            subscription->port->statistics->update([&](auto &s) {
                s.sourceEpoch = statistics.sourceEpoch;
            });
    }

    const auto aggregationInfo = transport::detail::TcpRecoveryFactory::aggregationPeriodInfo(*endpoint.recovery);
    {
        std::lock_guard lock(endpoint.port->mutex);
        endpoint.port->statistics.connection = statistics;

        if (endpoint.appliedRevision == endpoint.port->connectionRevision && endpoint.port->connectionIntent &&
            !endpoint.port->closeRequested.load(std::memory_order_acquire)) {
            endpoint.port->aggregationInfo = aggregationInfo;
            endpoint.port->aggregationSourceRevision = endpoint.appliedRevision;
        }

        endpoint.port->statistics.observedAt = std::chrono::steady_clock::now();
    }

    if (!polled) {
        if (!firstPoll && polled.error().error == transport::RecoveryError::WORK_LIMIT) {
            return EndpointProgress::TURN_EXHAUSTED;
        }

        failEndpoint(endpoint.port, recoveryDiagnostic(polled.error()));

        return EndpointProgress::RETIRED;
    }

    const auto &status = *polled;
    const bool enteredFactory = status.attempts != endpoint.actualAttempts;
    endpoint.actualAttempts = status.attempts;
    {
        std::lock_guard lock(endpoint.port->mutex);

        if (!status.sourceResetPending && endpoint.appliedClearRevision > endpoint.port->clearedRevision) {
            endpoint.port->clearedRevision = endpoint.appliedClearRevision;
        }

        // A paused cache belongs to its original stable queue epoch, never to a fabricated new transport attempt.
        if (status.phase == transport::RecoveryPhase::SUSPENDED && !status.terminal &&
            endpoint.appliedRevision == endpoint.port->connectionRevision && !endpoint.port->connectionIntent &&
            endpoint.port->clearRevision <= endpoint.port->clearedRevision && !endpoint.draining) {
            endpoint.port->pausedReplayFence = endpoint.appliedRevision;
        }

        // Factory entry is after the real source reset and before START/POLL/DELIVER; never retag an old offer.
        if (enteredFactory && !status.terminal && endpoint.appliedRevision == endpoint.port->connectionRevision &&
            endpoint.port->connectionIntent && !endpoint.draining) {
            endpoint.port->activeDeliveryRevision = endpoint.appliedRevision;
        }

        if (status.first.error != transport::RecoveryError::NONE && endpoint.port->diagnostic.code == ErrorCode::NONE) {
            endpoint.port->diagnostic = recoveryDiagnostic(status.first);
        }
    }

    if (!endpoint.draining && !status.terminal && online && endpoint.appliedRevision == revision) {
        observeState(endpoint.port,
                     status.phase == transport::RecoveryPhase::ACTIVE ? EndpointState::CONNECTED
                                                                      : EndpointState::CONNECTING,
                     revision);
    }

    return status.terminal ? EndpointProgress::RETIRED : EndpointProgress::PENDING;
}

/** Destroy native owners and callable leases before publishing detached endpoint quiescence.
 * @param endpoint Worker-local native slot, made empty.
 */
void retireNative(std::unique_ptr<NativeEndpoint> &endpoint) noexcept {
    const auto port = endpoint->port;
    requestEndpointClose(port);
    const auto status = endpoint->recovery->status();
    auto result = closeResult(port, status);
    const auto statistics = transport::detail::TcpRecoveryFactory::retireStatistics(*endpoint->recovery);
    {
        std::lock_guard lock(port->mutex);
        port->statistics.connection = statistics;
        port->statistics.observedAt = std::chrono::steady_clock::now();
    }
    endpoint.reset();
    retireEndpoint(port, result);
}
} // namespace

RuntimeImpl::RuntimeImpl(transport::RuntimeBootstrap owner, RuntimeOptions options)
    : RuntimeImpl(acquireOwnedBootstrap(owner), std::move(options)) {
    // The parameter owns SDK state until all fallible delegated storage has succeeded.
    ownedBootstrap.emplace(std::move(owner));
}

RuntimeImpl::RuntimeImpl(transport::detail::BootstrapLease bootstrap, RuntimeOptions options)
    : bootstrap(std::move(bootstrap)), options(std::move(options)), control(std::make_shared<RuntimeControl>()) {
    control->owner = std::this_thread::get_id();
    control->endpoints.resize(this->options.maximumEndpoints);
    control->changes = std::make_shared<SubscriptionChangeDomain>();
    control->changes->ports.resize(this->options.maximumEndpoints * this->options.maximumSubscriptionsPerEndpoint);
    control->changes->continuations.resize(this->options.maximumSubscriptionChangesPerTurn);
    control->changes->maximumWork = this->options.maximumSubscriptionChangesPerTurn;
    control->changes->remainingWork = control->changes->maximumWork;
    control->runtime = this;
}

RuntimeImpl::~RuntimeImpl() {
    if (std::this_thread::get_id() != control->owner) {
        std::terminate();
    }

    shutdown();
}

void RuntimeImpl::startWorker() {
    if (!worker.joinable()) {
        try {
            worker = std::thread([this] {
                run();
            });
        } catch (const std::system_error &) {
            reject(ErrorCode::RESOURCE_LIMIT, "runtime-thread-start");
        }
    }
}

bool RuntimeImpl::retireInactive(const std::shared_ptr<EndpointPort> &endpoint) {
    requestEndpointClose(endpoint);

    if (std::this_thread::get_id() == control->owner) {
        endpoint->retirementOnMain.store(true, std::memory_order_release);
        endpoint->retirementClaimed.store(true, std::memory_order_release);
        finishInactiveRetirement(endpoint);

        return true;
    }

    return retireEndpoint(endpoint, CloseResult{true});
}

void RuntimeImpl::shutdown() {
    if (std::this_thread::get_id() != control->owner) {
        reject(ErrorCode::WRONG_THREAD, "runtime-await-thread");
    }

    if (shutdownInProgress || retirementFrame || subscriptionObserverCallbackActive()) {
        reject(ErrorCode::INVALID_STATE, "runtime-retirement-reentry");
    }

    shutdownInProgress = true;

    struct ShutdownGuard {
        bool &active;

        ~ShutdownGuard() {
            active = false;
        }
    } shutdownGuard{shutdownInProgress};

    {
        std::lock_guard gate(control->mutex);
        control->stopping = true;

        for (const auto &endpoint : control->endpoints) {
            if (endpoint) {
                requestEndpointClose(endpoint);
            }
        }

        ++control->wakeSequence;
    }

    control->condition.notify_one();

    if (worker.joinable()) {
        worker.join();
    } else {
        for (const auto &endpoint : control->endpoints) {
            if (endpoint && !endpoint->retired.load(std::memory_order_acquire)) {
                retireInactive(endpoint);
            }
        }
    }

    {
        std::lock_guard gate(control->mutex);
        control->runtime = nullptr;
    }
}

void RuntimeImpl::run() noexcept {
    std::vector<std::unique_ptr<NativeEndpoint>> native;

    try {
        native.resize(options.maximumEndpoints);
        std::vector<std::shared_ptr<EndpointPort>> endpoints(options.maximumEndpoints);

        while (true) {
            std::uint64_t observedWake = 0;
            bool stopping = false;
            {
                std::lock_guard gate(control->mutex);
                observedWake = control->wakeSequence;
                stopping = control->stopping;
                std::ranges::copy(control->endpoints, endpoints.begin());
            }

            auto nextWake = std::chrono::steady_clock::time_point::max();

            for (std::size_t index = 0; index < endpoints.size(); ++index) {
                const auto &port = endpoints[index];

                if (!port || port->retired.load(std::memory_order_acquire)) {
                    continue;
                }

                if (port->retirementClaimed.load(std::memory_order_acquire) &&
                    port->retirementOnMain.load(std::memory_order_acquire)) {
                    continue;
                }

                if (stopping) {
                    requestEndpointClose(port);
                }

                bool connectRequested = false;
                {
                    std::lock_guard lock(port->mutex);
                    connectRequested = port->connectRequested && port->connectionIntent;
                }

                // Capture admission before draining its notification: a concurrent later connect must wait
                // for the next turn instead of activating ahead of its deferred CONNECTING selection.
                if (!notifyConnecting(port)) {
                    // A producer refill yields to other endpoints, but finite remaining debt still needs owner work.
                    nextWake = std::min(nextWake, std::chrono::steady_clock::now() + port->options.pollInterval);

                    continue;
                }

                bool failedThisTurn = false;

                try {
                    if (!native[index]) {
                        if (port->closeRequested.load(std::memory_order_acquire)) {
                            if (!retireInactive(port)) {
                                nextWake =
                                    std::min(nextWake, std::chrono::steady_clock::now() + port->options.pollInterval);
                            }

                            continue;
                        }

                        if (!connectRequested) {
                            // Close individual never-activated subscriptions without opening a queue.
                            for (std::size_t slot = 0; slot < port->subscriptions.size(); ++slot) {
                                std::shared_ptr<SubscriptionPort> subscription;
                                {
                                    std::lock_guard lock(port->mutex);
                                    subscription = port->subscriptions[slot];
                                }

                                if (subscription && subscription->closeRequested.load(std::memory_order_acquire) &&
                                    !subscription->retired.load(std::memory_order_acquire)) {
                                    retireSubscription(subscription);
                                }
                            }

                            continue;
                        }

                        native[index] = activate(bootstrap, port);
                    }

                    auto work = native[index]->profile.turnWork;
                    auto submitWork = native[index]->profile.submitWork;
                    auto observationWork = native[index]->profile.historyObservationWork;
                    bool retired = false;

                    for (std::size_t poll = 0; poll < MAXIMUM_RECOVERY_POLLS_PER_TURN; ++poll) {
                        const auto progress = progressEndpoint(*native[index], work, submitWork, poll == 0);

                        if (progress == EndpointProgress::RETIRED) {
                            retireNative(native[index]);
                            retired = true;

                            break;
                        }

                        if (progress == EndpointProgress::TURN_EXHAUSTED) {
                            break;
                        }
                    }

                    if (retired) {
                        continue;
                    }

                    if (native[index]->profile.family == core::SubscriptionFamily::HISTORY) {
                        for (const auto &subscription : native[index]->subscriptions) {
                            if (subscription && !subscription->stopping)
                                publishHistorySnapshot(*native[index]->recovery, subscription->handle,
                                                       subscription->port, observationWork);
                        }
                    }

                    auto wakeAt = std::chrono::steady_clock::now() + port->options.pollInterval;

                    if (const auto hint = native[index]->recovery->nextWake()) {
                        wakeAt = std::min(wakeAt, *hint);
                    }

                    nextWake = std::min(nextWake, wakeAt);
                } catch (const Error &failure) {
                    failEndpoint(port, failure.diagnostic());
                    failedThisTurn = true;
                } catch (const std::bad_alloc &) {
                    failEndpoint(port, {ErrorCode::ALLOCATION_FAILURE, "endpoint-worker"});
                    failedThisTurn = true;
                } catch (...) {
                    failEndpoint(port, {ErrorCode::RESOURCE_LIMIT, "endpoint-worker"});
                    failedThisTurn = true;
                }

                if (failedThisTurn) {
                    if (native[index]) {
                        retireNative(native[index]);
                    } else {
                        retireEndpoint(port, {});
                    }
                }
            }

            const auto allRetired = std::ranges::all_of(endpoints, [](const auto &port) {
                return !port || port->retired.load(std::memory_order_acquire);
            });

            if (stopping && allRetired) {
                break;
            }

            std::unique_lock gate(control->mutex);
            const auto changed = [&] {
                return control->wakeSequence != observedWake || (control->stopping && !stopping);
            };

            if (nextWake == std::chrono::steady_clock::time_point::max()) {
                control->condition.wait(gate, changed);
            } else {
                control->condition.wait_until(gate, nextWake, changed);
            }
        }
    } catch (...) {
        {
            std::lock_guard gate(control->mutex);
            control->stopping = true;
        }

        // Even worker infrastructure allocation failure cannot move native destruction to a producer.
        for (auto &endpoint : native) {
            if (endpoint) {
                failEndpoint(endpoint->port, {ErrorCode::ALLOCATION_FAILURE, "runtime-worker"});
                retireNative(endpoint);
            }
        }

        while (true) {
            std::uint64_t observedWake = 0;
            {
                std::lock_guard gate(control->mutex);
                observedWake = control->wakeSequence;
            }

            bool complete = true;

            // Sweep every worker-owned port before waiting: an A capture destructor may legitimately
            // await distinct endpoint B. Waiting on A first would prevent B from ever retiring.
            for (const auto &endpoint : control->endpoints) {
                if (endpoint && !endpoint->retired.load(std::memory_order_acquire) &&
                    !endpoint->retirementOnMain.load(std::memory_order_acquire)) {
                    failEndpoint(endpoint, {ErrorCode::ALLOCATION_FAILURE, "runtime-worker"});
                    complete = retireEndpoint(endpoint, {}) && complete;
                }
            }

            if (complete) {
                break;
            }

            std::unique_lock gate(control->mutex);
            control->condition.wait(gate, [&] {
                return control->wakeSequence != observedWake;
            });
        }
    }
}
} // namespace detail

namespace {
/** Validate and snapshot caller settings before publishing any runtime.
 * @param options Explicit finite settings, including optional DNS server input.
 * @return Validated owning settings with a bounded resolver snapshot; throws Error on refusal.
 */
RuntimeOptions prepareRuntimeOptions(RuntimeOptions options) {
    if (!options.maximumEndpoints || !options.maximumSubscriptionsPerEndpoint ||
        !options.maximumSymbolsPerSubscription || !options.maximumListenersPerSubscription ||
        !options.maximumStateListenersPerEndpoint || !options.maximumStateListenersPerSubscription ||
        !options.maximumSymbolUnits || options.maximumSymbolUnits > 128 ||
        !options.maximumEventTypeArgumentsPerSubscription || !options.maximumPendingStateChangesPerEndpoint ||
        !options.maximumChangeListenersPerSubscription || !options.maximumPendingSubscriptionChanges ||
        !options.maximumSubscriptionChangesPerTurn || options.dnsServers.size() > 4 || options.dnsPort == 0) {
        reject(ErrorCode::INVALID_ARGUMENT, "runtime-options");
    }

    if (options.maximumEndpoints > std::numeric_limits<std::size_t>::max() / options.maximumSubscriptionsPerEndpoint /
                                       sizeof(std::weak_ptr<detail::SubscriptionPort>) ||
        options.maximumSubscriptionChangesPerTurn >
            std::numeric_limits<std::size_t>::max() / sizeof(detail::SubscriptionContinuation) ||
        options.maximumPendingSubscriptionChanges >
            std::numeric_limits<std::size_t>::max() / sizeof(detail::SubscriptionChangePhase) ||
        options.maximumChangeListenersPerSubscription >
            std::numeric_limits<std::size_t>::max() / sizeof(detail::SubscriptionClosedSelection)) {
        reject(ErrorCode::RESOURCE_LIMIT, "runtime-change-storage");
    }

    const auto observerPorts = options.maximumEndpoints * options.maximumSubscriptionsPerEndpoint;

    if (options.maximumChangeListenersPerSubscription >
        (std::numeric_limits<std::size_t>::max() - options.maximumSubscriptionChangesPerTurn) / observerPorts) {
        reject(ErrorCode::RESOURCE_LIMIT, "runtime-change-leases");
    }

    constexpr std::size_t FRONTEND_SELECTOR_BANKS = 4; // Old, parsed, candidate and one conversion/retirement bank.
    const auto selectorBytes = sizeof(SubscriptionSelector) + (options.maximumSymbolUnits + 1) * sizeof(char16_t);

    constexpr std::size_t SELECTOR_BANKS_PER_CHANGE_FRAME = 3; // Snapshot, removed and added identities.
    constexpr std::size_t PREPARING_CHANGE_FRAMES = 2;         // Current retained snapshot and one complete candidate.
    const auto maximum = std::numeric_limits<std::size_t>::max();

    if (options.maximumPendingSubscriptionChanges >
        maximum - PREPARING_CHANGE_FRAMES - options.maximumChangeListenersPerSubscription) {
        reject(ErrorCode::RESOURCE_LIMIT, "runtime-change-snapshots");
    }

    const auto frames = options.maximumPendingSubscriptionChanges + options.maximumChangeListenersPerSubscription +
                        PREPARING_CHANGE_FRAMES;

    if (frames > (maximum - FRONTEND_SELECTOR_BANKS) / SELECTOR_BANKS_PER_CHANGE_FRAME) {
        reject(ErrorCode::RESOURCE_LIMIT, "runtime-change-snapshots");
    }

    const auto selectorBanks = FRONTEND_SELECTOR_BANKS + frames * SELECTOR_BANKS_PER_CHANGE_FRAME;

    if (options.maximumSymbolsPerSubscription > maximum / selectorBytes / selectorBanks) {
        reject(ErrorCode::RESOURCE_LIMIT, "runtime-selector-storage");
    }

    if (options.dnsServers.empty()) {
        auto discovered = detail::discoverSystemDnsServers();

        if (!discovered) {
            throw Error(discovered.error());
        }

        options.dnsServers = std::move(*discovered);
    }

    for (const auto &server : options.dnsServers) {
        if (std::ranges::all_of(server.address, [](auto value) {
                return value == 0;
            })) {
            reject(ErrorCode::DNS_CONFIGURATION, "runtime-dns");
        }
    }

    return options;
}
} // namespace

ApiRuntime::ApiRuntime(RuntimeOptions options) {
    producerAction([&] {
        options = prepareRuntimeOptions(std::move(options));

        auto bootstrap = transport::RuntimeBootstrap::create();

        if (!bootstrap) {
            throw Error({ErrorCode::BOOTSTRAP_FAILURE, bootstrap.error().stage, bootstrap.error().nativeCode,
                         static_cast<std::uint32_t>(bootstrap.error().error)});
        }

        impl_ = std::make_unique<detail::RuntimeImpl>(std::move(*bootstrap), std::move(options));
    });
}

ApiRuntime::ApiRuntime(transport::RuntimeBootstrap &bootstrap, RuntimeOptions options) {
    producerAction([&] {
        auto lease = transport::detail::BootstrapLease::acquireForRuntime(bootstrap);

        if (!lease) {
            const auto code = lease.error().error == transport::TcpError::WRONG_THREAD ? ErrorCode::WRONG_THREAD
                              : lease.error().error == transport::TcpError::RESOURCE_LIMIT
                                  ? ErrorCode::RESOURCE_LIMIT
                                  : ErrorCode::INVALID_ARGUMENT;
            throw Error(
                {code, lease.error().stage, lease.error().nativeCode, static_cast<std::uint32_t>(lease.error().error)});
        }

        options = prepareRuntimeOptions(std::move(options));
        impl_ = std::make_unique<detail::RuntimeImpl>(std::move(*lease), std::move(options));
    });
}

ApiRuntime::~ApiRuntime() = default;

std::shared_ptr<Endpoint> ApiRuntime::createEndpoint(EndpointOptions options) {
    return createEndpoint(EndpointConfiguration::create(options));
}

std::shared_ptr<Endpoint> ApiRuntime::createEndpoint(const EndpointConfiguration &configuration) {
    return producerAction([&] {
        const auto &options = configuration.options();
        const auto control = impl_->control;
        auto port = std::make_shared<detail::EndpointPort>();
        port->statistics.ownerIdentity = std::make_shared<const std::byte>();
        port->statistics.observedAt = std::chrono::steady_clock::now();
        port->control = control;
        port->owner = control->owner;
        port->limits = impl_->options;
        port->options = options;
        port->configuration = configuration;
        port->requestedAggregationPeriod = options.requestedAggregationPeriod;
        port->stateListeners = makeStateListeners(port, impl_->options.maximumStateListenersPerEndpoint);
        port->subscriptions.resize(impl_->options.maximumSubscriptionsPerEndpoint);
        port->stateChanges.resize(impl_->options.maximumPendingStateChangesPerEndpoint);
        auto endpoint = std::shared_ptr<Endpoint>(new Endpoint(port));
        {
            std::lock_guard gate(control->mutex);

            if (control->stopping || !control->runtime) {
                reject(ErrorCode::CLOSED, "runtime-closed");
            }

            const auto free = std::ranges::find_if(control->endpoints, [](const auto &item) {
                return !item || item->retired.load(std::memory_order_acquire);
            });

            if (free == control->endpoints.end()) {
                reject(ErrorCode::RESOURCE_LIMIT, "runtime-endpoint-count");
            }

            *free = std::move(port);
            ++control->wakeSequence;
        }

        control->condition.notify_one();

        return endpoint;
    });
}

void ApiRuntime::closeAndAwaitTermination() {
    impl_->shutdown();
}
} // namespace org::ttldtor::qdcxx::api
