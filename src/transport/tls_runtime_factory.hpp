// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "bootstrap_lease.hpp"
#include "qd/transport/tls_runtime.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

/** Private TLS assembly path; plain recovery does not include or link this adapter. */
struct TlsRuntimeFactory {
    /** Read native ciphertext octets and verified source identities without progress. */
    [[nodiscard]] static ConnectionStatistics statistics(const TlsRuntime &runtime) noexcept;
    /** Quiesce this owner's native operations and return the final detached cut. */
    [[nodiscard]] static ConnectionStatistics retireStatistics(TlsRuntime &runtime) noexcept;

    /** Create an independent TLS attempt while preserving existing TLS admission/failure ordering.
     * @param lease Stable process borrow retained by the outer owner through all attempts.
     * @param endpoint Immutable validated endpoint snapshot.
     * @param composer Owned composer, adopted only after preparation succeeds.
     * @param options Explicit TLS/native resource declarations.
     * @param tlsWork Caller-owned exact TLS preparation/disposal credits.
     * @param sessionWork Caller-owned pure-session/native preparation credits.
     * @return Prepared TLS owner or detached refusal, without starting I/O or calling providers.
     */
    [[nodiscard]] static std::expected<TlsRuntime, TlsDiagnostic> create(const BootstrapLease &lease,
                                                                         const client::EndpointOptions &endpoint,
                                                                         protocol::SubscriptionWriter &&composer,
                                                                         TlsOptions options, TlsWork &tlsWork,
                                                                         protocol::LiveWork &sessionWork);

    private:
    friend class transport::TlsRuntime;
    /** Shared implementation; process validation remains at its original late RuntimeCore checkpoint.
     * @param bootstrap Nullable stable process state.
     * @param endpoint Immutable endpoint snapshot.
     * @param composer Owned composer.
     * @param options Explicit resource declarations.
     * @param tlsWork Caller-owned exact TLS preparation/disposal credits.
     * @param sessionWork Caller-owned session/native preparation credits.
     * @return Prepared owner or detached refusal in the original admission order.
     */
    [[nodiscard]] static std::expected<TlsRuntime, TlsDiagnostic> create(RuntimeBootstrap::State *bootstrap,
                                                                         const client::EndpointOptions &endpoint,
                                                                         protocol::SubscriptionWriter &&composer,
                                                                         TlsOptions options, TlsWork &tlsWork,
                                                                         protocol::LiveWork &sessionWork);
};

} // namespace org::ttldtor::qdcxx::transport::detail
