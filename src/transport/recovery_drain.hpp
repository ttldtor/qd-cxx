// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/recovery.hpp"

#include <chrono>
#include <optional>

namespace org::ttldtor::qdcxx::transport::detail {

/** Shutdown relevance of one actual diagnostic, independent of run-wide historical causes. */
enum class RecoveryShutdownFailure {
    NONE,                 ///< No failure or a pre-admission work refusal.
    POST_FENCE_TRANSPORT, ///< First observed after the fence; affects only native input/output outside the prefix.
    TRANSPORT_GAP,        ///< Current transport/inactivity failure prevents a complete local-drain claim.
    FATAL                 ///< Fatal protocol, authentication, resource or actual retirement failure.
};

/** Classify an independently preserved primary or secondary diagnostic for the local prefix.
 * A post-fence read/write/clean-end is not classified as cancellation and its exact cause stays visible.
 * @param diagnostic Actual common and nested native evidence.
 * @param afterFence True only when this cause was first observed after the accepted shutdown fence.
 * @return Local outcome relevance; no native code or diagnostic is rewritten.
 */
[[nodiscard]] inline RecoveryShutdownFailure classifyRecoveryShutdown(RecoveryDiagnostic diagnostic,
                                                                      bool afterFence) noexcept {
    if (diagnostic.error == RecoveryError::NONE || diagnostic.error == RecoveryError::WORK_LIMIT)
        return RecoveryShutdownFailure::NONE;

    if (diagnostic.error != RecoveryError::TRANSPORT_FAILURE ||
        diagnostic.transport.protocolError != protocol::LiveError::NONE)
        return RecoveryShutdownFailure::FATAL;

    switch (diagnostic.transport.error) {
    case TcpError::NONE:

        if (diagnostic.stage == "recovery-peer-end")
            return afterFence ? RecoveryShutdownFailure::POST_FENCE_TRANSPORT : RecoveryShutdownFailure::TRANSPORT_GAP;

        return diagnostic.stage == "recovery-protocol-inactivity" ? RecoveryShutdownFailure::TRANSPORT_GAP
                                                                  : RecoveryShutdownFailure::FATAL;
    case TcpError::READ_FAILURE:
    case TcpError::WRITE_FAILURE:
        return afterFence ? RecoveryShutdownFailure::POST_FENCE_TRANSPORT : RecoveryShutdownFailure::TRANSPORT_GAP;
    case TcpError::DNS_FAILURE:
    case TcpError::CONNECT_FAILURE:
    case TcpError::DEADLINE:
        return RecoveryShutdownFailure::TRANSPORT_GAP;
    default:
        return RecoveryShutdownFailure::FATAL;
    }
}

/** Allocation-free local-drain deadline policy; the owner samples it before each delivery action. */
class RecoveryDrain {
    std::optional<std::chrono::steady_clock::time_point> deadline_; ///< Earliest accepted monotonic deadline.
    bool requested_ = false;                                        ///< A graceful request preceded any abort.
    bool aborted_ = false; ///< Abort is monotonic, including explicit stop and fatal retirement.
    bool expired_ = false; ///< This active drain reached its inclusive deadline before completion.

    public:
    /** Request a local drain without extending a previous deadline or undoing abort.
     * @param deadline Finite caller-selected monotonic endpoint.
     * @param now Current actual or test-supplied monotonic sample.
     */
    void request(std::chrono::steady_clock::time_point deadline, std::chrono::steady_clock::time_point now) noexcept {
        if (aborted_)
            return;

        requested_ = true;

        if (!deadline_ || deadline < *deadline_)
            deadline_ = deadline;

        sample(now);
    }

    /** Escalate without assigning a deadline-expiry cause. */
    void abort() noexcept {
        aborted_ = true;
    }

    /** Check expiry for all unfinished retirement, including native disposal.
     * @param now Current monotonic sample, taken again before newly entering user delivery.
     */
    void sample(std::chrono::steady_clock::time_point now) noexcept {
        if (active() && now >= *deadline_) {
            expired_ = true;
            aborted_ = true;
        }
    }

    /** Inspect active graceful eligibility.
     * @return True only after a request and before abort or deadline expiry.
     */
    [[nodiscard]] bool active() const noexcept {
        return requested_ && !aborted_;
    }

    /** Inspect the actual inclusive expiry cause.
     * @return True only if a sampled active deadline expired.
     */
    [[nodiscard]] bool expired() const noexcept {
        return expired_;
    }

    /** Inspect the next active drain wake hint.
     * @return Earliest deadline, or empty once graceful eligibility ended.
     */
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> nextWake() const noexcept {
        return active() ? deadline_ : std::nullopt;
    }
};

} // namespace org::ttldtor::qdcxx::transport::detail
