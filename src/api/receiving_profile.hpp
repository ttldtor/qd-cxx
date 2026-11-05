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
inline constexpr std::size_t MAXIMUM_RECEIVING_RECORDS = 16; ///< Existing complete receiving catalog ceiling.
inline constexpr std::size_t RECEIVING_EVENT_KINDS = 10;     ///< Explicitly supported native event kinds.
inline constexpr std::size_t RECEIVING_EVENT_SELECTIONS = (1U << RECEIVING_EVENT_KINDS) - 1;

///< Every nonempty finite selection; no dynamic type registry or catalog expansion.

/** Conservative stack initialization plus complete metadata read/write work; no dynamic allocation. */
inline constexpr std::size_t RECEIVING_BINDING_COPY_WORK =
    3 * sizeof(std::array<client::PreparedSubscriptionBinding, MAXIMUM_RECEIVING_RECORDS>);

/** One temporary logical selector and at most one physical regional copy, never shared with a callback. */
struct ReceivingSymbol {
    std::u16string logical;  ///< Owned immutable frontend snapshot, kept through synchronous desired submission.
    std::u16string physical; ///< Owned suffix-free text, shared by matching regional routes of this selector.
    char16_t exchange = 0;   ///< Syntactic logical exchange, preserving trailing attributes.
    std::array<bool, RECEIVING_EVENT_KINDS> regional{}; ///< Exact regional match per business kind.
    core::SubscriptionSelectorKind selector = core::SubscriptionSelectorKind::EXACT; ///< Explicit ALL has no text.
    std::optional<std::int32_t> sourceId; ///< Explicit indexed selector ID; absent PLAIN demands every Order source.
};

/** Worker routing reservation plus a separate producer-attempt metric; queue-owned storage remains separate. */
struct ReceivingRoutingLimits {
    std::size_t symbols = 0;               ///< Maximum logical snapshot and physical-copy slots.
    std::size_t temporaryTextUnits = 0;    ///< Combined logical snapshot plus physical UTF-16 units.
    std::size_t temporaryBytes = 0;        ///< Logical wrapper/input/text backing including terminators, not heap/RSS.
    std::size_t frontendSelectorBytes = 0; ///< Four banks for one producer attempt, separate from worker/queue copies.
    std::size_t retainedMetadataBytes = 0; ///< Fixed exchange array backing, separately retained in the profile.
    mux::QueueSubmitWork work{};           ///< Conservative per-attempt scans, copies, backing and disposal charge.
};

/** Complete independently bounded plan for every registered record in one supported type selection. */
struct ReceivingEventPlan {
    std::array<std::size_t, MAXIMUM_RECEIVING_RECORDS> bindingIndices{};
    ///< Exact selected indices in the unique immutable binding catalog, including nonconsecutive mixed kinds.
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
    collectors::HistoryLimits historyLimits{}; ///< Real shared HISTORY source and independent desired-view backing.
    std::vector<client::PreparedSubscriptionBinding> bindings; ///< Explicit routes grouped by business type.
    std::array<char16_t, MAXIMUM_RECEIVING_RECORDS> bindingExchanges{};
    ///< Exact aligned Quote/Trade record suffixes; fixed backing is included in sizeof(ReceivingProfile).
    std::array<std::optional<std::int32_t>, MAXIMUM_RECEIVING_RECORDS> bindingSources{};
    ///< Exact aligned Order sources; absent for nonindexed business projections.
    std::array<collectors::TickerVersionBinding, MAXIMUM_RECEIVING_RECORDS> versionBindings{};
    std::size_t versionBindingCount = 0; ///< Explicit schema-bound TICKER version gates.
    ReceivingRoutingLimits routing;      ///< Explicit additional routing workspace and pre-allocation work charge.
    std::array<ReceivingEventPlan, RECEIVING_EVENT_SELECTIONS> eventPlans;
    ///< Prepaid bounded plans for each nonempty supported type mask, independent of record IDs.
    mux::QueueSubmitWork submitWork;         ///< One whole desired replacement admission.
    mux::QueueWork queueWork;                ///< Identical initialization, normal and drain coordinates.
    mux::QueueWork historyObservationWork{}; ///< One aggregate observation seed per complete outer worker turn.
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

/** Prepay and copy selected immutable binding metadata into finite caller stack backing.
 * @param profile Valid complete immutable binding catalog.
 * @param plan Valid mask plan belonging to that catalog.
 * @param output Exactly MAXIMUM_RECEIVING_RECORDS slots; only the selected prefix is written.
 * @param work Receiver construction credits, debited once for attempted successful copying.
 * @return False before changing output or credits if shape or finite funding is insufficient.
 */
[[nodiscard]] bool copyReceivingBindings(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                                         std::span<client::PreparedSubscriptionBinding> output,
                                         client::ReceiverWork &work) noexcept;

/** Debit the complete conservative routing workspace before snapshot allocation or publication.
 * @param profile Immutable checked capacities and additional routing allowance.
 * @param work Remaining whole-owner-turn submission coordinates; failed preflight leaves them unchanged.
 * @return True after debiting routing only; false postpones this revision until a fresh owner turn.
 */
[[nodiscard]] bool prepayReceivingRouting(const ReceivingProfile &profile, mux::QueueSubmitWork &work) noexcept;

/** Check whether removing and restoring an existing regional suffix preserves the exact logical spelling.
 * @param symbol Borrowed logical selector, possibly with attributes or a repeated suffix.
 * @return True when regional conversion is invertible; symbols without a regional suffix need no conversion.
 */
[[nodiscard]] bool regionalSymbolRepresentable(std::u16string_view symbol) noexcept;

/** Reject a nonrepresentable selector only when this selection actually chooses a regional Quote route.
 * @param registry Immutable optional explicit registry; defaults contain only composite routes.
 * @param selection Immutable normalized selected event kinds.
 * @param symbol Complete printable logical selector, already bounded by runtime symbol limits.
 * @throws Error INVALID_ARGUMENT at subscription-symbol-representation for a lossy selected regional route;
 * unsupported oversized registry names/counts retain profile-schema refusal before native activation.
 */
void validateReceivingSymbol(const EventRegistry *registry, EventSelection selection, std::u16string_view symbol);

/** Select actual regional/composite routes and create at most one physical copy per logical selector.
 * @param profile Immutable complete schema/binding/exchange metadata.
 * @param plan Valid selected span within that profile, grouped by supported business kind.
 * @param symbols Complete owned logical snapshot, unchanged in count and logical contents.
 * @throws std::bad_alloc Physical string allocation fails; no desired command has been published.
 * @throws std::length_error A physical string exceeds its implementation limit.
 * @throws Error INVALID_ARGUMENT if selected regional conversion cannot preserve logical membership.
 */
void prepareReceivingSymbols(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                             std::span<ReceivingSymbol> symbols);

/** Test one selected binding against precomputed same-kind regional/fallback metadata.
 * @param profile Immutable metadata aligned with bindings.
 * @param bindingIndex Valid selected binding position.
 * @param symbol Complete prepared selector, kept alive through desired submission.
 * @return True for an exact regional match, or a composite route without a matching regional route of that kind.
 */
[[nodiscard]] bool receivingSymbolMatches(const ReceivingProfile &profile, std::size_t bindingIndex,
                                          const ReceivingSymbol &symbol) noexcept;

/** Construct the selected existing queue without opening registrations or activating native work.
 * @param profile Complete immutable prepared configuration.
 * @param work Actual independently supplied initialization credits.
 * @return Existing queue or its original typed refusal.
 */
[[nodiscard]] std::expected<mux::MuxQueue, mux::QueueFailure> createReceivingQueue(const ReceivingProfile &profile,
                                                                                   mux::QueueWork &work);
} // namespace org::ttldtor::qdcxx::api::detail
