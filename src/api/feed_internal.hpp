// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/endpoint_configuration.hpp"
#include "qd/events/candle_routes.hpp"

#include "../transport/bootstrap_lease.hpp"
#include "callback_execution.hpp"
#include "event_selection.hpp"
#include "on_demand_wire.hpp"
#include "time_series_query_policy.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <variant>

namespace org::ttldtor::qdcxx::api::detail {
struct RuntimeControl;

/** Detached count of accepted and still selected callback boxes. */
struct ListenerLedger {
    std::atomic<std::size_t> count{0};     ///< Includes removed callbacks until final selected invocation retires.
    std::size_t maximum;                   ///< Original immutable runtime reservation.
    std::weak_ptr<RuntimeControl> control; ///< Wake after final application capture retirement.
    std::weak_ptr<EndpointPort> endpoint;  ///< Notify inactive main retirement without owning the endpoint.
};

using ListenerFunction =
    std::variant<std::monostate, Subscription<events::Quote>::Listener, Subscription<events::Profile>::Listener,
                 Subscription<events::Trade>::Listener, Subscription<events::TradeETH>::Listener,
                 Subscription<events::Summary>::Listener, Subscription<events::TextMessage>::Listener,
                 Subscription<events::TextConfiguration>::Listener, Subscription<events::Message>::Listener,
                 Subscription<events::Configuration>::Listener, Subscription<events::Order>::Listener,
                 Subscription<events::MarketMaker>::Listener, Subscription<events::SpreadOrder>::Listener,
                 Subscription<events::AnalyticOrder>::Listener, Subscription<events::OtcMarketsOrder>::Listener,
                 Subscription<events::OrderImbalance>::Listener, Subscription<events::Series>::Listener,
                 Subscription<events::Greeks>::Listener, Subscription<events::Underlying>::Listener,
                 Subscription<events::TheoPrice>::Listener, Subscription<events::Candle>::Listener,
                 Subscription<events::TimeAndSale>::Listener, Subscription<events::OptionSale>::Listener,
                 Subscription<events::NuamTrade>::Listener, Subscription<events::NuamTimeAndSale>::Listener,
                 Subscription<events::NuamOrder>::Listener, Subscription<RegisteredEvent>::Listener,
                 Endpoint::StateChangeListener, Subscription<events::Quote>::StateChangeListener,
                 SubscriptionChangeListener>;

/** Shared application callable; its last release never occurs under a control mutex. */
struct ListenerBox {
    std::shared_ptr<ListenerLedger> ledger; ///< Keeps the detached count alive through callback destruction.
    ListenerFunction function;              ///< Application captures, moved in before publication.
    mutable bool changeInitialized = true;  ///< Change observers enter ordinary fanout after their initial phase.

    /** Adopt one already charged callable only after this box's allocation has succeeded.
     * A failed box allocation leaves captures with the caller until its reservation has been released.
     * @tparam Function Exact public callback type, moved without throwing.
     * @param ledger Detached selected/retiring reservation.
     * @param function Application callable, borrowed until construction moves it into the allocated box.
     */
    template <class Function>
    ListenerBox(std::shared_ptr<ListenerLedger> ledger, Function &&function) noexcept
        : ledger(std::move(ledger)),
          function(std::in_place_type<std::remove_cvref_t<Function>>, std::forward<Function>(function)) {
    }

    ListenerBox(const ListenerBox &) = delete;
    ListenerBox &operator=(const ListenerBox &) = delete;
    /** Destroy application captures before releasing their selected/retiring reservation. */
    ~ListenerBox();
};

/** Immutable entry in one complete listener selection. */
struct ListenerEntry {
    std::uint64_t id;                            ///< Non-reused logical identity.
    std::shared_ptr<const ListenerBox> callback; ///< Safe snapshot copy without copying application functors.
};

using ListenerSnapshot = std::vector<ListenerEntry>;

/** Advisory state observers, protected by the enclosing detached port mutex. */
struct StateListeners {
    std::shared_ptr<ListenerLedger> ledger;           ///< Independent accepted/selected/retiring observer reservation.
    std::shared_ptr<const ListenerSnapshot> snapshot; ///< Accepted immutable selection; null after terminal dispatch.
    std::uint64_t nextListener = 1; ///< Non-reused observer identity; independent of event-listener identities.
    Diagnostic diagnostic;          ///< First observer exception; never changes the owner's lifecycle diagnostic.
};

struct SubscriptionChangeDomain;

/** One selected callback epoch; data and change owners can overlap on different threads. */
struct SubscriptionCallbackFence {
    std::atomic<std::uint64_t> epoch{0}; ///< Checked nonwrapping callback identity within this execution channel.
    std::atomic<bool> active{false};     ///< False only after user code and final selected capture release return.
};

/** Weak after-return dependency; an expired owner necessarily completed its selected callback. */
struct SubscriptionCallbackBarrier {
    std::weak_ptr<SubscriptionChangeDomain> owner; ///< Detached execution domain, never a native/runtime owner.
    std::uint64_t epoch = 0; ///< Exact source callback epoch; a later callback cannot block this phase.
    std::size_t channel = 0; ///< Existing data selection or serialized change selection.
    std::shared_ptr<CallbackSlotPool> packetPool; ///< Pure fixed identity storage, never a selected payload owner.
    std::size_t packetIndex = 0;
    std::uint64_t packetGeneration = 0; ///< Exact packet retirement dependency, independent of thread scopes.
};

/** Complete immutable transition backing, charged by its pending/executing notification slots. */
struct SubscriptionChangeFrame {
    SubscriptionSnapshot snapshot;       ///< Full accepted post-change revision, never a physical application receipt.
    SubscriptionCallbackBarrier barrier; ///< Source callback must return before this frame can be selected.
    std::vector<SubscriptionSelector> added;   ///< Exact newly demanded logical identities.
    std::vector<SubscriptionSelector> removed; ///< Exact actually removed logical identities.
};

/** One causal root credit lease; final release is atomic and owns no native/runtime state. */
struct SubscriptionChangeLease {
    std::shared_ptr<SubscriptionChangeDomain> root; ///< Detached root kept alive through selected capture retirement.
    SubscriptionChangeLease() noexcept = default;
    /** @param root Validated root; one preflighted phase/terminal lease is charged without allocation. */
    explicit SubscriptionChangeLease(std::shared_ptr<SubscriptionChangeDomain> root) noexcept;
    SubscriptionChangeLease(const SubscriptionChangeLease &) = delete;
    SubscriptionChangeLease &operator=(const SubscriptionChangeLease &) = delete;
    SubscriptionChangeLease(SubscriptionChangeLease &&other) noexcept;
    SubscriptionChangeLease &operator=(SubscriptionChangeLease &&other) noexcept;
    /** Release the causal root credit only after selected application capture retirement. */
    ~SubscriptionChangeLease();
};

/** One bounded phase; initial notifications can target a single newly accepted observer. */
struct SubscriptionChangePhase {
    SubscriptionChangeKind kind = SubscriptionChangeKind::ADDED; ///< Phase order within the owning revision.
    SubscriptionChangeLease lease; ///< Causal root budget survives a concurrent dispatcher handoff.
    std::shared_ptr<const SubscriptionChangeFrame> frame; ///< Immutable arguments, retained through final release.
    std::shared_ptr<const ListenerSnapshot> target; ///< Initial-only selection; ordinary phases select live observers.
};

/** One fixed, charged after-return task identity; it never retains a callback packet. */
struct DeferredCallbackTask {
    std::shared_ptr<CallbackSlotPool> pool;
    std::size_t index = 0;
    std::uint64_t generation = 0;
    SubscriptionCallbackBarrier barrier;
    SubscriptionChangeLease lease;
};

/** One existing causal-driver reservation; targets and event tasks share the same finite storage. */
struct SubscriptionContinuation {
    std::shared_ptr<SubscriptionChangeDomain> target;
    DeferredCallbackTask callback;
};

/** Runtime-local detached callback owner; it owns no runtime facade, thread, socket or native progress. */
struct SubscriptionChangeDomain {
    std::mutex mutex; ///< Serializes bounded notification admission and dispatch claims, never application code.
    std::vector<std::weak_ptr<SubscriptionPort>> ports;  ///< Fixed pending/active/retiring logical slots.
    std::vector<SubscriptionContinuation> continuations; ///< Fixed shared root-turn target/task reservations.
    std::array<SubscriptionCallbackFence, 2> fences;     ///< Separate data/change callback lifetime channels.
    std::atomic<std::size_t> leases{0}; ///< Pending/executing causal phases, including foreign target handoff.
    std::size_t maximumWork = 0;        ///< Finite phase/registration admissions for one outer callback turn.
    std::size_t remainingWork = 0;      ///< Shared by all concurrent and reentrant callers until the turn retires.
    std::size_t dataCallbacks = 0;      ///< Existing data callback owners postpone nested observer delivery.
    bool dispatching = false;           ///< One caller owns detached notification delivery outside locks.
};

/** Reserved terminal arguments, independent of the ordinary phase ring. */
struct SubscriptionClosedSelection {
    ListenerEntry listener{};            ///< Exactly one charged observer to retire; zero ID denotes an empty slot.
    SubscriptionChangeLease lease;       ///< Reserved terminal phase retains its own causal dispatch budget.
    SubscriptionCallbackBarrier barrier; ///< Removal/close callback origin, independent of the older snapshot.
    std::shared_ptr<const SubscriptionChangeFrame> frame; ///< Accepted snapshot at removal or close.
};

/** Accepted and selected/retiring subscription-change observers. Protected by the domain mutex. */
struct SubscriptionObservers {
    std::shared_ptr<ListenerLedger> ledger; ///< Captures stay charged through selected and retiring releases.
    std::shared_ptr<const ListenerSnapshot> listeners; ///< Accepted immutable observer registry.
    std::vector<SubscriptionChangePhase> phases;       ///< Fixed ring; executing head remains occupied until release.
    std::vector<SubscriptionClosedSelection> closed;   ///< Reserved one terminal slot per possible charged callable.
    std::shared_ptr<const SubscriptionChangeFrame>
        latest;                     ///< Current accepted snapshot for reserved terminal delivery.
    std::size_t head = 0;           ///< Oldest pending/executing phase.
    std::size_t count = 0;          ///< Includes executing phase and final selected capture release.
    std::uint64_t nextListener = 1; ///< Non-reused observer identity.
    Diagnostic diagnostic;          ///< First advisory callback failure.
    bool closing = false;           ///< Exactly-once terminal selection has fenced future ordinary notifications.
};

/** Producer-only subscription state; no receiver, schema or native owner can escape here. */
struct SubscriptionPort {
    std::shared_ptr<core::detail::DeliveryStatisticsState> statistics; ///< Pure coherent delivery accounting owner.
    HistorySubscriptionSnapshot history;    ///< Last worker-only detached publication, protected by this port mutex.
    mutable std::mutex mutex;               ///< Protects accepted snapshots and detached observations only.
    std::condition_variable stateCondition; ///< Creator current-state wait; publication uses this port mutex.
    std::thread::id owner; ///< Immutable runtime creator affinity, retained after endpoint/runtime destruction.
    std::weak_ptr<RuntimeControl> control;             ///< Wake/lifetime gate, without worker ownership.
    std::weak_ptr<EndpointPort> endpoint;              ///< Endpoint policy/status domain, without a producer cycle.
    std::shared_ptr<ListenerLedger> listenerLedger;    ///< Selected/retiring callback reservation.
    std::shared_ptr<const ListenerSnapshot> listeners; ///< Complete accepted bounded selection.
    std::shared_ptr<CallbackExecutor> eventExecutor;   ///< Null inherits endpoint policy; released outside locks.
    std::shared_ptr<CallbackSlotPool> callbackPool;    ///< Pure stable fixed identities, never native state.
    std::size_t callbackSlot = CallbackSlotPool::NO_SLOT;
    std::optional<std::uint64_t> callbackHistoryRevision; ///< Last custom selection's actual applied mapping cut.
    Diagnostic callbackDispatchDiagnostic;
    std::uint64_t callbackSynchronousRetirementRevision = 0; ///< Existing worker-path actual completed selections.
    bool callbackSynchronousExecuting = false; ///< Protected by this port mutex; never inferred from post acceptance.

    StateListeners stateListeners; ///< Opt-in advisory lifecycle observations, retired after event captures.
    std::shared_ptr<SubscriptionChangeDomain>
        changes;                               ///< Detached local observer dispatch, independent of network state.
    SubscriptionObservers observers;           ///< Finite accepted-symbol notifications and capture reservations.
    std::vector<SubscriptionSelector> symbols; ///< Complete accepted typed logical desired set.
    std::optional<std::int64_t> fromTime;      ///< Present only in SDK-owned time-series mode, protected by mutex.
    bool timeSeries = false;        ///< Immutable mode; never inferred from a caller descriptor or raw record layout.
    bool snapshotDemand = false;    ///< Immutable private request interest, excluded from if-subscribed reads.
    std::size_t maximumSymbols;     ///< Original per-registration selector count.
    std::size_t maximumSymbolUnits; ///< Original per-selector capacity.
    std::uint64_t revision = 0;     ///< Accepted local desired revision, separate from queue tickets.
    std::uint64_t nextListener = 1; ///< Non-reused listener identity, zero never issued.
    std::vector<EventType> eventTypes; ///< Immutable normalized actual validated subscription arguments.
    EventSelection eventSelection;     ///< Immutable normalized set, independent of explicit local record routes.
    std::optional<events::PreparedCandleRoutes> candleRoutes; ///< Immutable producer-side Candle validation.
    EventDelivery
        eventDelivery; ///< Exact listener value shape, assigned together with the selection before publication.
    std::atomic<bool> closeRequested{false}; ///< Reserved monotonic control, independent of ordinary capacity.
    std::atomic<bool> retired{false};        ///< True only after callback/capture/registration retirement.
    SubscriptionState state = SubscriptionState::CONFIGURED; ///< Detached worker observation.
    Diagnostic diagnostic;                                   ///< First retained asynchronous failure.
};

/** One complete selected endpoint observation; storage is preallocated, including an executing lease. */
struct EndpointStateChange {
    EndpointState previous = EndpointState::NOT_CONNECTED; ///< Actual preceding published state.
    EndpointState current = EndpointState::NOT_CONNECTED;  ///< Actual newly published state.
    std::shared_ptr<const ListenerSnapshot> listeners;     ///< Complete immutable selection, released outside locks.
};

/** Complete immutable local-source admission. The worker alone opens the file. */
struct TapeSourceRequest {
    std::filesystem::path path; ///< Independently owned stable regular file path.
    TapeSourceOptions options;  ///< Explicit finite input allowances.
};

/** One charged passive request. Its private completion lock never guards native owner work. */
struct LatestEventRead {
    EventType type;        ///< Exact LastingEvent identity.
    std::u16string symbol; ///< Independently owned bounded logical symbol.
    bool ifSubscribed;     ///< Exact-demand and default-event policy.
    std::mutex mutex;      ///< Completion and timeout state only.
    std::condition_variable condition;
    bool completed = false;
    bool cancelled = false;
    Diagnostic failure;
    std::optional<RegisteredEvent> result;

    /** Copy the admitted request input; no caller-owned text survives submission. */
    LatestEventRead(EventType type, std::u16string_view symbol, bool ifSubscribed)
        : type(type), symbol(symbol), ifSubscribed(ifSubscribed) {
    }
};

/** Shared terminal publication plus one worker-owned temporary logical registration. */
struct LatestEventRequestState {
    EventType type;
    std::u16string symbol;
    std::chrono::steady_clock::time_point deadline;
    std::weak_ptr<RuntimeControl> control;
    std::mutex mutex; ///< Guards terminal outcome only; never held across source progress or callback execution.
    std::condition_variable condition;
    bool done = false;
    Diagnostic failure;
    std::optional<RegisteredEvent> result;

    std::uint64_t connectionRevision = 0; ///< Worker-only after producer admission.
    std::uint64_t clearRevision = 0;
    bool boundToSource = false;

    LatestEventRequestState(EventType type, std::u16string_view symbol, std::chrono::steady_clock::time_point deadline,
                            const std::shared_ptr<RuntimeControl> &control)
        : type(type), symbol(symbol), deadline(deadline), control(control) {
    }
};

/** One charged batch. The owner alone mutates its demand and bounded scratch buffers. */
struct LatestEventRequestBatch {
    std::vector<std::shared_ptr<LatestEventRequestState>> requests;
    std::vector<std::size_t> exactIdentity; ///< First occurrence of each exact symbol.
    std::vector<unsigned char> wanted;      ///< One bounded scratch marker per possible identity.
    std::vector<std::size_t> pending;
    std::vector<std::size_t> submitted;
    std::shared_ptr<SubscriptionPort> demand;

    explicit LatestEventRequestBatch(const std::vector<std::shared_ptr<LatestEventRequestState>> &requests);
};

/** Detached query inputs and one terminal result; native demand stays on the SDK owner. */
struct HistorySnapshotOperation {
    const EventType type;
    const std::u16string symbol;
    const std::int32_t sourceId;
    const HistorySnapshotOptions options;
    const bool passive;
    const std::optional<TimeSeriesQueryRange> timeSeriesRange;
    const std::chrono::steady_clock::time_point deadline;
    const std::weak_ptr<RuntimeControl> control;
    std::mutex mutex;
    std::condition_variable condition;
    bool done = false;
    Diagnostic failure;
    HistorySnapshot<RegisteredEvent> result;
    std::uint64_t connectionRevision = 0;
    std::uint64_t clearRevision = 0;
    bool boundToConnection = false;
    std::optional<std::uint64_t> sourceEpoch; ///< First actual source epoch observed by the worker.
    std::shared_ptr<SubscriptionPort> demand; ///< Owner-only temporary registration, charged through retirement.

    HistorySnapshotOperation(EventType type, std::u16string_view symbol, std::int32_t sourceId,
                             HistorySnapshotOptions options, bool passive,
                             std::chrono::steady_clock::time_point deadline,
                             const std::shared_ptr<RuntimeControl> &control,
                             std::optional<TimeSeriesQueryRange> range = std::nullopt)
        : type(type), symbol(symbol), sourceId(sourceId), options(options), passive(passive), timeSeriesRange(range),
          deadline(deadline), control(control) {
    }
};

/** Producer-only endpoint state; native lifetime is exclusively worker-owned. */
struct EndpointPort {
    mutable std::mutex mutex;                 ///< Protects bounded registration/address and detached observation state.
    std::condition_variable retiredCondition; ///< Main-owner await only; no native work runs here.
    std::condition_variable stateCondition;   ///< Current-state wait, independent of full capture quiescence.
    std::weak_ptr<RuntimeControl> control;    ///< Lifetime-gated dispatch, never a free raw runtime pointer.
    std::thread::id owner;   ///< Creator/main await affinity remains available after runtime destruction.
    RuntimeOptions limits;   ///< Finite immutable limits and resolved DNS snapshot.
    EndpointOptions options; ///< Finite immutable endpoint policy.
    std::shared_ptr<CallbackExecutor> eventExecutor; ///< Explicit event-only default; null is SDK worker.
    transport::detail::CallbackRetirementFence
        callbackFence; ///< Embedded; native owner borrows, registry pins through retirement.

    std::optional<EndpointConfiguration> configuration; ///< Immutable data-only policy retained after retirement.
    std::optional<std::chrono::milliseconds> requestedAggregationPeriod; ///< Mutable coalesced local intent only.
    std::uint64_t requestedAggregationRevision = 0; ///< Checked local request commits, independent of reconnect.
    std::optional<transport::AggregationPeriodInfo> aggregationInfo; ///< Worker-published actual source report.
    std::uint64_t aggregationSourceRevision = 0; ///< Control fence for the report; never a request ACK.
    std::vector<std::shared_ptr<SubscriptionPort>> subscriptions; ///< Fixed slots, including retiring registrations.
    std::shared_ptr<LatestEventRead> latestRead; ///< One pending/executing owner observation, protected by mutex.
    std::shared_ptr<LatestEventRequestBatch> latestEventRequest;   ///< One charged request group, protected by mutex.
    std::shared_ptr<HistorySnapshotOperation> historySnapshotRead; ///< One passive observation, protected by mutex.
    std::vector<std::shared_ptr<HistorySnapshotOperation>>
        historySnapshotRequests;   ///< Fixed active request slots, including native retirement.
    StateListeners stateListeners; ///< Opt-in advisory lifecycle observations, retired after all subscriptions.
    std::vector<EndpointStateChange> stateChanges; ///< Fixed ring; selection lease remains charged through release.
    std::size_t stateChangeHead = 0;     ///< Oldest pending/executing entry, reused only after its lease release.
    std::size_t stateChangeCount = 0;    ///< Pending, executing and selection-retiring entries together.
    bool stateChangeDispatching = false; ///< The sole owner has moved the head selection outside the mutex.
    bool connectionIntent = false; ///< Latest admitted logical transport intent; physical application can coalesce.
    std::uint64_t connectionRevision = 0;     ///< Checked control/delivery fence identity, never reused.
    std::uint64_t activeDeliveryRevision = 0; ///< Revision tagged only at a real fresh attempt after source reset.
    std::uint64_t pausedReplayFence =
        0; ///< Local cached replay authorization after native quiescence; not data provenance.
    std::uint64_t clearRevision = 0;   ///< Latest accepted logical clear; all new data/read selections are fenced.
    std::uint64_t clearedRevision = 0; ///< Latest actual applied reset/global-ACK completion, not inferred from state.
    std::string address;               ///< First accepted bounded spelling; immutable after connect publication.
    std::string host;                  ///< Validated bounded route, copied only once before worker publication.
    std::optional<OnDemandRequest> onDemand; ///< Immutable local replay request.
    std::optional<TapeSourceRequest> tape;   ///< Fixed non-network source, absent for TCP.
    bool tapePaused = false;                 ///< Latest local pause intent under the endpoint mutex.
    bool tapeAbortRequested = false;         ///< Monotonic explicit terminal abort for this tape endpoint.
    std::chrono::steady_clock::time_point tapeRestartDeadline; ///< First deadline of the pending restart revision.
    std::uint16_t port = 0;                                    ///< Original nonzero route port.
    bool connectRequested = false;                             ///< Published once under the endpoint mutex.
    std::atomic<std::chrono::steady_clock::duration::rep> closeDeadline{0}; ///< First admitted absolute drain deadline.
    std::atomic<bool> closeRequested{false};                                ///< Reserved monotonic drain admission.
    std::atomic<bool> retired{false};           ///< Native owner and all captures have retired.
    std::atomic<bool> retirementClaimed{false}; ///< One execution domain exclusively owns capture retirement.
    std::atomic<bool> retirementOnMain{false};  ///< Future workers skip an already claimed inactive main cleanup.
    bool retirementResultPrepared = false;      ///< Frozen native outcome survives deferred producer capture cleanup.
    EndpointState state = EndpointState::NOT_CONNECTED; ///< Detached worker observation.
    Diagnostic diagnostic;         ///< First asynchronous failure, retained after native destruction.
    EndpointStatistics statistics; ///< Coherent bounded publication, protected by mutex through final retirement.
    CloseResult terminal;          ///< Frozen actual local outcome, published before retired.
};

/** Shared producer dispatch gate. It owns no bootstrap, native state or joinable thread. */
struct RuntimeControl {
    std::shared_ptr<SubscriptionChangeDomain> changes; ///< Detached synchronous observer owner, with finite work.
    std::mutex callbackPolicyMutex; ///< Serializes transactional endpoint/subscription policy reservations.
    std::shared_ptr<CallbackSlotPool> callbackPool; ///< Lazy fixed pool, one allocation per opted-in runtime.

    std::mutex mutex;                  ///< Protects dispatch pointer, registry, stop admission and wake sequence.
    std::condition_variable condition; ///< Wakeable worker cadence/control wait.
    RuntimeImpl *runtime = nullptr;    ///< Accessible only while mutex holds the lifetime gate.
    std::thread::id owner;             ///< Immutable creator/main thread identity, safe after runtime retirement.
    std::vector<std::shared_ptr<EndpointPort>> endpoints; ///< Fixed pending/active/retiring slots.
    bool stopping = false;                                ///< Admission fence, published before joining.
    std::uint64_t wakeSequence = 0; ///< Coalesced wake hint; wrapping does not create command identities.
};

/** Main-thread lifetime owner. The worker never owns this facade or its joinable thread. */
struct RuntimeImpl {
    std::optional<transport::RuntimeBootstrap> ownedBootstrap; ///< Optional process owner, destroyed after the lease.
    transport::detail::BootstrapLease
        bootstrap;                           ///< Stable process lease, retained through full worker/capture retirement.
    RuntimeOptions options;                  ///< Explicit immutable capacities and resolver snapshot.
    std::shared_ptr<RuntimeControl> control; ///< Detached producer gate; raw dispatch is mutex-protected.
    std::thread worker;                      ///< Lazy joinable SDK worker, owned and joined only on main.
    bool shutdownInProgress = false;         ///< Creator-thread reentry guard, including capture destruction.

    /** Retain the owner parameter through fallible lease/control construction, then adopt it without failure.
     * @param owner Newly acquired process owner; an allocation failure before entry leaves its caller intact.
     * @param options Validated finite configuration and resolver snapshot.
     */
    RuntimeImpl(transport::RuntimeBootstrap owner, RuntimeOptions options);
    /** Adopt a stable host lease without global SDK operations.
     * @param bootstrap Counted process lease, retained through all worker/native/capture retirement.
     * @param options Validated finite configuration and resolver snapshot.
     */
    RuntimeImpl(transport::detail::BootstrapLease bootstrap, RuntimeOptions options);
    /** Fail fast on wrong-owner destruction; join before bootstrap cleanup. */
    ~RuntimeImpl();
    /** Start once while holding control->mutex, which pins this dispatch target's lifetime. */
    void startWorker();
    /** Request all retirement, join and fence future producer dispatch; creator/main thread only. */
    void shutdown();
    /** Retire a never-activated endpoint without creating a worker.
     * @param endpoint Detached pending endpoint.
     * @return True after all accepted and producer-held callable captures have retired.
     */
    bool retireInactive(const std::shared_ptr<EndpointPort> &endpoint);
    /** Run the sole native execution domain; every exit retires native state before returning. */
    void run() noexcept;
};
} // namespace org::ttldtor::qdcxx::api::detail
