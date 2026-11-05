// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/mux/queue.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace org::ttldtor::qdcxx::api {

/** Qualification of the last worker publication, without a future source-freshness guarantee. */
enum class HistoryObservationState {
    UNSUPPORTED,        ///< This endpoint selects another delivery contract.
    NOT_MAPPED,         ///< No complete current logical-to-source mapping has been observed.
    CURRENT,            ///< The AS-OF cut matches the current accepted facade fences.
    SOURCE_INACTIVE,    ///< No eligible connected source observation was available.
    RESET_PENDING,      ///< A producer clear or actual source-reset barrier remains pending.
    CALLBACK_PENDING,   ///< Selected user code or its final capture retirement has not completed.
    PUBLICATION_FAILED, ///< Finite observation work or owning snapshot preparation failed.
    RETIRED             ///< The logical owner is closing, failed or fully retired.
};

/** Owning physical-key/status observation; logical selectors may expand or overlap these source keys. */
using HistoryRangeObservation = mux::QueueRangeObservation;

/** Detached bounded AS-OF HISTORY cut; copies retain no collector, callback, socket or runtime owner.
 * A CURRENT result qualifies the last publication against current accepted facade fences. It does not
 * certify unread network input or future freshness. completeDeliveredRange is false when that qualification
 * fails. Old copied observations remain historical values and do not update themselves.
 */
struct HistorySubscriptionSnapshot {
    std::shared_ptr<const void> ownerIdentity; ///< Non-reused pure marker shared with this subscription's statistics.
    std::uint64_t publicationSerial = 0; ///< Checked worker source-publication sequence; zero precedes publication.
    std::chrono::steady_clock::time_point observedAt{}; ///< Actual worker observation time, not getter time.
    HistoryObservationState state = HistoryObservationState::UNSUPPORTED; ///< Exact current qualification.
    std::uint64_t logicalRevision = 0;                   ///< Accepted facade desired revision represented by this cut.
    std::optional<std::uint64_t> mappingLogicalRevision; ///< Actual complete mapping's applied facade revision.
    std::uint64_t connectionRevision = 0; ///< Accepted endpoint source-control fence represented by this cut.
    std::uint64_t clearRevision = 0;      ///< Latest accepted explicit endpoint clear at observation.
    std::uint64_t clearedRevision = 0;    ///< Actual applied complete reset/ACK barrier; never a producer ACK guess.
    std::uint64_t acceptedLogicalRevision = 0;    ///< Latest detached facade revision at getter qualification.
    std::uint64_t acceptedConnectionRevision = 0; ///< Latest detached source-control fence at getter qualification.
    std::uint64_t acceptedClearRevision =
        0; ///< Latest admitted clear request at getter qualification, not applied ACK.
    std::optional<std::uint64_t> sourceEpoch; ///< Actual recovery source ordinal, absent without an eligible source.
    std::uint64_t callbackRetirementRevision = 0; ///< Exact completed data-callback cut, not future quiescence.
    bool sourceResetPending = false; ///< Actual recovery reset state at observation, independent of explicit clear.
    std::vector<HistoryRangeObservation> ranges; ///< Owning finite physical routes for the represented logical set.
    bool final = false; ///< Registration and all selected/callback captures have actually retired.
};

} // namespace org::ttldtor::qdcxx::api
