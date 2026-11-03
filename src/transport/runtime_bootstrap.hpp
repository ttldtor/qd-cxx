// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tcp_runtime.hpp"

#include <atomic>
#include <thread>

namespace org::ttldtor::qdcxx::transport {

/** Stable application-owned SDK reference; runtime borrowers never perform its global cleanup. */
struct RuntimeBootstrap::State {
    const void *provenance =
        nullptr; ///< Opaque identity of the linked runtime instance; not an ABI/security attestation.
    std::thread::id owner = std::this_thread::get_id(); ///< Application bootstrap/main-thread identity.
    std::atomic_size_t borrowers{0}; ///< Live runtime/outer-owner borrows, including idle gaps and retirement.
    bool dnsInitialized = false;     ///< Whether this owner acquired one c-ares global reference.
    bool socketsInitialized = false; ///< Whether this owner acquired one native Windows socket reference.

    /** Release acquired process references only on their original thread after every borrower retires. */
    ~State();
};

namespace detail {

/** Private assembly path; the installed API always selects the real single native readiness sample. */
struct BootstrapFactory {
    using Readiness = bool (*)(void *) noexcept; ///< Per-invocation library-controlled readiness attempt.

    /** Reject unavailable native readiness before allocating an owner or acquiring any SDK reference.
     * @param readiness Single selected native attempt, or a finite private regression substitute.
     * @param argument Call-local state; never retained or published through the installed API.
     * @return Bootstrap owner or detached failure; false never performs SDK cleanup.
     */
    static std::expected<RuntimeBootstrap, TcpDiagnostic> create(Readiness readiness, void *argument);
};

} // namespace detail

} // namespace org::ttldtor::qdcxx::transport
