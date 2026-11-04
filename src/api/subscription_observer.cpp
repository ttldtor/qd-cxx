// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "subscription_observer.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::size_t DATA_CHANNEL = 0;   ///< Existing complete data fanout lifetime.
constexpr std::size_t CHANGE_CHANNEL = 1; ///< Serialized advisory fanout and final capture lifetime.
thread_local std::shared_ptr<SubscriptionChangeDomain> driverRoot; ///< Iterative physical continuation dispatcher.
thread_local std::shared_ptr<SubscriptionChangeDomain> turnRoot;   ///< Finite causal admission root, across runtimes.
thread_local std::shared_ptr<SubscriptionChangeDomain> callbackOwner; ///< Actual currently selected callback owner.
thread_local std::size_t callbackChannel = CHANGE_CHANNEL;            ///< Current source lifetime-fence channel.

/** @param code Static refusal category. @param stage Library-owned diagnostic checkpoint. */
[[noreturn]] void refuse(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** @param port Target whose atomic close fences are checked before observer admission. */
void requireObserverOpen(const SubscriptionPort &port) {
    const auto endpoint = port.endpoint.lock();

    if (port.closeRequested.load(std::memory_order_acquire) || port.retired.load(std::memory_order_acquire) ||
        !endpoint || endpoint->closeRequested.load(std::memory_order_acquire) ||
        endpoint->retired.load(std::memory_order_acquire) || port.observers.closing) {
        refuse(ErrorCode::CLOSED, "subscription-change-closed");
    }
}

/** @return Exact selected source callback dependency; no owner is borrowed beyond its lifetime. */
SubscriptionCallbackBarrier callbackBarrier() noexcept {
    if (!callbackOwner) {
        return {};
    }

    return {callbackOwner, callbackOwner->fences[callbackChannel].epoch.load(std::memory_order_acquire),
            callbackChannel};
}

/** @param barrier Exact source callback lifetime. @return True until that invocation and its captures return. */
bool blocked(const SubscriptionCallbackBarrier &barrier) noexcept {
    const auto owner = barrier.owner.lock();

    return owner && owner->fences[barrier.channel].active.load(std::memory_order_acquire) &&
           owner->fences[barrier.channel].epoch.load(std::memory_order_acquire) == barrier.epoch;
}

/** @param target Locked target. @return Causal root protected by the admission guard. */
SubscriptionChangeDomain &workRoot(SubscriptionChangeDomain &target) noexcept {
    return turnRoot ? *turnRoot : target;
}

/** @param domain Locked domain whose old causal turn can be reset only after every lease has retired. */
void refreshWork(SubscriptionChangeDomain &domain) noexcept {
    if (!domain.dispatching && domain.dataCallbacks == 0 && domain.leases.load(std::memory_order_acquire) == 0) {
        domain.remainingWork = domain.maximumWork;
    }
}

/** @param target Locked target. @param required Complete registration/phase credit cost, never refunded after commit.
 */
void checkWork(SubscriptionChangeDomain &target, std::size_t required) {
    auto &root = workRoot(target);
    refreshWork(root);

    if (&root != &target) {
        refreshWork(target);
    }

    if (required > root.remainingWork || required > target.remainingWork ||
        (driverRoot && required > driverRoot->remainingWork)) {
        refuse(ErrorCode::WORK_LIMIT, "subscription-change-turn");
    }

    if (callbackOwner && callbackOwner->fences[callbackChannel].epoch.load(std::memory_order_acquire) ==
                             std::numeric_limits<std::uint64_t>::max()) {
        refuse(ErrorCode::RESOURCE_LIMIT, "subscription-change-callback-epoch");
    }
}

/** @param target Locked target. @param used Already preflighted complete admission cost. */
void spendWork(SubscriptionChangeDomain &target, std::size_t used) noexcept {
    auto &root = workRoot(target);
    root.remainingWork -= used;

    if (&root != &target) {
        target.remainingWork -= used;
    }

    if (driverRoot && driverRoot.get() != &root && driverRoot.get() != &target) {
        driverRoot->remainingWork -= used;
    }
}

/** @param target Locked target. @return Whether the current callback requires a cross-domain continuation. */
bool needsContinuation(const std::shared_ptr<SubscriptionChangeDomain> &target) noexcept {
    return callbackOwner && turnRoot && target != turnRoot;
}

/** @param target Locked target; the causal root is also held by the admission guard. */
void checkContinuation(const std::shared_ptr<SubscriptionChangeDomain> &target) {
    const auto check = [](SubscriptionChangeDomain &root, const std::shared_ptr<SubscriptionChangeDomain> &next) {
        if (std::ranges::find(root.continuations, next) == root.continuations.end() &&
            std::ranges::find(root.continuations, nullptr) == root.continuations.end()) {
            refuse(ErrorCode::WORK_LIMIT, "subscription-change-continuations");
        }
    };

    if (needsContinuation(target)) {
        check(*turnRoot, target);
    }

    if (callbackOwner && driverRoot && turnRoot != driverRoot) {
        check(*driverRoot, turnRoot);
    }
}

/** @param target Preflighted target; publication cannot allocate or fail. */
void publishContinuation(const std::shared_ptr<SubscriptionChangeDomain> &target) noexcept {
    const auto publish = [](SubscriptionChangeDomain &root, const std::shared_ptr<SubscriptionChangeDomain> &next) {
        if (std::ranges::find(root.continuations, next) == root.continuations.end()) {
            *std::ranges::find(root.continuations, nullptr) = next;
        }
    };

    if (needsContinuation(target)) {
        publish(*turnRoot, target);
    }

    if (callbackOwner && driverRoot && turnRoot != driverRoot) {
        publish(*driverRoot, turnRoot);
    }
}

/** @param port Protected owner. @param symbols Complete candidate. @param revision Checked next accepted identity.
 * @return Fully owned immutable backing; no callback, native state or caller view is retained.
 */
std::shared_ptr<SubscriptionChangeFrame>
frameFor(const SubscriptionPort &port, const std::vector<SubscriptionSelector> &symbols, std::uint64_t revision) {
    auto frame = std::make_shared<SubscriptionChangeFrame>();
    frame->snapshot = {revision, symbols, port.eventTypes};
    frame->barrier = callbackBarrier();

    return frame;
}

/** @param port Protected owner. @param kind Phase kind. @param frame Complete immutable backing.
 * @param target Initial-only selection; ordinary fanout selects the current initialized registry.
 */
void appendPhase(SubscriptionPort &port, SubscriptionChangeKind kind,
                 const std::shared_ptr<const SubscriptionChangeFrame> &frame,
                 std::shared_ptr<const ListenerSnapshot> target = {}) noexcept {
    SubscriptionChangePhase phase;
    phase.kind = kind;
    phase.lease = SubscriptionChangeLease(turnRoot ? turnRoot : port.changes);
    phase.frame = frame;
    phase.target = std::move(target);
    auto &observers = port.observers;
    observers.phases[(observers.head + observers.count) % observers.phases.size()] = std::move(phase);
    ++observers.count;
}

/** @param port Protected owner. @param entry Charged observer. @param root Reserved causal root for this terminal
 * phase. */
void reserveTerminal(SubscriptionPort &port, const ListenerEntry &entry,
                     const std::shared_ptr<SubscriptionChangeDomain> &root) noexcept {
    auto &observers = port.observers;
    const auto available = std::ranges::find_if(observers.closed, [](const auto &slot) {
        return slot.listener.id == 0;
    });

    if (available == observers.closed.end()) {
        std::terminate(); // One terminal slot was allocated for each possible charged callable.
    }

    available->listener = entry;
    available->frame = observers.latest;
    available->barrier = callbackBarrier();
    available->lease = SubscriptionChangeLease(root);
}

/** Selected immutable callable/argument leases; all final application release occurs outside locks. */
struct SelectedChange {
    std::shared_ptr<SubscriptionPort> port;
    SubscriptionChangePhase phase;
    ListenerEntry closed{};
    std::shared_ptr<const ListenerSnapshot> listeners;
    bool ordinary = false;
};

/** @param domain Locked target. @param selected Empty output. @return True after selecting one runnable phase. */
bool selectChange(SubscriptionChangeDomain &domain, SelectedChange &selected) noexcept {
    for (const auto &weak : domain.ports) {
        const auto port = weak.lock();

        if (!port) {
            continue;
        }

        auto &observers = port->observers;
        const auto terminal = std::ranges::find_if(observers.closed, [](const auto &slot) {
            return slot.listener.id != 0 && !blocked(slot.barrier);
        });

        if (terminal != observers.closed.end()) {
            selected.port = port;
            selected.closed = std::move(terminal->listener);
            terminal->listener.id = 0;
            selected.phase.kind = SubscriptionChangeKind::CLOSED;
            selected.phase.frame = std::move(terminal->frame);
            selected.phase.lease = std::move(terminal->lease);
            terminal->barrier = {};

            return true;
        }

        if (observers.count == 0 || blocked(observers.phases[observers.head].frame->barrier)) {
            continue;
        }

        selected.port = port;
        selected.phase = std::move(observers.phases[observers.head]);
        selected.listeners = selected.phase.target ? selected.phase.target : observers.listeners;
        selected.ordinary = true;

        if (selected.phase.target) {
            const auto id = selected.phase.target->front().id;

            if (!observers.listeners ||
                std::ranges::find(*observers.listeners, id, &ListenerEntry::id) == observers.listeners->end()) {
                selected.listeners.reset();
            } else {
                selected.phase.target->front().callback->changeInitialized = true;
            }
        }

        return true;
    }

    return false;
}

/** @param domain Locked target. @return Whether any unretired phase remains, including blocked work. */
bool hasPending(const SubscriptionChangeDomain &domain) noexcept {
    return std::ranges::any_of(domain.ports, [](const auto &weak) {
        const auto port = weak.lock();

        return port && (port->observers.count != 0 || std::ranges::any_of(port->observers.closed, [](const auto &slot) {
                            return slot.listener.id != 0;
                        }));
    });
}

/** @param domain Exclusive claimed callback owner. @param channel Existing data or change channel. */
void beginCallback(SubscriptionChangeDomain &domain, std::size_t channel) noexcept {
    auto &fence = domain.fences[channel];

    if (fence.active.load(std::memory_order_relaxed)) {
        std::terminate(); // Data owner and change claimant each serialize their own channel.
    }

    const auto previous = fence.epoch.load(std::memory_order_relaxed);

    if (previous != std::numeric_limits<std::uint64_t>::max()) {
        fence.epoch.store(previous + 1, std::memory_order_relaxed);
    }

    // At saturation, new observer-producing reentry is refused rather than reusing a live dependency identity.
    fence.active.store(true, std::memory_order_release);
}

/** @param domain Claimed detached owner. Drain only prepaid runnable selections, never native/network work. */
void drainDomain(const std::shared_ptr<SubscriptionChangeDomain> &domain) noexcept {
    for (;;) {
        SelectedChange selected;
        {
            std::lock_guard lock(domain->mutex);

            if (domain->dataCallbacks != 0 || !selectChange(*domain, selected)) {
                return;
            }

            beginCallback(*domain, CHANGE_CHANNEL);
        }

        auto previousOwner = std::move(callbackOwner);
        auto previousRoot = std::move(turnRoot);
        const auto previousChannel = callbackChannel;
        callbackOwner = domain;
        callbackChannel = CHANGE_CHANNEL;
        turnRoot = selected.phase.lease.root;
        const auto &frame = *selected.phase.frame;
        const auto &symbols = selected.phase.kind == SubscriptionChangeKind::REMOVED ? frame.removed : frame.added;
        const SubscriptionChange change{selected.phase.kind, frame.snapshot,
                                        selected.phase.kind == SubscriptionChangeKind::CLOSED
                                            ? std::span<const SubscriptionSelector>{}
                                            : std::span<const SubscriptionSelector>(symbols)};
        const auto invoke = [&](const ListenerEntry &entry) {
            try {
                std::get<SubscriptionChangeListener>(entry.callback->function)(change);
            } catch (...) {
                std::lock_guard lock(domain->mutex);

                if (selected.port->observers.diagnostic.code == ErrorCode::NONE) {
                    selected.port->observers.diagnostic = {ErrorCode::CALLBACK_FAILURE, "subscription-change-listener"};
                }
            }
        };

        if (selected.closed.callback) {
            invoke(selected.closed);
        } else if (selected.listeners) {
            for (const auto &entry : *selected.listeners) {
                if (selected.phase.target || entry.callback->changeInitialized) {
                    invoke(entry);
                }
            }
        }

        // Callback context, root credits and barrier stay live through final user capture destruction.
        selected.listeners.reset();
        selected.closed.callback.reset();
        selected.phase.target.reset();
        selected.phase.frame.reset();
        domain->fences[CHANGE_CHANNEL].active.store(false, std::memory_order_release);
        callbackOwner = std::move(previousOwner);
        callbackChannel = previousChannel;
        turnRoot = std::move(previousRoot);
        selected.phase.lease = {};

        if (selected.ordinary) {
            std::lock_guard lock(domain->mutex);
            auto &observers = selected.port->observers;
            observers.head = (observers.head + 1) % observers.phases.size();
            --observers.count;
        }
    }
}
} // namespace

SubscriptionObserverAdmission::SubscriptionObserverAdmission(std::shared_ptr<SubscriptionChangeDomain> target)
    : target_(std::move(target)), root_(turnRoot ? turnRoot : target_), driver_(driverRoot ? driverRoot : root_),
      targetLock_(target_->mutex, std::defer_lock) {
    if (root_ != target_) {
        rootLock_ = std::unique_lock<std::mutex>(root_->mutex, std::defer_lock);
    }

    if (driver_ != target_ && driver_ != root_) {
        driverLock_ = std::unique_lock<std::mutex>(driver_->mutex, std::defer_lock);
    }

    if (rootLock_.mutex() && driverLock_.mutex()) {
        std::lock(targetLock_, rootLock_, driverLock_);
    } else if (rootLock_.mutex()) {
        std::lock(targetLock_, rootLock_);
    } else if (driverLock_.mutex()) {
        std::lock(targetLock_, driverLock_);
    } else {
        targetLock_.lock();
    }
}

SubscriptionChangeLease::SubscriptionChangeLease(std::shared_ptr<SubscriptionChangeDomain> root) noexcept
    : root(std::move(root)) {
    this->root->leases.fetch_add(1, std::memory_order_relaxed);
}

SubscriptionChangeLease::SubscriptionChangeLease(SubscriptionChangeLease &&other) noexcept
    : root(std::move(other.root)) {
}

SubscriptionChangeLease &SubscriptionChangeLease::operator=(SubscriptionChangeLease &&other) noexcept {
    if (this != &other) {
        if (root) {
            root->leases.fetch_sub(1, std::memory_order_release);
        }

        root = std::move(other.root);
    }

    return *this;
}

SubscriptionChangeLease::~SubscriptionChangeLease() {
    if (root) {
        root->leases.fetch_sub(1, std::memory_order_release);
    }
}

SubscriptionSnapshot subscriptionSnapshotLocked(const SubscriptionPort &port) {
    return {port.revision, port.symbols, port.eventTypes};
}

void initializeSubscriptionObservers(const std::shared_ptr<SubscriptionPort> &port, const RuntimeOptions &limits) {
    auto &observers = port->observers;
    observers.ledger = std::make_shared<ListenerLedger>();
    observers.ledger->maximum = limits.maximumChangeListenersPerSubscription;
    observers.ledger->control = port->control;
    observers.ledger->endpoint = port->endpoint;
    observers.listeners = std::make_shared<const ListenerSnapshot>();
    observers.phases.resize(limits.maximumPendingSubscriptionChanges);
    observers.closed.resize(limits.maximumChangeListenersPerSubscription);
    const auto control = port->control.lock();

    if (!control) {
        refuse(ErrorCode::CLOSED, "endpoint-closed");
    }

    port->changes = control->changes;
    std::lock_guard lock(port->changes->mutex);
    const auto slot = std::ranges::find_if(port->changes->ports, [](const auto &weak) {
        const auto old = weak.lock();

        return !old || (old->retired.load(std::memory_order_acquire) &&
                        old->observers.ledger->count.load(std::memory_order_acquire) == 0);
    });

    if (slot == port->changes->ports.end()) {
        refuse(ErrorCode::RESOURCE_LIMIT, "subscription-change-ports");
    }

    *slot = port;
}

void prepareSubscriptionChangeLocked(SubscriptionPort &port, const std::vector<SubscriptionSelector> &symbols,
                                     std::uint64_t revision) {
    auto &observers = port.observers;

    if (!observers.listeners || observers.listeners->empty()) {
        return;
    }

    auto frame = frameFor(port, symbols, revision);

    for (const auto &old : port.symbols) {
        if (std::ranges::find(symbols, old) == symbols.end()) {
            frame->removed.push_back(old);
        }
    }

    for (const auto &current : symbols) {
        const auto old = std::ranges::find(port.symbols, current);

        if (old == port.symbols.end() || !current.sameDemand(*old)) {
            frame->added.push_back(current);
        }
    }

    const auto phases = static_cast<std::size_t>(!frame->removed.empty()) + !frame->added.empty();

    if (phases > observers.phases.size() - observers.count) {
        refuse(ErrorCode::RESOURCE_LIMIT, "subscription-change-pending");
    }

    checkWork(*port.changes, phases);
    checkContinuation(port.changes);
    spendWork(*port.changes, phases);
    publishContinuation(port.changes);
    observers.latest = frame;

    if (!frame->removed.empty()) {
        appendPhase(port, SubscriptionChangeKind::REMOVED, frame);
    }

    if (!frame->added.empty()) {
        appendPhase(port, SubscriptionChangeKind::ADDED, frame);
    }
}

std::uint64_t addSubscriptionObserver(const std::shared_ptr<SubscriptionPort> &port,
                                      SubscriptionChangeListener listener) {
    if (!listener) {
        refuse(ErrorCode::INVALID_ARGUMENT, "subscription-change-listener");
    }

    const auto ledger = port->observers.ledger;
    std::shared_ptr<ListenerBox> callback;
    std::shared_ptr<const ListenerSnapshot> previous;
    std::uint64_t id;
    {
        SubscriptionObserverAdmission admission(port->changes);
        std::lock_guard portLock(port->mutex);
        requireObserverOpen(*port);
        auto &observers = port->observers;
        const auto work = std::size_t{1} + !port->symbols.empty();
        checkWork(*port->changes, work);

        if (ledger->count.load(std::memory_order_acquire) >= ledger->maximum ||
            observers.nextListener == std::numeric_limits<std::uint64_t>::max()) {
            refuse(ErrorCode::RESOURCE_LIMIT, "subscription-change-listeners");
        }

        if (!port->symbols.empty()) {
            if (observers.count == observers.phases.size()) {
                refuse(ErrorCode::RESOURCE_LIMIT, "subscription-change-pending");
            }

            checkContinuation(port->changes);
        }

        auto frame = frameFor(*port, port->symbols, port->revision);
        frame->added = port->symbols;
        auto candidate = std::make_shared<ListenerSnapshot>(*observers.listeners);
        auto target = std::make_shared<ListenerSnapshot>();
        candidate->reserve(ledger->maximum);
        target->reserve(1);
        callback = std::make_shared<ListenerBox>(ledger, std::move(listener));
        ledger->count.fetch_add(1, std::memory_order_relaxed);
        callback->changeInitialized = port->symbols.empty();
        id = observers.nextListener++;
        candidate->push_back({id, callback});
        target->push_back({id, callback});
        previous = std::move(observers.listeners);
        observers.listeners = std::move(candidate);
        observers.latest = frame;
        spendWork(*port->changes, work);

        if (!port->symbols.empty()) {
            publishContinuation(port->changes);
            appendPhase(*port, SubscriptionChangeKind::ADDED, frame, std::move(target));
        }
    }

    previous.reset();
    callback.reset();
    dispatchSubscriptionChanges(port->changes);

    return id;
}

void removeSubscriptionObserver(const std::shared_ptr<SubscriptionPort> &port, std::uint64_t id) {
    std::shared_ptr<const ListenerSnapshot> previous;
    {
        SubscriptionObserverAdmission admission(port->changes);
        auto &observers = port->observers;

        if (!observers.listeners) {
            return;
        }

        const auto found = std::ranges::find(*observers.listeners, id, &ListenerEntry::id);

        if (found == observers.listeners->end()) {
            return;
        }

        checkWork(*port->changes, 1);
        checkContinuation(port->changes);
        auto candidate = std::make_shared<ListenerSnapshot>();
        candidate->reserve(observers.listeners->size() - 1);

        for (const auto &entry : *observers.listeners) {
            if (entry.id != id) {
                candidate->push_back(entry);
            }
        }

        spendWork(*port->changes, 1);
        publishContinuation(port->changes);
        reserveTerminal(*port, *found, turnRoot ? turnRoot : port->changes);
        previous = std::move(observers.listeners);
        observers.listeners = std::move(candidate);
    }

    previous.reset();
    dispatchSubscriptionChanges(port->changes);
}

void closeSubscriptionObservers(const std::shared_ptr<SubscriptionPort> &port, bool dispatch) noexcept {
    std::shared_ptr<const ListenerSnapshot> previous;
    {
        std::lock_guard lock(port->changes->mutex);
        auto &observers = port->observers;

        if (!observers.closing) {
            observers.closing = true;

            if (observers.listeners) {
                for (const auto &entry : *observers.listeners) {
                    reserveTerminal(*port, entry, port->changes);
                }
            }

            previous = std::move(observers.listeners);
        }
    }

    previous.reset();

    if (dispatch) {
        dispatchSubscriptionChanges(port->changes);
    }
}

void dispatchSubscriptionChanges(const std::shared_ptr<SubscriptionChangeDomain> &domain) noexcept {
    if (callbackOwner) {
        return; // Admission already reserved the root's finite after-return continuation.
    }

    {
        std::lock_guard lock(domain->mutex);

        if (domain->dispatching || domain->dataCallbacks != 0) {
            return;
        }

        domain->dispatching = true;
    }

    auto previousRoot = std::move(turnRoot);
    auto previousDriver = std::move(driverRoot);
    driverRoot = domain;
    turnRoot = domain;
    auto current = domain;

    for (;;) {
        if (current) {
            drainDomain(current);

            if (current != domain) {
                std::lock_guard lock(current->mutex);
                current->dispatching = false;

                if (!hasPending(*current)) {
                    refreshWork(*current);
                }
            }

            current.reset();
        }

        std::shared_ptr<SubscriptionChangeDomain> next;
        {
            std::lock_guard lock(domain->mutex);

            if (domain->dataCallbacks != 0) {
                domain->dispatching = false;
                turnRoot = std::move(previousRoot);
                driverRoot = std::move(previousDriver);

                return; // Existing data callback exit retains and drains all reserved target continuations.
            }

            const auto slot = std::ranges::find_if(domain->continuations, [](const auto &target) {
                return target != nullptr;
            });

            if (slot != domain->continuations.end()) {
                next = std::move(*slot);
            } else {
                // One final root pass catches reentry from the last foreign target without recursive dispatch.
                const bool rootPending = std::ranges::any_of(domain->ports, [](const auto &weak) {
                    const auto port = weak.lock();

                    return port && ((port->observers.count != 0 &&
                                     !blocked(port->observers.phases[port->observers.head].frame->barrier)) ||
                                    std::ranges::any_of(port->observers.closed, [](const auto &terminal) {
                                        return terminal.listener.id != 0 && !blocked(terminal.barrier);
                                    }));
                });

                if (rootPending) {
                    current = domain;
                    continue;
                }

                domain->dispatching = false;

                if (!hasPending(*domain)) {
                    refreshWork(*domain);
                }

                turnRoot = std::move(previousRoot);
                driverRoot = std::move(previousDriver);

                return;
            }
        }

        {
            std::lock_guard lock(next->mutex);

            if (!next->dispatching && next->dataCallbacks == 0) {
                next->dispatching = true;
                current = next;
            }
        }
        // A busy target already has a real callback owner; its causal leases preserve the original root budget.
        next.reset();
    }
}

bool subscriptionObserverCallbackActive() noexcept {
    return callbackOwner != nullptr;
}

SubscriptionDataCallbackScope::SubscriptionDataCallbackScope(std::shared_ptr<SubscriptionChangeDomain> domain) noexcept
    : domain_(std::move(domain)), previousDriver_(std::move(driverRoot)), previousRoot_(std::move(turnRoot)),
      previousOwner_(std::move(callbackOwner)), previousChannel_(callbackChannel) {
    driverRoot = previousDriver_ ? previousDriver_ : domain_;
    turnRoot = previousRoot_ ? previousRoot_ : domain_;
    callbackOwner = domain_;
    callbackChannel = DATA_CHANNEL;
    std::lock_guard lock(domain_->mutex);
    ++domain_->dataCallbacks;
    beginCallback(*domain_, DATA_CHANNEL);
}

SubscriptionDataCallbackScope::~SubscriptionDataCallbackScope() {
    domain_->fences[DATA_CHANNEL].active.store(false, std::memory_order_release);
    {
        std::lock_guard lock(domain_->mutex);
        --domain_->dataCallbacks;
    }

    callbackOwner = std::move(previousOwner_);
    callbackChannel = previousChannel_;
    turnRoot = std::move(previousRoot_);
    driverRoot = std::move(previousDriver_);
    dispatchSubscriptionChanges(domain_);
}
} // namespace org::ttldtor::qdcxx::api::detail
