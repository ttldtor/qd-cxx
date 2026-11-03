// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

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
                 Endpoint::StateChangeListener, Subscription<events::Quote>::StateChangeListener>;

/** Shared application callable; its last release never occurs under a control mutex. */
struct ListenerBox {
    std::shared_ptr<ListenerLedger> ledger; ///< Keeps the detached count alive through callback destruction.
    ListenerFunction function;              ///< Application captures, moved in before publication.

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

/** Producer-only subscription state; no queue, receiver, schema or native owner can escape here. */
struct SubscriptionPort {
    mutable std::mutex mutex;               ///< Protects accepted snapshots and detached observations only.
    std::condition_variable stateCondition; ///< Creator current-state wait; publication uses this port mutex.
    std::thread::id owner; ///< Immutable runtime creator affinity, retained after endpoint/runtime destruction.
    std::weak_ptr<RuntimeControl> control;             ///< Wake/lifetime gate, without worker ownership.
    std::weak_ptr<EndpointPort> endpoint;              ///< Endpoint policy/status domain, without a producer cycle.
    std::shared_ptr<ListenerLedger> listenerLedger;    ///< Selected/retiring callback reservation.
    std::shared_ptr<const ListenerSnapshot> listeners; ///< Complete accepted bounded selection.
    StateListeners stateListeners;       ///< Opt-in advisory lifecycle observations, retired after event captures.
    std::vector<std::u16string> symbols; ///< Complete accepted exact desired set.
    std::size_t maximumSymbols;          ///< Original per-registration selector count.
    std::size_t maximumSymbolUnits;      ///< Original per-selector capacity.
    std::uint64_t revision = 0;          ///< Accepted local desired revision, separate from queue tickets.
    std::uint64_t nextListener = 1;      ///< Non-reused listener identity, zero never issued.
    EventType eventType;                 ///< Immutable business projection; its explicit record routes are separate.
    std::atomic<bool> closeRequested{false}; ///< Reserved monotonic control, independent of ordinary capacity.
    std::atomic<bool> retired{false};        ///< True only after callback/capture/registration retirement.
    SubscriptionState state = SubscriptionState::CONFIGURED; ///< Detached worker observation.
    Diagnostic diagnostic;                                   ///< First retained asynchronous failure.
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
    std::vector<std::shared_ptr<SubscriptionPort>> subscriptions; ///< Fixed slots, including retiring registrations.
    StateListeners stateListeners; ///< Opt-in advisory lifecycle observations, retired after all subscriptions.
    std::shared_ptr<const ListenerSnapshot> connectingListeners; ///< One producer-published CONNECTING selection.
    bool connectingNotification = false; ///< Deferred first transition, dispatched before every worker exit path.
    std::string address;                 ///< First accepted bounded spelling; immutable after connect publication.
    std::string host;                    ///< Validated bounded route, copied only once before worker publication.
    std::uint16_t port = 0;              ///< Original nonzero route port.
    bool connectRequested = false;       ///< Published once under the endpoint mutex.
    std::atomic<std::chrono::steady_clock::duration::rep> closeDeadline{0}; ///< First admitted absolute drain deadline.
    std::atomic<bool> closeRequested{false};                                ///< Reserved monotonic drain admission.
    std::atomic<bool> retired{false};           ///< Native owner and all captures have retired.
    std::atomic<bool> retirementClaimed{false}; ///< One execution domain exclusively owns capture retirement.
    std::atomic<bool> retirementOnMain{false};  ///< Future workers skip an already claimed inactive main cleanup.
    bool retirementResultPrepared = false;      ///< Frozen native outcome survives deferred producer capture cleanup.
    EndpointState state = EndpointState::NOT_CONNECTED; ///< Detached worker observation.
    Diagnostic diagnostic; ///< First asynchronous failure, retained after native destruction.
    CloseResult terminal;  ///< Frozen actual local outcome, published before retired.
};

/** Shared producer dispatch gate. It owns no bootstrap, native state or joinable thread. */
struct RuntimeControl {
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
    transport::RuntimeBootstrap bootstrap;   ///< Constructed on main before worker and destroyed after join.
    RuntimeOptions options;                  ///< Explicit immutable capacities and resolver snapshot.
    std::shared_ptr<RuntimeControl> control; ///< Detached producer gate; raw dispatch is mutex-protected.
    std::thread worker;                      ///< Lazy joinable SDK worker, owned and joined only on main.
    bool shutdownInProgress = false;         ///< Creator-thread reentry guard, including capture destruction.

    /** Adopt main-owned bootstrap and construct fixed detached control storage.
     * @param bootstrap Explicit process lifetime owner.
     * @param options Validated finite configuration, with DNS resolution settings already snapped.
     */
    RuntimeImpl(transport::RuntimeBootstrap bootstrap, RuntimeOptions options);
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
