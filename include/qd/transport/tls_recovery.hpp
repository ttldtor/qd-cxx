// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/recovery.hpp"
#include "qd/transport/tls_runtime.hpp"

#include <expected>
#include <memory>
#include <vector>

namespace org::ttldtor::qdcxx::transport {

/** Detached common recovery state plus exact TLS numeric failure provenance. */
struct TlsRecoveryStatus {
    RecoveryStatus recovery{}; ///< Actual scheduler/queue/native state.
    TlsDiagnostic
        first{}; ///< First actual terminal TLS/factory/composition failure; transient slot/work gates excluded.
    TlsDiagnostic secondary{}; ///< First distinct later TLS/cleanup failure.
};

/** Opt-in finite verified-TLS reconnect with fresh protocol/authentication epochs.
 * Bootstrap's stable process State outlives this whole owner, including CONFIGURED, BACKOFF and CLOSED,
 * and any operation returning after a callback destroys or replaces the public handle. Bootstrap may
 * move; its transferred owning facade retains that State until final recovery destruction and native
 * retirement. Every controlled-host premise outlives the run and in-flight operations. Declared
 * total failed-prefix ceilings reserve at least three private/four ambient prefixes for this run;
 * external/past debt must remain within the residual declared budget. This is an application
 * obligation, not measured headroom, SDK isolation or runtime attestation of consumer headers.
 * All methods, moves, inspection and destruction use the creating serialized owner thread.
 */
class TlsRecovery {
    struct Impl;
    std::shared_ptr<Impl> impl_; ///< Stable outer operation lease, separate from unique public ownership.
    /** Adopt complete unactivated state without allocating.
     * @param impl Complete stable owner after final transactional input adoption.
     */
    explicit TlsRecovery(std::shared_ptr<Impl> impl) noexcept;

    public:
    /** Public ownership cannot be copied.
     * @param other Unused source.
     */
    TlsRecovery(const TlsRecovery &other) = delete;
    /** Public ownership cannot be copied.
     * @param other Unused source.
     * @return No result; copying is unavailable.
     */
    TlsRecovery &operator=(const TlsRecovery &other) = delete;
    /** Transfer public ownership without cutting off its run.
     * @param other Source, made moved from.
     */
    TlsRecovery(TlsRecovery &&other) noexcept;
    /** Cut off the old destination before transfer; self-move is a no-op.
     * @param other Source on the same creating thread.
     * @return This handle.
     */
    TlsRecovery &operator=(TlsRecovery &&other) noexcept;
    /** Request-only cutoff inside callbacks; permitted idle fallback may drain synchronously. */
    ~TlsRecovery();
    /** Prepare immutable bounded candidates, without entering the TLS SDK or invoking credentials.
     * All allocation, including the stable outer Core/control, precedes final input adoption.
     * RecoveryOptions.dns is authoritative; an explicitly supplied TlsOptions.dns must match it.
     * @param bootstrap Explicit process owner; its stable State is borrowed through final recovery destruction,
     * including returning operations and final native retirement. Moving its owning facade is supported.
     * @param endpoint Original immutable TLS endpoint, unchanged on failure.
     * @param blueprint Explicit outgoing records, unchanged on failure.
     * @param queues Compatible pristine stable sources, unchanged on failure.
     * @param options Explicit finite recovery/copy/replay/liveness/route values.
     * @param tls Same qualified SDK resource/host profile across every attempted connection.
     * @param work Actual independent configuration preparation work, without refunds.
     * @return One unactivated owner or detached refusal preserving all caller ownership.
     */
    [[nodiscard]] static std::expected<TlsRecovery, RecoveryDiagnostic>
    create(RuntimeBootstrap &bootstrap, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           const TlsOptions &tls, RecoveryPreparationWork &work);
    /** Request explicit start without entering a factory/SDK or activating a route.
     * @return Acceptance or creating-thread/state refusal.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic> start() noexcept;
    /** Progress actual bounded TLS/native/protocol/queue work under one stable outer lease.
     * Provider/consumer code may stop, destroy or move-replace the public handle. Old return values
     * cannot authorize a replacement or successor; caller work references do not escape this turn.
     * @param work Actual common/replay/native/session/queue credits, never implicitly replenished.
     * @param tlsWork Actual qualified F+R/H/E/close credits, without failure refunds.
     * @return Detached status or transient pre-admission work/thread/reentry refusal.
     */
    [[nodiscard]] std::expected<TlsRecoveryStatus, RecoveryDiagnostic> poll(RecoveryTurnWork &work, TlsWork &tlsWork);
    /** Record allocation-free monotonic cutoff, including from a credential/consumer callback.
     * Performs no inline SDK/native cleanup or callback dispatch.
     * @return Acceptance or creating-thread/moved-from refusal; normal busy guards do not block stop.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic> requestStop() noexcept;
    /** Request drain of the local queue-admitted delivery prefix, immediately aborting native TLS I/O.
     * Later poll calls may deliver only that prefix until the earliest requested deadline expires.
     * This does not flush output, complete close-notify, await the peer or deliver a Core-held event.
     * Allowed in this owner's callback; stop/destruction still escalates to immediate abort.
     * @param deadline Inclusive steady-clock deadline covering all unfinished local retirement.
     * @return Acceptance or creating-thread/moved-from refusal; no inline SDK cleanup or callbacks.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic>
    requestDrain(std::chrono::steady_clock::time_point deadline) noexcept;
    /** Inspect actual common and TLS cause evidence.
     * @return Fixed detached status; moved-from handles report CLOSED/stopped.
     */
    [[nodiscard]] TlsRecoveryStatus status() const noexcept;
    /** Inspect admitted immutable vector/scopes.
     * @return Logical requested capacities, or zeros after move; never process RSS.
     */
    [[nodiscard]] RecoveryCapacity capacity() const noexcept;
    /** Inspect the earliest selected monotonic/native/TLS progress hint.
     * @return Hint or empty; no timer, notifier or background scheduler is installed.
     */
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> nextWake() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport
