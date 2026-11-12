// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/indexed_event_source.hpp"
#include "qd/events/market_enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owning indexed option-sale event for one underlying symbol. */
struct OptionSale {
    std::u16string symbol;                                ///< Exact underlying symbol.
    std::int64_t eventTime = 0;                           ///< Independent FIELD Unix milliseconds.
    std::int64_t index = 0;                               ///< Signed business Index, distinct from timeSequence.
    std::int32_t eventFlags = 0;                          ///< Indexed snapshot, transaction and removal flags.
    std::int64_t timeSequence = 0;                        ///< Signed Time seconds and raw unsigned Sequence words.
    std::int32_t timeNanoPart = 0;                        ///< Nanosecond part or zero when absent.
    char16_t exchangeCode = 0;                            ///< Exact low UTF-16 unit from ExchangeCode.
    core::WideDecimal price{0};                           ///< Sale price, or NaN.
    core::WideDecimal size{0};                            ///< Sale size, or NaN.
    core::WideDecimal bidPrice{0};                        ///< Current bid price, or NaN.
    core::WideDecimal askPrice{0};                        ///< Current ask price, or NaN.
    std::optional<std::u16string> exchangeSaleConditions; ///< Nullable decoded Java short string.
    std::int32_t flags = 0;                               ///< Raw business flags; independent of eventFlags.
    core::WideDecimal underlyingPrice{0};                 ///< Underlying price, or NaN.
    core::WideDecimal volatility{0};                      ///< Implied volatility, or NaN.
    core::WideDecimal delta{0};                           ///< Option delta, or NaN.
    std::optional<std::u16string> optionSymbol;           ///< Owned nullable option symbol; empty differs from null.
    std::optional<std::array<detail::BusinessNumericSnapshot, 7>> businessNumericSnapshot{};
    ///< Inline source doubles and matching wide validity keys; no source payload is borrowed.

    /** @return Process-lifetime DEFAULT source with ID zero. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Signed business Index independent of the time sequence. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Complete business time/sequence word. */
    [[nodiscard]] std::int64_t getTimeSequence() const noexcept;
    /** @return Business Unix milliseconds, including all ten raw millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Combined business milliseconds and raw nanosecond part, with Java signed wrapping. */
    [[nodiscard]] std::int64_t getTimeNanos() const noexcept;
    /** @return Raw nanosecond part. */
    [[nodiscard]] std::int32_t getTimeNanoPart() const noexcept;
    /** @return Low 22 sequence bits from timeSequence. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Exact exchange-code UTF-16 unit. */
    [[nodiscard]] char16_t getExchangeCode() const noexcept;
    /** @return Business sale price, including NaN when unavailable. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Business sale size, including NaN when unavailable. */
    [[nodiscard]] double getSize() const noexcept;
    /** @return Business bid price, including NaN when unavailable. */
    [[nodiscard]] double getBidPrice() const noexcept;
    /** @return Business ask price, including NaN when unavailable. */
    [[nodiscard]] double getAskPrice() const noexcept;
    /** @return Business underlying price, including NaN when unavailable. */
    [[nodiscard]] double getUnderlyingPrice() const noexcept;
    /** @return Business implied volatility, including NaN when unavailable. */
    [[nodiscard]] double getVolatility() const noexcept;
    /** @return Business option delta, including NaN when unavailable. */
    [[nodiscard]] double getDelta() const noexcept;
    /** @return Borrowed nullable sale conditions until this event is assigned, moved or destroyed. */
    [[nodiscard]] const std::optional<std::u16string> &getExchangeSaleConditions() const noexcept;
    /** @return Borrowed nullable option symbol until this event is assigned, moved or destroyed. */
    [[nodiscard]] const std::optional<std::u16string> &getOptionSymbol() const noexcept;
    /** @return Raw trade-through-exempt character from the business flags. */
    [[nodiscard]] char16_t getTradeThroughExempt() const noexcept;
    /** @return Raw aggressor-side code from the business flags. */
    [[nodiscard]] Side getAggressorSide() const noexcept;
    /** @return True if the business flags mark a spread leg. */
    [[nodiscard]] bool isSpreadLeg() const noexcept;
    /** @return True if the business flags mark extended trading hours. */
    [[nodiscard]] bool isExtendedTradingHours() const noexcept;
    /** @return True if the business flags mark a valid tick. */
    [[nodiscard]] bool isValidTick() const noexcept;
    /** @return Raw sale type code from the business flags. */
    [[nodiscard]] TimeAndSaleType getType() const noexcept;
    /** @return True for a NEW sale type. */
    [[nodiscard]] bool isNew() const noexcept;
    /** @return True for a CORRECTION sale type. */
    [[nodiscard]] bool isCorrection() const noexcept;
    /** @return True for a CANCEL sale type. */
    [[nodiscard]] bool isCancel() const noexcept;
};

/** Construct the standard OptionSale record with a zero high indexed word.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning schema with standard required fields or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultOptionSaleSchema(core::SchemaLimits limits);

/** Immutable complete-field OptionSale mapping, independent of caller storage. */
class PreparedOptionSale {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;

    /** @param mapping Validated owning field plan.
     * @param layout Exact optional source indexed-time layout.
     */
    explicit PreparedOptionSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Resolve the required sale properties and optional time nanosecond part.
     * @param schema Validated immutable local schema, borrowed during this call.
     * @param localId Exact selected local record ID.
     * @param limits Finite mapping preparation allowances.
     * @return Complete independently owned plan or typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedOptionSale, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed immutable context until this plan is assigned, moved from or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owning input and project independent event storage.
     * @param symbol Exact underlying symbol, borrowed through return.
     * @param values Complete local-order fields and exact immutable context.
     * @param limits Finite symbol, string and total UTF-16 ownership allowances.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Actual indexed delivery flags.
     * @param indexedRemovalTime Exact raw coordinate when validating an indexed removal.
     * @return Independent event or atomic typed refusal.
     */
    [[nodiscard]] std::expected<OptionSale, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0,
        std::int32_t eventFlags = 0, std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Validate borrowed input and project independent event storage.
     * @param symbol Exact underlying symbol, borrowed through return.
     * @param values Complete borrowed local-order fields and immutable context.
     * @param limits Finite symbol, string and total UTF-16 ownership allowances.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Actual indexed delivery flags.
     * @param indexedRemovalTime Exact raw coordinate when validating an indexed removal.
     * @return Independent event or atomic typed refusal; no input storage is retained.
     */
    [[nodiscard]] std::expected<OptionSale, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
