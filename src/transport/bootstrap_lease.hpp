// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "runtime_bootstrap.hpp"

#include <expected>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {

/** Counted stable process borrow; never owns SDK cleanup or invokes callbacks on release.
 * Keep before every dependent native/protocol/configuration owner so it is destroyed last.
 * The application must still retain its bootstrap owner on the original cleanup thread.
 */
class BootstrapLease {
    RuntimeBootstrap::State *state_ = nullptr; ///< Stable application-owned state; null after transfer.
    friend struct RuntimeCore;
    friend struct TcpRuntimeFactory;
    friend struct TcpRecoveryFactory;
    friend struct TlsRuntimeFactory;

    /** Adopt one already counted borrow without allocation.
     * @param state Initialized stable process state with one reserved borrower.
     */
    explicit BootstrapLease(RuntimeBootstrap::State *state) noexcept : state_(state) {
    }

    /** Attempt one bounded atomic reservation, without tying runtime affinity to cleanup affinity.
     * @param state Nullable stable process state.
     * @return Independent token or static configuration/capacity refusal; no allocation or SDK calls.
     */
    [[nodiscard]] static std::expected<BootstrapLease, TcpDiagnostic> acquire(RuntimeBootstrap::State *state) noexcept;

    public:
    /** Transfer exactly one borrow and clear the source.
     * @param other Token whose source becomes empty.
     */
    BootstrapLease(BootstrapLease &&other) noexcept : state_(std::exchange(other.state_, nullptr)) {
    }

    /** Release the destination's borrow and transfer the source; self-move preserves the token.
     * @param other Source token, cleared unless it is this object.
     * @return This token; dependent children must already have retired before replacement.
     */
    BootstrapLease &operator=(BootstrapLease &&other) noexcept;
    /** Release only the counted borrow, after dependent owners have retired. */
    ~BootstrapLease();
    BootstrapLease(const BootstrapLease &) = delete;
    BootstrapLease &operator=(const BootstrapLease &) = delete;

    /** Acquire independently of the movable facade, retaining its current stable State.
     * @param bootstrap Application owner; its destruction remains a caller precondition.
     * @return Token or static refusal; failed admission leaves the owner unchanged.
     */
    [[nodiscard]] static std::expected<BootstrapLease, TcpDiagnostic> acquire(RuntimeBootstrap &bootstrap) noexcept;

    /** Acquire for the high-level runtime on the bootstrap creator thread, without SDK initialization.
     * @param bootstrap Existing live same-instance owner; original pre-thread startup remains a precondition.
     * @return Stable lease or configuration/thread/capacity refusal; no global SDK calls.
     */
    [[nodiscard]] static std::expected<BootstrapLease, TcpDiagnostic>
    acquireForRuntime(RuntimeBootstrap &bootstrap) noexcept;

    /** Inspect whether this token still retains a stable borrow.
     * @return False only after this token has been moved from.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return state_ != nullptr;
    }

    /** Inspect the stable process state for private native assembly and lifetime diagnostics.
     * @return Borrowed State, valid through this token and the application's owning bootstrap lifetime.
     */
    [[nodiscard]] RuntimeBootstrap::State *state() const noexcept {
        return state_;
    }
};

} // namespace org::ttldtor::qdcxx::transport::detail
