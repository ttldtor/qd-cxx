// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/trade.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Independently owned TradeETH; decimal raw and source business-double views are separate. */
struct TradeETH {
    std::u16string symbol;            ///< Exact logical symbol, including a regional record suffix.
    std::int64_t eventTime = 0;       ///< Explicit FIELD metadata milliseconds, independent of business time.
    std::int64_t timeSequence = 0;    ///< Signed seconds in high32 and complete packed Sequence in low32.
    std::int32_t timeNanoPart = 0;    ///< Raw signed nanosecond remainder; not clamped.
    char16_t exchangeCode = 0;        ///< Low UTF-16 unit of ETHLast.Exchange, or the record suffix if absent.
    std::int64_t tradeId = 0;         ///< Raw two-slot TradeId bits, or zero when absent.
    core::WideDecimal price{0};       ///< Generated wide-decimal view of ETHLast.Price.
    core::WideDecimal size{0};        ///< Generated wide-decimal view of ETHLast.Size.
    core::WideDecimal change{0};      ///< Generated wide-decimal view of ETHLast.Change; absent is NaN.
    std::int32_t dayId = 0;           ///< Signed days since 1970-01-01, without calendar conversion.
    core::WideDecimal dayVolume{0};   ///< Generated wide-decimal view of ETHVolume; absent is NaN.
    core::WideDecimal dayTurnover{0}; ///< Generated wide-decimal view of ETHDayTurnover; absent is NaN.
    std::int32_t flags = 0;           ///< Complete ETHLast.Flags; no Tick fallback or forced ETH bit.
    std::optional<std::array<detail::BusinessNumericSnapshot, 5>> businessNumericSnapshot{};
    ///< Inline owned source values; each raw edit independently selects the current decimal view instead.

    /** @return Signed business milliseconds, including the unsigned packed ten-bit millisecond component. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Business nanoseconds with Java long wrapping and the unmodified signed nano part. */
    [[nodiscard]] std::int64_t getTimeNanos() const noexcept;
    /** @return Low22 Sequence bits, without uniqueness or monotonicity guarantees. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Source business price while price.raw matches its snapshot; otherwise the current raw view. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Source fractional size while size.raw matches its snapshot; otherwise the current raw view. */
    [[nodiscard]] double getSizeAsDouble() const noexcept;
    /** @return Java binary64-to-long size: truncate, saturate, and map NaN to zero. */
    [[nodiscard]] std::int64_t getSize() const noexcept;
    /** @return Source business change while change.raw matches its snapshot; otherwise the current raw view. */
    [[nodiscard]] double getChange() const noexcept;
    /** @return Source fractional day volume while its raw snapshot matches; otherwise the current raw view. */
    [[nodiscard]] double getDayVolumeAsDouble() const noexcept;
    /** @return Java binary64-to-long day volume: truncate, saturate, and map NaN to zero. */
    [[nodiscard]] std::int64_t getDayVolume() const noexcept;
    /** @return Source business turnover while its raw snapshot matches; otherwise the current raw view. */
    [[nodiscard]] double getDayTurnover() const noexcept;
    /** @return Direction encoded by flags bits1..3; reserved6/7 return UNDEFINED. */
    [[nodiscard]] Direction getTickDirection() const noexcept;
    /** @return Whether flags bit0 reports an extended-hours last trade. */
    [[nodiscard]] bool isExtendedTradingHours() const noexcept;
};

/** Construct an explicit complete native composite TradeETH schema, including the optional nano part.
 * @param limits Mandatory finite complete schema allowances, including zero.
 * @return Owned ordinary TradeETH record0 with twelve logical fields and documented lookup aliases, or error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultTradeETHSchema(core::SchemaLimits limits);

/** Immutable complete-shape TradeETH mapping, retaining no source values or transport. */
class PreparedTradeETH {
    EventMapping mapping_;
    char16_t recordExchange_;

    /** @param mapping Complete owned field plan.
     * @param recordExchange Exact syntactic record exchange, or zero for composite.
     */
    PreparedTradeETH(EventMapping mapping, char16_t recordExchange);

    public:
    /** Bind exact TradeETH keys and supported physical representations.
     * @param schema Validated complete caller schema; no declarations are borrowed after return.
     * @param localId Explicit ordinary local record identity.
     * @param limits Finite name, shape, key, selection, type and comparison allowances.
     * @return Atomic owned mapping or original mapping/resource/allocation error.
     */
    [[nodiscard]] static std::expected<PreparedTradeETH, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Immutable exact origin/record context, borrowed while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate all fields and produce a complete independent event.
     * @param symbol Physical QD symbol; regional output restores the record exchange.
     * @param values Context-tagged complete local-order fields, unchanged during the call.
     * @param symbolUnits Finite maximum result UTF-16 symbol units.
     * @param eventTime Explicit independent FIELD metadata milliseconds, default zero.
     * @return Owning TradeETH or original context/shape/resource/allocation error; no partial result escapes.
     */
    [[nodiscard]] std::expected<TradeETH, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                            std::size_t symbolUnits, std::int64_t eventTime = 0) const;
    /** Project borrowed values without intermediate owning fields.
     * @param symbol Physical QD symbol, unchanged during the call.
     * @param values Complete context-tagged borrowed fields, alive through this synchronous call.
     * @param symbolUnits Finite maximum result symbol units.
     * @param eventTime Explicit independent metadata milliseconds, default zero.
     * @return Complete independent TradeETH or typed error; nothing retains the input views.
     */
    [[nodiscard]] std::expected<TradeETH, MappingError> mapView(std::u16string_view symbol,
                                                                BorrowedRecordValueView values, std::size_t symbolUnits,
                                                                std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
