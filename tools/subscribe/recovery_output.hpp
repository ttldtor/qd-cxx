// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "configuration.hpp"

#include "qd/client/subscription_receiver.hpp"
#include "qd/transport/recovery.hpp"
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
#    include "qd/transport/tls_runtime.hpp"
#endif

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::connect {

/** App-local presentation labels retaining only detached opaque epoch tokens.
 * This is a bounded encounter-order table, not a source route, recovery log or scheduler.
 * The one-family CLI permits its initial queue epoch and at most one reset per entered attempt.
 * All methods are serialized on the application owner thread; arbitrary foreign tokens are not input.
 */
class RecoveryEpochLabels {
    std::vector<mux::QueueIdentity> epochs_; ///< No records, queue port, callback or native owner is retained.
    std::size_t limit_;                      ///< Checked maximumAttempts + one initial epoch.

    public:
    /** Reserve the complete finite label table before activation.
     * @param maximumAttempts Immutable positive whole-run Recovery attempt ceiling.
     * @param maximumBytes Independent application requested token-array allowance, excluding allocator overhead.
     * @throws std::runtime_error Static shape/capacity refusal; allocation exceptions occur before activation.
     */
    RecoveryEpochLabels(std::uint32_t maximumAttempts, std::size_t maximumBytes);
    /** Preserve one stable run-local label table; copying it would fork the presentation scope.
     * @param other Existing table.
     */
    RecoveryEpochLabels(const RecoveryEpochLabels &other) = delete;
    /** Keep every sink reference bound to the original stable table.
     * @param other Existing table.
     * @return Unavailable.
     */
    RecoveryEpochLabels &operator=(const RecoveryEpochLabels &other) = delete;
    /** Keep the table's address stable through all callbacks.
     * @param other Existing table.
     */
    RecoveryEpochLabels(RecoveryEpochLabels &&other) = delete;
    /** Prevent replacement beneath active sink references.
     * @param other Existing table.
     * @return Unavailable.
     */
    RecoveryEpochLabels &operator=(RecoveryEpochLabels &&other) = delete;
    /** Label one actual detached token supplied by the selected queue's data or control observation.
     * @param epoch Actual observed token; invalid tokens produce null, never a fabricated identity.
     * @return Stable one-based encounter ordinal for this table, or null for an invalid token.
     * @throws std::runtime_error Static capacity failure; no table growth beyond its original limit.
     */
    [[nodiscard]] std::optional<std::uint64_t> label(const mux::QueueIdentity &epoch);
};

/** App-owned synchronous whole-batch sink for the explicitly selected recovery route.
 * Each mode preserves every generic field; Quote/Profile adds an explicit owned projection row.
 * Streams and labels outlive callbacks/final flush. No owner pointer or callback borrow is retained.
 * Output failure throws static text; partial external side effects are never retried or rolled back.
 */
class RecoveryOutputConsumer final : public client::SubscriptionConsumer {
    std::ostream &output_;
    RecoveryEpochLabels &epochs_;
    core::RecordContext context_;
    core::SubscriptionFamily family_;
    std::int32_t localId_;
    OutputProfile profile_;
    client::ReceiverProjectionWork projectionPerBatch_;
    std::size_t fieldCount_;
    bool compactOnly_;
    std::uint64_t ordinal_ = 0;
    std::uint64_t completedRecords_ = 0;
    bool begun_ = false;
    bool failed_ = false;

    public:
    /** Capture only immutable selected provenance and explicit callback projection allowances.
     * @param output Borrowed synchronous stdout sink.
     * @param epochs Stable application-owned bounded encounter-label table shared with evidence writers.
     * @param schema Exact validated schema used by receiver registration.
     * @param localId Actual selected schema record position, never inferred from a name or sender ID.
     * @param family Exact selected queue family.
     * @param profile Generic output or an explicitly prepared Quote/Profile route.
     * @param projectionPerBatch Finite application credits copied once per whole callback, not per record.
     * @throws std::runtime_error Static selection refusal before activation.
     */
    RecoveryOutputConsumer(std::ostream &output, RecoveryEpochLabels &epochs, const core::LocalSchema &schema,
                           std::int32_t localId, core::SubscriptionFamily family, OutputProfile profile,
                           client::ReceiverProjectionWork projectionPerBatch);
    /** Preserve one publication/counter scope.
     * @param other Existing sink.
     */
    RecoveryOutputConsumer(const RecoveryOutputConsumer &other) = delete;
    /** Prevent forking a live publication scope.
     * @param other Existing sink.
     * @return Unavailable.
     */
    RecoveryOutputConsumer &operator=(const RecoveryOutputConsumer &other) = delete;
    /** Keep receiver-held callback state at a stable address.
     * @param other Existing sink.
     */
    RecoveryOutputConsumer(RecoveryOutputConsumer &&other) = delete;
    /** Prevent replacement of live callback references.
     * @param other Existing sink.
     * @return Unavailable.
     */
    RecoveryOutputConsumer &operator=(RecoveryOutputConsumer &&other) = delete;
    /** Publish and flush the distinct version/selection header before source activation.
     * @throws std::runtime_error Static duplicate-begin or sink failure.
     */
    void begin();
    /** Publish every record of one callback, even when the application target is crossed inside it.
     * @param batch Callback-only full generic values and prepared projection service.
     * @return CONTINUE after checked publication; runtime owns later cutoff/stop/drain decisions.
     * @throws std::runtime_error Static selection/projection/overflow/sink failure, without arbitrary what() text.
     */
    mux::BatchAction onBatch(const client::SubscriptionBatch &batch) override;
    /** Read only completely flushed successful callback-record units, not queue or peer acknowledgement.
     * @return Whole-batch successful publication count; failure does not undo earlier external bytes.
     */
    [[nodiscard]] std::uint64_t completedRecords() const noexcept;
    /** Inspect a permanent frontend publication refusal after a returning owner turn.
     * This app-only observation never requests cutoff or accesses a queue/recovery owner.
     * @return True after begin, callback or final publication fails; serialized with all sink operations.
     */
    [[nodiscard]] bool failed() const noexcept;
    /** Check final stdout publication separately from actual recovery/queue terminal evidence.
     * @throws std::runtime_error Static lifecycle/sink failure.
     */
    void flush();
};

/** Fixed frontend command names; no arbitrary input text is accepted by evidence writers. */
enum class RecoveryOutputCommand { INITIAL_REPLACE, REMOVE_INITIAL };
/** Fixed provenance of an actual diagnostic observation, without a new runtime state machine. */
enum class RecoveryOutputDiagnostic { FIRST, SECONDARY, TURN, HISTORY_QUERY };

/** Write actual common owner state; caller decides when an observation is worth publishing.
 * @param output Borrowed diagnostic sink.
 * @param status Actual detached public status, unmodified by this writer.
 * @throws std::runtime_error Static sink failure.
 */
void writeRecoveryStatus(std::ostream &output, const transport::RecoveryStatus &status);
/** Write only typed numeric diagnostic members; no stage, peer, path or exception text is echoed.
 * @param output Borrowed diagnostic sink.
 * @param scope Fixed frontend observation category.
 * @param diagnostic Actual detached public diagnostic.
 * @throws std::runtime_error Static category/sink failure.
 */
void writeRecoveryDiagnostic(std::ostream &output, RecoveryOutputDiagnostic scope,
                             const transport::RecoveryDiagnostic &diagnostic);
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
/** Publish actual TLS failure codes without formatted SDK, certificate, peer or stage text.
 * @param output Borrowed checked diagnostic sink.
 * @param scope Fixed frontend observation category.
 * @param diagnostic Detached actual TLS evidence, independent of the common recovery category.
 * @throws std::runtime_error Static category or sink failure.
 */
void writeRecoveryTlsDiagnostic(std::ostream &output, RecoveryOutputDiagnostic scope,
                                const transport::TlsDiagnostic &diagnostic);
#endif
/** Publish actual command acceptance, distinct from later authoritative application or wire admission.
 * @param output Borrowed diagnostic sink.
 * @param command Fixed frontend command category.
 * @param ticket Actual nonempty command ticket; no result is polled or retained.
 * @throws std::runtime_error Static invalid-ticket/category/sink failure.
 */
void writeRecoveryCommandAccepted(std::ostream &output, RecoveryOutputCommand command,
                                  const mux::MuxCommandTicket &ticket);
/** Publish an already observed detached completion without querying a handle.
 * @param output Borrowed diagnostic sink.
 * @param command Fixed frontend command category.
 * @param result Actual immutable command completion.
 * @throws std::runtime_error Static category/sink failure.
 */
void writeRecoveryCommandResult(std::ostream &output, RecoveryOutputCommand command,
                                const mux::MuxCommandResult &result);
/** Publish/flush the actual coalesced reset interval before the caller attempts exact-sequence ACK.
 * @param output Borrowed diagnostic sink.
 * @param epochs Same app-local labels used by data output.
 * @param notice Actual copied notice; this function never acknowledges it or calls an owner.
 * @throws std::runtime_error Static label/sink failure; caller must not ACK after failed publication.
 */
void writeRecoveryReset(std::ostream &output, RecoveryEpochLabels &epochs, const mux::QueueResetNotice &notice);
/** Publish the actual result of the caller's exact-sequence ACK attempt.
 * @param output Borrowed diagnostic sink.
 * @param sequence Exact attempted notice sequence.
 * @param admission Actual public control result, including STALE.
 * @throws std::runtime_error Static sink failure.
 */
void writeRecoveryResetAcknowledgement(std::ostream &output, std::uint64_t sequence, mux::ControlAdmission admission);
/** Publish one successful current owner query as detached AS-OF evidence, never a freshness lease.
 * @param output Borrowed diagnostic sink.
 * @param epochs Same bounded app-local label table.
 * @param localId Already validated selected local ID.
 * @param symbol Exact queried key text, borrowed during this call.
 * @param status Actual successful public range result, without recomputing completeness.
 * @throws std::runtime_error Static label/sink failure.
 */
void writeRecoveryHistory(std::ostream &output, RecoveryEpochLabels &epochs, std::int32_t localId,
                          std::u16string_view symbol, const mux::QueueRangeStatus &status);
/** Publish a refused observation without reusing a previous successful range or reporting complete=false.
 * @param output Borrowed diagnostic sink.
 * @param localId Already validated selected local ID.
 * @param symbol Exact attempted key text.
 * @param requestedFrom Actual authoritative/requested raw bound known by the caller.
 * @param diagnostic Actual query refusal; nested detail can use writeRecoveryDiagnostic separately.
 * @throws std::runtime_error Static sink failure.
 */
void writeRecoveryHistoryUnavailable(std::ostream &output, std::int32_t localId, std::u16string_view symbol,
                                     std::int64_t requestedFrom, const transport::RecoveryDiagnostic &diagnostic);
/** Publish actual frozen local retirement and separate optional native wire evidence.
 * @param output Borrowed diagnostic sink.
 * @param terminal Actual public terminal; no terminal is synthesized from counters/deadlines.
 * @throws std::runtime_error Static sink failure.
 */
void writeRecoveryTerminal(std::ostream &output, const transport::RecoveryTerminal &terminal);
/** Publish actual per-family local terminal and counts, independent of native output acceptance.
 * @param output Borrowed diagnostic sink.
 * @param epochs Same bounded display-label table.
 * @param family Actual adopted queue family.
 * @param terminal Actual detached family terminal.
 * @throws std::runtime_error Static family/label/sink failure.
 */
void writeRecoveryQueueTerminal(std::ostream &output, RecoveryEpochLabels &epochs, core::SubscriptionFamily family,
                                const mux::QueueTerminal &terminal);

} // namespace org::ttldtor::qdcxx::connect
