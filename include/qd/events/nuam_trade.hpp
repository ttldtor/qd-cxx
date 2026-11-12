// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/trade.hpp"

namespace org::ttldtor::qdcxx::events {
/** Independently owned Nuam trade and its separately updated statistics. */
struct NuamTrade : Trade {
    std::int64_t tradeStatTime = 0;            ///< Statistics update time in UTC milliseconds.
    core::WideDecimal lastSignificantPrice{0}; ///< Last significant exchange trade price.
    core::WideDecimal lastPriceForAll{0};      ///< Last exchange price for all deals.
    std::int32_t numberOfTrades = 0;           ///< Exchange-reported count of trades.
    core::WideDecimal vwap{0};                 ///< Exchange-reported volume-weighted average price.
    std::optional<std::array<detail::BusinessNumericSnapshot, 3>> nuamNumericSnapshot{};
    ///< Original business-double values and corresponding raw decimal values.

    /** @return Statistics update time in UTC milliseconds. */
    [[nodiscard]] std::int64_t getTradeStatTime() const noexcept;
    /** @return Original last significant price while its raw representation remains unchanged. */
    [[nodiscard]] double getLastSignificantPrice() const noexcept;
    /** @return Original last price for all deals while its raw representation remains unchanged. */
    [[nodiscard]] double getLastPriceForAll() const noexcept;
    /** @return Exchange-reported trade count. */
    [[nodiscard]] std::int32_t getNumberOfTrades() const noexcept;
    /** @return Original VWAP while its raw representation remains unchanged. */
    [[nodiscard]] double getVWAP() const noexcept;
};

/** Construct the default composite NuamTrade schema with native extra statistics fields.
 * @param limits Mandatory finite schema allowances.
 * @return Owned schema or schema construction error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultNuamTradeSchema(core::SchemaLimits limits);

/** Immutable complete-shape NuamTrade field mapping. */
class PreparedNuamTrade {
    EventMapping mapping_;
    char16_t recordExchange_;

    /** @param mapping Complete owned field plan.
     * @param recordExchange Exact regional record exchange, or zero for composite.
     */
    PreparedNuamTrade(EventMapping mapping, char16_t recordExchange);

    public:
    /** Bind required statistics and supported Trade keys and physical representations.
     * @param schema Complete validated schema, borrowed only during construction.
     * @param localId Explicit ordinary local record identity.
     * @param limits Mandatory finite mapping allowances.
     * @return Owned mapping or validation, resource or allocation error.
     */
    [[nodiscard]] static std::expected<PreparedNuamTrade, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed origin and record context valid while this plan is alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Map a complete owning record.
     * @param symbol Physical symbol, borrowed during the call.
     * @param values Complete context-tagged local fields, borrowed during the call.
     * @param symbolUnits Maximum result symbol length.
     * @param eventTime Independent metadata milliseconds.
     * @return Owned event or context, shape, resource or allocation error.
     */
    [[nodiscard]] std::expected<NuamTrade, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                             std::size_t symbolUnits, std::int64_t eventTime = 0) const;
    /** Map complete borrowed fields without retaining them.
     * @param symbol Physical symbol, borrowed during the call.
     * @param values Complete context-tagged field views, borrowed during the call.
     * @param symbolUnits Maximum result symbol length.
     * @param eventTime Independent metadata milliseconds.
     * @return Owned event or context, shape, resource or allocation error.
     */
    [[nodiscard]] std::expected<NuamTrade, MappingError> mapView(std::u16string_view symbol,
                                                                 BorrowedRecordValueView values,
                                                                 std::size_t symbolUnits,
                                                                 std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
