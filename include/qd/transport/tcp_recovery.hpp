// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/recovery.hpp"

#include <expected>
#include <memory>
#include <vector>

namespace org::ttldtor::qdcxx::transport {

/** Opt-in finite reconnect owner with stable family delivery queues and fresh plain protocol epochs.
 * All operations, moves and destruction are serialized on the creating thread. Bootstrap's stable
 * process State outlives this whole owner, including CONFIGURED, BACKOFF and CLOSED, and any operation
 * returning after a callback destroys or replaces the public handle. Bootstrap may move; its transferred
 * owning facade must retain that State until final recovery destruction and native retirement.
 * Participating translation units use the same qualified
 * standard-library profile/ABI; the factory does not attest foreign target overrides.
 * Default TcpRuntime behavior is unchanged. No raw protocol event/snapshot escapes this owner.
 */
class TcpRecovery {
    struct Impl;
    std::shared_ptr<Impl> impl_; ///< Stable operation lease, independent of unique public ownership.
    /** Adopt complete unactivated state without allocating.
     * @param impl Stable complete owner after transactional input adoption.
     */
    explicit TcpRecovery(std::shared_ptr<Impl> impl) noexcept;

    public:
    /** Public ownership cannot be copied.
     * @param other Unused source.
     */
    TcpRecovery(const TcpRecovery &other) = delete;
    /** Public ownership cannot be copied.
     * @param other Unused source.
     * @return No result; copying is unavailable.
     */
    TcpRecovery &operator=(const TcpRecovery &other) = delete;
    /** Transfer unique public ownership without cutting off its run.
     * @param other Source handle, made moved from.
     */
    TcpRecovery(TcpRecovery &&other) noexcept;
    /** Cut off the old destination before transfer; self-move has no effect.
     * @param other Source handle on the same creating owner thread.
     * @return This handle.
     */
    TcpRecovery &operator=(TcpRecovery &&other) noexcept;
    /** Request-only cutoff inside callbacks; idle fallback disposal may drain synchronously. */
    ~TcpRecovery();
    /** Prepare without network activation or consumer callbacks.
     * Unknown copy profiles fail before any recovery allocation. Every fallible allocation precedes
     * final nonthrowing input adoption, including the stable Core/control allocation.
     * @param bootstrap Explicit process owner; its stable State is borrowed through final recovery destruction,
     * including returning operations and final native retirement. Moving its owning facade is supported.
     * @param endpoint Original immutable endpoint; original retry scalars remain effective for recovery.
     * @param blueprint Explicit outgoing records, unchanged on every failure.
     * @param queues Compatible pristine owners; existing registrations/desired commands are permitted.
     * @param options Explicit finite liveness, route, replay and copy capacities.
     * @param work Actual independent preparation credits, without refunds.
     * @return One unactivated owner or static typed refusal preserving caller inputs.
     */
    [[nodiscard]] static std::expected<TcpRecovery, RecoveryDiagnostic>
    create(RuntimeBootstrap &bootstrap, const client::EndpointOptions &endpoint,
           std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
           RecoveryPreparationWork &work);
    /** Request explicit start without entering a factory or activating a route.
     * @return Acceptance or owner/thread/state refusal.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic> start() noexcept;
    /** Progress bounded actual recovery, native, protocol and stable queue work.
     * A consumer may stop/destroy/move-replace this public owner. The old operation lease remains
     * stable; no returned old continuation can activate a replacement or another factory.
     * @param work Actual caller coordinates, never retained by reference or reset.
     * @return Current detached status or transient pre-admission work/thread/reentry refusal.
     */
    [[nodiscard]] std::expected<RecoveryStatus, RecoveryDiagnostic> poll(RecoveryTurnWork &work);
    /** Record allocation-free monotonic cutoff, including from this owner's consumer callback.
     * No inline native cleanup, callback dispatch or waiting occurs.
     * @return Acceptance or wrong-thread/moved-from refusal.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic> requestStop() noexcept;
    /** Request drain of the local queue-admitted delivery prefix, immediately aborting native I/O.
     * Later poll calls may deliver only that prefix until the earliest requested deadline expires.
     * No output flush, peer acknowledgement or delivery of an unadmitted Core-held event is promised.
     * Allowed in this owner's callback; existing stop/destruction always escalates to immediate abort.
     * @param deadline Inclusive steady-clock deadline covering all unfinished local retirement.
     * @return Acceptance or creating-thread/moved-from refusal; no inline cleanup or callback dispatch.
     */
    [[nodiscard]] std::expected<void, RecoveryDiagnostic>
    requestDrain(std::chrono::steady_clock::time_point deadline) noexcept;
    /** Inspect current fixed status on the serialized owner.
     * @return Detached evidence; moved-from handles report CLOSED/stopped.
     */
    [[nodiscard]] RecoveryStatus status() const noexcept;
    /** Inspect admitted scopes with their explicit byte/count overlap.
     * @return Logical resource vector, or zeros after move; never process RSS.
     */
    [[nodiscard]] RecoveryCapacity capacity() const noexcept;
    /** Prepare one empty logical receiver on an adopted family queue, without I/O or callbacks.
     * Requires the creating thread outside owner progress/callbacks. Allowed before start and during
     * temporary reconnect/reset; stop, drain, final retirement and terminal owners reject before work
     * debit. Uses the owner's exact schema provenance and immutable selector policy. Success submits
     * no desired command: use the returned handle's existing add/remove/replace/clear operations.
     * @param family Required adopted delivery family; a missing queue is an owner-state refusal.
     * @param bindings Borrowed unique local typed routes; unlisted records remain generic.
     * @param consumer Application callable, preserved on refusal and retained on successful registration.
     * @param receiverLimits Finite immutable receiver plan reservations.
     * @param clientLimits Complete finite per-registration queue reservations.
     * @param receiverWork Actual preparation credits; attempted failures are not refunded.
     * @param queueWork Actual queue inspection/open credits, never retained by reference.
     * @return Existing empty producer handle, owner refusal, or complete original ReceiverFailure.
     * Returned handles expose no queue owner and do not extend native recovery lifetime.
     */
    [[nodiscard]] std::expected<mux::MuxHandle, RecoveryReceiverFailure> openSubscriptionReceiver(
        core::SubscriptionFamily family, std::span<const client::PreparedSubscriptionBinding> bindings,
        const std::shared_ptr<client::SubscriptionConsumer> &consumer, client::ReceiverLimits receiverLimits,
        mux::ClientLimits clientLimits, client::ReceiverWork &receiverWork, mux::QueueWork &queueWork);
    /** Observe one adopted HISTORY range after the enclosing poll/callback returns.
     * Requires the creating owner thread, current ACTIVE recovery, no cutoff or held complete input,
     * and an actually connected, authenticated, nonterminal backend. A gated query is unknown;
     * an older successful result does not remain a current completeness claim.
     * No I/O, callback, stop, reset acknowledgement or operational failure recording is performed.
     * @param handle Registration belonging to this owner's adopted HISTORY queue.
     * @param key Exact schema/symbol/selector key; borrowed units are used only during this call.
     * @param work Actual queue/history observation credits, debited only by an eligible queue query.
     * @return Detached admitted-prefix AS-OF evidence, or owner/state refusal or exact nested queue failure.
     * Only completeDeliveredRange establishes scoped local completion; unread input and peer ACK are excluded.
     */
    [[nodiscard]] std::expected<mux::QueueRangeStatus, RecoveryDiagnostic>
    historyRangeStatus(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work) const;
    /** Inspect the earliest monotonic/native hint without registering a timer.
     * @return Wake hint, or empty when no progress deadline remains.
     */
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> nextWake() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport
