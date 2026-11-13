// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "bootstrap_lease.hpp"
#include "qd/transport/tls_recovery.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

class RecoveryCore;

/** Private worker assembly using the same qualified backend and stable process lease. */
struct TlsRecoveryFactory {
    [[nodiscard]] static std::expected<TlsRecovery, RecoveryDiagnostic>
    create(const BootstrapLease &bootstrap, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           const TlsOptions &tls, RecoveryPreparationWork &work);

    /** Borrow the worker-owned core until this stable facade is destroyed. */
    [[nodiscard]] static RecoveryCore &core(TlsRecovery &recovery) noexcept;

    private:
    friend class transport::TlsRecovery;
    static std::expected<TlsRecovery, RecoveryDiagnostic>
    create(RuntimeBootstrap *bootstrap, const BootstrapLease *lease, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           const TlsOptions &tls, RecoveryPreparationWork &work);
};

} // namespace org::ttldtor::qdcxx::transport::detail
