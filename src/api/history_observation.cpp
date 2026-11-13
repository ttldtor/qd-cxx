// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "history_observation.hpp"
#include "../core/delivery_statistics.hpp"
#include "../transport/recovery_core.hpp"
#include "../transport/tcp_recovery_factory.hpp"
#include "callback_execution.hpp"

#include <limits>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
/** Suppress current completion while retaining explicitly historical source diagnostics.
 * @param snapshot Independently owned publication or getter copy.
 * @param state Exact failed current qualification.
 */
void invalidate(HistorySubscriptionSnapshot &snapshot, HistoryObservationState state) noexcept {
    snapshot.state = state;

    for (auto &range : snapshot.ranges)
        range.status.completeDeliveredRange = false;
}

/** Requalify while both detached control mutexes are held in their existing global order.
 * @param endpoint Current detached endpoint policy, locked by the caller.
 * @param port Current detached subscription policy, locked after endpoint by the caller.
 * @param snapshot Owning copy, never the published source of another reader.
 */
void qualify(const EndpointPort &endpoint, const SubscriptionPort &port,
             HistorySubscriptionSnapshot &snapshot) noexcept {
    snapshot.final = port.retired.load(std::memory_order_acquire);
    snapshot.acceptedLogicalRevision = port.revision;
    snapshot.acceptedConnectionRevision = endpoint.connectionRevision;
    snapshot.acceptedClearRevision = endpoint.clearRevision;

    if (endpoint.options.contract != DeliveryContract::HISTORY) {
        invalidate(snapshot, HistoryObservationState::UNSUPPORTED);
    } else if (port.closeRequested.load(std::memory_order_acquire) || snapshot.final ||
               port.state == SubscriptionState::FAILED || endpoint.closeRequested.load(std::memory_order_acquire) ||
               endpoint.retired.load(std::memory_order_acquire)) {
        invalidate(snapshot, HistoryObservationState::RETIRED);
    } else if (endpoint.clearRevision > endpoint.clearedRevision || snapshot.clearRevision != endpoint.clearRevision) {
        invalidate(snapshot, HistoryObservationState::RESET_PENDING);
    } else if (!endpoint.connectionIntent || endpoint.activeDeliveryRevision != endpoint.connectionRevision ||
               snapshot.connectionRevision != endpoint.connectionRevision) {
        invalidate(snapshot, HistoryObservationState::SOURCE_INACTIVE);
    } else if (snapshot.logicalRevision != port.revision || !snapshot.mappingLogicalRevision ||
               *snapshot.mappingLogicalRevision != port.revision) {
        invalidate(snapshot, HistoryObservationState::NOT_MAPPED);
    } else {
        const auto callback = historyCallbackRetirementLocked(port);

        if (!callback.quiescent || callback.revision != snapshot.callbackRetirementRevision)
            invalidate(snapshot, HistoryObservationState::CALLBACK_PENDING);
    }
}
} // namespace

HistoryCallbackRetirement historyCallbackRetirement(const SubscriptionPort &port) noexcept {
    const auto callback = callbackRetirementSnapshot(port);

    return {callback.quiescent, callback.retirementRevision};
}

HistoryCallbackRetirement historyCallbackRetirementLocked(const SubscriptionPort &port) noexcept {
    const auto callback = callbackRetirementSnapshotLocked(port);

    return {callback.quiescent, callback.retirementRevision};
}

HistorySubscriptionSnapshot copyHistorySnapshot(const std::shared_ptr<SubscriptionPort> &port) {
    const auto endpoint = port->endpoint.lock();

    if (!endpoint) {
        std::lock_guard lock(port->mutex);
        auto snapshot = port->history;
        snapshot.final = port->retired.load(std::memory_order_acquire);
        invalidate(snapshot, HistoryObservationState::RETIRED);

        return snapshot;
    }

    std::lock_guard endpointLock(endpoint->mutex);
    std::lock_guard lock(port->mutex);
    auto snapshot = port->history;
    qualify(*endpoint, *port, snapshot);

    return snapshot;
}

namespace {
using ObservedRanges = std::expected<std::vector<mux::QueueRangeObservation>, HistoryObservationState>;

template <class ReadRanges>
void publishHistoryCut(std::optional<std::uint64_t> sourceEpoch, bool sourceResetPending,
                       const std::shared_ptr<SubscriptionPort> &port, ReadRanges readRanges) noexcept {
    const auto endpoint = port->endpoint.lock();

    if (!endpoint)
        return;

    try {
        HistorySubscriptionSnapshot candidate;
        {
            std::lock_guard endpointLock(endpoint->mutex);
            std::lock_guard lock(port->mutex);

            if (endpoint->options.contract != DeliveryContract::HISTORY)
                return;

            candidate.ownerIdentity = port->history.ownerIdentity;

            if (port->history.publicationSerial == std::numeric_limits<std::uint64_t>::max()) {
                invalidate(port->history, HistoryObservationState::PUBLICATION_FAILED);

                return;
            }

            candidate.publicationSerial = port->history.publicationSerial + 1;
            candidate.logicalRevision = port->revision;
            candidate.connectionRevision = endpoint->connectionRevision;
            candidate.clearRevision = endpoint->clearRevision;
            candidate.clearedRevision = endpoint->clearedRevision;
        }
        const auto statistics = port->statistics->snapshot();
        candidate.mappingLogicalRevision = statistics.activeDesiredRevision;
        candidate.sourceEpoch = sourceEpoch;
        candidate.sourceResetPending = sourceResetPending;
        const auto callback = historyCallbackRetirement(*port);
        candidate.callbackRetirementRevision = callback.revision;
        candidate.observedAt = std::chrono::steady_clock::now();
        candidate.state = HistoryObservationState::CURRENT;

        if (!callback.quiescent) {
            candidate.state = HistoryObservationState::CALLBACK_PENDING;
        } else if (candidate.sourceResetPending) {
            candidate.state = HistoryObservationState::RESET_PENDING;
        } else {
            auto ranges = readRanges();

            if (ranges) {
                candidate.ranges = std::move(*ranges);

                if (candidate.ranges.empty())
                    candidate.state = HistoryObservationState::NOT_MAPPED;

                for (const auto &range : candidate.ranges) {
                    if (range.status.resetPending)
                        candidate.state = HistoryObservationState::RESET_PENDING;
                    else if (range.status.acceptedUnstagedData)
                        candidate.state = HistoryObservationState::SOURCE_INACTIVE;
                    else if (range.status.paused && candidate.state == HistoryObservationState::CURRENT)
                        candidate.state = HistoryObservationState::CALLBACK_PENDING;
                    else if (!range.status.mappingCurrent && candidate.state == HistoryObservationState::CURRENT)
                        candidate.state = HistoryObservationState::NOT_MAPPED;
                }
            } else {
                candidate.state = ranges.error();
            }
        }

        {
            std::lock_guard endpointLock(endpoint->mutex);
            std::lock_guard lock(port->mutex);
            qualify(*endpoint, *port, candidate);

            if (candidate.state != HistoryObservationState::CURRENT)
                invalidate(candidate, candidate.state);

            std::swap(port->history, candidate);
        }
        // Only independently owning pure metadata retires here, outside the detached control mutexes.
    } catch (...) {
        std::lock_guard lock(port->mutex);
        invalidate(port->history, HistoryObservationState::PUBLICATION_FAILED);
    }
}
} // namespace

namespace {
template <class Recovery>
void publishNetworkHistory(const Recovery &recovery, const mux::MuxHandle &handle,
                           const std::shared_ptr<SubscriptionPort> &port, mux::QueueWork &work) noexcept {
    publishHistoryCut(recovery.statistics().sourceEpoch, recovery.status().sourceResetPending, port,
                      [&]() -> ObservedRanges {
                          auto ranges = recovery.historyRangeSnapshot(handle, work);

                          if (!ranges) {
                              return std::unexpected(ranges.error().error == transport::RecoveryError::INVALID_STATE
                                                         ? HistoryObservationState::SOURCE_INACTIVE
                                                         : HistoryObservationState::PUBLICATION_FAILED);
                          }

                          return std::move(*ranges);
                      });
}
} // namespace

void publishHistorySnapshot(const transport::detail::RecoveryCore &recovery, const mux::MuxHandle &handle,
                            const std::shared_ptr<SubscriptionPort> &port, mux::QueueWork &work) noexcept {
    publishNetworkHistory(recovery, handle, port, work);
}

void publishHistorySnapshot(const transport::TcpRecovery &recovery, const mux::MuxHandle &handle,
                            const std::shared_ptr<SubscriptionPort> &port, mux::QueueWork &work) noexcept {
    publishNetworkHistory(recovery, handle, port, work);
}

void publishTapeHistorySnapshot(const mux::MuxQueue &queue, std::uint64_t epoch, const mux::MuxHandle &handle,
                                const std::shared_ptr<SubscriptionPort> &port, mux::QueueWork &work) noexcept {
    publishHistoryCut(epoch, false, port, [&]() -> ObservedRanges {
        auto ranges = queue.rangeSnapshot(handle, work);

        if (!ranges) {
            return std::unexpected(ranges.error().error == mux::QueueError::INVALID_STATE
                                       ? HistoryObservationState::SOURCE_INACTIVE
                                       : HistoryObservationState::PUBLICATION_FAILED);
        }

        return std::move(*ranges);
    });
}

} // namespace org::ttldtor::qdcxx::api::detail
