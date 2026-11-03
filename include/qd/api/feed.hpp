// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/event_registry.hpp"
#include "qd/client/endpoint_options.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace org::ttldtor::qdcxx::api {
namespace detail {
struct RuntimeImpl;
struct EndpointPort;
struct SubscriptionPort;
} // namespace detail

/** Detached high-level failures. No peer payload, credential or address is diagnostic text. */
enum class ErrorCode {
    NONE,               ///< No failure has been recorded.
    INVALID_ARGUMENT,   ///< Caller input does not satisfy the public contract.
    INVALID_STATE,      ///< Operation conflicts with the accepted lifecycle or fixed address.
    RESOURCE_LIMIT,     ///< A finite configured capacity or native thread reservation was exhausted.
    WORK_LIMIT,         ///< The derived bounded preparation or progress allowance was exhausted.
    ALLOCATION_FAILURE, ///< Required owned storage could not be allocated.
    WRONG_THREAD,       ///< A checked operation used an incompatible creator thread.
    CLOSED,             ///< Admission is fenced by endpoint, subscription or runtime retirement.
    CALLBACK_FAILURE,   ///< Event listener failure closes its subscription; advisory state failure is recorded
                        ///< separately.
    TRANSPORT_FAILURE,  ///< Connection, recovery or native I/O failed; inspect the static stage and codes.
    PROTOCOL_FAILURE,   ///< Peer input failed the existing bounded protocol profile.
    BOOTSTRAP_FAILURE,  ///< The explicit process/main-thread runtime bootstrap could not be acquired.
    DNS_CONFIGURATION,  ///< No supported bounded IPv4 resolver snapshot could be obtained.
    UNSUPPORTED_PROFILE ///< The requested combination is outside this public receiving profile.
};

/** Static/numeric failure snapshot, independent of runtime and native owners. */
struct Diagnostic {
    ErrorCode code = ErrorCode::NONE; ///< High-level failure category.
    std::string_view stage = "none";  ///< Static library-owned label, never peer or caller text.
    int nativeCode = 0;               ///< Native code when available; zero otherwise.
    std::uint32_t detailCode = 0;     ///< Nested source category, meaningful with the static stage.
};

/** Synchronous public-API rejection; asynchronous failures are exposed by getDiagnostic(). */
class Error final : public std::runtime_error {
    Diagnostic diagnostic_;

    public:
    /** Preserve one detached static diagnostic.
     * @param diagnostic Library-owned static stage and numeric failure.
     */
    explicit Error(Diagnostic diagnostic);
    /** Inspect the rejection after its source owner has retired.
     * @return Detached diagnostic retained by this exception.
     */
    [[nodiscard]] Diagnostic diagnostic() const noexcept;
};

/** Finite high-level registry limits; these are capacities, not process RSS or time guarantees. */
struct RuntimeOptions {
    std::size_t maximumEndpoints = 4;                ///< Maximum pending, active and retiring endpoint slots together.
    std::size_t maximumSubscriptionsPerEndpoint = 8; ///< Pending, active and retiring registrations together.
    std::size_t maximumSymbolsPerSubscription = 16;  ///< Maximum distinct exact ASCII selectors per subscription.
    std::size_t maximumListenersPerSubscription = 4; ///< Pending, active and selected/retiring listener slots together.
    std::size_t maximumSymbolUnits = 128;            ///< Maximum units of one exact printable ASCII selector.
    std::vector<transport::DnsServer>
        dnsServers;             ///< Explicit IPv4 resolvers; empty discovers a bounded system snapshot.
    std::uint16_t dnsPort = 53; ///< Shared UDP/TCP resolver port, in 1..65535.
    std::size_t maximumStateListenersPerEndpoint = 4; ///< Accepted and selected/retiring endpoint state observers.
    std::size_t maximumStateListenersPerSubscription =
        4; ///< Accepted and selected/retiring subscription state observers.
    std::size_t maximumEventTypeArgumentsPerSubscription = 64;
    ///< Maximum raw type arguments inspected during one subscription creation, including duplicates.
    std::size_t maximumPendingStateChangesPerEndpoint = 8; ///< Pending/executing/retiring state selections together.
};

/** Existing delivery semantics selected for one high-level endpoint. */
enum class DeliveryContract {
    TICKER, ///< Initial/latest delivery with possible conflation, as for ordinary Java FEED Quote subscriptions.
    STREAM  ///< Every admitted selected source record, including equal consecutive records.
};

/** Endpoint policy; defaults need no caller-owned queue or turn-work objects. */
struct EndpointOptions {
    DeliveryContract contract = DeliveryContract::TICKER; ///< Selected source contract for this endpoint.
    std::chrono::milliseconds connectTimeout{5000};       ///< Whole native connection attempt deadline.
    std::chrono::milliseconds dnsTimeout{5000};           ///< Whole two-family lookup deadline.
    std::chrono::milliseconds readTimeout{30000};         ///< Connected input inactivity deadline.
    std::chrono::milliseconds writeTimeout{5000};         ///< Pending-output inactivity deadline.
    std::chrono::milliseconds pollInterval{1};    ///< SDK worker progress cadence; not a thread per subscription.
    std::chrono::milliseconds retryDelay{250};    ///< Delay between finite fresh recovery attempts.
    std::chrono::milliseconds drainTimeout{1000}; ///< Local admitted-prefix retirement allowance.
    std::uint32_t maximumAttempts = 3;            ///< Total connection attempts, including the initial attempt.
    std::optional<EventRegistry> eventRegistry; ///< Immutable owning snapshot; absence selects built-in Quote/Profile.
};

/** Last SDK-worker observation, never an upstream acknowledgement or a whole-network guarantee. */
enum class EndpointState {
    NOT_CONNECTED, ///< Created without an accepted connection request.
    CONNECTING,    ///< Connection, protocol negotiation or finite recovery is in progress.
    CONNECTED,     ///< The existing recovery wrapper reports an active receiving session.
    CLOSING,       ///< Reserved close is accepted; native and callback retirement may remain.
    CLOSED,        ///< Native state and event captures retired; opt-in state observers may still be retiring.
    FAILED         ///< A failure is retained; inspect the diagnostic and await actual retirement.
};

/** Local logical-subscription lifecycle. Symbol mutations remain asynchronous. */
enum class SubscriptionState {
    CONFIGURED, ///< Created; worker registration has not yet become active.
    ACTIVE,     ///< Worker registration is active; accepted symbol changes may still be pending.
    CLOSING,    ///< Reserved close is accepted; an already selected callback may still execute.
    CLOSED,     ///< Registration and event captures retired; opt-in state observers may still be retiring.
    FAILED      ///< A failure is retained; this subscription no longer admits ordinary mutations.
};

/** Result category for a creator-thread current-state wait, independent of lifecycle failure codes. */
enum class WaitStatus {
    REACHED,         ///< The current state equals the requested target.
    TERMINAL,        ///< A different absorbing CLOSED or FAILED state prevents reaching the target.
    CANCELLED,       ///< Cancellation was requested before either state condition was observed.
    DEADLINE_EXPIRED ///< The steady-clock deadline was reached before another condition was observed.
};

/** Detached observation captured atomically when a current-state wait finishes.
 * @tparam State EndpointState or SubscriptionState, matching the waited owner.
 */
template <class State> struct StateWaitResult {
    WaitStatus status;     ///< Exact target, other terminal, cancellation or deadline result.
    State state;           ///< Current lifecycle state under the publishing owner's mutex.
    Diagnostic diagnostic; ///< Owner's first lifecycle failure at that observation; advisory errors stay separate.
};

/** Actual local endpoint retirement; DRAINED is limited to the admitted local prefix. */
struct CloseResult {
    bool drained = false;         ///< True only for actual DRAINED without expiry or unresolved gap.
    bool deadlineExpired = false; ///< Local drain allowance escalated to abort.
    bool hadGap = false;          ///< Recovery or queue evidence retained an upstream/data gap.
    Diagnostic diagnostic;        ///< First retained failure, independent of the closed worker state.
};

class Endpoint;
class Feed;

/** Independently owning supported event value; inspect its actual class with std::visit or std::get_if. */
using RegisteredEvent = std::variant<events::Quote, events::Profile>;

/** Typed logical subscription, backed by the existing receiver and shared queue.
 * Methods are producer-safe. Event-listener calls are serialized on the runtime's SDK worker.
 * A listener's span lasts only through that invocation; copying an Event produces an owning value.
 * @tparam Event Existing events::Quote, events::Profile or the owning RegisteredEvent variant.
 */
template <class Event> class Subscription final {
    static_assert(std::is_same_v<Event, events::Quote> || std::is_same_v<Event, events::Profile> ||
                  std::is_same_v<Event, RegisteredEvent>);
    std::shared_ptr<detail::SubscriptionPort> port_;
    friend class Feed;
    explicit Subscription(std::shared_ptr<detail::SubscriptionPort> port) noexcept;

    public:
    using Listener =
        std::function<void(std::span<const Event>)>; ///< Synchronous callback receiving ready owning values.
    using ListenerId = std::uint64_t;                ///< Non-reused listener identity within this subscription.
    using StateChangeListener =
        std::function<void(SubscriptionState, SubscriptionState)>; ///< Advisory old/new observation.
    using StateChangeListenerId =
        std::uint64_t; ///< Non-reused state-observer identity, separate from event identities.
    /** Add an advisory state observer; no initial notification is made.
     * Selection is atomic with each distinct transition; selected observers run in registration order outside locks.
     * Calls use the SDK worker, or creator-thread retirement if this runtime has never started its worker.
     * Exceptions are retained by getStateChangeDiagnostic(), without failing the subscription or stopping selection.
     * Captures remain charged until destroyed; endpoint await includes their final retirement.
     * @param listener Nonempty callback receiving actual old/new states by value; application blocking is unbounded.
     * @return Identity for removeStateChangeListener; throws Error for closed admission or finite-capacity refusal.
     */
    StateChangeListenerId addStateChangeListener(StateChangeListener listener);
    /** Remove an observer from future transition selections, including after close.
     * @param id Previously returned state-observer identity; absence is an idempotent no-op.
     * An already selected invocation may still run. Capture destruction occurs outside all control locks.
     */
    void removeStateChangeListener(StateChangeListenerId id);
    /** Inspect the first advisory observer exception independently of the subscription lifecycle.
     * @return Detached CALLBACK_FAILURE or NONE; owner state and getDiagnostic() are unaffected.
     */
    [[nodiscard]] Diagnostic getStateChangeDiagnostic() const noexcept;
    /** Request nonblocking close; native/callback retirement stays with the runtime owner. */
    ~Subscription();
    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;
    /** Add a bounded listener without invoking it inline.
     * @param listener Nonempty callback; its allocations and blocking behavior are application-owned.
     * Throwing from a listener fails this subscription, without retrying it or failing other clients.
     * Captures are released outside all control locks; close/await quiescence releases retained captures.
     * @return Identity usable by removeEventListener; throws Error on synchronous rejection.
     */
    ListenerId addEventListener(Listener listener);
    /** Remove a listener for future worker observations; an already selected callback may finish.
     * @param id Previously returned listener identity; absence is an idempotent no-op.
     */
    void removeEventListener(ListenerId id);
    /** Accept an exact selector; application return is not a wire acknowledgement.
     * @param symbol Nonempty printable ASCII, within runtime limits, without All/wildcard syntax.
     */
    void addSymbols(std::string_view symbol);
    /** Atomically accept several exact selectors, with duplicates coalesced.
     * @param symbols Complete bounded set to add.
     */
    void addSymbols(std::initializer_list<std::string_view> symbols);
    /** Accept removal of one selector; absence is an idempotent no-op.
     * @param symbol Exact selector to remove.
     */
    void removeSymbols(std::string_view symbol);
    /** Replace the complete accepted set atomically. Backend failure is reported by getDiagnostic().
     * @param symbols New bounded exact set; empty withdraws all interests.
     */
    void setSymbols(std::initializer_list<std::string_view> symbols);
    /** Withdraw all accepted selectors asynchronously. */
    void clearSymbols();
    /** Request closure without waiting, including from a listener; ordinary capacity cannot block it. */
    void close() noexcept;
    /** Inspect a detached lifecycle observation.
     * @return Last observed logical lifecycle.
     */
    [[nodiscard]] SubscriptionState getState() const noexcept;
    /** Wait for a current logical state without progressing or closing the subscription.
     * Only the runtime creator may wait, including after runtime/endpoint destruction. Worker/foreign calls
     * reject WRONG_THREAD; any creator callback/capture-retirement context rejects INVALID_STATE, even if ready.
     * The target is checked first, then a different CLOSED/FAILED, cancellation and the deadline, under one mutex.
     * Intermediate transitions may be missed. CLOSED is not an opt-in state-observer quiescence barrier.
     * @param target Valid current-state target; invalid enum values reject INVALID_ARGUMENT through Error.
     * @param deadline Absolute steady-clock deadline; max means no deadline. It does not bound OS scheduling.
     * @param stop Caller-owned cancellation token; cancellation has no lifecycle side effects.
     * @return Detached current state, owner diagnostic and reason; no state observer or native work is registered.
     * @throws Error For invalid target, wrong thread or creator retirement context.
     * @throws std::system_error If native C++ synchronization fails.
     */
    [[nodiscard]] StateWaitResult<SubscriptionState>
    awaitState(SubscriptionState target,
               std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(),
               std::stop_token stop = {}) const;
    /** Inspect the first asynchronous failure.
     * @return Static/numeric detached diagnostic, or NONE.
     */
    [[nodiscard]] Diagnostic getDiagnostic() const noexcept;
};

extern template class Subscription<events::Quote>;
extern template class Subscription<events::Profile>;
extern template class Subscription<RegisteredEvent>;

/** Endpoint's typed registration entry point; owns no SDK thread or bootstrap. */
class Feed final {
    std::shared_ptr<detail::EndpointPort> port_;
    friend class Endpoint;
    explicit Feed(std::shared_ptr<detail::EndpointPort> port);
    /** Preserve the existing single-class listener shape while selecting that class's registered routes.
     * @param type Existing supported projection.
     * @return Unpublished-or-atomically-registered producer port; throws Error on refusal.
     */
    std::shared_ptr<detail::SubscriptionPort> createSubscriptionPort(EventType type);
    /** Normalize one bounded borrowed input before atomically publishing a logical registration.
     * @param types Raw supported type arguments, including duplicates, borrowed only during the call.
     * @param registeredDelivery True selects owning variants; false is used only by the one-type wrapper.
     * @return One port with its complete immutable selection and exact listener value shape.
     */
    std::shared_ptr<detail::SubscriptionPort> createSubscriptionPort(std::span<const EventType> types,
                                                                     bool registeredDelivery);

    public:
    /** Create a bounded logical registration, before or after connect, without application polling.
     * All registered records of the selected type participate; an unregistered type rejects synchronously.
     * @tparam Event Existing Quote or Profile, explicitly registered for this endpoint or provided by its defaults.
     * @return Producer handle; native preparation failures are observed asynchronously by its diagnostic.
     */
    template <class Event> [[nodiscard]] std::shared_ptr<Subscription<Event>> createSubscription() {
        static_assert(std::is_same_v<Event, events::Quote> || std::is_same_v<Event, events::Profile>);
        auto subscription = std::shared_ptr<Subscription<Event>>(new Subscription<Event>({}));
        subscription->port_ =
            createSubscriptionPort(std::is_same_v<Event, events::Quote> ? EventType::QUOTE : EventType::PROFILE);

        return subscription;
    }

    /** Create one logical subscription for an immutable normalized set of supported registered types.
     * Raw argument count is checked before inspection and may reject RESOURCE_LIMIT; empty/unknown enums
     * reject INVALID_ARGUMENT.
     * Duplicates coalesce. Closed admission precedes missing registered types and logical-slot checks.
     * Every requested type must be registered; refusal opens no native/producer registration.
     * @param types Caller-owned bounded type list, borrowed only during this call; Quote/Profile are supported.
     * @return One variant-valued producer handle; throws Error on atomic validation/resource refusal.
     */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>> createSubscription(std::span<const EventType> types);
    /** Create the same immutable type selection from an inline argument list.
     * @param types Bounded raw list; duplicates count toward maximumEventTypeArgumentsPerSubscription then coalesce.
     * @return One owning variant subscription, or the same synchronous Error as the span overload.
     */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>>
    createSubscription(std::initializer_list<EventType> types);
};

/** Producer endpoint handle; destruction requests asynchronous close and never joins its own worker. */
class Endpoint final {
    std::shared_ptr<detail::EndpointPort> port_;
    std::shared_ptr<Feed> feed_;
    friend class ApiRuntime;
    explicit Endpoint(std::shared_ptr<detail::EndpointPort> port);

    public:
    using StateChangeListener = std::function<void(EndpointState, EndpointState)>; ///< Advisory old/new observation.
    using StateChangeListenerId = std::uint64_t; ///< Non-reused state-observer identity within this endpoint.
    /** Add an advisory state observer without an initial notification or inline invocation.
     * Selection is atomic with each distinct transition; selected observers run in registration order outside locks.
     * Calls use the SDK worker, or creator-thread retirement if this runtime has never started its worker.
     * Exceptions are retained by getStateChangeDiagnostic(), without changing owner state, diagnostic or CloseResult.
     * CLOSED preserves event-capture/native retirement; these opt-in observers may still retire until endpoint await.
     * @param listener Nonempty old/new state callback; captures and application blocking remain application-owned.
     * @return Identity for removeStateChangeListener; throws Error for closed admission or finite-capacity refusal.
     */
    StateChangeListenerId addStateChangeListener(StateChangeListener listener);
    /** Remove an observer from future selections, including after close; a selected invocation may still run.
     * @param id Previously returned identity; absence is an idempotent no-op. Captures are destroyed outside locks.
     */
    void removeStateChangeListener(StateChangeListenerId id);
    /** Inspect the first advisory state-observer exception separately from native/lifecycle failure.
     * @return Detached CALLBACK_FAILURE or NONE; getDiagnostic(), state and CloseResult remain unaffected.
     */
    [[nodiscard]] Diagnostic getStateChangeDiagnostic() const noexcept;
    ~Endpoint();
    Endpoint(const Endpoint &) = delete;
    Endpoint &operator=(const Endpoint &) = delete;
    /** Inspect the stable typed feed entry point.
     * @return Shared producer facade; keeping it does not extend the native runtime lifetime.
     */
    [[nodiscard]] std::shared_ptr<Feed> getFeed() const noexcept;
    /** Start an asynchronous plain binary-QTP connection; no I/O or callbacks run on this caller.
     * The first accepted address is fixed. Repeating it while logically online is a no-op; after disconnect
     * the same address resumes with a fresh source epoch. Another address or
     * reconnecting a closed/failed endpoint rejects. Automatic finite recovery remains internal.
     * @param address ASCII host:port or [IPv6]:port; no TLS/URI/login/configuration extensions.
     */
    void connect(std::string_view address);
    /** Disconnect logically now, retaining desired interests and projected cache while physically offline.
     * Actual native retirement is asynchronous. New old-attempt native input is fenced immediately;
     * an already selected whole listener snapshot may finish. After native pause quiescence, projected cache
     * may replay to new/changed subscriptions with its original source identity. State observations remain ordered.
     * CLOSED/FAILED and repeated disconnection are no-ops. No SDK thread starts before an initial connect.
     * @throws Error If finite notification/revision admission refuses; state, intent and fence stay unchanged.
     */
    void disconnect();
    /** Disconnect now and request real asynchronous SDK cache/pending-data clear, including while disconnected.
     * NOT_CONNECTED is an immediate logical fence, not clear-completion acknowledgement. Every new data/read
     * selection is fenced immediately; caller-owned Event copies and retained immutable batches stay valid.
     * Existing desired symbols/listeners survive. Future fresh-source activation resets old source data too.
     * @throws Error For finite admission refusal; no partial state, intent, clear or delivery-fence change.
     */
    void disconnectAndClear();
    /** Request a fresh source attempt with the same fixed address and desired interests.
     * No-op before connect, after disconnect, or in CLOSED/FAILED. Actual attempts/work are never replenished;
     * asynchronous source reset/attempt exhaustion remains observable. No callback or I/O runs inline.
     * @throws Error For finite notification/revision admission refusal, without partial mutation.
     */
    void reconnect();
    /** Request admitted-prefix drain, escalating after drainTimeout; never wait or consume an ordinary slot. */
    void close() noexcept;
    /** Request close and await actual native/callback quiescence; only the runtime creator may wait.
     * Worker/listener and other threads reject with WRONG_THREAD before waiting. Repeated waits return
     * the same detached terminal. If no worker was started, creator-thread retirement needs no worker.
     * Reentering retirement of this endpoint from capture destruction rejects with INVALID_STATE.
     * @return Detached actual local outcome. Application callbacks and OS retirement have no hard time bound.
     */
    [[nodiscard]] CloseResult closeAndAwaitTermination();
    /** Inspect the latest committed producer/worker state observation.
     * @return Detached lifecycle state, not peer acknowledgement.
     */
    [[nodiscard]] EndpointState getState() const noexcept;
    /** Wait for a current endpoint state without connecting, closing or progressing native work.
     * Only the runtime creator may wait. Worker/foreign calls reject WRONG_THREAD; any creator callback/
     * capture-retirement context rejects INVALID_STATE, even if already ready. Synchronous rejections throw Error.
     * Under one mutex, precedence is target, different CLOSED/FAILED, cancellation, then deadline.
     * Intermediate transitions may be missed. Use closeAndAwaitTermination for actual callback/native quiescence.
     * @param target Valid current-state target; invalid enum values reject INVALID_ARGUMENT through Error.
     * @param deadline Absolute steady-clock deadline; max means no deadline. It does not bound OS scheduling.
     * @param stop Caller-owned cancellation token; cancellation has no lifecycle side effects.
     * @return Detached current state, owner diagnostic and reason; state observers may still be executing.
     * @throws Error For invalid target, wrong thread or creator retirement context.
     * @throws std::system_error If native C++ synchronization fails.
     */
    [[nodiscard]] StateWaitResult<EndpointState>
    awaitState(EndpointState target,
               std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(),
               std::stop_token stop = {}) const;
    /** Inspect first asynchronous failure.
     * @return Static/numeric detached diagnostic, or NONE.
     */
    [[nodiscard]] Diagnostic getDiagnostic() const noexcept;
};

/** Explicit process/main-thread SDK lifetime and joined worker owner.
 * The default constructor creates on main before application threads; the explicit bootstrap overload may
 * borrow a pre-thread owner later on that same main thread. Coordinate all foreign c-ares users.
 * Destroy this runtime on its creating thread; owned mode requires application threads to retire first.
 * Borrowed mode retains the host owner until runtime destruction. Endpoint/subscription handles may outlive
 * this object as closed detached ports. The worker starts lazily on the first connect request.
 */
class ApiRuntime final {
    std::unique_ptr<detail::RuntimeImpl> impl_;

    public:
    /** Acquire an owned bootstrap and a bounded DNS configuration snapshot, without connecting.
     * @param options Finite registry limits and optional explicit IPv4 DNS settings.
     */
    explicit ApiRuntime(RuntimeOptions options = {});
    /** Borrow existing same-linked-instance SDK lifetime without global init/cleanup or a connection.
     * @param bootstrap Live owner created on main before application threads; retain its stable state until
     * this runtime is destroyed, even after close. Moving its facade preserves the borrow. The creating
     * main thread must construct/destroy this runtime. Foreign SDK/user obligations remain caller preconditions.
     * @param options Finite settings; invalid token/provenance rejects INVALID_ARGUMENT, wrong creator thread
     * rejects WRONG_THREAD, exhausted borrow capacity rejects RESOURCE_LIMIT, all before native activation.
     */
    explicit ApiRuntime(transport::RuntimeBootstrap &bootstrap, RuntimeOptions options = {});
    ~ApiRuntime();
    ApiRuntime(const ApiRuntime &) = delete;
    ApiRuntime &operator=(const ApiRuntime &) = delete;
    ApiRuntime(ApiRuntime &&) = delete;
    ApiRuntime &operator=(ApiRuntime &&) = delete;
    /** Create an independent producer endpoint with its immutable registry or built-in full Quote/Profile defaults.
     * Failed wrapper/control allocation rolls back its reservation without publishing an active endpoint.
     * Producer dispatch uses a lifetime-gated detached control port, never an unprotected RuntimeImpl*.
     * @param options Existing policy/deadlines and optional immutable registry; finite capacities use RuntimeOptions.
     * @return Endpoint facade; no socket is opened and no listener is called inline.
     */
    [[nodiscard]] std::shared_ptr<Endpoint> createEndpoint(EndpointOptions options = {});
    /** Close every endpoint, join the SDK worker and preserve bootstrap until safe main-thread cleanup.
     * Only the creating main/owner thread may call this operation; worker calls reject immediately.
     * Reentrant shutdown or shutdown from accepted capture destruction rejects with INVALID_STATE.
     */
    void closeAndAwaitTermination();
};
} // namespace org::ttldtor::qdcxx::api
