// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_configuration.hpp"
#include "recovery_drain.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <optional>
#include <span>
#include <thread>

namespace org::ttldtor::qdcxx::transport::detail {

/** Actual bounded attempt facts; raw protocol ownership remains inside the recovery operation. */
struct RecoveryAttemptFacts {
    bool present = false;         ///< One actual prepared/active/retiring native owner exists.
    bool connected = false;       ///< Actual plain connection or completed TLS verification.
    bool authenticated = false;   ///< Actual unchallenged/acknowledged eligibility, independent of demand.
    bool terminal = false;        ///< Published native quiescence, distinct from private destruction.
    std::uint64_t generation = 0; ///< Current actual peer generation, never a reconstructed sender ID.
    protocol::LiveAuthentication authentication = protocol::LiveAuthentication::UNCHALLENGED; ///< Actual auth gate.
    std::size_t pendingOperations = 0; ///< Submitted native/DNS/connecting lifetimes awaiting owned retirement.
};

/** Selected real failure classification, preserving existing typed/static evidence. */
struct RecoveryAttemptFailure {
    RecoveryDiagnostic diagnostic{};                ///< Actual current failure or clean peer-end checkpoint.
    RecoveryCause cause = RecoveryCause::TRANSPORT; ///< Scheduler-level classification.
    bool retryable = false; ///< Only failures after successful factory preparation may actually retry.
    std::optional<RecoveryDiagnostic> secondary{}; ///< Actual distinct native/cleanup evidence, if present.
};

/** Private embedded plain/TLS adapter; no SDK dependency or user-extensible facade belongs here.
 * The enclosing Impl declares the adapter BEFORE RecoveryCore, keeping it alive through Core disposal.
 * Public operations retain that same Impl before any provider/consumer call. TLS turn-budget references
 * exist only in the derived adapter's scoped call guard and are cleared before the operation lease ends.
 */
class RecoveryBackend {
    public:
    /** Destroy only after the associated Core has retired its actual attempt. */
    virtual ~RecoveryBackend() = default;
    /** Enter the actual one-attempt factory inside the already charged combined transaction.
     * @param configuration Immutable admitted inputs, including one retry1 endpoint.
     * @param writer Matching freshly prepared or retained first composer.
     * @param work Actual mutable caller coordinates, without resetting them.
     * @return Prepared owner or terminal factory failure evidence.
     */
    virtual std::expected<void, RecoveryDiagnostic>
    prepare(RecoveryConfiguration &configuration, protocol::SubscriptionWriter &&writer, RecoveryTurnWork &work) = 0;
    /** Start the same prepared runtime.
     * @return Actual activation result.
     */
    virtual std::expected<void, RecoveryDiagnostic> start() = 0;
    /** Perform one actual native/TLS/session poll.
     * @param work Independent mutable caller coordinates.
     * @param decodeInput False temporarily gates session operations at zero while the one event slot is held.
     * @return At most one owning event, or a typed pre-admission rejection.
     */
    virtual std::expected<TcpPollResult, RecoveryDiagnostic> poll(RecoveryTurnWork &work, bool decodeInput) = 0;
    /** Admit one fixed local protocol descriptor.
     * @param descriptor Borrowed bounded fixed message/property arrays.
     * @param work Actual session composition credits.
     * @return Whole local output admission or typed refusal.
     */
    virtual std::expected<void, RecoveryDiagnostic> protocol(protocol::LiveProtocolView descriptor,
                                                             protocol::LiveWork &work) = 0;
    /** Admit one outgoing description chunk.
     * @param ids Bounded exact sender IDs from the retained immutable blueprint.
     * @param work Actual session credits.
     * @return Whole local output admission or typed refusal.
     */
    virtual std::expected<void, RecoveryDiagnostic> describe(std::span<const std::int32_t> ids,
                                                             protocol::LiveWork &work) = 0;
    /** Admit one whole selected subscription frame.
     * @param message Actual selected add/remove identity.
     * @param requests Stable borrowed bank entries, valid during the synchronous call.
     * @param work Actual session composition credits.
     * @return Whole local output admission or typed refusal.
     */
    virtual std::expected<void, RecoveryDiagnostic> subscribe(protocol::SubscriptionMessage message,
                                                              std::span<const core::SubscriptionRequest> requests,
                                                              protocol::LiveWork &work) = 0;
    /** Admit one explicit payload-free heartbeat.
     * @param work Actual session composition credits.
     * @return Whole local output admission or typed refusal.
     */
    virtual std::expected<void, RecoveryDiagnostic> heartbeat(protocol::LiveWork &work) = 0;
    /** Request-only native cutoff, including inside the adapter's provider callback. */
    virtual void abort() noexcept = 0;

    /** Request an explicitly reusable plain pause; default adapters retain their ordinary abort policy. */
    virtual void suspend() noexcept {
        abort();
    }

    /** Destroy the terminal runtime, completing actual private SDK/native owner disposal. */
    virtual void dispose() noexcept = 0;

    /** Inspect actual source counters; uninstrumented adapters explicitly provide no metric producer.
     * @return Detached owner-local cut without progress.
     */
    [[nodiscard]] virtual ConnectionStatistics statistics() const noexcept {
        ConnectionStatistics result;
        result.readBytes.availability = core::StatisticAvailability::UNSUPPORTED;
        result.writtenBytes.availability = core::StatisticAvailability::UNSUPPORTED;
        result.readMessages.availability = core::StatisticAvailability::UNSUPPORTED;
        result.writtenMessages.availability = core::StatisticAvailability::UNSUPPORTED;
        result.readDataRecords.availability = core::StatisticAvailability::UNSUPPORTED;
        result.writtenSubscriptionRecords.availability = core::StatisticAvailability::UNSUPPORTED;

        return result;
    }

    /** Retire owned native callbacks before preserving their actual final counters.
     * @return Detached final cut; default adapters retain their existing disposal path.
     */
    [[nodiscard]] virtual ConnectionStatistics retireStatistics() noexcept {
        const auto result = statistics();
        dispose();

        return result;
    }

    /** Inspect actual current facts without retaining a protocol owner.
     * @return Fixed detached state.
     */
    [[nodiscard]] virtual RecoveryAttemptFacts facts() const noexcept = 0;
    /** Inspect actual retained failure or clean peer-end classification.
     * @return Fixed typed/static evidence.
     */
    [[nodiscard]] virtual RecoveryAttemptFailure failure() const noexcept = 0;
    /** Inspect the actual terminal of this attempt's native byte transport before private disposal.
     * @return Detached final wire evidence, or empty before native retirement/no attempt.
     */
    [[nodiscard]] virtual std::optional<TcpTerminal> wireTerminal() const noexcept = 0;
    /** Inspect actual native/TLS progress hint.
     * @return Monotonic deadline or empty when no active deadline remains.
     */
    [[nodiscard]] virtual std::optional<RecoveryClock::time_point> nextWake() const noexcept = 0;
};

/** Stable shared scheduling/replay owner, used by both real thin runtime entry points.
 * No user callable is stored here. The enclosing public operation holds an outer Impl lease BEFORE
 * calling any method that can reach a provider or queue consumer. This class never moves after
 * construction, so its pure-state origin/tickets cannot target a replacement public handle.
 */
class RecoveryCore {
    RecoveryConfiguration configuration_; ///< Complete candidates, followed by one explicit input adoption.
    RecoveryBackend &backend_;            ///< Embedded adapter, declared earlier in the enclosing stable Impl.
    RecoveryState state_;                 ///< Approved actual production scheduler, with this stable origin.
    RecoveryWork retirement_; ///< Copied final Core/one-held-event retirement reserve, not a caller reference.
    std::thread::id thread_;  ///< Creating serialized owner; not a background thread.
    std::optional<protocol::LiveEvent> event_; ///< At most one undecided complete event under the current epoch cap.
    RecoveryDiagnostic first_{};               ///< First fixed failure across this run.
    RecoveryDiagnostic secondary_{};           ///< First distinct subsequent failure.
    RecoveryDrain drain_;                      ///< Earliest local-drain deadline and monotonic abort policy.
    std::optional<RecoveryTerminal> terminal_; ///< Frozen after all actual owned retirement barriers.
    std::optional<TcpTerminal> wire_; ///< Latest actually retired attempt; cleared at the next factory-entry commit.
    ConnectionStatistics retiredStatistics_; ///< Distinct disposed sources folded once, never reset by reconnect.
    std::optional<std::size_t> nativeOperationsAtCutoff_; ///< First explicit fence sample, including an observed zero.
    std::array<bool, 3> converged_{};      ///< Cached funded bank comparison at the exact represented revision.
    std::array<bool, 3> resetRequested_{}; ///< Actual reset-publication acceptance for the current retiring epoch.
    std::array<std::uint64_t, 3> desiredRevisions_{}; ///< Last constant-time observed owner-applied revision.
    std::uint64_t readiness_ = 0;      ///< Checked readiness change identity, never inferred from schema shape.
    std::uint64_t peerGeneration_ = 0; ///< Last actual current generation.
    protocol::LiveAuthentication authentication_ = protocol::LiveAuthentication::UNCHALLENGED; ///< Last actual gate.
    std::size_t description_ = 0;     ///< Next outgoing declaration in this fresh attempt.
    std::size_t family_ = 0;          ///< Fixed fair family cursor.
    std::size_t controlFamily_ = 0;   ///< Independent bounded offline/control-progress cursor.
    bool descriptor_ = false;         ///< The local initial descriptor was actually admitted in this attempt.
    bool connectionFenced_ = false;   ///< Explicit nonterminal control blocks old native intake/output/delivery.
    bool cutoff_ = false;             ///< No new factory/input/output; only an active local drain may dispatch.
    bool executing_ = false;          ///< Reject recursive public progress without replacing active caller budgets.
    bool readinessExhausted_ = false; ///< A readiness change identity cannot wrap into an old value.
    bool abortIssued_ = false;        ///< Actual request-only abort was sent to the current inner runtime.
    bool finalRetirement_ = false;    ///< No successor is possible; the owned fallback event reserve may be used.
    bool shutdownFailed_ = false;     ///< A fatal current cause or failed final retirement prevents successful drain.
    bool shutdownGap_ = false;        ///< An unresolved current transport/inactivity cause prevents successful drain.
    bool heldDataDiscarded_ = false;  ///< Complete DATA was released during stop without queue admission.

    /** Request one fixed stop mode on each unfinished adopted queue.
     * @param mode Graceful local prefix or immediate abort.
     */
    void stopQueues(mux::StopMode mode) noexcept;
    /** Sample the active deadline and fence later delivery if it expired.
     * @param now Fresh monotonic sample, including after native disposal.
     */
    void sampleDrain(RecoveryClock::time_point now) noexcept;
    /** Classify current shutdown causes independently of recovered historical diagnostics. */
    void observeShutdownCause() noexcept;
    /** Preserve and classify each actual native retirement cause relative to the local prefix.
     * @param afterFence These backend causes were first observed after the accepted shutdown fence.
     */
    void observeRetiringBackend(bool afterFence) noexcept;
    /** Preserve any actual wire terminal before disposing the current native/private attempt. */
    void captureWire() noexcept;
    /** Freeze a detached result only after scheduler, native, held-event and all queue barriers. */
    void publishTerminal() noexcept;

    /** Record fixed first/secondary failure evidence.
     * @param diagnostic Actual cause, without borrowed user text.
     */
    void record(RecoveryDiagnostic diagnostic) noexcept;
    /** Publish actual pure-clock/inactivity/identity causes as bounded public diagnostics. */
    void recordModelCause() noexcept;
    /** Sample only fixed/constant-time real facts; comparisons are cached from funded bank work.
     * @return Current scheduler facts.
     */
    [[nodiscard]] RecoveryFacts facts() noexcept;
    /** Execute one outstanding model ticket on the actual owner path.
     * @param ticket Current exact action identity.
     * @param work Actual mutable caller coordinates.
     * @return Matching observation with actual admission/disposal proof only.
     */
    RecoveryObservation execute(RecoveryTicket ticket, RecoveryTurnWork &work);
    /** Progress a single queue's safe offline/control work before native/output actions.
     * @param work Independent actual queue coordinates.
     * @return Static failure, or empty for progress/idle/transient work refusal.
     */
    std::optional<RecoveryDiagnostic> controls(RecoveryTurnWork &work);
    /** Progress one current-desired bank reconciliation action.
     * @param ticket Current replay action.
     * @param work Actual new-owner/session coordinates.
     * @return Actual whole-output admission or bounded refusal.
     */
    RecoveryObservation replay(RecoveryTicket ticket, RecoveryTurnWork &work);
    /** Release a held complete event only after admitting its possible ledger-disposal work.
     * @param work Actual session disposal coordinates.
     * @return True when no held event remains.
     */
    bool releaseEvent(protocol::LiveWork &work) noexcept;
    /** Continue one family/reset step without creating a successor attempt.
     * @param ticket Current reset/retirement action.
     * @param work Actual component coordinates.
     * @return Matching actual barrier evidence or continuing result.
     */
    RecoveryObservation reset(RecoveryTicket ticket, RecoveryTurnWork &work);

    public:
    /** Construct the stable Core after all candidate preparation and before final input adoption.
     * @param configuration Fully prepared, still-uncommitted candidate.
     * @param backend Embedded adapter that outlives this Core.
     * @param retirement Final held-event/Core disposal reserve prepaid before enclosing owner allocation.
     */
    RecoveryCore(RecoveryConfiguration &&configuration, RecoveryBackend &backend, RecoveryWork retirement) noexcept;
    /** Perform permitted idle fallback retirement after all public operation leases have returned. */
    ~RecoveryCore();
    /** A live Core is stable and cannot be copied/moved.
     * @param other Unused source.
     */
    RecoveryCore(const RecoveryCore &other) = delete;
    /** Stable ownership cannot be replaced.
     * @param other Unused source.
     * @return No result; assignment is unavailable.
     */
    RecoveryCore &operator=(const RecoveryCore &other) = delete;
    /** Complete the sole nonthrowing input commit after the enclosing shared control also exists.
     * @param blueprint Same immutable inspected vector.
     * @param queues Same inspected source owners.
     * @return True for the sole actual adoption.
     */
    bool adopt(std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues) noexcept;
    /** Inspect thread/reentry refusal before installing any derived TLS caller-budget scope.
     * @return Detached refusal or empty on owner admission.
     */
    [[nodiscard]] std::optional<RecoveryDiagnostic> rejection() const noexcept;
    /** Request start without entering a factory.
     * @return Acceptance or owner/thread/state refusal.
     */
    std::expected<void, RecoveryDiagnostic> start() noexcept;
    /** Progress the actual bounded production integration on one selected steady clock.
     * @param work Mutable actual credits, never stored or reset.
     * @return Detached status or transient pre-admission refusal.
     */
    std::expected<RecoveryStatus, RecoveryDiagnostic> poll(RecoveryTurnWork &work);
    /** Fence old input while preserving stable queue owners.
     * @param clear Request real cache/pending reset, without invalidating caller-owned immutable values.
     * @return Acceptance or owner/state refusal; request-only and permitted inside an executing callback.
     */
    std::expected<void, RecoveryDiagnostic> requestDisconnect(bool clear) noexcept;
    /** Request a fresh actual source attempt without resetting lifetime factory or ticket admission.
     * @return Acceptance or owner/state/remaining-attempt refusal.
     */
    std::expected<void, RecoveryDiagnostic> requestReconnect() noexcept;
    /** Request-only monotonic cutoff; caller obeys the creating-thread precondition. */
    void cutoff() noexcept;
    /** Request stop with checked thread affinity, including from a consumer/provider callback.
     * @return Acceptance or wrong-thread refusal; normal busy guards do not block stop.
     */
    std::expected<void, RecoveryDiagnostic> requestStop() noexcept;
    /** Request only local admitted-prefix drain while immediately fencing native attempts/I/O.
     * @param deadline Earliest inclusive monotonic deadline, covering all unfinished retirement.
     * @return Acceptance or wrong-thread refusal; callback reentry does not block this request.
     */
    std::expected<void, RecoveryDiagnostic> requestDrain(RecoveryClock::time_point deadline) noexcept;
    /** Copy fixed current status.
     * @return Detached public phase/attempt/cause evidence.
     */
    [[nodiscard]] RecoveryStatus status() const noexcept;
    /** Inspect one actual owner-local lifetime cut without progress.
     * @return Retired-source totals and current actual counters/identities.
     */
    [[nodiscard]] ConnectionStatistics statistics() const noexcept;
    /** Quiesce only the current owned native attempt before a facade's final detached publication.
     * @return Preserved lifetime counters, including raced native completions.
     */
    [[nodiscard]] ConnectionStatistics retireStatistics() noexcept;
    /** Inspect immutable admitted resources.
     * @return Explicit logical vector with documented byte overlap.
     */
    [[nodiscard]] RecoveryCapacity capacity() const noexcept;
    /** Prepare one empty receiver on an adopted family without native progress.
     * @param family Exact immutable adopted queue family.
     * @param bindings Borrowed typed routes under the owner's original schema.
     * @param consumer Application callable, retained only after successful registration.
     * @param receiverLimits Explicit immutable plan reservations.
     * @param clientLimits Explicit complete queue registration reservations.
     * @param receiverWork Actual preparation credits; failures may consume work.
     * @param queueWork Actual queue inspection/open credits.
     * @return Empty handle or unchanged typed owner/receiver failure.
     */
    [[nodiscard]] std::expected<mux::MuxHandle, RecoveryReceiverFailure> openSubscriptionReceiver(
        core::SubscriptionFamily family, std::span<const client::PreparedSubscriptionBinding> bindings,
        const std::shared_ptr<client::SubscriptionConsumer> &consumer, client::ReceiverLimits receiverLimits,
        mux::ClientLimits clientLimits, client::ReceiverWork &receiverWork, mux::QueueWork &queueWork);
    /** Observe the adopted HISTORY queue only under current owner/backend eligibility.
     * @param handle Exact adopted HISTORY registration.
     * @param key Borrowed key used synchronously; nothing is retained.
     * @param work Existing mutable queue/history observation allowance.
     * @return Unchanged detached queue evidence or a nonmutating owner/state/nested queue refusal.
     */
    [[nodiscard]] std::expected<mux::QueueRangeStatus, RecoveryDiagnostic>
    historyRangeStatus(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work) const;
    /** Inspect the earliest native/model wake hint.
     * @return Monotonic hint without a notifier.
     */
    [[nodiscard]] std::optional<RecoveryClock::time_point> nextWake() const noexcept;
};

/** Preserve actual native/pure typed evidence in the common diagnostic.
 * @param diagnostic Existing native checkpoint.
 * @return Recovery-level category with exact nested evidence.
 */
[[nodiscard]] RecoveryDiagnostic recoveryTransportDiagnostic(TcpDiagnostic diagnostic) noexcept;

/** Classify actual native failure for finite retry; never used to retry a failed entered factory.
 * @param diagnostic Actual native failure, or NONE for a clean peer end.
 * @return Conservative typed retry classification.
 */
[[nodiscard]] RecoveryAttemptFailure recoveryTransportFailure(TcpDiagnostic diagnostic) noexcept;

/** Admit the actual stable outer owner and its one held protocol-event fallback before allocation.
 * @param configuration Prepared immutable candidate; its own copy/disposal charges are already paid.
 * @param ownerBytes Actual sizeof the private enclosing Impl, never an inferred foreign ABI size.
 * @param work Actual caller preparation remainder.
 * @return Copied final-retirement reserve or work/overflow refusal, without consuming input owners.
 */
[[nodiscard]] std::expected<RecoveryWork, RecoveryDiagnostic>
admitRecoveryOwner(const RecoveryConfiguration &configuration, std::size_t ownerBytes, RecoveryWork &work) noexcept;

} // namespace org::ttldtor::qdcxx::transport::detail
