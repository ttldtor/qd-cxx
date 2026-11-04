// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/market_enums.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Complete independent trading-session summary, with separate wide and source business-double views. */
struct Summary {
    std::u16string symbol;              ///< Exact owning logical symbol, including a regional suffix.
    std::int64_t eventTime = 0;         ///< Independent FIELD metadata milliseconds; no business time is synthesized.
    std::int32_t dayId = 0;             ///< Raw signed days since 1970-01-01, without calendar validation.
    core::WideDecimal dayOpenPrice{0};  ///< Wide-accessor open price; raw zero is NaN.
    core::WideDecimal dayHighPrice{0};  ///< Wide-accessor high price.
    core::WideDecimal dayLowPrice{0};   ///< Wide-accessor low price.
    core::WideDecimal dayClosePrice{0}; ///< Optional wide-accessor close price, absent NaN.
    std::int32_t prevDayId = 0;         ///< Raw signed previous-day identifier.
    core::WideDecimal prevDayClosePrice{0}; ///< Wide-accessor previous close price.
    core::WideDecimal prevDayVolume{0};     ///< Optional wide-accessor previous volume, absent NaN.
    std::int64_t openInterest =
        0; ///< Full production mapping-long value; pinned delegate loss corrected (ADR-0010); absent zero.
    std::int32_t flags = 0; ///< Complete business flags, including unrecognized high bits.
    std::optional<std::array<detail::BusinessNumericSnapshot, 6>> businessNumericSnapshot{};
    ///< Inline source doubles and raw validity keys; no schema, cursor or separate allocation is retained.

    /** @return Raw signed day identifier; no date formatting or allocation. */
    [[nodiscard]] std::int32_t getDayId() const noexcept;
    /** @return Raw signed previous-day identifier. */
    [[nodiscard]] std::int32_t getPrevDayId() const noexcept;
    /** @return Current public business open interest; mapping uses actual getAsLong semantics per ADR-0010. */
    [[nodiscard]] std::int64_t getOpenInterest() const noexcept;
    /** @return Source business open price while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getDayOpenPrice() const noexcept;
    /** @return Source business high price while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getDayHighPrice() const noexcept;
    /** @return Source business low price while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getDayLowPrice() const noexcept;
    /** @return Source business close price while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getDayClosePrice() const noexcept;
    /** @return Source business previous close while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getPrevDayClosePrice() const noexcept;
    /** @return Source business previous volume while its raw snapshot matches, otherwise the current wide view. */
    [[nodiscard]] double getPrevDayVolume() const noexcept;
    /** @return PriceType from business flags bits2..3, preserving the stored complete word. */
    [[nodiscard]] PriceType getDayClosePriceType() const noexcept;
    /** @return PriceType from business flags bits0..1, preserving the stored complete word. */
    [[nodiscard]] PriceType getPrevDayClosePriceType() const noexcept;
};

/** Construct an explicit complete native composite Summary record0.
 * @param limits Mandatory finite schema allowances, including zero.
 * @return Owned ten-field binary schema with exact local/property keys, or original schema error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultSummarySchema(core::SchemaLimits limits);

/** Immutable complete-shape mapper; no source fields or schema containers are borrowed. */
class PreparedSummary {
    EventMapping mapping_;
    char16_t recordExchange_;
    /** @param mapping Complete owned immutable field selections.
     * @param recordExchange Syntactic exchange suffix, zero for composite.
     */
    PreparedSummary(EventMapping mapping, char16_t recordExchange);

    public:
    /** Bind the actual Summary getter profile before any values are projected.
     * @param schema Validated complete caller declarations; not retained by reference.
     * @param localId Explicit local record identity.
     * @param limits Finite selection/type/name/comparison allowances.
     * @return Atomic owned mapper or original mapping/resource/allocation refusal.
     */
    [[nodiscard]] static std::expected<PreparedSummary, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Exact borrowed record context while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owned local-order fields and publish an independent Summary.
     * @param symbol Physical symbol, alive only through the call; regional output restores the record suffix.
     * @param values Complete exact context/count/type/shape input, including unselected extras.
     * @param symbolUnits Finite maximum owned output symbol units.
     * @param eventTime Independent FIELD milliseconds, default zero.
     * @return Complete owning event or original refusal; no partial event escapes.
     */
    [[nodiscard]] std::expected<Summary, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                           std::size_t symbolUnits, std::int64_t eventTime = 0) const;
    /** Validate complete borrowed local-order fields and publish an independent Summary.
     * @param symbol Physical symbol, unchanged during the call.
     * @param values Complete borrowed fields, alive and unchanged through return.
     * @param symbolUnits Finite maximum owned output symbol units.
     * @param eventTime Independent FIELD milliseconds, default zero.
     * @return Complete owning event or original refusal; no borrowed payload survives.
     */
    [[nodiscard]] std::expected<Summary, MappingError> mapView(std::u16string_view symbol,
                                                               BorrowedRecordValueView values, std::size_t symbolUnits,
                                                               std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
