// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/mux/queue.hpp"

namespace org::ttldtor::qdcxx::mux::detail {
/** Number of fair visits to each configured client slot before source admission can reopen. */
inline constexpr std::size_t QUEUE_PHASES = 5;

/** Scheduling observations that do not inspect producer command or completion slots. */
struct QueueScheduleClient {
    QueueClientState state;
    std::size_t recordOwners;
    std::size_t recordBytes;
    std::size_t queuedRecords;
};

/** Narrow observations for the serialized execution owner; never call concurrently with queue mutation.
 * Detached lease release may run concurrently; its record reservations remain atomic.
 */
struct QueueScheduleAccess {
    /** @return Unrouted source records, or zero for an empty owner. */
    [[nodiscard]] static std::size_t pendingRecords(const MuxQueue &queue) noexcept;
    /** @return Retained record owners, or zero for an empty queue owner. */
    [[nodiscard]] static std::size_t dataItems(const MuxQueue &queue) noexcept;
    /** @return Scheduling occupancy, or INVALID_STATE/INVALID_CLIENT for an empty owner/foreign handle. */
    [[nodiscard]] static std::expected<QueueScheduleClient, QueueFailure> client(const MuxQueue &queue,
                                                                                 const MuxHandle &handle) noexcept;
};
} // namespace org::ttldtor::qdcxx::mux::detail
