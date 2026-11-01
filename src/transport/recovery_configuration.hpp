// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/recovery.hpp"
#include "recovery_state.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace org::ttldtor::qdcxx::transport::detail {

/** One fixed requested-capacity replay bank; entry text points only into its own stable slab. */
struct RecoveryReplayBank {
    std::unique_ptr<core::SubscriptionRequest[]> entries; ///< Exactly the admitted entry count; no vector growth.
    std::unique_ptr<char16_t[]> symbols; ///< Exactly the admitted UTF16 count; no per-entry string allocation.
    std::size_t size = 0;                ///< Initialized current entries.
    std::size_t units = 0;               ///< Initialized current selector text.
    std::uint64_t revision = 0;          ///< Exact owner-applied desired revision represented by this bank.
};

/** Three simultaneous preallocated banks for one persistent family. */
struct RecoveryReplay {
    std::array<RecoveryReplayBank, 3> banks; ///< Latest desired, locally emitted and one candidate.
    RecoveryReplayLimits limits;             ///< Per-bank immutable requested capacities.
};

/** Prepared owner inputs; publication transfers only complete candidates and noexcept input moves. */
struct RecoveryConfiguration {
    client::EndpointOptions endpoint;                ///< One independent immutable retry1 snapshot.
    std::vector<protocol::OutgoingRecord> blueprint; ///< Actual caller backing, adopted only on complete success.
    RecoveryQueues queues;                ///< Stable family/registration owners, adopted only on complete success.
    std::array<RecoveryReplay, 3> replay; ///< Exact fixed backing allocated before transfer.
    std::optional<protocol::SubscriptionWriter> firstWriter; ///< Actual validated writer reused by attempt one.
    RecoveryOptions options;   ///< Fixed scalars/arrays only; contains no callable or borrowed input.
    RecoveryCapacity capacity; ///< Admitted immutable logical vector and explicit overlapping byte scopes.
    RecoveryConfig scheduler;  ///< Original finite retry scalars and selected liveness intervals.
    RecoveryWork disposal;     ///< New-owner disposal work prepaid at preparation, never a borrowed caller reference.
    bool adopted = false;      ///< Input ownership was transferred exactly once after every fallible allocation.

    /** Prepare all allocations before adopting either input owner.
     * @param endpoint Immutable original snapshot; remains unchanged and is not retained merely for retry policy.
     * @param blueprint Actual caller vector, unchanged even on successful candidate preparation.
     * @param queues Actual delivery owners, unchanged even on successful candidate preparation.
     * @param options Required finite recovery policy/capacities, borrowed unchanged during preparation.
     * @param security Wrapper-specific required endpoint policy.
     * @param work Independent caller preparation coordinates, never refilled or refunded.
     * @return Complete uncommitted candidate with one first writer; caller ownership remains unchanged.
     */
    [[nodiscard]] static std::expected<RecoveryConfiguration, RecoveryDiagnostic>
    prepare(const client::EndpointOptions &endpoint, const std::vector<protocol::OutgoingRecord> &blueprint,
            RecoveryQueues &queues, const RecoveryOptions &options, client::EndpointSecurity security,
            RecoveryPreparationWork &work);

    /** Commit unchanged validated inputs after the stable Core/control allocation also succeeds.
     * The caller serializes input access and preserves the exact inspected values until this call.
     * @param blueprint Same vector inspected during preparation, transferred once.
     * @param queues Same delivery owners inspected during preparation, transferred once.
     * @return True for the sole successful commit; a repeated call preserves both new input owners.
     */
    [[nodiscard]] bool adopt(std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues) noexcept;

    /** Access one fixed family slot without constructing an owner.
     * @param family TICKER0, STREAM1 or HISTORY2.
     * @return Existing optional source owner; the caller supplies an in-range fixed family position.
     */
    [[nodiscard]] std::optional<mux::MuxQueue> &queue(std::size_t family) noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
