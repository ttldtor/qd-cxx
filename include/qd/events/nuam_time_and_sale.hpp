// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/time_and_sale.hpp"

namespace org::ttldtor::qdcxx::events {
/** Owning Nuam sale report with the matching engine's match identifier. */
struct NuamTimeAndSale : TimeAndSale {
    std::int64_t matchId = 0; ///< Matching engine identifier, with Java long bit semantics.

    /** @return Exact matching engine identifier. */
    [[nodiscard]] std::int64_t getMatchId() const noexcept {
        return matchId;
    }
};

/** Construct the standard composite NuamTimeAndSale record.
 * @param limits Finite complete schema allowances.
 * @return Owned schema or a typed construction refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError>
defaultNuamTimeAndSaleSchema(core::SchemaLimits limits);

/** Immutable complete-shape mapper for an indexed Nuam sale report. */
class PreparedNuamTimeAndSale {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;
    char16_t recordExchange_;
    PreparedNuamTimeAndSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout,
                            char16_t exchange) noexcept;

    public:
    /** Bind all required Nuam sale properties and optional text fields.
     * @param schema Validated schema borrowed only during preparation.
     * @param localId Exact local record identity.
     * @param limits Finite preparation allowances.
     * @return Owned plan or a typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedNuamTimeAndSale, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Origin and context, valid while this plan remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Map owning input into an independent event.
     * @param symbol Source symbol borrowed through return.
     * @param values Complete context-tagged values.
     * @param limits Finite output text allowances.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed transaction and snapshot flags.
     * @param indexedRemovalTime Coordinate of an indexed removal, when present.
     * @return Owned event or a typed refusal.
     */
    [[nodiscard]] std::expected<NuamTimeAndSale, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0,
        std::int32_t eventFlags = 0, std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Map borrowed input without retaining source storage.
     * @param symbol Source symbol borrowed through return.
     * @param values Complete context-tagged borrowed values.
     * @param limits Finite output text allowances.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed transaction and snapshot flags.
     * @param indexedRemovalTime Coordinate of an indexed removal, when present.
     * @return Owned event or a typed refusal.
     */
    [[nodiscard]] std::expected<NuamTimeAndSale, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
