// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include "feed_internal.hpp"
#include "receiving_profile.hpp"
#include "system_dns.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {
constexpr unsigned char FIRST_PRINTABLE_ASCII = 0x20;                 ///< Exact selector minimum byte.
constexpr unsigned char LAST_PRINTABLE_ASCII = 0x7e;                  ///< Exact selector maximum byte.
constexpr std::size_t MAXIMUM_HOST_BYTES = 253;                       ///< Existing bounded DNS A-label route ceiling.
constexpr std::size_t MAXIMUM_ADDRESS_BYTES = MAXIMUM_HOST_BYTES + 8; ///< Brackets, colon and maximum port spelling.

/** Throw a detached synchronous rejection without formatting caller or peer data.
 * @param code High-level category.
 * @param stage Static library-owned checkpoint.
 */
[[noreturn]] void reject(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
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

/** Publish a distinct lifecycle observation and select advisory observers atomically.
 * Only the endpoint's native or claimed creator-retirement execution domain calls this helper.
 * @tparam Port Detached endpoint or subscription port.
 * @tparam State Its public lifecycle enum.
 * @param port Owner with serialized state publication.
 * @param next Actual new lifecycle observation; CLOSED and FAILED are absorbing.
 */
template <class Port, class State> void observeState(const std::shared_ptr<Port> &port, State next) noexcept {
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    State previous;
    {
        std::lock_guard lock(port->mutex);
        previous = port->state;

        if (previous == next || previous == State::CLOSED || previous == State::FAILED) {
            return;
        }

        port->state = next;
        listeners = port->stateListeners.snapshot;
    }

    port->stateCondition.notify_all();
    notifyState(port, listeners, previous, next);
}

/** Drain the one producer-published initial transition before later owner-domain observations.
 * @param port Endpoint whose connection admission already published CONNECTING.
 */
void notifyConnecting(const std::shared_ptr<detail::EndpointPort> &port) noexcept {
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    {
        std::lock_guard lock(port->mutex);

        if (!port->connectingNotification) {
            return;
        }

        port->connectingNotification = false;
        listeners = std::move(port->connectingListeners);
    }

    notifyState(port, listeners, EndpointState::NOT_CONNECTED, EndpointState::CONNECTING);
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

    if (retirementFrame) {
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
    observeState(port, SubscriptionState::CLOSING);
    std::shared_ptr<const detail::ListenerSnapshot> listeners;
    std::vector<std::u16string> symbols;
    {
        std::lock_guard lock(port->mutex);
        listeners = std::move(port->listeners);
        symbols = std::move(port->symbols);
    }

    listeners.reset();
    symbols.clear();

    if (port->listenerLedger->count.load(std::memory_order_acquire) != 0) {
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

/** Atomically replace one bounded frontend desired snapshot.
 * @param port Detached producer state.
 * @param inputs Borrowed bounded requested selectors.
 * @param mutation Complete desired transformation.
 */
void mutateSymbols(const std::shared_ptr<detail::SubscriptionPort> &port, std::span<const std::string_view> inputs,
                   SymbolMutation mutation) {
    producerAction([&] {
        std::vector<std::u16string> parsed;
        parsed.reserve(std::min(inputs.size(), port->maximumSymbols));

        for (const auto input : inputs) {
            auto symbol = exactSymbol(input, port->maximumSymbolUnits);

            if (std::ranges::find(parsed, symbol) == parsed.end()) {
                if (parsed.size() >= port->maximumSymbols) {
                    reject(ErrorCode::RESOURCE_LIMIT, "subscription-symbol-count");
                }

                parsed.push_back(std::move(symbol));
            }
        }

        {
            std::lock_guard lock(port->mutex);
            requireOpen(*port);
            auto candidate = mutation == SymbolMutation::REPLACE ? std::vector<std::u16string>{} : port->symbols;

            for (auto &symbol : parsed) {
                const auto found = std::ranges::find(candidate, symbol);

                if (mutation == SymbolMutation::REMOVE) {
                    if (found != candidate.end()) {
                        candidate.erase(found);
                    }
                } else if (found == candidate.end()) {
                    candidate.push_back(std::move(symbol));
                }
            }

            if (candidate.size() > port->maximumSymbols) {
                reject(ErrorCode::RESOURCE_LIMIT, "subscription-symbol-count");
            }

            if (candidate != port->symbols) {
                if (port->revision == std::numeric_limits<std::uint64_t>::max()) {
                    reject(ErrorCode::RESOURCE_LIMIT, "subscription-revision");
                }

                port->symbols.swap(candidate);
                ++port->revision;
            }
        }

        wake(port->control);
    });
}

/** Validate the public policy before reserving an endpoint slot.
 * @param options Caller-selected finite policy.
 */
void validateEndpointOptions(const EndpointOptions &options) {
    if (options.eventRegistry && !options.eventRegistry->valid()) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-registry");
    }

    const auto durationValid = [](std::chrono::milliseconds value) {
        return value.count() > 0 && value.count() <= INT_MAX;
    };

    if ((options.contract != DeliveryContract::TICKER && options.contract != DeliveryContract::STREAM) ||
        !durationValid(options.connectTimeout) || !durationValid(options.dnsTimeout) ||
        !durationValid(options.readTimeout) || !durationValid(options.writeTimeout) ||
        !durationValid(options.pollInterval) || !durationValid(options.drainTimeout) || options.maximumAttempts == 0 ||
        (options.maximumAttempts > 1 && !durationValid(options.retryDelay))) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-options");
    }
}

/** Validate and split only plain bounded host:port syntax; existing endpoint validates route spelling.
 * @param address Explicit borrowed address.
 * @return Owning host and nonzero port.
 */
std::pair<std::string, std::uint16_t> parseAddress(std::string_view address) {
    if (address.empty() || address.size() > MAXIMUM_ADDRESS_BYTES ||
        !std::ranges::all_of(address, [](unsigned char value) {
            return value > FIRST_PRINTABLE_ASCII && value <= LAST_PRINTABLE_ASCII;
        })) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
    }

    const auto separator = address.rfind(':');

    if (separator == std::string_view::npos || separator == 0) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
    }

    auto host = address.substr(0, separator);
    const auto portText = address.substr(separator + 1);
    unsigned port = 0;
    const auto converted = std::from_chars(portText.data(), portText.data() + portText.size(), port);

    if (portText.empty() || portText.size() > 5 || converted.ec != std::errc{} ||
        converted.ptr != portText.data() + portText.size() || port == 0 ||
        port > std::numeric_limits<std::uint16_t>::max()) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
    }

    if (host.front() == '[') {
        if (host.size() < 3 || host.back() != ']' || host.find(':') == std::string_view::npos) {
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
        }

        host.remove_prefix(1);
        host.remove_suffix(1);
    } else if (host.find(':') != std::string_view::npos) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
    }

    if (host.empty() || host.size() > MAXIMUM_HOST_BYTES || host.find_first_of("/[]@?=#\\") != std::string_view::npos) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");
    }

    return {std::string(host), static_cast<std::uint16_t>(port)};
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

template <class Event> void Subscription<Event>::clearSymbols() {
    mutateSymbols(port_, {}, SymbolMutation::REPLACE);
}

template <class Event> void Subscription<Event>::close() noexcept {
    if (port_) {
        port_->closeRequested.store(true, std::memory_order_release);
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

        for (const auto type : types) {
            switch (type) {
            case EventType::QUOTE:
                quote = true;
                break;
            case EventType::PROFILE:
                profile = true;
                break;
            default:
                reject(ErrorCode::INVALID_ARGUMENT, "subscription-event-types");
            }
        }

        const auto selection = quote && profile ? detail::EventSelection::BOTH
                               : quote          ? detail::EventSelection::QUOTE
                                                : detail::EventSelection::PROFILE;
        const auto delivery = registeredDelivery ? detail::EventDelivery::REGISTERED
                              : quote            ? detail::EventDelivery::QUOTE
                                                 : detail::EventDelivery::PROFILE;
        auto subscription = std::make_shared<detail::SubscriptionPort>();
        subscription->control = port_->control;
        subscription->endpoint = port_;
        subscription->owner = port_->owner;
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
        {
            std::lock_guard lock(port_->mutex);

            if (port_->closeRequested.load(std::memory_order_acquire) ||
                port_->retired.load(std::memory_order_acquire)) {
                reject(ErrorCode::CLOSED, "endpoint-closed");
            }

            bool registeredQuote = !port_->options.eventRegistry;
            bool registeredProfile = !port_->options.eventRegistry;

            if (port_->options.eventRegistry) {
                for (const auto &registration : port_->options.eventRegistry->registrations()) {
                    registeredQuote = registeredQuote || registration.type == EventType::QUOTE;
                    registeredProfile = registeredProfile || registration.type == EventType::PROFILE;
                }
            }

            const auto missingQuote = quote && !registeredQuote;
            const auto missingProfile = profile && !registeredProfile;

            if (missingQuote || missingProfile) {
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

void Endpoint::connect(std::string_view address) {
    producerAction([&] {
        auto [host, port] = parseAddress(address);
        std::string copiedAddress(address);
        const auto control = port_->control.lock();

        if (!control) {
            reject(ErrorCode::CLOSED, "runtime-closed");
        }

        {
            // This lock pins the dispatch pointer through thread creation and address publication.
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
                if (port_->address == copiedAddress) {
                    return;
                }

                reject(ErrorCode::INVALID_STATE, "endpoint-address-fixed");
            }

            control->runtime->startWorker();
            port_->address = std::move(copiedAddress);
            port_->host = std::move(host);
            port_->port = port;
            port_->connectRequested = true;
            port_->state = EndpointState::CONNECTING;
            port_->connectingListeners = port_->stateListeners.snapshot;
            port_->connectingNotification = true;
            ++control->wakeSequence;
        }

        port_->stateCondition.notify_all();
        control->condition.notify_one();
    });
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

    if (retirementReentered(port_.get())) {
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

    /** Project and deliver one complete event type under a stable listener selection.
     * @tparam Event Existing owning Quote, Profile or their RegisteredEvent variant.
     * @param batch Existing receiver's callback-only projection service.
     * @param listeners Whole-batch immutable callable selection.
     * @return Existing queue acknowledgement/pause action.
     */
    template <class Event>
    mux::BatchAction deliver(const client::SubscriptionBatch &batch,
                             const std::shared_ptr<const ListenerSnapshot> &listeners) {
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
        for (const auto &listener : *listeners) {
            if (port_->closeRequested.load(std::memory_order_acquire)) {
                break;
            }

            try {
                std::get<typename Subscription<Event>::Listener>(listener.callback->function)(projectedEvents);
            } catch (...) {
                failSubscription(port_, {ErrorCode::CALLBACK_FAILURE, "subscription-listener"});

                return mux::BatchAction::PAUSE;
            }
        }

        return mux::BatchAction::CONTINUE;
    }

    public:
    /** Retain only detached state and an explicit complete callback projection profile.
     * @param port Detached logical subscription.
     * @param projectionWork Finite independently replenished callback-boundary seed.
     */
    TypedConsumer(std::shared_ptr<SubscriptionPort> port, client::ReceiverProjectionWork projectionWork)
        : port_(std::move(port)), projectionWork_(projectionWork) {
    }

    /** Invoke ready owning values with no control mutex held.
     * @param batch Existing callback-scoped generic/projection view.
     * @return Queue action; application exceptions fail only this logical subscription.
     */
    mux::BatchAction onBatch(const client::SubscriptionBatch &batch) override {
        const auto endpoint = port_->endpoint.lock();

        if (port_->closeRequested.load(std::memory_order_acquire) || !endpoint) {
            return mux::BatchAction::PAUSE;
        }

        std::shared_ptr<const ListenerSnapshot> listeners;
        {
            std::lock_guard lock(port_->mutex);
            listeners = port_->listeners;
        }

        if (!listeners || listeners->empty()) {
            return mux::BatchAction::CONTINUE;
        }

        try {
            switch (port_->eventDelivery) {
            case EventDelivery::QUOTE:
                return deliver<events::Quote>(batch, listeners);
            case EventDelivery::PROFILE:
                return deliver<events::Profile>(batch, listeners);
            case EventDelivery::REGISTERED:
                return deliver<RegisteredEvent>(batch, listeners);
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
std::unique_ptr<NativeEndpoint> activate(transport::RuntimeBootstrap &bootstrap,
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
    } else {
        queues.stream.emplace(std::move(*queue));
    }

    auto work = native->profile.preparationWork;
    auto recovery =
        transport::TcpRecovery::create(bootstrap, native->profile.endpoint, std::move(native->profile.blueprint),
                                       std::move(queues), native->profile.recoveryOptions, work);

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
 * @return Actual native registration, or detached Error preserving a static failure category.
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
    auto handle = endpoint.recovery->openSubscriptionReceiver(
        endpoint.profile.family, std::span(endpoint.profile.bindings).subspan(plan.firstBinding, plan.bindingCount),
        result->consumer, plan.limits, endpoint.profile.clientLimits, receiverWork, queueWork);

    if (!handle) {
        if (const auto *failure = std::get_if<transport::RecoveryDiagnostic>(&handle.error())) {
            throw Error(recoveryDiagnostic(*failure));
        }

        const auto &failure = std::get<client::ReceiverFailure>(handle.error());

        if (const auto *queue = std::get_if<mux::QueueFailure>(&failure.cause)) {
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

    std::vector<std::u16string> symbols;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock(subscription.port->mutex);
        revision = subscription.port->revision;

        if (subscription.hasSubmitted && revision == subscription.submittedRevision) {
            return;
        }

        symbols = subscription.port->symbols;
    }

    std::vector<core::SubscriptionRequest> requests;
    const auto &plan = receivingEventPlan(endpoint.profile, subscription.port->eventSelection);

    if (plan.bindingCount == 0 || symbols.size() > endpoint.profile.clientLimits.commandEntries / plan.bindingCount) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-route-entries"});
    }

    requests.reserve(symbols.size() * plan.bindingCount);

    for (const auto &binding : std::span(endpoint.profile.bindings).subspan(plan.firstBinding, plan.bindingCount)) {
        const auto context = endpoint.profile.schema.recordContext(binding.localId);

        for (const auto &symbol : symbols) {
            requests.push_back({{context, symbol}, {}});
        }
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

    if (!polled) {
        if (!firstPoll && polled.error().error == transport::RecoveryError::WORK_LIMIT) {
            return EndpointProgress::TURN_EXHAUSTED;
        }

        failEndpoint(endpoint.port, recoveryDiagnostic(polled.error()));

        // A fixed turn profile refusal is visible. Existing owner fallback retires on this same worker;
        // retrying identical insufficient work indefinitely would conceal a broken admission profile.
        return EndpointProgress::RETIRED;
    }

    const auto &status = *polled;

    if (status.first.error != transport::RecoveryError::NONE) {
        std::lock_guard lock(endpoint.port->mutex);

        if (endpoint.port->diagnostic.code == ErrorCode::NONE) {
            endpoint.port->diagnostic = recoveryDiagnostic(status.first);
        }
    }

    if (!endpoint.draining && !status.terminal) {
        observeState(endpoint.port, status.phase == transport::RecoveryPhase::ACTIVE ? EndpointState::CONNECTED
                                                                                     : EndpointState::CONNECTING);
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
    endpoint.reset();
    retireEndpoint(port, result);
}
} // namespace

RuntimeImpl::RuntimeImpl(transport::RuntimeBootstrap bootstrap, RuntimeOptions options)
    : bootstrap(std::move(bootstrap)), options(std::move(options)), control(std::make_shared<RuntimeControl>()) {
    control->owner = std::this_thread::get_id();
    control->endpoints.resize(this->options.maximumEndpoints);
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

    if (shutdownInProgress || retirementFrame) {
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
                    connectRequested = port->connectRequested;
                }

                // Capture admission before draining its notification: a concurrent later connect must wait
                // for the next turn instead of activating ahead of its deferred CONNECTING selection.
                notifyConnecting(port);
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

ApiRuntime::ApiRuntime(RuntimeOptions options) {
    producerAction([&] {
        if (!options.maximumEndpoints || !options.maximumSubscriptionsPerEndpoint ||
            !options.maximumSymbolsPerSubscription || !options.maximumListenersPerSubscription ||
            !options.maximumStateListenersPerEndpoint || !options.maximumStateListenersPerSubscription ||
            !options.maximumSymbolUnits || options.maximumSymbolUnits > 128 ||
            !options.maximumEventTypeArgumentsPerSubscription || options.dnsServers.size() > 4 ||
            options.dnsPort == 0) {
            reject(ErrorCode::INVALID_ARGUMENT, "runtime-options");
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

        auto bootstrap = transport::RuntimeBootstrap::create();

        if (!bootstrap) {
            throw Error({ErrorCode::BOOTSTRAP_FAILURE, bootstrap.error().stage, bootstrap.error().nativeCode,
                         static_cast<std::uint32_t>(bootstrap.error().error)});
        }

        impl_ = std::make_unique<detail::RuntimeImpl>(std::move(*bootstrap), std::move(options));
    });
}

ApiRuntime::~ApiRuntime() = default;

std::shared_ptr<Endpoint> ApiRuntime::createEndpoint(EndpointOptions options) {
    return producerAction([&] {
        validateEndpointOptions(options);
        const auto control = impl_->control;
        auto port = std::make_shared<detail::EndpointPort>();
        port->control = control;
        port->owner = control->owner;
        port->limits = impl_->options;
        port->options = options;
        port->stateListeners = makeStateListeners(port, impl_->options.maximumStateListenersPerEndpoint);
        port->subscriptions.resize(impl_->options.maximumSubscriptionsPerEndpoint);
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
