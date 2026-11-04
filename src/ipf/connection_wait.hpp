// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "connection_control.hpp"
#include <stop_token>
#include <thread>

namespace org::ttldtor::qdcxx::ipf::connection_detail {
/** Notify only after synchronizing the false-predicate/wait handoff; never invokes SDK/user code. */
struct CompletionWaitNotify {
    Control &gate;

    /** Lock the same gate as the waiter; caller never registers or destroys this callback under that gate. */
    void operator()() const noexcept {
        std::lock_guard lock(gate.mutex);
        gate.condition.notify_all();
    }
};

/** Release one already admitted slot only after callback deregistration; no mutex/allocation on release. */
struct CompletionWaitLease {
    Control &gate;

    /** @param value Stable gate whose waiter count was incremented before frame construction. */
    explicit CompletionWaitLease(Control &value) noexcept : gate(value) {
    }

    CompletionWaitLease(const CompletionWaitLease &) = delete;
    CompletionWaitLease &operator=(const CompletionWaitLease &) = delete;

    /** The frame's retained gate outlives this lease. */
    ~CompletionWaitLease() {
        gate.completionWaiters.fetch_sub(1);
    }
};

/** Actual fixed per-wait control backing; reverse member destruction releases lock before stop callback. */
struct CompletionWaitFrame {
    std::shared_ptr<Control> gate;
    CompletionWaitLease lease;
    std::stop_callback<CompletionWaitNotify> cancellation;
    std::unique_lock<std::mutex> lock;
    std::size_t turns;

    /** Adopt one reserved slot outside the gate, register cancellation, then reacquire the wait gate.
     * @param value Retained control identity, independent of runtime/facade lifetime.
     * @param stop Caller cancellation, without closing the connection.
     * @param maximum Maximum pending predicate/wait turns for this call.
     */
    CompletionWaitFrame(std::shared_ptr<Control> value, std::stop_token stop, std::size_t maximum)
        : gate(std::move(value)), lease(*gate), cancellation(stop, CompletionWaitNotify{*gate}), lock(gate->mutex),
          turns(maximum) {
    }
};

/** Select one result at a mutex-protected observation point, with sticky completion first.
 * @param port Stable connection, without any facade/native borrow.
 * @param deadline Absolute monotonic deadline.
 * @param stop Caller cancellation.
 * @return Terminal/immediate result, or no result while waiting may still make progress.
 */
inline std::optional<IpfCompletionWaitStatus> completionWaitState(const Port &port, Clock::time_point deadline,
                                                                  std::stop_token stop) {
    std::lock_guard lock(port.mutex);

    if (port.observed.initialSnapshotComplete) {
        return IpfCompletionWaitStatus::COMPLETED;
    }

    if (port.closed.load()) {
        return IpfCompletionWaitStatus::CLOSED;
    }

    if (stop.stop_requested()) {
        return IpfCompletionWaitStatus::CANCELLED;
    }

    if (Clock::now() >= deadline) {
        return IpfCompletionWaitStatus::TIMED_OUT;
    }

    return {};
}

/** Wait only on retained control state, never on a worker/native/runtime facade.
 * @param port Caller-retained stable operation lease; limits are immutable after publication.
 * @param deadline Absolute monotonic deadline, including the finite clock endpoints.
 * @param stop Caller cancellation; callback registration/destruction occurs outside the wait gate.
 * @return First-snapshot/close/cancel/deadline outcome, or detached self/capacity/work refusal.
 */
inline std::expected<IpfCompletionWaitStatus, IpfConnectionDiagnostic>
awaitCompleted(const std::shared_ptr<Port> &port, Clock::time_point deadline, std::stop_token stop) {
    if (auto state = completionWaitState(*port, deadline, stop)) {
        return *state;
    }

    auto gate = port->control.lock();

    if (!gate) {
        if (auto state = completionWaitState(*port, deadline, stop)) {
            return *state;
        }

        return std::unexpected(
            IpfConnectionDiagnostic{{IpfTransferError::INVALID_STATE, "connection-wait-detached"}, {}});
    }

    {
        std::lock_guard lock(gate->mutex);

        if (auto state = completionWaitState(*port, deadline, stop)) {
            return *state;
        }

        if (gate->worker == std::this_thread::get_id()) {
            return std::unexpected(
                IpfConnectionDiagnostic{{IpfTransferError::INVALID_STATE, "connection-wait-self"}, {}});
        }

        if (port->limits.completionWaitTurns == 0) {
            return std::unexpected(IpfConnectionDiagnostic{{IpfTransferError::WORK_LIMIT, "connection-wait-work"}, {}});
        }

        if (gate->completionWaiters.load() >= port->limits.completionWaiters) {
            return std::unexpected(
                IpfConnectionDiagnostic{{IpfTransferError::RESOURCE_LIMIT, "connection-wait-capacity"}, {}});
        }

        gate->completionWaiters.fetch_add(1);
    }

    CompletionWaitFrame frame(std::move(gate), stop, port->limits.completionWaitTurns);

    for (;;) {
        if (auto state = completionWaitState(*port, deadline, stop)) {
            return *state;
        }

        if (frame.turns == 0) {
            return std::unexpected(IpfConnectionDiagnostic{{IpfTransferError::WORK_LIMIT, "connection-wait-work"}, {}});
        }

        --frame.turns;
        frame.gate->condition.wait_until(frame.lock, deadline);
    }
}
} // namespace org::ttldtor::qdcxx::ipf::connection_detail
