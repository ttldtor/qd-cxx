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

/** Owning time-and-sale report, including separate indexed and trade flag words. */
struct TimeAndSale {
    std::u16string symbol;                                ///< Logical event symbol with any regional exchange suffix.
    std::int64_t eventTime = 0;                           ///< Independent FIELD Unix milliseconds, zero when absent.
    std::int32_t eventFlags = 0;                          ///< Indexed snapshot, transaction and removal flags.
    std::int64_t index = 0;                               ///< Signed seconds and unsigned millisecond/sequence word.
    std::int32_t timeNanoPart = 0;                        ///< Optional nanosecond remainder, zero when absent.
    char16_t exchangeCode = 0;                            ///< Actual trade exchange code from the source field.
    std::int64_t tradeId = 0;                             ///< Optional exchange trade identifier, zero when absent.
    core::WideDecimal price{0};                           ///< Trade price or NaN.
    core::WideDecimal size{0};                            ///< Trade size or NaN.
    core::WideDecimal bidPrice{0};                        ///< Bid price or NaN.
    core::WideDecimal askPrice{0};                        ///< Ask price or NaN.
    std::optional<std::u16string> exchangeSaleConditions; ///< Owned decoded source short string.
    std::int32_t flags = 0;                               ///< Raw trade flags, distinct from eventFlags.
    std::optional<std::u16string> buyer;                  ///< Optional owned buyer text.
    std::optional<std::u16string> seller;                 ///< Optional owned seller text.
    std::optional<std::array<detail::BusinessNumericSnapshot, 4>> businessNumericSnapshot{};
    ///< Inline source business values and wide validity keys; no source storage is retained.

    /** @return Process-lifetime DEFAULT indexed source, with ID zero. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Raw indexed-event flags. */
    [[nodiscard]] std::int32_t getEventFlags() const noexcept;
    /** @return Exact signed indexed time/sequence bits. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Business Unix milliseconds from the packed index. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Business nanoseconds with Java long wrap semantics. */
    [[nodiscard]] std::int64_t getTimeNanos() const noexcept;
    /** @return Low 22 sequence bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Nanosecond remainder, zero when absent from the source schema. */
    [[nodiscard]] std::int32_t getTimeNanoPart() const noexcept;
    /** @return Source trade exchange code. */
    [[nodiscard]] char16_t getExchangeCode() const noexcept;
    /** @return Source trade ID, zero when absent from the source schema. */
    [[nodiscard]] std::int64_t getTradeId() const noexcept;
    /** @return Source business price, or the current raw decimal value after mutation. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Source fractional size, or the current raw decimal value after mutation. */
    [[nodiscard]] double getSizeAsDouble() const noexcept;
    /** @return Java binary64-to-long size, truncating and saturating, with NaN converted to zero. */
    [[nodiscard]] std::int64_t getSize() const noexcept;
    /** @return Source business bid price, or the current raw decimal value after mutation. */
    [[nodiscard]] double getBidPrice() const noexcept;
    /** @return Source business ask price, or the current raw decimal value after mutation. */
    [[nodiscard]] double getAskPrice() const noexcept;
    /** @return Borrowed owned exchange sale conditions until this event is modified or destroyed. */
    [[nodiscard]] const std::optional<std::u16string> &getExchangeSaleConditions() const noexcept;
    /** @return Raw eight-bit trade-through-exempt character from the business flags. */
    [[nodiscard]] char16_t getTradeThroughExempt() const noexcept;
    /** @return Side encoded in the business flags, with reserved code 3 mapped to UNDEFINED. */
    [[nodiscard]] Side getAggressorSide() const noexcept;
    /** @return True if the report is a spread leg. */
    [[nodiscard]] bool isSpreadLeg() const noexcept;
    /** @return True if the report is an extended-hours sale. */
    [[nodiscard]] bool isExtendedTradingHours() const noexcept;
    /** @return True if the report is a valid intraday tick. */
    [[nodiscard]] bool isValidTick() const noexcept;
    /** @return Type encoded in the business flags, with reserved code 3 mapped to NEW. */
    [[nodiscard]] TimeAndSaleType getType() const noexcept;
    /** @return Whether the business flags identify a new report. */
    [[nodiscard]] bool isNew() const noexcept;
    /** @return Whether the business flags identify a correction. */
    [[nodiscard]] bool isCorrection() const noexcept;
    /** @return Whether the business flags identify a cancellation. */
    [[nodiscard]] bool isCancel() const noexcept;
    /** @return Borrowed owned buyer text until this event is modified or destroyed. */
    [[nodiscard]] const std::optional<std::u16string> &getBuyer() const noexcept;
    /** @return Borrowed owned seller text until this event is modified or destroyed. */
    [[nodiscard]] const std::optional<std::u16string> &getSeller() const noexcept;
};

/** Construct the standard composite TimeAndSale schema.
 * @param limits Finite complete schema allowances.
 * @return Independent schema with standard enabled fields or a typed refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultTimeAndSaleSchema(core::SchemaLimits limits);

/** Immutable prepared field mapper, independent of the source schema and payload storage. */
class PreparedTimeAndSale {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;
    char16_t recordExchange_;

    /** @param mapping Independently owned validated field plan.
     * @param layout Exact optional indexed source coordinate layout.
     * @param exchange Regional record suffix, or zero for a composite record.
     */
    PreparedTimeAndSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout,
                        char16_t exchange) noexcept;

    public:
    /** Resolve the required and optional Java fields and accepted aliases.
     * @param schema Validated immutable local schema, borrowed through return.
     * @param localId Exact selected local record ID.
     * @param limits Finite preparation allowances.
     * @return Independently owned plan or atomic typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedTimeAndSale, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Immutable record context, valid until this mapper is moved from, assigned or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Project complete owning input into an independent event.
     * @param symbol Physical QD symbol, borrowed through return.
     * @param values Complete context-tagged fields.
     * @param limits Finite symbol, per-string and aggregate UTF-16 allowances.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Indexed snapshot, transaction and removal flags.
     * @param indexedRemovalTime Exact source coordinate for an indexed removal, when present.
     * @return Independent event or atomic typed refusal.
     */
    [[nodiscard]] std::expected<TimeAndSale, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0,
        std::int32_t eventFlags = 0, std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Project complete borrowed input into an independent event.
     * @param symbol Physical QD symbol, borrowed through return.
     * @param values Complete context-tagged views, alive through return.
     * @param limits Finite symbol, per-string and aggregate UTF-16 allowances.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Indexed snapshot, transaction and removal flags.
     * @param indexedRemovalTime Exact source coordinate for an indexed removal, when present.
     * @return Independent event or atomic typed refusal, without retaining input views.
     */
    [[nodiscard]] std::expected<TimeAndSale, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
