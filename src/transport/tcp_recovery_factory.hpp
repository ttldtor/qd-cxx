// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "bootstrap_lease.hpp"
#include "callback_retirement.hpp"
#include "qd/transport/aggregation.hpp"
#include "qd/transport/tcp_recovery.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

/** Private recovery assembly from stable process state, independent of its movable public facade. */
struct TcpRecoveryFactory {
    /** Bind actual high-level packet retirement before start; default low-level callers have no fence. */
    static void setCallbackRetirementFence(TcpRecovery &recovery, CallbackRetirementFence *fence) noexcept;

    /** Quiesce only the owned native attempt before final detached facade statistics publication.
     * @param recovery Existing worker-owned recovery; no application callback is invoked.
     * @return Preserved actual lifetime totals after native callback retirement.
     */
    [[nodiscard]] static ConnectionStatistics retireStatistics(TcpRecovery &recovery) noexcept;
    /** @param recovery Worker-owned actual recovery. @param period Validated optional intent.
     * @return Coalesced acceptance or owner/range refusal; no inline native progress.
     */
    [[nodiscard]] static std::expected<void, RecoveryDiagnostic>
    requestAggregationPeriod(TcpRecovery &recovery, std::optional<std::chrono::milliseconds> period) noexcept;
    /** @param recovery Existing worker-owned recovery. @return Only actual current-source peer info. */
    [[nodiscard]] static std::optional<AggregationPeriodInfo>
    aggregationPeriodInfo(const TcpRecovery &recovery) noexcept;
    /** Prepare from a counted outer lease; SDK progress and worker affinity remain caller driven.
     * @param bootstrap Outer lease retained across all native and callback retirement.
     * @param endpoint Original immutable route configuration.
     * @param blueprint Outgoing records adopted only after fallible preparation.
     * @param queues Compatible queue owners, preserved on failure.
     * @param options Finite recovery settings.
     * @param work Caller-owned preparation credits, without refunds.
     * @return Prepared owner or existing typed refusal; public preparation/error order is preserved.
     */
    [[nodiscard]] static std::expected<TcpRecovery, RecoveryDiagnostic>
    create(const BootstrapLease &bootstrap, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           RecoveryPreparationWork &work);

    private:
    friend class transport::TcpRecovery;
    /** Shared public/private assembly path, preserving configuration preparation before lease admission.
     * @param bootstrap Nullable stable process state.
     * @param endpoint Original immutable route configuration.
     * @param blueprint Outgoing records adopted transactionally.
     * @param queues Queue owners preserved on failure.
     * @param options Finite settings.
     * @param work Actual caller preparation credits.
     * @return Prepared owner or existing static refusal.
     */
    [[nodiscard]] static std::expected<TcpRecovery, RecoveryDiagnostic>
    create(RuntimeBootstrap::State *bootstrap, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           RecoveryPreparationWork &work);
};

} // namespace org::ttldtor::qdcxx::transport::detail
