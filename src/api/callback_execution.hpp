// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "../transport/callback_retirement.hpp"
#include "qd/api/feed.hpp"
#include "qd/core/delivery_projection.hpp"
#include <limits>
#include <mutex>

namespace org::ttldtor::qdcxx::api::detail {
struct SubscriptionChangeDomain;
struct RuntimeControl;
struct SubscriptionPort;

/** Pure packet lifetime outside host task identity; all application references retire outside locks. */
class CallbackInvocation {
    public:
    virtual ~CallbackInvocation() = default;
    virtual void prepare() = 0;
    virtual CallbackPostResult post(CallbackTask task) = 0;
    virtual void execute() noexcept = 0;
    virtual void finish(Diagnostic diagnostic) noexcept = 0;
};

/** Passive coherent completion cut, consumed by qualified history publication. */
struct CallbackRetirementSnapshot {
    bool quiescent = true;
    std::uint64_t retirementRevision = 0;
};

/** Passive completion fence, without native progress or waiting. */
[[nodiscard]] CallbackRetirementSnapshot callbackRetirementSnapshot(const SubscriptionPort &port) noexcept;
/** Same coherent cut while caller already holds the subscription mutex (endpoint-before-sub order). */
[[nodiscard]] CallbackRetirementSnapshot callbackRetirementSnapshotLocked(const SubscriptionPort &port) noexcept;

/** All cross-thread packet leases, detached from any native progress owner. */
struct CallbackPacketLease {
    std::shared_ptr<SubscriptionChangeDomain> domain;
    transport::detail::CallbackRetirementFence *endpoint =
        nullptr; ///< Registry/native owner outlives all counted packets.
    std::weak_ptr<RuntimeControl> control;
};

/** Stable finite identities; stale host stubs retain this one fixed allocation, not packets. */
class CallbackSlotPool final : public std::enable_shared_from_this<CallbackSlotPool> {
    struct Slot {
        std::unique_ptr<CallbackInvocation> packet;
        CallbackPacketLease lease;
        std::uint64_t generation = 0;
        std::uint64_t retirementRevision = 0;
        Diagnostic diagnostic;
        bool assigned = false;
        bool desired = false;
        bool reserved = false;
        bool preparing = false;
        bool started = false;
        bool posting = false;
        bool running = false;
        bool claimed = false;
        bool finished = false;
        bool retiring = false;
        bool release = false;
    };

    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::size_t maximumReservations_;
    std::size_t reservations_ = 0;
    void retire(std::size_t index, std::uint64_t generation) noexcept;
    CallbackTaskRunResult execute(std::size_t index, std::uint64_t generation, bool deferred) noexcept;

    public:
    static constexpr std::size_t NO_SLOT = std::numeric_limits<std::size_t>::max();
    // Four physical rows, at most five outputs each. Even charging every output the independent
    // maximum symbol (128), two selected text fields (8192 each), terminators and opaque body
    // (16384) remains below the prepaid one-MiB payload allowance. No source batch is retained.
    static constexpr std::size_t PHYSICAL_RECORDS = 4;
    static constexpr std::size_t MAXIMUM_EVENTS = PHYSICAL_RECORDS * core::MAX_DELIVERY_PROJECTIONS;
    static_assert(MAXIMUM_EVENTS * (2 * (128 + 2 * 8192 + 3) + 16384) <= 1024 * 1024);
    static constexpr std::size_t PACKET_BYTES = 1024 * 1024 + 4096 + MAXIMUM_EVENTS * sizeof(RegisteredEvent);
    [[nodiscard]] static std::size_t storageBytes(std::size_t slots);
    CallbackSlotPool(std::size_t slots, std::size_t reservations);
    /** Caller serializes a whole multi-subscription policy commit before using these reservations. */
    [[nodiscard]] bool canReserve(std::size_t additional) const noexcept;
    [[nodiscard]] std::size_t assign();
    void reserve(std::size_t index, bool desired);
    void release(std::size_t index) noexcept;
    [[nodiscard]] bool reserved(std::size_t index) const noexcept;
    void begin(std::size_t index, CallbackPacketLease lease);
    void abortPreparation(std::size_t index) noexcept;
    void submit(std::size_t index, std::unique_ptr<CallbackInvocation> packet);
    void cancel(std::size_t index, Diagnostic reason) noexcept;
    void abandon(std::size_t index, std::uint64_t generation) noexcept;
    [[nodiscard]] CallbackTaskRunResult run(std::size_t index, std::uint64_t generation) noexcept;
    [[nodiscard]] CallbackTaskRunResult resume(std::size_t index, std::uint64_t generation) noexcept;
    [[nodiscard]] bool outstanding(std::size_t index, std::uint64_t generation) const noexcept;
    [[nodiscard]] CallbackRetirementSnapshot snapshot(std::size_t index) const noexcept;
};
} // namespace org::ttldtor::qdcxx::api::detail
