// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_configuration.hpp"
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
#    include "recovery_tls_input.hpp"
#endif

#include "qd/client/subscription_receiver.hpp"
#include "qd/transport/recovery.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::connect {

/** Checked source-derived HISTORY whole-frame admission envelope, in distinct logical coordinates.
 * Values describe the selected fixed profile; they are not CPU instructions, time, allocations or RSS.
 */
struct RecoveryHistoryEnvelope {
    std::size_t maximumFrameRecords = 0; ///< Actual LiveLimits.data.emittedRecords bound.
    std::size_t frameSlots = 0;          ///< cacheFrames + viewSlots*pendingFrames + one candidate.
    std::size_t rowVisits = 0;           ///< Pinned whole-row structural precharge.
    std::size_t fixedVisits = 0;         ///< Pinned maintenance metadata precharge.
    std::size_t stageVisits = 0;         ///< Complete frame, maintenance, policy and worst equality visits.
    std::size_t stageTextUnits = 0;      ///< Complete structural text and worst equality examinations.
    std::size_t stageBytes = 0;          ///< All retained/candidate epoch disposal and equality-byte work.
};

/** App-owned complete preparation for one explicit receiving invocation.
 * No queue, desired set, receiver callback, native runtime, timer or SDK bootstrap is owned here.
 * Configuration/profile members remain unchanged after registration; blueprint transfers once into Recovery.
 * The immutable endpoint snapshot preserves exact schema origin and the original finite retry/timeouts.
 */
struct PreparedRecovery {
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
    std::optional<transport::TlsResourceLimits> tlsLimits; ///< Exact immutable TLS input ceilings, without SDK state.
#endif
    core::LocalSchema schema;            ///< Complete validated BINARY_FIELDS_V1 declarations and aliases.
    client::EndpointOptions endpoint;    ///< Original immutable settings/schema; no native owner.
    core::RecordContext selectedContext; ///< Exact selected local record provenance, without ID accessors.
    core::SubscriptionFamily family = core::SubscriptionFamily::STREAM; ///< One chosen delivery contract.
    transport::RecoveryOptions recoveryOptions{};         ///< Fixed copy/replay/selector/explicit-DNS profile.
    transport::RecoveryPreparationWork preparationWork{}; ///< One initial wrapper/writer/queue allowance.
    std::vector<protocol::OutgoingRecord> blueprint;      ///< Complete declarations, never a reduced selected subset.
    mux::MuxLimits interests{};                           ///< Actual one-client aggregate interest reservations.
    mux::QueueLimits queueLimits{};                       ///< Actual shared delivery/command/control reservations.
    mux::ClientLimits clientLimits{};            ///< One receiver's complete detached-data and command reservations.
    collectors::TickerLimits tickerLimits{};     ///< Fixed selected TICKER cache/view profile.
    collectors::TickerOptions tickerOptions{};   ///< Explicit retention/quantum/selector policy.
    collectors::HistoryLimits historyLimits{};   ///< Fixed selected HISTORY point/frame/view profile.
    collectors::HistoryOptions historyOptions{}; ///< Explicit snapshot retrieval/selector policy.
    RecoveryHistoryEnvelope historyEnvelope{};   ///< Source-bound whole-frame stage derivation.
    std::vector<client::PreparedSubscriptionBinding> receiverBindings; ///< Empty generic or one explicit mapper.
    client::ReceiverLimits receiverLimits{}; ///< Fixed plan capacity; registration remains a public factory call.
    client::ReceiverWork receiverWork{};     ///< Explicit one-registration preparation credits.
    client::ReceiverProjectionWork projectionWork{}; ///< One whole callback's fixed owned-projection credits.
    mux::QueueSubmitWork submitWork{};               ///< One caller submission turn; runtime never refills mid-call.
    mux::QueueWork queueWork{};                      ///< Fixed queue turn seed, unchanged for normal/drain operation.
    mux::QueueWork historyObservationWork{}; ///< Independent allowance shared by at most sixteen exact queries/turn.
    transport::RecoveryTurnWork turnWork{};  ///< Fixed Recovery/native/session/queue turn seed.
    std::size_t epochLabelBytes = 0;         ///< Independent app token-array request allowance, not queue storage.
    bool namedRoute = false;                 ///< Explicit DNS path selected; numeric IP bypasses DNS.

    /** Adopt independently owned schema/settings before filling the remaining local profile.
     * @param schema Complete validated declarations, transferred without provenance change.
     * @param endpoint Immutable snapshot of those exact declarations/settings.
     */
    PreparedRecovery(core::LocalSchema &&schema, client::EndpointOptions &&endpoint)
        : schema(std::move(schema)), endpoint(std::move(endpoint)) {
    }

    /** Transfer this unactivated application preparation.
     * @param other Complete owning candidate.
     */
    PreparedRecovery(PreparedRecovery &&other) noexcept = default;
    /** Replace an unactivated profile; runtime owners never reference a replaced profile.
     * @param other Complete owning candidate.
     * @return This profile.
     */
    PreparedRecovery &operator=(PreparedRecovery &&other) noexcept = default;
    /** Avoid unaccounted full-schema/blueprint copies.
     * @param other Existing profile.
     */
    PreparedRecovery(const PreparedRecovery &other) = delete;
    /** Preserve one complete preparation scope.
     * @param other Existing profile.
     * @return Unavailable.
     */
    PreparedRecovery &operator=(const PreparedRecovery &other) = delete;
};

/** Prepare only the supported plain receiving profile before signals/bootstrap/network activation.
 * TLS refuses before file inspection until its complete controlled-host profile is independently proved.
 * Explicit JSON requires the compiled existing loader and an ordinary bounded file; no autodetection.
 * All failures use static text and preserve caller inputs, without paths, endpoint or secret text.
 * @param configuration Complete parsed owned arguments, borrowed unchanged.
 * @return Complete independently owned application profile or static shape/capacity/file/schema refusal.
 */
[[nodiscard]] std::expected<PreparedRecovery, std::string_view>
prepareRecovery(const RecoveryConfiguration &configuration);

/** Construct exactly the selected original queue type, without submitting desire or activating I/O.
 * @param profile Complete unmodified preparation.
 * @param configuration Same immutable frontend selection used for preparation.
 * @param work Explicit actual queue initialization/retirement credits, debited by the public factory.
 * @return Original MuxQueue or original typed queue failure; no wrapper error vocabulary is introduced.
 */
[[nodiscard]] std::expected<mux::MuxQueue, mux::QueueFailure>
createSelectedQueue(const PreparedRecovery &profile, const RecoveryConfiguration &configuration, mux::QueueWork &work);

/** Copy one explicitly chosen fixed queue allowance at a caller turn boundary.
 * @param profile Complete immutable profile.
 * @return Fresh independent credits, identical during normal operation and drain.
 */
[[nodiscard]] mux::QueueWork freshQueueWork(const PreparedRecovery &profile);
/** Copy one explicitly chosen fixed Recovery allowance at a caller turn boundary.
 * @param profile Complete immutable profile.
 * @return Fresh independent credits, identical during normal operation and drain.
 */
[[nodiscard]] transport::RecoveryTurnWork freshRecoveryTurn(const PreparedRecovery &profile);
/** Copy a separate observation allowance, never borrowed from a poll or reset/collector phase.
 * @param profile Complete immutable profile.
 * @return One shared queue/history work object for all current exact-key queries in this application turn.
 */
[[nodiscard]] mux::QueueWork freshHistoryObservationWork(const PreparedRecovery &profile);

} // namespace org::ttldtor::qdcxx::connect
