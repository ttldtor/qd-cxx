// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/event_registry.hpp"
#include "qd/api/feed.hpp"
#include "qd/client/subscription_receiver.hpp"
#include "qd/transport/tcp_recovery.hpp"

#include "event_selection.hpp"

#include <array>
#include <string>
#include <vector>

namespace org::ttldtor::qdcxx::api::detail {
/** Complete independently bounded plan for every registered record in one supported type selection. */
struct ReceivingEventPlan {
    std::size_t firstBinding = 0;                    ///< First route in ReceivingProfile::bindings.
    std::size_t bindingCount = 0;                    ///< Number of consecutive routes in this type selection.
    client::ReceiverLimits limits{};                 ///< Aggregate complete immutable receiver plan allowance.
    client::ReceiverWork work{};                     ///< Aggregate per-registration preparation seed.
    client::ReceiverProjectionWork projectionWork{}; ///< Complete callback projection allowance.
};

/** Worker-owned immutable receiving configuration; it owns no native object or callback. */
struct ReceivingProfile {
    core::LocalSchema schema;         ///< Complete selected declarations, preserving validated registry provenance.
    client::EndpointOptions endpoint; ///< Independently owned validated endpoint snapshot.
    core::SubscriptionFamily family;  ///< Selected existing source delivery semantics.
    transport::RecoveryOptions recoveryOptions;      ///< Explicit route and finite replay/copy reservations.
    std::vector<protocol::OutgoingRecord> blueprint; ///< Complete outgoing declarations, transferred once.
    mux::MuxLimits interests;                        ///< Aggregate capacities for independent logical subscriptions.
    mux::QueueLimits queueLimits;                    ///< Aggregate registration and detached-data reservations.
    mux::ClientLimits clientLimits;                  ///< Independent per-registration reservations.
    collectors::TickerLimits tickerLimits;           ///< Existing shared cache and independent desired views.
    std::vector<client::PreparedSubscriptionBinding> bindings; ///< Explicit routes grouped by business type.
    std::array<ReceivingEventPlan, 2> eventPlans; ///< Aggregate Quote/Profile plans, independent of record IDs.
    ReceivingEventPlan allEvents;    ///< Aggregate plan for one logical selection of both supported event kinds.
    mux::QueueSubmitWork submitWork; ///< One whole desired replacement admission.
    mux::QueueWork queueWork;        ///< Identical initialization, normal and drain coordinates.
    transport::RecoveryPreparationWork preparationWork; ///< Original recovery/writer/queue preparation seed.
    transport::RecoveryTurnWork turnWork;               ///< Identical normal and drain owner-turn coordinates.

    /** Adopt complete schema and its validated immutable endpoint.
     * @param schema Complete binary declarations, transferred without changing provenance.
     * @param endpoint Independent immutable snapshot of those declarations and finite settings.
     */
    ReceivingProfile(core::LocalSchema schema, client::EndpointOptions endpoint);
};

/** Validate and prepare the finite plain receiving profile without activating SDK/native state.
 * @param runtime Explicit registry and symbol capacities, with resolved DNS snapshot.
 * @param options Explicit endpoint policy.
 * @param host Validated bounded endpoint host.
 * @param port Validated nonzero endpoint port.
 * @return Complete worker-owned preparation; throws detached Error on incompatible capacity or profile.
 */
[[nodiscard]] ReceivingProfile prepareReceivingProfile(const RuntimeOptions &runtime, const EndpointOptions &options,
                                                       const std::string &host, std::uint16_t port);

/** Select complete immutable preparation for a supported business projection, independently of local record IDs.
 * @param profile Worker-owned immutable receiving preparation.
 * @param selection Immutable supported type set.
 * @return Borrowed complete per-type plan; throws a detached unsupported-profile error for an unknown type.
 */
[[nodiscard]] const ReceivingEventPlan &receivingEventPlan(const ReceivingProfile &profile, EventSelection selection);

/** Construct the selected existing queue without opening registrations or activating native work.
 * @param profile Complete immutable prepared configuration.
 * @param work Actual independently supplied initialization credits.
 * @return Existing queue or its original typed refusal.
 */
[[nodiscard]] std::expected<mux::MuxQueue, mux::QueueFailure> createReceivingQueue(const ReceivingProfile &profile,
                                                                                   mux::QueueWork &work);
} // namespace org::ttldtor::qdcxx::api::detail
