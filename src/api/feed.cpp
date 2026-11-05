// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"
#include "../core/delivery_statistics.hpp"
#include "qd/api/feed_filter.hpp"

#include "../transport/tcp_recovery_factory.hpp"
#include "endpoint_configuration.hpp"
#include "event_registry_defaults.hpp"
#include "feed_internal.hpp"
#include "history_observation.hpp"
#include "receiving_profile.hpp"
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

/** Preserve a subscription's first bounded asynchronous failure.
 * @param port Detached producer state.
 * @param diagnostic Static/numeric failure snapshot.
 */
void failSubscription(const std::shared_ptr<detail::SubscriptionPort> &port, Diagnostic diagnostic) noexcept {
    {
        std::lock_guard lock(port->mutex);

        if (port->state == SubscriptionState::CLOSED) {
            return;
        }

        if (port->diagnostic.code == ErrorCode::NONE) {
            port->diagnostic = diagnostic;
        }

        port->closeRequested.store(true, std::memory_order_release);
    }

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
    detail::closeSubscriptionObservers(port);
    observeState(port, SubscriptionState::CLOSING);
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    {
        std::lock_guard lock(port->mutex);
        listeners = std::move(port->listeners);
    }

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

    if (selector.kind() == SubscriptionSymbolKind::TIME_SERIES) {
        reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series");
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

            const bool unchanged =
                candidate.size() == port->symbols.size() && std::ranges::all_of(candidate, [&](const auto &symbol) {
                    const auto previous = std::ranges::find(port->symbols, symbol);

                    return previous != port->symbols.end() && symbol.sameDemand(*previous);
                });

            if (!unchanged) {
                if (port->revision == std::numeric_limits<std::uint64_t>::max()) {
                    reject(ErrorCode::RESOURCE_LIMIT, "subscription-revision");
                }

                detail::prepareSubscriptionChangeLocked(*port, candidate, port->revision + 1);
                port->symbols.swap(candidate);
                ++port->revision;
                port->statistics->update([&](auto &s) {
                    s.desiredSelectors = port->symbols.size();
                    s.desiredRevision = port->revision;
                });
            }
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

template <class Event> SubscriptionStatistics Subscription<Event>::getStatistics() const noexcept {
    return port_->statistics->snapshot();
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
template class Subscription<RegisteredEvent>;

Feed::Feed(std::shared_ptr<detail::EndpointPort> port) : port_(std::move(port)) {
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

std::shared_ptr<detail::SubscriptionPort> Feed::createSubscriptionPort(std::span<const EventType> types,
                                                                       bool registeredDelivery) {
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
            default:
                reject(ErrorCode::INVALID_ARGUMENT, "subscription-event-types");
            }
        }

        const auto selection = static_cast<detail::EventSelection>(
            (quote ? 1U : 0U) | (profile ? 2U : 0U) | (trade ? 4U : 0U) | (tradeETH ? 8U : 0U) | (summary ? 16U : 0U) |
            (textMessage ? 32U : 0U) | (textConfiguration ? 64U : 0U) | (message ? 128U : 0U) |
            (configuration ? 256U : 0U) | (order ? 512U : 0U));
        const auto delivery = registeredDelivery  ? detail::EventDelivery::REGISTERED
                              : quote             ? detail::EventDelivery::QUOTE
                              : profile           ? detail::EventDelivery::PROFILE
                              : trade             ? detail::EventDelivery::TRADE
                              : tradeETH          ? detail::EventDelivery::TRADE_ETH
                              : summary           ? detail::EventDelivery::SUMMARY
                              : textMessage       ? detail::EventDelivery::TEXT_MESSAGE
                              : textConfiguration ? detail::EventDelivery::TEXT_CONFIGURATION
                              : message           ? detail::EventDelivery::MESSAGE
                              : order             ? detail::EventDelivery::ORDER
                                                  : detail::EventDelivery::CONFIGURATION;
        auto subscription = std::make_shared<detail::SubscriptionPort>();
        subscription->statistics = std::make_shared<core::detail::DeliveryStatisticsState>();
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

            // Business capability is explicit; an indexed layout alone does not make a TimeSeriesEvent.
            if (port_->options.contract == DeliveryContract::HISTORY && selection != detail::EventSelection::ORDER) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-history-event-type");
            }

            if (order && port_->options.contract != DeliveryContract::HISTORY) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "order-requires-history");
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
            bool registeredOrder =
                !port_->options.eventRegistry && port_->options.contract == DeliveryContract::HISTORY;

            if (port_->options.eventRegistry) {
                for (const auto &registration : port_->options.eventRegistry->registrations()) {
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
                (configuration && !registeredConfiguration) || (order && !registeredOrder)) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "subscription-unregistered-event");
            }

            const auto free = std::ranges::find_if(port_->subscriptions, [](const auto &item) {
                return !item || item->retired.load(std::memory_order_acquire);
            });

            if (free == port_->subscriptions.end()) {
                reject(ErrorCode::RESOURCE_LIMIT, "endpoint-subscription-count");
            }

            *free = subscription;
        }

        wake(port_->control);

        return subscription;
    });
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
            EventRegistryWork work;
            auto prepared = contract == DeliveryContract::HISTORY ? detail::defaultHistoryEventRegistry({}, work)
                                                                  : EventRegistry::builtIn({}, work);

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
constexpr std::size_t MAXIMUM_RECOVERY_POLLS_PER_TURN = 64;

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

        ~DispatchCompletion() {
            state->update([&](auto &s) {
                s.inFlightEvents -= events;
                auto &counter = outcome == DispatchOutcome::DELIVERED         ? s.deliveredEvents
                                : outcome == DispatchOutcome::CALLBACK_FAILED ? s.callbackFailedEvents
                                                                              : s.discardedEvents;
                counter.add(events);
            });
        }
    };

    /** Project and deliver one complete event type under a stable listener selection.
     * @tparam Event Existing owning Quote, Profile or their RegisteredEvent variant.
     * @param batch Existing receiver's callback-only projection service.
     * @param listeners Whole-batch immutable callable selection.
     * @return Existing queue acknowledgement/pause action.
     */
    template <class Event>
    mux::BatchAction deliver(const client::SubscriptionBatch &batch,
                             const std::shared_ptr<const ListenerSnapshot> &listeners, DispatchCompletion &completion) {
        std::vector<Event> projectedEvents;
        projectedEvents.reserve(batch.records().size());
        auto work = projectionWork_;

        for (std::size_t index = 0; index < batch.records().size(); ++index) {
            auto projected = batch.project(index, work);

            if (!projected) {
                failSubscription(port_, {ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});

                return mux::BatchAction::PAUSE;
            }

            if constexpr (std::is_same_v<Event, RegisteredEvent>) {
                if (auto *quote = std::get_if<events::Quote>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*quote));
                } else if (auto *trade = std::get_if<events::Trade>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*trade));
                } else if (auto *tradeETH = std::get_if<events::TradeETH>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*tradeETH));
                } else if (auto *message = std::get_if<events::Message>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*message));
                } else if (auto *configuration = std::get_if<events::Configuration>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*configuration));
                } else if (auto *order = std::get_if<events::Order>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*order));
                } else if (auto *summary = std::get_if<events::Summary>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*summary));
                } else if (auto *message = std::get_if<events::TextMessage>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*message));
                } else if (auto *configuration = std::get_if<events::TextConfiguration>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*configuration));
                } else if (auto *profile = std::get_if<events::Profile>(&projected->value)) {
                    projectedEvents.emplace_back(std::move(*profile));
                } else {
                    failSubscription(port_, {ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});

                    return mux::BatchAction::PAUSE;
                }
            } else {
                if (!std::holds_alternative<Event>(projected->value)) {
                    failSubscription(port_, {ErrorCode::PROTOCOL_FAILURE, "subscription-projection"});

                    return mux::BatchAction::PAUSE;
                }

                projectedEvents.push_back(std::get<Event>(std::move(projected->value)));
            }
        }

        // One selection lasts through the whole underlying batch. Reentrant mutation affects a later batch.
        bool complete = true;

        for (const auto &listener : *listeners) {
            if (port_->closeRequested.load(std::memory_order_acquire)) {
                complete = false;
                break;
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
                failSubscription(port_, {ErrorCode::CALLBACK_FAILURE, "subscription-listener"});

                return mux::BatchAction::PAUSE;
            }
        }

        if (complete)
            completion.outcome = DispatchOutcome::DELIVERED;

        return mux::BatchAction::CONTINUE;
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
        DispatchCompletion completion{port_->statistics, batch.records().size()};
        const auto endpoint = port_->endpoint.lock();

        if (port_->closeRequested.load(std::memory_order_acquire) || !endpoint) {
            return mux::BatchAction::PAUSE;
        }

        SubscriptionDataCallbackScope callbackScope(port_->changes);
        std::shared_ptr<const ListenerSnapshot> listeners;
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
            listeners = port_->listeners;
        }

        if (!listeners || listeners->empty()) {
            return mux::BatchAction::CONTINUE;
        }

        try {
            switch (port_->eventDelivery) {
            case EventDelivery::QUOTE:
                return deliver<events::Quote>(batch, listeners, completion);
            case EventDelivery::PROFILE:
                return deliver<events::Profile>(batch, listeners, completion);
            case EventDelivery::TRADE:
                return deliver<events::Trade>(batch, listeners, completion);
            case EventDelivery::TRADE_ETH:
                return deliver<events::TradeETH>(batch, listeners, completion);
            case EventDelivery::MESSAGE:
                return deliver<events::Message>(batch, listeners, completion);
            case EventDelivery::CONFIGURATION:
                return deliver<events::Configuration>(batch, listeners, completion);
            case EventDelivery::SUMMARY:
                return deliver<events::Summary>(batch, listeners, completion);
            case EventDelivery::ORDER:
                return deliver<events::Order>(batch, listeners, completion);
            case EventDelivery::TEXT_MESSAGE:
                return deliver<events::TextMessage>(batch, listeners, completion);
            case EventDelivery::TEXT_CONFIGURATION:
                return deliver<events::TextConfiguration>(batch, listeners, completion);
            case EventDelivery::REGISTERED:
                return deliver<RegisteredEvent>(batch, listeners, completion);
            default:
                throw Error({ErrorCode::UNSUPPORTED_PROFILE, "subscription-event-type"});
            }
        } catch (const std::bad_alloc &) {
            failSubscription(port_, {ErrorCode::ALLOCATION_FAILURE, "subscription-projection"});
        } catch (...) {
            failSubscription(port_, {ErrorCode::RESOURCE_LIMIT, "subscription-projection"});
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
    std::array<client::PreparedSubscriptionBinding, MAXIMUM_RECEIVING_RECORDS> selectedBindings{};

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
                               symbol.sourceId()});
        }
    }

    std::vector<core::SubscriptionRequest> requests;

    if (plan.bindingCount == 0 || symbols.size() > endpoint.profile.clientLimits.commandEntries / plan.bindingCount) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-route-entries"});
    }

    requests.reserve(symbols.size() * plan.bindingCount);
    prepareReceivingSymbols(endpoint.profile, plan, symbols);

    for (std::size_t offset = 0; offset < plan.bindingCount; ++offset) {
        const auto index = plan.bindingIndices[offset];
        const auto &binding = endpoint.profile.bindings[index];
        const auto context = endpoint.profile.schema.recordContext(binding.localId);

        for (const auto &symbol : symbols) {
            if (receivingSymbolMatches(endpoint.profile, index, symbol)) {
                const auto &physical = endpoint.profile.bindingExchanges[index] ? symbol.physical : symbol.logical;
                // General indexed subscriptions request the full raw index domain from zero. Time-series
                // millisecond ranges require their own verified mapper and are not inferred here.
                requests.push_back({{context, physical, symbol.selector},
                                    endpoint.profile.family == core::SubscriptionFamily::HISTORY
                                        ? std::optional<std::int64_t>{0}
                                        : std::nullopt});
            }
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

        if (port->closeRequested.load(std::memory_order_acquire)) {
            if (!native->stopping) {
                native->handle.requestStop(mux::StopMode::ABORT);
                native->stopping = true;
            }

            if (native->handle.terminal()) {
                native.reset();
                retireSubscription(port);
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
            std::numeric_limits<std::size_t>::max() / sizeof(std::shared_ptr<detail::SubscriptionChangeDomain>) ||
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
