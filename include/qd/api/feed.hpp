// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/callback_execution.hpp"
#include "qd/api/event_registry.hpp"
#include "qd/api/history_snapshot.hpp"
#include "qd/api/subscription_selector.hpp"
#include "qd/client/endpoint_options.hpp"
#include "qd/events/analytic_order.hpp"
#include "qd/events/candle.hpp"
#include "qd/events/candle_symbol.hpp"
#include "qd/events/configuration.hpp"
#include "qd/events/greeks.hpp"
#include "qd/events/market_maker.hpp"
#include "qd/events/nuam_time_and_sale.hpp"
#include "qd/events/nuam_trade.hpp"
#include "qd/events/option_sale.hpp"
#include "qd/events/order.hpp"
#include "qd/events/order_imbalance.hpp"
#include "qd/events/otc_markets_order.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/series.hpp"
#include "qd/events/spread_order.hpp"
#include "qd/events/summary.hpp"
#include "qd/events/text_events.hpp"
#include "qd/events/theo_price.hpp"
#include "qd/events/time_and_sale.hpp"
#include "qd/events/trade.hpp"
#include "qd/events/trade_eth.hpp"
#include "qd/events/underlying.hpp"
#include "qd/transport/aggregation.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
class EndpointConfiguration;
struct EndpointConfigurationSnapshot;

namespace detail {
struct RuntimeImpl;
struct EndpointPort;
struct SubscriptionPort;
} // namespace detail

/** Detached high-level failures. No peer payload, credential or address is diagnostic text. */
enum class ErrorCode {
    NONE,                ///< No failure has been recorded.
    INVALID_ARGUMENT,    ///< Caller input does not satisfy the public contract.
    INVALID_STATE,       ///< Operation conflicts with the accepted lifecycle or fixed address.
    RESOURCE_LIMIT,      ///< A finite configured capacity or native thread reservation was exhausted.
    WORK_LIMIT,          ///< The derived bounded preparation or progress allowance was exhausted.
    ALLOCATION_FAILURE,  ///< Required owned storage could not be allocated.
    WRONG_THREAD,        ///< A checked operation used an incompatible creator thread.
    CLOSED,              ///< Admission is fenced by endpoint, subscription or runtime retirement.
    CALLBACK_FAILURE,    ///< Event listener failure closes its subscription; advisory state failure is recorded
                         ///< separately.
    TRANSPORT_FAILURE,   ///< Connection, recovery or native I/O failed; inspect the static stage and codes.
    PROTOCOL_FAILURE,    ///< Peer input failed the existing bounded protocol profile.
    BOOTSTRAP_FAILURE,   ///< The explicit process/main-thread runtime bootstrap could not be acquired.
    DNS_CONFIGURATION,   ///< No supported bounded IPv4 resolver snapshot could be obtained.
    UNSUPPORTED_PROFILE, ///< The requested combination is outside this public receiving profile.
    EXECUTOR_REJECTED,   ///< Host explicitly refused an event task; no retry.
    EXECUTOR_FAILURE,    ///< Host post threw before completion.
    EXECUTOR_DROPPED,    ///< Accepted task was destroyed without running.
    EXECUTOR_CONTRACT,   ///< Host refused or threw after actual execution began.
    CALLBACK_CANCELLED,  ///< Queued event selection cancelled by close or source reset; advisory.
    TAPE_IO_FAILURE,     ///< The selected regular local file could not be opened or read.
    TAPE_TIMING_RANGE,   ///< The relative tape span or a monotonic playback deadline exceeded its bound.
    TAPE_RESTART_TIMEOUT ///< The old tape epoch could not retire and reset within the restart allowance.
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
    std::size_t maximumSymbolsPerSubscription = 16;  ///< Maximum distinct logical selector identities per subscription.
    std::size_t maximumListenersPerSubscription = 4; ///< Pending, active and selected/retiring listener slots together.
    std::size_t maximumSymbolUnits = 128; ///< Maximum event-text units in one activated printable ASCII selector.
    std::vector<transport::DnsServer>
        dnsServers;             ///< Explicit IPv4 resolvers; empty discovers a bounded system snapshot.
    std::uint16_t dnsPort = 53; ///< Shared UDP/TCP resolver port, in 1..65535.
    std::size_t maximumStateListenersPerEndpoint = 4; ///< Accepted and selected/retiring endpoint state observers.
    std::size_t maximumStateListenersPerSubscription =
        4; ///< Accepted and selected/retiring subscription state observers.
    std::size_t maximumEventTypeArgumentsPerSubscription = 64;
    ///< Maximum raw type arguments inspected during one subscription creation, including duplicates.
    std::size_t maximumPendingStateChangesPerEndpoint = 8; ///< Pending/executing/retiring state selections together.
    std::size_t maximumChangeListenersPerSubscription = 4; ///< Accepted, selected and capture-retiring observers.
    std::size_t maximumPendingSubscriptionChanges =
        8; ///< Pending/executing symbol notification phases per subscription.
    std::size_t maximumSubscriptionChangesPerTurn = 32;  ///< Shared runtime admission credits during one callback turn.
    std::size_t maximumCustomCallbackSubscriptions = 16; ///< Simultaneous selected/custom reservations.
    std::size_t maximumCallbackBytes =
        32 * 1024 * 1024; ///< Conservative logical owned packet budget, excluding host captures.
};

/** Existing delivery semantics selected for one high-level endpoint. */
enum class DeliveryContract {
    TICKER, ///< Initial/latest delivery with possible conflation, as for ordinary Java FEED Quote subscriptions.
    STREAM, ///< Every admitted selected source record, including equal consecutive records.
    HISTORY ///< Indexed shared storage and per-subscription range delivery; only verified HISTORY routes qualify.
};

/** Finite shared HISTORY storage policy, separate from per-subscription delivery FIFO reservations. */
struct HistoryReceivingOptions {
    std::size_t maximumPoints = 4096; ///< Shared distinct indexed points across all physical record/symbol partitions.
    std::size_t maximumFrames = 256;  ///< Peak retained source frames, including old plus incoming replacement.
    std::size_t maximumBytes = 64 * 1024 * 1024; ///< Peak shared source-event backing; excludes session/desired scopes.
    std::size_t maximumResidentBytes = 128 * 1024 * 1024; ///< Collector fixed backing plus shared source-frame peak.
    std::size_t pendingUpdatesPerSubscription = 128;      ///< Independent collector update FIFO item capacity per view.
    std::size_t maximumObservationBytes = 8 * 1024 * 1024;     ///< Two SDK-owned per-subscription publication banks.
    std::size_t pendingBytesPerSubscription = 4 * 1024 * 1024; ///< Independent whole-source-owner backing per view.
};

/** Maximum endpoint and subscription local aggregation period. */
inline constexpr std::chrono::milliseconds MAXIMUM_LOCAL_AGGREGATION_PERIOD{86400000};

/** Endpoint policy; defaults need no caller-owned queue or turn-work objects. */
struct EndpointOptions {
    static constexpr std::size_t MAXIMUM_BATCH_RECORDS = 4;     ///< Qualified physical callback record ceiling.
    static constexpr std::size_t MAXIMUM_PENDING_RECORDS = 128; ///< Qualified detached record slot ceiling.
    DeliveryContract contract = DeliveryContract::TICKER;       ///< Selected source contract for this endpoint.
    std::chrono::milliseconds connectTimeout{5000};             ///< Whole native connection attempt deadline.
    std::chrono::milliseconds dnsTimeout{5000};                 ///< Whole two-family lookup deadline.
    std::chrono::milliseconds readTimeout{30000};               ///< Connected input inactivity deadline.
    std::chrono::milliseconds writeTimeout{5000};               ///< Pending-output inactivity deadline.
    std::chrono::milliseconds pollInterval{1};    ///< SDK worker progress cadence; not a thread per subscription.
    std::chrono::milliseconds retryDelay{250};    ///< Delay between finite fresh recovery attempts.
    std::chrono::milliseconds drainTimeout{1000}; ///< Local admitted-prefix retirement allowance.
    std::uint32_t maximumAttempts = 3;            ///< Total connection attempts, including the initial attempt.
    std::optional<EventRegistry>
        eventRegistry;           ///< Immutable owning snapshot; absence selects the current built-in market registry.
    bool enableWildcard = false; ///< Explicit controlled-peer STREAM All permission; never enabled by text or peer.
    std::optional<std::chrono::milliseconds> requestedAggregationPeriod; ///< Initial server request; null asks for
                                                                         ///< server default, never an effective result.
    HistoryReceivingOptions history;  ///< Used only by the explicit HISTORY contract; never application work credits.
    bool unitaryOrderSources = false; ///< SDK default/plain/negative sources use unitary bid/ask pairs when true;
                                      ///< explicit sources are independent.
    std::size_t batchRecords = MAXIMUM_BATCH_RECORDS;     ///< Maximum whole physical records per callback selection.
    std::size_t pendingRecords = MAXIMUM_PENDING_RECORDS; ///< Detached record slots per subscription.
    std::chrono::milliseconds aggregationPeriod{0};       ///< Local delivery cadence; zero selects immediate delivery.
};

/** Last SDK-worker observation, never an upstream acknowledgement or a whole-network guarantee. */
enum class EndpointState {
    NOT_CONNECTED, ///< Created without an accepted connection request.
    CONNECTING,    ///< Source opening, protocol negotiation or finite recovery is in progress.
    CONNECTED,     ///< An active network receiving session or a ready local tape source.
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
class FeedFilter;
class FeedFilterContext;
struct FeedFilterCategory;

/** Select how source timestamps affect the delivery of a complete tape envelope. */
enum class TapePlaybackPolicy {
    IMMEDIATE,  ///< Deliver eligible envelopes as soon as the Feed can accept them.
    SOURCE_TIME ///< Pace eligible envelopes by source time on the runtime's monotonic clock.
};

/** Terminal policy for a file-backed endpoint. */
enum class TapeStopMode {
    DRAIN, ///< Apply the endpoint's admitted-prefix close policy.
    ABORT  ///< Discard queued delivery and cancel not-started callbacks without waiting for their execution.
};

/** Finite allowances and playback policy for one local tape read. */
struct TapeSourceOptions {
    std::size_t maximumFileBytes = 64 * 1024 * 1024; ///< Maximum physical input bytes.
    std::size_t maximumFrames = 65'536;              ///< Maximum envelopes, including markers and controls.
    TapePlaybackPolicy playback = TapePlaybackPolicy::IMMEDIATE;
    std::uint32_t speedPercent = 100; ///< Relative speed, from 1% to 10,000%; ignored by IMMEDIATE.
    std::chrono::milliseconds maximumSourceSpan{std::chrono::hours{24}};
    ///< Maximum forward span in SOURCE_TIME, positive and at most 365 days; ignored by IMMEDIATE.
    bool initiallyPaused = false; ///< Open the file and wait for resume before reading the first envelope.
};

/** Independently owning supported event value; inspect its actual class with std::visit or std::get_if. */
using RegisteredEvent =
    std::variant<events::Quote, events::Profile, events::Trade, events::TradeETH, events::Summary, events::TextMessage,
                 events::TextConfiguration, events::Message, events::Configuration, events::Order, events::MarketMaker,
                 events::SpreadOrder, events::AnalyticOrder, events::OtcMarketsOrder, events::OrderImbalance,
                 events::Series, events::Greeks, events::Underlying, events::TheoPrice, events::Candle,
                 events::TimeAndSale, events::OptionSale, events::NuamTrade, events::NuamTimeAndSale>;

/** Independently owned observation of one complete accepted local subscription revision. */
struct SubscriptionSnapshot {
    std::uint64_t revision = 0; ///< Accepted desired revision, never a worker application or peer acknowledgement.
    std::vector<SubscriptionSelector> symbols; ///< Exact typed logical identities, independently owned.
    std::vector<EventType> eventTypes;         ///< Immutable normalized supported business types, not local record IDs.
    std::optional<std::int64_t> fromTime; ///< Time-series mode's signed Unix milliseconds; absent for ordinary mode.
};

using SubscriptionStatistics = core::SubscriptionStatistics; ///< Detached event-delivery lifetime counters and gauges.

/** Ordered subscription-observer phase; replacement removes before adding at the same revision. */
enum class SubscriptionChangeKind {
    ADDED,   ///< Initial nonempty state or additions to an accepted revision.
    REMOVED, ///< Actual removed typed identities; the complete post-change set is already published.
    CLOSED   ///< This observer was removed or its subscription closed; symbols is empty.
};

/** Borrowed notification, valid only through its callback; copy snapshot or symbols to retain owned values. */
struct SubscriptionChange {
    SubscriptionChangeKind kind;                   ///< Actual notification phase.
    const SubscriptionSnapshot &snapshot;          ///< Complete owning backing snapshot, borrowed for this invocation.
    std::span<const SubscriptionSelector> symbols; ///< Phase identities, borrowed from immutable selected storage.
};

/** Advisory observer; callbacks run outside all mutation/container locks and never recursively. */
using SubscriptionChangeListener = std::function<void(const SubscriptionChange &)>;

/** Typed logical subscription, backed by the existing receiver and shared queue.
 * Methods are producer-safe. Event-listener calls are serialized on the runtime's SDK worker.
 * A listener's span lasts only through that invocation; copying an Event produces an owning value.
 * @tparam Event Existing events::Quote, events::Profile, events::Trade, events::TradeETH, events::Summary,
 * events::TextMessage, events::TextConfiguration, events::Message, events::Configuration, events::Order,
 * events::MarketMaker, events::SpreadOrder, events::AnalyticOrder, events::OtcMarketsOrder,
 * events::OrderImbalance, events::Series or RegisteredEvent.
 */
template <class Event> class Subscription final {
    static_assert(std::is_same_v<Event, events::Quote> || std::is_same_v<Event, events::Profile> ||
                  std::is_same_v<Event, events::Trade> || std::is_same_v<Event, events::TradeETH> ||
                  std::is_same_v<Event, events::Summary> || std::is_same_v<Event, events::TextMessage> ||
                  std::is_same_v<Event, events::TextConfiguration> || std::is_same_v<Event, events::Message> ||
                  std::is_same_v<Event, events::Configuration> || std::is_same_v<Event, events::MarketMaker> ||
                  std::is_same_v<Event, events::SpreadOrder> || std::is_same_v<Event, events::AnalyticOrder> ||
                  std::is_same_v<Event, events::OtcMarketsOrder> || std::is_same_v<Event, events::OrderImbalance> ||
                  std::is_same_v<Event, events::Series> || std::is_same_v<Event, events::Greeks> ||
                  std::is_same_v<Event, events::Underlying> || std::is_same_v<Event, events::TheoPrice> ||
                  std::is_same_v<Event, events::Candle> || std::is_same_v<Event, events::TimeAndSale> ||
                  std::is_same_v<Event, events::OptionSale> || std::is_same_v<Event, events::NuamTrade> ||
                  std::is_same_v<Event, events::NuamTimeAndSale> || std::is_same_v<Event, events::Order> ||
                  std::is_same_v<Event, RegisteredEvent>);
    std::shared_ptr<detail::SubscriptionPort> port_;
    friend class Feed;
    explicit Subscription(std::shared_ptr<detail::SubscriptionPort> port) noexcept;
    /** @param symbols Complete borrowed canonical batch, validated before one atomic add. */
    void addCandleSymbols(std::span<const events::CandleSymbol> symbols);
    /** @param symbols Complete borrowed canonical batch, validated before one atomic replacement. */
    void setCandleSymbols(std::span<const events::CandleSymbol> symbols);

    public:
    using Listener =
        std::function<void(std::span<const Event>)>; ///< Synchronous callback receiving ready owning values.
    using ListenerId = std::uint64_t;                ///< Non-reused listener identity within this subscription.
    using StateChangeListener =
        std::function<void(SubscriptionState, SubscriptionState)>; ///< Advisory old/new observation.
    using StateChangeListenerId =
        std::uint64_t; ///< Non-reused state-observer identity, separate from event identities.
    using ChangeListener = SubscriptionChangeListener; ///< Advisory accepted-symbol observer.
    using ChangeListenerId = std::uint64_t; ///< Non-reused identity, separate from event and state listeners.
    /** Copy one complete accepted local revision, including exact decorated identities.
     * The result remains valid after mutation and runtime retirement. It does not advance native progress.
     * @return Independently owned symbols/types/revision; allocation failures throw Error without mutation.
     */
    [[nodiscard]] SubscriptionSnapshot getSnapshot() const;
    /** Read the retained time-series range property without native progress, including after close.
     * @return Signed Unix milliseconds, initially MAX; ordinary subscriptions throw UNSUPPORTED_PROFILE.
     */
    [[nodiscard]] std::int64_t getFromTime() const;
    /** Replace the time-series range and effective selectors in one accepted local revision.
     * Equal values do nothing. Failure preserves the complete old range, symbols and revision.
     * Already begun callbacks may finish; obsolete not-started HISTORY delivery is withdrawn by the SDK.
     * @param fromTime Signed Unix milliseconds; MAX disables ordinary historical data while retaining demand.
     */
    void setFromTime(std::int64_t fromTime);
    /** Copy one coherent subscription-local accounting cut, without native progress or callback waiting.
     * The detached value retains no callback/schema/runtime and survives shutdown. Final means capture quiescence.
     * @return Owner identity, lifetime counters, bounded gauges and monotonic observation time.
     */
    [[nodiscard]] SubscriptionStatistics getStatistics() const noexcept;
    /** Read the accepted subscription-local aggregation override.
     * @return Override, or nullopt when the endpoint-local period is used.
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> getAggregationPeriod() const noexcept;
    /** Set or clear the local aggregation override; the running interval retains its original deadline.
     * @param period Nonnegative local period up to 24 hours, or nullopt to clear the override.
     */
    void setAggregationPeriod(std::optional<std::chrono::milliseconds> period);
    /** Inspect the effective local period used for the most recent listener invocation.
     * @return Maximum endpoint and subscription period at selection, or nullopt before the first invocation.
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> getAggregationPeriodInfo() const noexcept;
    /** Copy the last worker HISTORY observation and qualify it against current accepted facade fences.
     * Reads perform no native progress or callback waiting. A CURRENT cut is AS-OF, not a promise about
     * unread network input or future freshness; another contract reports UNSUPPORTED.
     * @return Owning bounded ranges and reset evidence; allocation failures throw Error without mutation.
     */
    [[nodiscard]] HistorySubscriptionSnapshot getHistorySnapshot() const;
    /** Copy the immutable normalized event-type set.
     * @return Owning finite set in EventType declaration order; allocation failures throw Error.
     */
    [[nodiscard]] std::vector<EventType> getEventTypes() const;
    /** Test the immutable business-type set, without consulting record IDs or progressing the network.
     * @param type Supported or unknown enumeration value; unknown values are absent.
     * @return True only for a selected supported type.
     */
    [[nodiscard]] bool containsEventType(EventType type) const noexcept;
    /** Register an accepted-symbol observer, delivering existing nonempty state first.
     * An uncontended caller dispatches synchronously after commit, before returning, including offline.
     * Reentrant/concurrent calls reserve finite deferred phases and return without waiting; the current
     * callback owner delivers them after return. Admission refusal leaves the old revision and selection intact.
     * Exceptions are advisory, recorded separately; selected fanout continues and effects are never retried.
     * @param listener Nonempty callable; borrowed arguments expire at return, and captures remain charged until
     * destroyed.
     * @return Non-reused removal identity; throws Error for invalid, closed or finite-capacity/work refusal.
     */
    ChangeListenerId addChangeListener(ChangeListener listener);
    /** Remove an observer and reserve its exactly-once CLOSED phase, including after subscription close.
     * A currently selected phase can finish. Removal/closed-capture retirement occurs outside all locks.
     * @param id Previously returned observer identity; absent identities are idempotent no-ops.
     * @throws Error On allocation or finite reentrant/concurrent work/continuation refusal, before removal.
     */
    void removeChangeListener(ChangeListenerId id);
    /** Inspect the first observer exception, independently of event delivery and lifecycle errors.
     * @return Detached CALLBACK_FAILURE or NONE.
     */
    [[nodiscard]] Diagnostic getChangeDiagnostic() const noexcept;
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
    /** Inspect event scheduling failure or benign queued cancellation independently of state observers.
     * @return Detached typed/static diagnostic; does not progress host tasks.
     */
    [[nodiscard]] Diagnostic getCallbackDispatchDiagnostic() const noexcept;
    /** Select the event executor; null inherits the endpoint policy (endpoint null means SDK worker).
     * Selection is sampled with listeners; already selected work keeps its original policy.
     * @param executor Owned policy object; workers remain application-owned. Throws Error after close or on capacity
     * refusal.
     */
    void setEventExecutor(std::shared_ptr<CallbackExecutor> executor);
    /** Inspect accepted event policy intent, without waiting for an old selection. @return Override or null. */
    [[nodiscard]] std::shared_ptr<CallbackExecutor> getEventExecutor() const;

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
     * @throws Error INVALID_ARGUMENT at subscription-symbol-representation if a selected regional Quote/Trade route
     * cannot restore this exact spelling; no part of the desired mutation is published.
     */
    void addSymbols(std::string_view symbol);
    /** Atomically accept several exact selectors, with duplicates coalesced.
     * @param symbols Complete bounded set to add.
     * @throws Error INVALID_ARGUMENT if any selected regional Quote/Trade symbol is not exactly representable;
     * the whole batch is refused at subscription-symbol-representation.
     */
    void addSymbols(std::initializer_list<std::string_view> symbols);
    /** Accept removal of one selector; absence is an idempotent no-op.
     * @param symbol Exact selector to remove.
     */
    void removeSymbols(std::string_view symbol);
    /** Replace the complete accepted set atomically. Backend failure is reported by getDiagnostic().
     * @param symbols New bounded exact set; empty withdraws all interests.
     * @throws Error INVALID_ARGUMENT at subscription-symbol-representation for a lossy selected regional route;
     * previously accepted selectors remain unchanged and no partial replacement is sent.
     */
    void setSymbols(std::initializer_list<std::string_view> symbols);

    /** Accept one CandleSymbol's canonical identity through the existing exact-selector policy.
     * @param symbol Immutable value, borrowed for this call; no input views are retained.
     * @throws Error INVALID_ARGUMENT for null/non-ASCII/unsupported selector text, before mutation.
     */
    void addSymbols(const events::CandleSymbol &symbol);

    /** Atomically add an object batch; equal canonical texts coalesce.
     * @tparam Symbol Exactly events::CandleSymbol; empty braces keep the original string overload.
     * @param symbols Complete borrowed object batch; all inputs are validated before publication.
     * @throws Error Existing argument, count, route or allocation refusal; the old desired set remains.
     */
    template <class Symbol>
        requires std::is_same_v<Symbol, events::CandleSymbol>
    void addSymbols(std::initializer_list<Symbol> symbols) {
        addCandleSymbols(std::span(symbols.begin(), symbols.size()));
    }

    /** Remove by canonical Candle identity; absent values are idempotent.
     * @param symbol Borrowed object; null/non-ASCII text is rejected before mutation.
     * @throws Error Existing exact-selector validation, closed-state or allocation refusal.
     */
    void removeSymbols(const events::CandleSymbol &symbol);

    /** Atomically replace with canonical object identities; duplicates coalesce.
     * @tparam Symbol Exactly events::CandleSymbol; empty braces keep the original string overload.
     * @param symbols Complete borrowed batch, never retained.
     * @throws Error Existing argument, count, route or allocation refusal; the old desired set remains.
     */
    template <class Symbol>
        requires std::is_same_v<Symbol, events::CandleSymbol>
    void setSymbols(std::initializer_list<Symbol> symbols) {
        setCandleSymbols(std::span(symbols.begin(), symbols.size()));
    }

    /** Atomically add explicit selectors. Current market types ignore INDEXED source IDs as Java does.
     * @param selectors Borrowed batch of at most maximumSymbolsPerSubscription values, including duplicates.
     * @throws Error Unsupported TIME_SERIES, disabled/non-STREAM ALL, invalid text or finite capacity refusal;
     * no part of the batch is published. No caller value or source name is retained by reference.
     */
    void addSelectors(std::span<const SubscriptionSelector> selectors);
    /** Atomically replace the complete logical selector set.
     * @param selectors Borrowed bounded batch; empty withdraws all demand. Identity includes kind and source ID.
     * @throws Error Same atomic validation and finite-capacity refusals as addSelectors.
     */
    void setSelectors(std::span<const SubscriptionSelector> selectors);
    /** Remove one typed identity; an absent identity is an idempotent no-op.
     * @param selector Borrowed value; removing PLAIN never removes an overlapping INDEXED or ALL identity.
     * @throws Error Existing validation, closed-state or allocation refusal.
     */
    void removeSelector(const SubscriptionSelector &selector);
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
extern template class Subscription<events::Trade>;
extern template class Subscription<events::NuamTrade>;
extern template class Subscription<events::TradeETH>;
extern template class Subscription<events::Summary>;
extern template class Subscription<events::TextMessage>;
extern template class Subscription<events::TextConfiguration>;
extern template class Subscription<events::Message>;
extern template class Subscription<events::Configuration>;
extern template class Subscription<events::Order>;
extern template class Subscription<events::MarketMaker>;
extern template class Subscription<events::SpreadOrder>;
extern template class Subscription<events::AnalyticOrder>;
extern template class Subscription<events::OtcMarketsOrder>;
extern template class Subscription<events::OrderImbalance>;
extern template class Subscription<events::Series>;
extern template class Subscription<events::Greeks>;
extern template class Subscription<events::Underlying>;
extern template class Subscription<events::TheoPrice>;
extern template class Subscription<events::Candle>;
extern template class Subscription<events::TimeAndSale>;
extern template class Subscription<events::NuamTimeAndSale>;
extern template class Subscription<events::OptionSale>;
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
     * @param timeSeries True selects verified time-series mode; false preserves ordinary admission.
     * @return One port with its complete immutable selection and exact listener value shape.
     */
    std::shared_ptr<detail::SubscriptionPort> createSubscriptionPort(std::span<const EventType> types,
                                                                     bool registeredDelivery, bool timeSeries = false);

    /** Resolve one passive TICKER lookup through the endpoint's serialized owner.
     * @param type Registered LastingEvent identity.
     * @param symbol Borrowed exact logical symbol.
     * @param ifSubscribed Select exact logical demand and allow a default event before data.
     * @return Detached retained/default value or absence. Refused admission/lookup throws Error.
     */
    [[nodiscard]] std::optional<RegisteredEvent> readLastEvent(EventType type, std::u16string_view symbol,
                                                               bool ifSubscribed) const;
    /** Convert one bounded ASCII symbol and use the same exact logical read.
     * @param type Registered LastingEvent identity.
     * @param symbol Borrowed ASCII symbol.
     * @return Detached retained/default value or absence.
     */
    [[nodiscard]] std::optional<RegisteredEvent> readLastEvent(EventType type, std::string_view symbol) const;

    /** @tparam Event Supported owning native business type. @return Its immutable registration identity. */
    template <class Event> static constexpr EventType eventTypeFor() {
        static_assert(std::is_same_v<Event, events::Quote> || std::is_same_v<Event, events::Profile> ||
                      std::is_same_v<Event, events::Trade> || std::is_same_v<Event, events::TradeETH> ||
                      std::is_same_v<Event, events::Summary> || std::is_same_v<Event, events::TextMessage> ||
                      std::is_same_v<Event, events::TextConfiguration> || std::is_same_v<Event, events::Message> ||
                      std::is_same_v<Event, events::Configuration> || std::is_same_v<Event, events::MarketMaker> ||
                      std::is_same_v<Event, events::SpreadOrder> || std::is_same_v<Event, events::AnalyticOrder> ||
                      std::is_same_v<Event, events::OtcMarketsOrder> || std::is_same_v<Event, events::OrderImbalance> ||
                      std::is_same_v<Event, events::Series> || std::is_same_v<Event, events::Greeks> ||
                      std::is_same_v<Event, events::Underlying> || std::is_same_v<Event, events::TheoPrice> ||
                      std::is_same_v<Event, events::Candle> || std::is_same_v<Event, events::TimeAndSale> ||
                      std::is_same_v<Event, events::OptionSale> || std::is_same_v<Event, events::NuamTrade> ||
                      std::is_same_v<Event, events::NuamTimeAndSale> || std::is_same_v<Event, events::Order>);

        if constexpr (std::is_same_v<Event, events::Quote>)
            return EventType::QUOTE;
        else if constexpr (std::is_same_v<Event, events::Profile>)
            return EventType::PROFILE;
        else if constexpr (std::is_same_v<Event, events::Trade>)
            return EventType::TRADE;
        else if constexpr (std::is_same_v<Event, events::TradeETH>)
            return EventType::TRADE_ETH;
        else if constexpr (std::is_same_v<Event, events::Summary>)
            return EventType::SUMMARY;
        else if constexpr (std::is_same_v<Event, events::TextMessage>)
            return EventType::TEXT_MESSAGE;
        else if constexpr (std::is_same_v<Event, events::TextConfiguration>)
            return EventType::TEXT_CONFIGURATION;
        else if constexpr (std::is_same_v<Event, events::Message>)
            return EventType::MESSAGE;
        else if constexpr (std::is_same_v<Event, events::Configuration>)
            return EventType::CONFIGURATION;
        else if constexpr (std::is_same_v<Event, events::MarketMaker>)
            return EventType::MARKET_MAKER;
        else if constexpr (std::is_same_v<Event, events::SpreadOrder>)
            return EventType::SPREAD_ORDER;
        else if constexpr (std::is_same_v<Event, events::AnalyticOrder>)
            return EventType::ANALYTIC_ORDER;
        else if constexpr (std::is_same_v<Event, events::OtcMarketsOrder>)
            return EventType::OTC_MARKETS_ORDER;
        else if constexpr (std::is_same_v<Event, events::OrderImbalance>)
            return EventType::ORDER_IMBALANCE;
        else if constexpr (std::is_same_v<Event, events::Series>)
            return EventType::SERIES;
        else if constexpr (std::is_same_v<Event, events::Greeks>)
            return EventType::GREEKS;
        else if constexpr (std::is_same_v<Event, events::Underlying>)
            return EventType::UNDERLYING;
        else if constexpr (std::is_same_v<Event, events::TheoPrice>)
            return EventType::THEO_PRICE;
        else if constexpr (std::is_same_v<Event, events::Candle>)
            return EventType::CANDLE;
        else if constexpr (std::is_same_v<Event, events::TimeAndSale>)
            return EventType::TIME_AND_SALE;
        else if constexpr (std::is_same_v<Event, events::OptionSale>)
            return EventType::OPTION_SALE;
        else if constexpr (std::is_same_v<Event, events::NuamTimeAndSale>)
            return EventType::NUAM_TIME_AND_SALE;
        else if constexpr (std::is_same_v<Event, events::NuamTrade>)
            return EventType::NUAM_TRADE;
        else
            return EventType::ORDER;
    }

    public:
    /** Copy the latest committed TICKER value for the supplied event's exact logical symbol.
     * The call does not connect, start a worker, subscribe or request data. The runtime owner reads the current
     * collector state; calls from its own listener use the same serialized read directly. An external call can wait
     * up to one second for the bounded local request. Busy or timed-out admission throws Error.
     * @tparam Event Supported owning LastingEvent type.
     * @param event Independently owned fallback, unchanged when no current value is retained.
     * @return Detached retained event, or the supplied fallback on absence, stale source or another delivery family.
     * @throws Error Unsupported event type, invalid symbol, or finite local admission or lookup refusal.
     */
    template <class Event> [[nodiscard]] Event getLastEvent(Event event) const {
        std::u16string_view symbol;

        if constexpr (std::is_same_v<Event, events::Candle>) {
            if (event.eventSymbol)
                symbol = event.eventSymbol->getSymbol().value_or(std::u16string_view{});
        } else {
            symbol = event.symbol;
        }

        if (auto found = readLastEvent(eventTypeFor<Event>(), symbol, false))
            return std::get<Event>(std::move(*found));

        return event;
    }

    /** Read an owning current TICKER value only for an exact accepted subscription.
     * A subscribed symbol without current retained data yields its default event. Wildcard demand is excluded.
     * The local request and refusal rules are the same as getLastEvent.
     * @tparam Event Supported owning LastingEvent type.
     * @param symbol Exact borrowed UTF-16 logical symbol, including a canonical Candle spelling if applicable.
     * @return Current value, symbol-initialized default, or empty when exact demand is absent.
     * @throws Error Same refusals as getLastEvent.
     */
    template <class Event>
    [[nodiscard]] std::optional<Event> getLastEventIfSubscribed(std::u16string_view symbol) const {
        if (auto found = readLastEvent(eventTypeFor<Event>(), symbol, true))
            return std::get<Event>(std::move(*found));

        return std::nullopt;
    }

    /** Read the current or default TICKER event using the same ASCII spelling as addSymbols.
     * @tparam Event Supported owning LastingEvent type.
     * @param symbol Exact borrowed ASCII logical symbol.
     * @return Current value, symbol-initialized default, or empty when exact demand is absent.
     * @throws Error Same refusals as getLastEvent.
     */
    template <class Event> [[nodiscard]] std::optional<Event> getLastEventIfSubscribed(std::string_view symbol) const {
        if (auto found = readLastEvent(eventTypeFor<Event>(), symbol))
            return std::get<Event>(std::move(*found));

        return std::nullopt;
    }

    /** Read the current or default Candle using an already parsed exact symbol.
     * @tparam Event The owning Candle type.
     * @param symbol Exact Candle symbol; null spelling yields absence.
     * @return Current or default Candle for accepted exact demand, otherwise empty.
     * @throws Error Same refusals as getLastEvent.
     */
    template <class Event>
        requires std::is_same_v<Event, events::Candle>
    [[nodiscard]] std::optional<Event> getLastEventIfSubscribed(const events::CandleSymbol &symbol) const {
        const auto text = symbol.getSymbol();

        return text ? getLastEventIfSubscribed<Event>(*text) : std::nullopt;
    }

    /** Create a bounded logical registration, before or after connect, without application polling.
     * An unregistered type rejects synchronously. A Candle selector uses exactly one qualified source record.
     * Ordinary Order, MarketMaker, SpreadOrder, AnalyticOrder and OtcMarketsOrder require HISTORY.
     * Quote-derived Order and source-specific OrderImbalance use TICKER. Candle uses TICKER or HISTORY.
     * Unsupported selections reject before publication.
     * @tparam Event Supported owning event type registered for this endpoint or provided by its defaults.
     * @return Producer handle; native preparation failures are observed asynchronously by its diagnostic.
     */
    template <class Event> [[nodiscard]] std::shared_ptr<Subscription<Event>> createSubscription() {
        auto subscription = std::shared_ptr<Subscription<Event>>(new Subscription<Event>({}));
        subscription->port_ = createSubscriptionPort(eventTypeFor<Event>());

        return subscription;
    }

    /** Create one logical subscription for an immutable normalized set of supported registered types.
     * Raw argument count is checked before inspection and may reject RESOURCE_LIMIT; empty/unknown enums
     * reject INVALID_ARGUMENT.
     * Duplicates coalesce. Closed admission precedes missing registered types and logical-slot checks.
     * Every requested type must be registered; refusal opens no native/producer registration.
     * @param types Caller-owned bounded type list, borrowed only during this call; Quote/Profile/Trade are supported.
     * @return One variant-valued producer handle; throws Error on atomic validation/resource refusal.
     */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>> createSubscription(std::span<const EventType> types);
    /** Create the same immutable type selection from an inline argument list.
     * @param types Bounded raw list; duplicates count toward maximumEventTypeArgumentsPerSubscription then coalesce.
     * @return One owning variant subscription, or the same synchronous Error as the span overload.
     */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>>
    createSubscription(std::initializer_list<EventType> types);

    /** Create a HISTORY registration whose range is owned and applied by the SDK.
     * A supported native time-series mapper such as Candle grants eligibility. Unsupported types reject
     * UNSUPPORTED_PROFILE before native admission.
     * @tparam Event Native owning business type with a verified time-series mapper.
     * @return The existing subscription facade with an initial MAX fromTime, or a synchronous Error.
     */
    template <class Event> [[nodiscard]] std::shared_ptr<Subscription<Event>> createTimeSeriesSubscription() {
        const auto type = eventTypeFor<Event>();
        auto subscription = std::shared_ptr<Subscription<Event>>(new Subscription<Event>({}));
        subscription->port_ = createSubscriptionPort(std::span(&type, 1), false, true);

        return subscription;
    }

    /** Create a variant registration with the same verified HISTORY eligibility and range semantics.
     * @param types Bounded immutable business type arguments, normalized as by createSubscription.
     * @return One owning time-series mode facade, or an atomic synchronous Error.
     */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>>
    createTimeSeriesSubscription(std::span<const EventType> types);
    /** @param types Inline business type arguments. @return The same owning facade as the span overload. */
    [[nodiscard]] std::shared_ptr<Subscription<RegisteredEvent>>
    createTimeSeriesSubscription(std::initializer_list<EventType> types);
};

/** Detached last-published endpoint cut; keeping it retains only an opaque identity token.
 * Plain TCP counts actual native QTP-stream octets; complete-message/record counts have distinct boundaries.
 * Reads never progress the worker or wait for callbacks. A blocked callback can delay the next publication.
 */
struct EndpointStatistics {
    std::shared_ptr<const void> ownerIdentity; ///< Opaque non-aliasing token; compare only for owner equality.
    std::uint64_t generation = 1; ///< Owner-lifetime observation generation; reconnect/baselines never reset it.
    std::chrono::steady_clock::time_point observedAt; ///< Monotonic time of this coherent publication, not read time.
    transport::ConnectionStatistics connection;       ///< Lifetime actual transport/protocol/recovery measurements.
    bool final = false; ///< True only after native and all callback/capture retirement; totals then remain frozen.
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
    /** Select the default event executor; null restores the SDK worker.
     * Selection is sampled with listeners; already selected work keeps its original policy.
     * @param executor Owned policy object; workers remain application-owned. Throws Error after close or on capacity
     * refusal.
     */
    void setEventExecutor(std::shared_ptr<CallbackExecutor> executor);
    /** Inspect accepted default event policy intent without waiting. @return Policy or null for SDK worker. */
    [[nodiscard]] std::shared_ptr<CallbackExecutor> getEventExecutor() const;

    ~Endpoint();
    Endpoint(const Endpoint &) = delete;
    Endpoint &operator=(const Endpoint &) = delete;
    /** Inspect the stable typed feed entry point.
     * @return Shared producer facade; keeping it does not extend the native runtime lifetime.
     */
    [[nodiscard]] std::shared_ptr<Feed> getFeed() const noexcept;
    /** Build an independent classifier using this endpoint's immutable registry and delivery contract.
     * Include qd/api/feed_filter.hpp when calling this method. No endpoint/control lock is held during build.
     * Plain categories start no I/O; an explicit context resolver may perform its finite bootstrap wait.
     * @param categories Ordered owning category values, borrowed while the complete classifier is prepared.
     * @param context Optional explicit resource capability; absence keeps ordinary classification independent of I/O.
     * @return Immutable independent filter; invalid input or finite preparation failure throws Error before return.
     */
    [[nodiscard]] FeedFilter createFilter(std::span<const FeedFilterCategory> categories,
                                          std::shared_ptr<FeedFilterContext> context = {}) const;
    /** Start an asynchronous plain binary-QTP connection; no I/O or callbacks run on this caller.
     * The first accepted address is fixed. Repeating it while logically online is a no-op; after disconnect
     * the same address resumes with a fresh source epoch. Another address or
     * reconnecting a closed/failed endpoint rejects. Automatic finite recovery remains internal.
     * @param address ASCII host:port or [IPv6]:port; no TLS/URI/login/configuration extensions.
     */
    void connect(std::string_view address);
    /** Start one asynchronous read of a regular local binary tape.
     * Configure desired subscriptions before calling. The worker opens and retires the file,
     * and a clean EOF drains the admitted prefix before closing this endpoint.
     * @param path Borrowed stable local path, copied at admission; maximum 4096 native path units.
     * @param options Positive file/envelope allowances and optional relative playback and initial pause.
     * @throws Error Invalid input, closed admission or an already fixed source.
     */
    void connectTape(const std::filesystem::path &path, TapeSourceOptions options = {});
    /** Publish tape pause or resume intent for the existing runtime worker.
     * Pausing holds the next unadmitted envelope and freezes relative playback time. Admitted work may drain.
     * @param paused True to pause intake; false to resume the same source and clock.
     * @throws Error The endpoint has no tape source or is already closing.
     */
    void setTapePaused(bool paused);
    /** Reopen an active tape from byte zero on this endpoint, preserving subscriptions and pause intent.
     * An accepted request enters CONNECTING and fences the old source. Queued old work is discarded.
     * CONNECTED identifies an opened new epoch. A failure reaches FAILED with a retained diagnostic.
     * Additional requests while this restart is pending join the same operation. Stop or close takes precedence.
     * @throws Error The endpoint has no active tape, is closing, or cannot accept another revision.
     */
    void restartTape();
    /** End the file-backed endpoint and apply the requested terminal policy.
     * A later abort can escalate an unfinished drain. Use closeAndAwaitTermination for the actual result.
     * @param mode Drain or abort; running callbacks remain subject to the existing quiescence contract.
     * @throws Error Invalid mode, no tape source, or an already terminal endpoint.
     */
    void stopTape(TapeStopMode mode = TapeStopMode::DRAIN);
    /** Inspect current local intent without progressing native work; absent means server default.
     * @return Owning millisecond value, including zero, independently of any peer-reported range.
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> getRequestedAggregationPeriod() const noexcept;
    /** Commit a local request; the existing worker coalesces updates into bounded protocol output.
     * This does not reconnect or start a worker. Offline/closed handles retain local intent only.
     * Peer reports can describe an earlier request and are not acknowledgements of this mutation.
     * @param period Nonnegative milliseconds up to the accepted maximum, or null to request server default.
     * @throws Error Invalid value or exhausted local request revision; no partial intent change.
     */
    void setRequestedAggregationPeriod(std::optional<std::chrono::milliseconds> period);
    /** Inspect the current source's last peer-reported range without I/O or request inference.
     * @return Owning range, or absent for unknown/legacy/malformed/unavailable/offline state.
     */
    [[nodiscard]] std::optional<transport::AggregationPeriodInfo> getAggregationPeriodInfo() const noexcept;
    /** Activate the immutable configured route explicitly; construction never auto-connects.
     * Include qd/api/endpoint_configuration.hpp to construct the configuration.
     * @throws Error If no route was configured, or ordinary connect admission rejects.
     */
    void connect();
    /** Inspect owning immutable local policy and the current accepted connection intent.
     * Include qd/api/endpoint_configuration.hpp for the returned value's definition.
     * @return Detached snapshot; acceptedAddress is a local request, never peer/DNS/negotiation evidence.
     */
    [[nodiscard]] EndpointConfigurationSnapshot getEffectiveConfiguration() const;
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
    /** Copy the last coherent worker publication without network progress, callback wait or reset.
     * Values survive endpoint/runtime shutdown and preserve owner identity across source epochs.
     * @return Bounded detached owning snapshot; final becomes true only after actual capture quiescence.
     */
    [[nodiscard]] EndpointStatistics getStatistics() const noexcept;
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
    /** Create an independent producer endpoint with its immutable registry or built-in full Quote/Profile/Trade
     * defaults. Failed wrapper/control allocation rolls back its reservation without publishing an active endpoint.
     * Producer dispatch uses a lifetime-gated detached control port, never an unprotected RuntimeImpl*.
     * @param options Existing policy/deadlines and optional immutable registry; finite capacities use RuntimeOptions.
     * @return Endpoint facade; no socket is opened and no listener is called inline.
     */
    [[nodiscard]] std::shared_ptr<Endpoint> createEndpoint(EndpointOptions options = {});
    /** Create an independent endpoint from an already resolved immutable explicit configuration.
     * @param configuration Validated owning policy and optional route; retained independently of its caller.
     * @return Inactive endpoint. Call connect() explicitly to activate its configured route.
     */
    [[nodiscard]] std::shared_ptr<Endpoint> createEndpoint(const EndpointConfiguration &configuration);
    /** Close every endpoint, join the SDK worker and preserve bootstrap until safe main-thread cleanup.
     * Only the creating main/owner thread may call this operation; worker calls reject immediately.
     * Reentrant shutdown or shutdown from accepted capture destruction rejects with INVALID_STATE.
     */
    void closeAndAwaitTermination();
};
} // namespace org::ttldtor::qdcxx::api
