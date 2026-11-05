// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/market_enums.hpp"
#include "qd/events/order_index.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Complete owning ordinary Order; indexed identity, business time and FIELD eventTime are independent. */
struct Order {
    std::u16string symbol;             ///< Exact owning event symbol, without a source suffix.
    std::int64_t eventTime = 0;        ///< Independent FIELD milliseconds, zero when not selected.
    std::int64_t index = 0;            ///< Record source high32 plus unsigned wire Index low32.
    std::int32_t eventFlags = 0;       ///< Indexed snapshot/transaction/removal flags, separate from business flags.
    std::int64_t timeSequence = 0;     ///< Raw signed Time high32 and unsigned Sequence low32.
    std::int32_t timeNanoPart = 0;     ///< Raw signed nanosecond remainder.
    std::int64_t actionTime = 0;       ///< Optional action milliseconds, absent zero.
    std::int64_t orderId = 0;          ///< Optional complete raw order ID, absent zero.
    std::int64_t auxOrderId = 0;       ///< Optional complete raw auxiliary order ID, absent zero.
    core::WideDecimal price{0};        ///< Wide-accessor price; raw zero denotes NaN.
    core::WideDecimal size{0};         ///< Wide-accessor size, independent of its direct business-double view.
    core::WideDecimal executedSize{0}; ///< Optional executed size, absent NaN.
    std::int64_t count = 0;            ///< Production getCount signed32 widened to signed64, absent zero.
    std::int32_t flags = 0;            ///< Complete business flags, including unrecognized high bits.
    std::int64_t tradeId = 0;          ///< Optional complete trade ID, absent zero.
    core::WideDecimal tradePrice{0};   ///< Optional execution price, absent NaN.
    core::WideDecimal tradeSize{0};    ///< Optional execution size, absent NaN.
    std::optional<std::u16string> marketMaker; ///< Owned byte-valued SHORT_STRING text, absent or raw zero null.
    std::optional<std::array<detail::BusinessNumericSnapshot, 5>> businessNumericSnapshot{};
    ///< Inline direct source doubles and matching wide validity keys; no source payload is borrowed.
    /** @return Full public indexed identity without allocation. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Source ID extracted from the current public index with OrderBase semantics. */
    [[nodiscard]] std::int32_t getSourceId() const noexcept;
    /** @return Independently owned source resolved from the current index.
     * @throws std::invalid_argument Manually changed index encodes an invalid source.
     * @throws std::bad_alloc Source name allocation fails.
     */
    [[nodiscard]] OrderSource getSource() const;
    /** @return Business Unix milliseconds from Time/Sequence, independent of collector indexing. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Java long-wrapped business nanoseconds, including the raw nano remainder. */
    [[nodiscard]] std::int64_t getTimeNanos() const noexcept;
    /** @return Unsigned low22 business sequence, without uniqueness guarantees. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Source business price while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Source business size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getSizeAsDouble() const noexcept;
    /** @return Java long narrowing of business size: truncate, saturate and map NaN to zero. */
    [[nodiscard]] std::int64_t getSize() const noexcept;
    /** @return True when business size is neither zero nor NaN, including negative values. */
    [[nodiscard]] bool hasSize() const noexcept;
    /** @return Source business executed size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getExecutedSize() const noexcept;
    /** @return Current owning production count value, including negative values. */
    [[nodiscard]] std::int64_t getCount() const noexcept;
    /** @return Source business trade price while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getTradePrice() const noexcept;
    /** @return Source business trade size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getTradeSize() const noexcept;
    /** @return Business exchange code from flags bits4..10. */
    [[nodiscard]] char16_t getExchangeCode() const noexcept;
    /** @return Business side from flags bits2..3, reserved code mapped to UNDEFINED. */
    [[nodiscard]] Side getOrderSide() const noexcept;
    /** @return Business scope from flags bits0..1. */
    [[nodiscard]] Scope getScope() const noexcept;
    /** @return Business action from flags bits11..14, reserved codes mapped to UNDEFINED. */
    [[nodiscard]] OrderAction getAction() const noexcept;
};

/** Construct the standard native default Order logical projection with explicit VOID/Index source mapping.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning indexed default record with standard enabled optional Count/MMID, or schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultOrderSchema(core::SchemaLimits limits);

/** Immutable complete-field Order mapper, independent of source schema and cursor lifetime. */
class PreparedOrder {
    EventMapping mapping_;
    std::int32_t sourceId_;
    /** @param mapping Independently owned complete shape and field selections.
     * @param sourceId Validated exact record source identity.
     */
    PreparedOrder(EventMapping mapping, std::int32_t sourceId) noexcept;

    public:
    /** Resolve exact required/optional fields and record source before projecting values.
     * @param schema Validated immutable source, borrowed only through this call.
     * @param localId Explicit selected record identity; no event-name scan occurs.
     * @param limits Finite shape, selection, key, type, name and comparison bounds.
     * @return Complete independently owned mapping or original atomic refusal.
     */
    [[nodiscard]] static std::expected<PreparedOrder, MappingError> prepare(const core::LocalSchema &schema,
                                                                            std::int32_t localId, MappingLimits limits);
    /** @return Borrowed immutable context until assignment, move-from or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Validated source ID, independent of collector index and source capabilities. */
    [[nodiscard]] std::int32_t sourceId() const noexcept;
    /** Validate complete owning input and produce an independently owned Order.
     * @param symbol Exact symbol, borrowed only during this call.
     * @param values Complete local-order values with exact immutable context.
     * @param limits Symbol, per-string and total owned UTF-16 bounds.
     * @param eventTime Independent selected FIELD milliseconds, default zero.
     * @param eventFlags Actual selected indexed delivery flags, default zero.
     * @return Complete owning event or original context, value, resource or allocation refusal.
     */
    [[nodiscard]] std::expected<Order, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                         EventValueLimits limits, std::int64_t eventTime = 0,
                                                         std::int32_t eventFlags = 0) const;
    /** Validate complete borrowed input and produce an independently owned Order.
     * @param symbol Exact symbol, borrowed only through return.
     * @param values Complete local-order fields, all payload storage alive through return.
     * @param limits Independent finite owned UTF-16 bounds.
     * @param eventTime Independent selected FIELD milliseconds, default zero.
     * @param eventFlags Actual selected indexed delivery flags, default zero.
     * @return Complete owning event or original refusal; no caller payload is retained.
     */
    [[nodiscard]] std::expected<Order, MappingError> mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                             EventValueLimits limits, std::int64_t eventTime = 0,
                                                             std::int32_t eventFlags = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
