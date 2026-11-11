// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/order.hpp"

namespace org::ttldtor::qdcxx::events {
/** Owning OTC quotation with independent indexed identity, ordinary order fields and OTC flags. */
struct OtcMarketsOrder {
    Order order;                       ///< Common owning order properties, including source and delivery flags.
    std::int32_t quoteAccessPayment{}; ///< Signed access payment: positive rebate, negative fee, zero neither.
    std::int32_t otcMarketsFlags{};    ///< Full raw OTC flags, independent of the common flags.

    /** @return Indexed identity of this OTC order. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Exact source ID extracted from the current index. */
    [[nodiscard]] std::int32_t getSourceId() const noexcept;
    /** @return Independently owned source resolved from the current index.
     * @throws std::invalid_argument A manually changed index contains an invalid source.
     * @throws std::bad_alloc Source name allocation fails.
     */
    [[nodiscard]] OrderSource getSource() const;
    /** @return Raw signed payment without clipping to the customary -30..30 range. */
    [[nodiscard]] std::int32_t getQuoteAccessPayment() const noexcept;
    /** @return Whether the quotation is open. */
    [[nodiscard]] bool isOpen() const noexcept;
    /** @return Whether this quotation represents unsolicited customer interest. */
    [[nodiscard]] bool isUnsolicited() const noexcept;
    /** @return Quotation price type; reserved code 3 maps to UNPRICED. */
    [[nodiscard]] OtcMarketsPriceType getOtcMarketsPriceType() const noexcept;
    /** @return Whether displayed size is saturated. */
    [[nodiscard]] bool isSaturated() const noexcept;
    /** @return Whether the quotation is eligible for automatic execution. */
    [[nodiscard]] bool isAutoExecution() const noexcept;
    /** @return Whether the quotation is conditional under the NMS restrictions. */
    [[nodiscard]] bool isNmsConditional() const noexcept;
};

/** Build the default OTC indexed logical records for DEFAULT and pink sources.
 * @param limits Complete finite schema allowances.
 * @return Owning standard records with indexed low-half roles, or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError>
defaultOtcMarketsOrderSchema(core::SchemaLimits limits);

/** Immutable complete-field mapper for the exact OTC order family. */
class PreparedOtcMarketsOrder {
    EventMapping mapping_;
    std::int32_t sourceId_;
    /** @param mapping Complete independently owned selection state.
     * @param sourceId Validated exact record source ID.
     */
    PreparedOtcMarketsOrder(EventMapping mapping, std::int32_t sourceId) noexcept;

    public:
    /** Select required common and OTC-specific fields from the exact named record.
     * @param schema Validated source, borrowed only during the call.
     * @param localId Explicit local record ID.
     * @param limits Finite schema shape, selection, name and comparison allowances.
     * @return Complete independently owned mapper or original refusal.
     */
    [[nodiscard]] static std::expected<PreparedOtcMarketsOrder, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed context, valid until assignment, move-from or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Exact validated source ID, independent of publication capabilities. */
    [[nodiscard]] std::int32_t sourceId() const noexcept;
    /** Validate complete owning input and project an owning OTC event.
     * @param symbol Exact unsuffixed event symbol, borrowed through return.
     * @param values Complete canonical local-order fields and context.
     * @param limits Finite owned symbol and string bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed delivery flags, separate from business and OTC flags.
     * @return Complete event or original validation/resource/allocation refusal.
     */
    [[nodiscard]] std::expected<OtcMarketsOrder, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                                   EventValueLimits limits, std::int64_t eventTime = 0,
                                                                   std::int32_t eventFlags = 0) const;
    /** Validate complete borrowed input and project an owning OTC event.
     * @param symbol Exact unsuffixed event symbol, borrowed through return.
     * @param values Complete canonical local-order fields and context, borrowed through return.
     * @param limits Finite owned symbol and string bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed delivery flags, separate from business and OTC flags.
     * @return Complete event or original refusal; no input storage is retained.
     */
    [[nodiscard]] std::expected<OtcMarketsOrder, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
