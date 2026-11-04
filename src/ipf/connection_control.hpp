// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/ipf/instrument_profile_connection.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>

namespace org::ttldtor::qdcxx::ipf::connection_detail {
using Clock = std::chrono::steady_clock;

/** Saturating advisory failure increment.
 * @param value Stable mutex-protected counter.
 */
inline void increment(std::uint64_t &value) noexcept {
    if (value != std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

/** Independent observer-node allowance; no State back-reference. */
struct ObserverLedger {
    std::atomic_size_t used{0};
    const std::size_t maximum;

    /** @param limit Independent simultaneous node allowance. */
    explicit ObserverLedger(std::size_t limit) : maximum(limit) {
    }
};

/** Stack-first observer reservation, released after its capture backing. */
struct ObserverLease {
    std::shared_ptr<ObserverLedger> ledger;

    /** @param value Stable ledger reserved before any node allocation. */
    explicit ObserverLease(std::shared_ptr<ObserverLedger> value) : ledger(std::move(value)) {
        auto used = ledger->used.load();

        do {
            if (used >= ledger->maximum) {
                throw ProfileCollectorError::LIMIT_EXCEEDED;
            }
        } while (!ledger->used.compare_exchange_strong(used, used + 1));
    }

    /** @param other Existing reservation, emptied without allocation. */
    ObserverLease(ObserverLease &&other) noexcept : ledger(std::move(other.ledger)) {
    }

    ObserverLease(const ObserverLease &) = delete;

    /** Release after callback backing, without a Port back-reference. */
    ~ObserverLease() {
        if (ledger) {
            ledger->used.fetch_sub(1);
        }
    }
};

/** Owning callback; lease is declared before callback for last-release accounting. */
struct Observer {
    ObserverLease lease;
    InstrumentProfileConnection::StateChangeListener callback;
    std::uint64_t token = 0;
    std::size_t after = 0;
    bool registered = true;

    /** @param value Admitted node reservation.
     * @param function Owning callback moved without allocation.
     */
    Observer(ObserverLease value, InstrumentProfileConnection::StateChangeListener function)
        : lease(std::move(value)), callback(std::move(function)) {
    }
};

/** Detached control gate; no facade/native borrow escapes it. */
struct Control {
    std::mutex mutex;
    std::condition_variable condition;
    Runtime *runtime = nullptr;
    bool stopping = false;
};

/** One immutable ordered state transition; four reserved slots cover the one-shot phase graph. */
struct Transition {
    IpfConnectionState before;
    IpfConnectionState after;
};

/** Stable producer port, independently retained across callbacks and native drain. */
struct Port {
    std::weak_ptr<Control> control;
    mutable std::mutex mutex;
    IpfConnectionObservation observed;
    std::atomic<bool> closed{false};
    bool started = false;
    Clock::time_point startedAt{};
    std::stop_source stop;
    std::string source;
    bool local = false;
    std::uint64_t domain = 0;
    std::size_t pendingBackingLimit = 0;
    IpfUrlRequest request;
    std::shared_ptr<InstrumentProfileCollector> collector;
    IpfConnectionLimits limits;
    std::chrono::milliseconds period{60000};
    std::shared_ptr<ObserverLedger> observerLedger;
    std::vector<std::shared_ptr<Observer>> observers;
    std::vector<std::shared_ptr<Observer>> selected;
    std::array<Transition, 4> transitions{};
    std::size_t produced = 0;
    std::size_t consumed = 0;
    std::uint64_t token = 0;

    /** @param gate Detached runtime gate, retained weakly.
     * @param bounds Already admitted finite node/vector limits.
     */
    Port(std::shared_ptr<Control> gate, IpfConnectionLimits bounds)
        : control(gate), limits(bounds), observerLedger(std::make_shared<ObserverLedger>(bounds.listenerSlots)) {
        observers.reserve(bounds.listenerSlots);
        selected.reserve(bounds.listenerSlots);
    }

    /** Commit one of the finite one-shot state edges under mutex.
     * @param value Next phase.
     */
    void transition(IpfConnectionState value) noexcept {
        if (observed.state == value) {
            return;
        }

        transitions[produced++] = {observed.state, value};
        observed.state = value;
    }

    /** Allocation-free request cutoff; no SDK/join/user invocation. */
    void close() noexcept {
        {
            std::lock_guard lock(mutex);

            if (closed.load()) {
                return;
            }

            closed.store(true);
            transition(IpfConnectionState::CLOSED);
        }
        stop.request_stop();

        if (auto gate = control.lock()) {
            // Serialize publication with the waiter's false-predicate/unlock handoff.
            std::lock_guard wake(gate->mutex);
            gate->condition.notify_all();
        }
    }

    /** Deliver the finite journal, selecting each callback outside all owner/native locks. */
    void dispatch() noexcept {
        for (;;) {
            selected.clear();
            Transition event{};
            std::size_t sequence = 0;
            {
                std::lock_guard lock(mutex);

                if (consumed == produced) {
                    return;
                }

                sequence = consumed + 1;
                event = transitions[consumed++];

                for (const auto &observer : observers) {
                    if (observer->registered && observer->after < sequence) {
                        selected.push_back(observer);
                    }
                }
            }

            for (const auto &observer : selected) {
                bool invoke = false;
                {
                    std::lock_guard lock(mutex);
                    invoke = observer->registered;
                }

                if (invoke) {
                    try {
                        observer->callback(event.before, event.after);
                    } catch (...) {
                        std::lock_guard lock(mutex);
                        increment(observed.callbackFailures);
                    }
                }
            }
        }
    }

    /** Retain first attempt/collector failure without secondary-close overwrite.
     * @param error Independent earliest diagnostic.
     */
    void fail(IpfConnectionDiagnostic error) {
        std::lock_guard lock(mutex);

        if (!observed.diagnostic) {
            observed.diagnostic = error;
        } else if (error.collector) {
            observed.diagnostic->collector = error.collector;
        }
    }

    /** Reserve before owning a callback node and publish without inline invocation.
     * @param function Nonempty caller-owned callback; moved only after slot reservation.
     * @return Non-reused token or detached refusal; allocation failure throws before publication.
     */
    std::expected<std::uint64_t, IpfConnectionDiagnostic>
    listen(InstrumentProfileConnection::StateChangeListener function) {
        if (!function) {
            return std::unexpected(
                IpfConnectionDiagnostic{{IpfTransferError::INVALID_OPTIONS, "connection-listener"}, {}});
        }

        try {
            auto listener = std::make_shared<Observer>(ObserverLease(observerLedger), std::move(function));
            std::lock_guard lock(mutex);

            if (closed.load() || token == std::numeric_limits<std::uint64_t>::max()) {
                return std::unexpected(
                    IpfConnectionDiagnostic{{IpfTransferError::INVALID_STATE, "connection-listener-state"}, {}});
            }

            listener->token = ++token;
            listener->after = produced;
            observers.push_back(listener);

            return listener->token;
        } catch (ProfileCollectorError) {
            return std::unexpected(
                IpfConnectionDiagnostic{{IpfTransferError::RESOURCE_LIMIT, "connection-listener-slots"}, {}});
        }
    }

    /** Stop future selection; retire the last unselected capture outside the lock.
     * @param identity Existing registration token.
     * @return True after removing a current registration.
     */
    bool forget(std::uint64_t identity) {
        std::shared_ptr<Observer> removed;
        {
            std::lock_guard lock(mutex);
            const auto position = std::find_if(observers.begin(), observers.end(), [&](const auto &entry) {
                return entry->token == identity;
            });

            if (position == observers.end()) {
                return false;
            }

            removed = *position;
            removed->registered = false;
            observers.erase(position);
        }

        return true;
    }

    /** Release callbacks/provider/collector only after work retirement, outside all locks. */
    void retire() {
        dispatch();
        selected.clear();
        std::vector<std::shared_ptr<Observer>> retired;
        {
            std::lock_guard lock(mutex);
            observers.swap(retired);
        }
        retired.clear();
        request.provider.reset();
        request.authorization.reset();
        collector.reset();
        std::lock_guard lock(mutex);
        observed.activeAttempt = false;
        observed.retired = true;
    }
};

} // namespace org::ttldtor::qdcxx::ipf::connection_detail
