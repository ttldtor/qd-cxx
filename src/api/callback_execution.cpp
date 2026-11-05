// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "callback_execution.hpp"
#include "../core/delivery_statistics.hpp"
#include "feed_internal.hpp"
#include "subscription_observer.hpp"
#include <algorithm>
#include <utility>

namespace org::ttldtor::qdcxx::api {
CallbackTask::CallbackTask(std::shared_ptr<detail::CallbackSlotPool> pool, std::size_t index,
                           std::uint64_t generation) noexcept
    : pool_(std::move(pool)), index_(index), generation_(generation) {
}

CallbackTask::CallbackTask(CallbackTask &&other) noexcept
    : pool_(std::move(other.pool_)), index_(other.index_), generation_(other.generation_) {
}

CallbackTask &CallbackTask::operator=(CallbackTask &&other) noexcept {
    if (this != &other) {
        if (pool_)
            pool_->abandon(index_, generation_);

        pool_ = std::move(other.pool_);
        index_ = other.index_;
        generation_ = other.generation_;
    }

    return *this;
}

CallbackTask::~CallbackTask() {
    if (pool_)
        pool_->abandon(index_, generation_);
}

CallbackTaskRunResult CallbackTask::run() const noexcept {
    return pool_ ? pool_->run(index_, generation_) : CallbackTaskRunResult::CANCELLED;
}

namespace detail {
namespace {
struct PostingContext {
    CallbackSlotPool *pool;
    std::size_t index;
    bool nested;
    SubscriptionCallbackBarrier source;
    PostingContext *previous;
};

thread_local PostingContext *postingContext = nullptr;
} // namespace

std::size_t CallbackSlotPool::storageBytes(std::size_t slots) {
    constexpr auto controlAllowance = 4 * sizeof(void *);

    if (slots > (std::numeric_limits<std::size_t>::max() - sizeof(CallbackSlotPool) - controlAllowance) / sizeof(Slot))
        throw Error({ErrorCode::RESOURCE_LIMIT, "callback-pool-bytes"});

    return sizeof(CallbackSlotPool) + controlAllowance + slots * sizeof(Slot);
}

CallbackRetirementSnapshot callbackRetirementSnapshot(const SubscriptionPort &port) noexcept {
    std::lock_guard lock(port.mutex);

    return callbackRetirementSnapshotLocked(port);
}

CallbackRetirementSnapshot callbackRetirementSnapshotLocked(const SubscriptionPort &port) noexcept {
    auto result = port.callbackPool ? port.callbackPool->snapshot(port.callbackSlot) : CallbackRetirementSnapshot{};
    // Queue selection precedes adapter entry. Its actual in-flight gauge closes that admission window,
    // including an obsolete/listener-free offer that is immediately discarded by the adapter.
    result.quiescent =
        result.quiescent && !port.callbackSynchronousExecuting && port.statistics->snapshot().inFlightEvents == 0;
    result.retirementRevision = result.retirementRevision > UINT64_MAX - port.callbackSynchronousRetirementRevision
                                    ? UINT64_MAX
                                    : result.retirementRevision + port.callbackSynchronousRetirementRevision;

    return result;
}

CallbackSlotPool::CallbackSlotPool(std::size_t slots, std::size_t reservations)
    : slots_(slots), maximumReservations_(reservations) {
}

bool CallbackSlotPool::canReserve(std::size_t additional) const noexcept {
    std::lock_guard lock(mutex_);

    return additional <= maximumReservations_ - reservations_;
}

std::size_t CallbackSlotPool::assign() {
    std::lock_guard lock(mutex_);

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].assigned && !slots_[i].retiring && !slots_[i].packet && !slots_[i].preparing) {
            slots_[i].assigned = true;
            slots_[i].release = false;

            return i;
        }
    }

    throw Error({ErrorCode::RESOURCE_LIMIT, "callback-slots"});
}

bool CallbackSlotPool::reserved(std::size_t index) const noexcept {
    std::lock_guard lock(mutex_);

    return slots_[index].reserved;
}

void CallbackSlotPool::reserve(std::size_t index, bool desired) {
    std::lock_guard lock(mutex_);
    auto &slot = slots_[index];

    if (desired && !slot.reserved) {
        if (reservations_ == maximumReservations_)
            throw Error({ErrorCode::RESOURCE_LIMIT, "callback-packet-budget"});

        ++reservations_;
        slot.reserved = true;
    }

    slot.desired = desired;

    if (!desired && slot.reserved && !slot.packet && !slot.retiring && !slot.preparing) {
        --reservations_;
        slot.reserved = false;
    }
}

void CallbackSlotPool::release(std::size_t index) noexcept {
    std::lock_guard lock(mutex_);
    auto &slot = slots_[index];
    slot.desired = false;
    slot.release = true;

    if (!slot.packet && !slot.retiring && !slot.preparing) {
        if (slot.reserved)
            --reservations_;

        slot.reserved = false;
        slot.assigned = false;
    }
}

void CallbackSlotPool::begin(std::size_t index, CallbackPacketLease lease) {
    std::lock_guard lock(mutex_);
    auto &slot = slots_[index];

    if (!slot.reserved || slot.packet || slot.preparing || slot.retiring || slot.generation == UINT64_MAX ||
        slot.retirementRevision == UINT64_MAX)
        throw Error({ErrorCode::RESOURCE_LIMIT, "callback-slot-admission"});

    ++slot.generation;
    slot.preparing = true;
    slot.finished = slot.started = slot.claimed = slot.running = slot.posting = false;
    slot.diagnostic = {};
    slot.lease = std::move(lease);
    slot.lease.endpoint->outstanding.fetch_add(1, std::memory_order_acq_rel);
}

void CallbackSlotPool::abortPreparation(std::size_t index) noexcept {
    std::uint64_t generation;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];
        generation = slot.generation;
        slot.preparing = false;
        slot.finished = true;
    }
    retire(index, generation);
}

void CallbackSlotPool::submit(std::size_t index, std::unique_ptr<CallbackInvocation> packet) {
    std::uint64_t generation;
    CallbackInvocation *invocation;
    std::shared_ptr<SubscriptionChangeDomain> domain;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];
        generation = slot.generation;
        slot.packet = std::move(packet);
        // Cancellation can win during projection/allocation, but cannot destroy a preparing caller's storage.
        slot.posting = true;
        invocation = slot.packet.get();
        domain = slot.lease.domain;
    }
    Diagnostic preparation;
    try {
        invocation->prepare();
    } catch (const Error &error) {
        preparation = error.diagnostic();
    } catch (const std::bad_alloc &) {
        preparation = {ErrorCode::ALLOCATION_FAILURE, "callback-prepare"};
    } catch (...) {
        preparation = {ErrorCode::RESOURCE_LIMIT, "callback-prepare"};
    }
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];
        slot.preparing = false;

        if (preparation.code != ErrorCode::NONE) {
            slot.diagnostic = preparation;
            slot.finished = true;
        }

        if (slot.finished)
            slot.posting = false;
    }

    {
        std::lock_guard lock(mutex_);

        if (slots_[index].finished)
            invocation = nullptr;
    }

    if (!invocation) {
        retire(index, generation);

        return;
    }

    // A posting lease prevents inline/concurrent completion from retiring the selected policy early.
    CallbackPostResult result = CallbackPostResult::REJECTED;
    bool threw = false;
    PostingContext posting{this, index, subscriptionObserverCallbackActive(), currentSubscriptionCallbackBarrier(),
                           postingContext};
    postingContext = &posting;
    try {
        SubscriptionDataCallbackScope scope(domain, shared_from_this(), index, generation);
        result = invocation->post(CallbackTask(shared_from_this(), index, generation));
    } catch (...) {
        threw = true;
    }
    postingContext = posting.previous;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];

        if (threw || result == CallbackPostResult::REJECTED) {
            slot.diagnostic = {slot.started ? ErrorCode::EXECUTOR_CONTRACT
                               : threw      ? ErrorCode::EXECUTOR_FAILURE
                                            : ErrorCode::EXECUTOR_REJECTED,
                               "callback-post"};

            if (!slot.running)
                slot.finished = true;
        }

        slot.posting = false;
    }
    retire(index, generation);
}

void CallbackSlotPool::cancel(std::size_t index, Diagnostic reason) noexcept {
    std::uint64_t generation;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];

        if ((!slot.packet && !slot.preparing) || slot.running || slot.finished)
            return;

        slot.diagnostic = reason;
        slot.finished = true;
        slot.claimed = true;
        generation = slot.generation;
    }
    retire(index, generation);
}

void CallbackSlotPool::abandon(std::size_t index, std::uint64_t generation) noexcept {
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];

        if (slot.generation != generation || !slot.packet || slot.claimed || slot.finished)
            return;

        slot.diagnostic = {ErrorCode::EXECUTOR_DROPPED, "callback-task-drop"};
        slot.finished = true;
    }
    retire(index, generation);
}

CallbackTaskRunResult CallbackSlotPool::run(std::size_t index, std::uint64_t generation) noexcept {
    return execute(index, generation, false);
}

CallbackTaskRunResult CallbackSlotPool::resume(std::size_t index, std::uint64_t generation) noexcept {
    return execute(index, generation, true);
}

CallbackTaskRunResult CallbackSlotPool::execute(std::size_t index, std::uint64_t generation, bool deferred) noexcept {
    CallbackInvocation *invocation;
    std::shared_ptr<SubscriptionChangeDomain> domain;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];

        if (slot.generation != generation || !slot.packet || slot.finished || slot.running ||
            (slot.claimed && !deferred))
            return CallbackTaskRunResult::CANCELLED;

        slot.claimed = true;

        if (!deferred && subscriptionObserverCallbackActive() &&
            !(postingContext && postingContext->pool == this && postingContext->index == index &&
              !postingContext->nested)) {
            // No pool lock is taken by the pure observer-active query.
            invocation = nullptr;
        } else {
            slot.running = true;
            slot.started = true;
            invocation = slot.packet.get();
            domain = slot.lease.domain;
        }
    }

    if (!invocation) {
        try {
            const auto *source = postingContext && postingContext->pool == this && postingContext->index == index &&
                                         postingContext->nested
                                     ? &postingContext->source
                                     : nullptr;
            deferCallbackTask(shared_from_this(), index, generation, source);

            return CallbackTaskRunResult::DEFERRED;
        } catch (const Error &error) {
            cancel(index, error.diagnostic());
        } catch (...) {
            cancel(index, {ErrorCode::ALLOCATION_FAILURE, "callback-defer"});
        }

        return CallbackTaskRunResult::CANCELLED;
    }

    {
        SubscriptionDataCallbackScope scope(std::move(domain), shared_from_this(), index, generation);
        invocation->execute();
    }
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];
        slot.running = false;
        slot.finished = true;
    }
    retire(index, generation);

    return CallbackTaskRunResult::COMPLETED;
}

void CallbackSlotPool::retire(std::size_t index, std::uint64_t generation) noexcept {
    std::unique_ptr<CallbackInvocation> packet;
    CallbackPacketLease lease;
    Diagnostic diagnostic;
    {
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];

        if (slot.generation != generation || !slot.lease.domain || (!slot.packet && !slot.finished) || !slot.finished ||
            slot.preparing || slot.posting || slot.running || slot.retiring)
            return;

        slot.retiring = true;
        packet = std::move(slot.packet);
        lease = std::move(slot.lease);
        diagnostic = slot.diagnostic;
    }
    {
        SubscriptionDataCallbackScope scope(lease.domain, shared_from_this(), index, generation);

        if (packet)
            packet->finish(diagnostic);

        packet.reset(); // Final user captures and policy destructors run under the same after-return barrier.
        std::lock_guard lock(mutex_);
        auto &slot = slots_[index];
        ++slot.retirementRevision;
        slot.retiring = false;
        // Registry replacement/terminal publication must first obtain this same quiescence lock.
        // Release the borrowed embedded fence here, before allowing its endpoint owner to retire.
        lease.endpoint->outstanding.fetch_sub(1, std::memory_order_acq_rel);

        if (!slot.desired && slot.reserved) {
            --reservations_;
            slot.reserved = false;
        }

        if (slot.release)
            slot.assigned = false;
    }

    if (auto control = lease.control.lock()) {
        {
            std::lock_guard lock(control->mutex);
            ++control->wakeSequence;
        }
        control->condition.notify_one();
    }
}

bool CallbackSlotPool::outstanding(std::size_t index, std::uint64_t generation) const noexcept {
    std::lock_guard lock(mutex_);
    const auto &slot = slots_[index];

    return slot.generation == generation && (slot.preparing || slot.packet || slot.retiring);
}

CallbackRetirementSnapshot CallbackSlotPool::snapshot(std::size_t index) const noexcept {
    std::lock_guard lock(mutex_);
    const auto &slot = slots_[index];

    return {!slot.packet && !slot.retiring && !slot.preparing, slot.retirementRevision};
}
} // namespace detail
} // namespace org::ttldtor::qdcxx::api
