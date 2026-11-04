// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "bootstrap_lease.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

/** Private plain assembly path shared by public facade creation and stable recovery borrows. */
struct TcpRuntimeFactory {
    /** Retire only this owned runtime and return its final native/protocol counters.
     * @param runtime Existing serialized-owner handle, kept alive until all owned completions return.
     * @return Detached final cut; no application callback is invoked.
     */
    [[nodiscard]] static ConnectionStatistics retireStatistics(TcpRuntime &runtime) noexcept;
    /** Create one independent native attempt from stable process state.
     * @param lease Live process borrow retained by the outer owner through all attempts.
     * @param schema Owned schema, adopted only after preparation succeeds.
     * @param composer Owned composer, adopted only after preparation succeeds.
     * @param sessionLimits Explicit finite pure-session allowances.
     * @param options Explicit finite native configuration.
     * @param work Caller-owned preparation credits; original debit/error order applies.
     * @return Prepared native owner or static refusal, without starting I/O.
     */
    [[nodiscard]] static std::expected<TcpRuntime, TcpDiagnostic>
    create(const BootstrapLease &lease, core::LocalSchema &&schema, protocol::SubscriptionWriter &&composer,
           protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &work);

    /** Fence a plain attempt for reuse, classifying only its subsequent owned local read cancellation.
     * @param runtime Existing owner; no state is allocated or transferred.
     * @return Existing owner-thread/moved rejection or accepted native cutoff.
     */
    [[nodiscard]] static std::expected<void, TcpDiagnostic> requestPause(TcpRuntime &runtime) noexcept;

    private:
    friend class transport::TcpRuntime;
    /** Shared implementation; nullable State preserves public factory failure ordering.
     * @param bootstrap Stable process state, possibly absent after a facade move.
     * @param schema Owned schema.
     * @param composer Owned composer.
     * @param sessionLimits Explicit pure-session allowances.
     * @param options Explicit native configuration.
     * @param work Caller-owned preparation credits.
     * @return Prepared owner or static refusal.
     */
    [[nodiscard]] static std::expected<TcpRuntime, TcpDiagnostic>
    create(RuntimeBootstrap::State *bootstrap, core::LocalSchema &&schema, protocol::SubscriptionWriter &&composer,
           protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &work);
};

} // namespace org::ttldtor::qdcxx::transport::detail
