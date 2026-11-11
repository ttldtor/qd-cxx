// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/event_projection.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <variant>

namespace org::ttldtor::qdcxx::api {
/** Supported business projections; custom records remain part of the complete generic schema. */
enum class EventType {
    QUOTE,                  ///< Existing schema-bound Quote adapter.
    PROFILE,                ///< Existing schema-bound Profile adapter.
    TRADE,                  ///< Schema-bound composite or regional Trade adapter.
    TRADE_ETH,              ///< Distinct extended-hours TradeETH adapter, including applicable regional records.
    SUMMARY,                ///< Complete owning trading-session summary.
    TEXT_MESSAGE,           ///< Owning message with nullable exact text.
    TEXT_CONFIGURATION,     ///< Owning lasting configuration with nullable exact text.
    MESSAGE = 7,            ///< Exact owning serialized Message attachment; STREAM only.
    CONFIGURATION = 8,      ///< Owning serialized Configuration with independent signed version metadata.
    ORDER = 9,              ///< Owning indexed Order with an explicit native or derived projection family.
    MARKET_MAKER = 10,      ///< Owning indexed MarketMaker; ordinary reception requires HISTORY.
    SPREAD_ORDER = 11,      ///< Owning indexed SpreadOrder; reception requires HISTORY.
    ANALYTIC_ORDER = 12,    ///< Owning indexed AnalyticOrder; reception requires HISTORY.
    OTC_MARKETS_ORDER = 13, ///< Owning indexed OtcMarketsOrder; reception requires HISTORY.
    ORDER_IMBALANCE = 14    ///< Owning source-specific OrderImbalance; reception requires TICKER.
};

/** Explicit source of optional event metadata, independent of indexed record identity. */
enum class EventTimePolicy {
    UNSPECIFIED,   ///< Do not assert an event timestamp.
    FIELD_METADATA ///< Use original FIELD metadata when present; otherwise remain unspecified.
};

/** One explicit schema-bound route; several different records may project the same business type. */
struct EventRegistration {
    EventType type;             ///< Actual supported projection, never inferred from a record name.
    std::int32_t localRecordId; ///< Explicit dense record position in the supplied validated schema.
    events::MappingLimits mappingLimits{128, 256, 32, 64, 160, 128, 20480, 256 * 64 * 129 + 32 * 128};
    ///< Finite per-route preparation bounds for existing exact-key and exact-type validation.
    EventTimePolicy eventTimePolicy = EventTimePolicy::FIELD_METADATA;    ///< Source of optional event metadata.
    events::EventProjection projection = events::EventProjection::NATIVE; ///< Explicit physical-to-business family.
};

/** Independent finite bounds for one immutable registry, excluding caller inputs and allocator bookkeeping. */
struct EventRegistryLimits {
    core::SchemaLimits schema{16, 128, 256, 128, 16384}; ///< Complete retained schema, including generic records.
    std::size_t registrations =
        16; ///< Maximum explicit routes; each physical record permits one native plus one compatible derived family.
    std::size_t storageBytes = 8 * 1024 * 1024; ///< Schema/container/shared allocation-request bytes, not process RSS.
};

/** Explicit preparation credits; attempted work is consumed on failure and never replenished internally. */
struct EventRegistryWork {
    std::size_t visits = 1000000;        ///< Schema element/name-unit inspections and route/duplicate inspections.
    std::size_t preparation = 100000000; ///< Sum of each attempted mapper's declared count/unit/comparison bounds.
};

/** Registry-local refusal, separate from existing schema and mapper errors. */
enum class EventRegistryError {
    INVALID_INPUT,     ///< Invalid schema origin, event enum, timestamp policy, record ID or duplicate record route.
    RESOURCE_LIMIT,    ///< Finite route/storage bound or checked arithmetic exceeded.
    WORK_LIMIT,        ///< Caller preparation credits exhausted before the attempted operation.
    ALLOCATION_FAILURE ///< Candidate registry storage could not be allocated; no partial result escapes.
};

/** Atomic registry refusal retaining the original schema/mapper category. */
struct EventRegistryFailure {
    std::variant<EventRegistryError, core::SchemaError, events::MappingError> cause; ///< Exact original category.
    std::optional<std::size_t> registrationIndex; ///< Failing input route; absent for schema/aggregate refusal.
};

/** Immutable owning schema and validated typed routes, with no transport, callback or process-global state.
 * Copies share immutable storage and exact schema identity without allocation. Independent create calls from
 * the same validated schema preserve that origin; independently recreated equal schemas remain distinct.
 * Concurrent const inspection is supported. A moved-from value permits valid(), storageBytes(), destruction or
 * assignment.
 */
class EventRegistry final {
    struct State;
    std::shared_ptr<const State> state_;

    /** Adopt a complete immutable candidate.
     * @param state Independently owned complete schema and route storage.
     */
    explicit EventRegistry(std::shared_ptr<const State> state) noexcept;

    public:
    /** Validate every route before publishing any immutable registry.
     * Existing adapters accept their documented exact lookup keys, optional fields and full type alternatives.
     * Caller aliases and extra fields/records are preserved; no names or conversions are inferred.
     * @param schema Validated complete input, borrowed during this call and bounded-copied with its identity.
     * @param registrations Explicit routes, borrowed during this call; distinct records may share a type.
     * @param limits Finite complete schema, route and retained-storage allowances; zero is a real bound.
     * @param work Remaining preparation credits, debited for attempted work including refusals.
     * @return Immutable owning registry or original typed error; no callback, native state or network work starts.
     */
    [[nodiscard]] static std::expected<EventRegistry, EventRegistryFailure>
    create(const core::LocalSchema &schema, std::span<const EventRegistration> registrations,
           EventRegistryLimits limits, EventRegistryWork &work);

    /** Prepare the nine standard native routes through the normal registration boundary.
     * @param limits Finite complete schema, route and retained-storage allowances.
     * @param work Remaining preparation credits; attempted construction consumes them on refusal.
     * @return Immutable complete default registry or original schema/mapper/registry error.
     */
    [[nodiscard]] static std::expected<EventRegistry, EventRegistryFailure> builtIn(EventRegistryLimits limits,
                                                                                    EventRegistryWork &work);

    /** Inspect whether this value owns an immutable registry.
     * @return False only after ownership has been moved from this value.
     */
    [[nodiscard]] bool valid() const noexcept;
    /** Inspect all retained declarations, including unregistered generic records and extra fields.
     * @return Immutable schema reference, valid while this value or a copy keeps its storage alive; requires valid().
     */
    [[nodiscard]] const core::LocalSchema &schema() const noexcept;
    /** Inspect explicit routes in caller order without allocation or mutable registration access.
     * @return Immutable borrowed routes, valid while an owning copy lives; requires valid().
     */
    [[nodiscard]] std::span<const EventRegistration> registrations() const noexcept;
    /** Inspect the already prepared optional TICKER version field for one registered record.
     * @param localRecordId Explicit local record identity.
     * @return Selected field index, or empty for an unregistered record or absent version; requires valid().
     */
    [[nodiscard]] std::optional<std::size_t> versionField(std::int32_t localRecordId) const noexcept;
    /** Inspect conservative retained storage, including schema/route capacities and shared control metadata.
     * @return Logical allocation-request bytes; excludes allocator bookkeeping and independently created copies.
     */
    [[nodiscard]] std::size_t storageBytes() const noexcept;
};
} // namespace org::ttldtor::qdcxx::api
