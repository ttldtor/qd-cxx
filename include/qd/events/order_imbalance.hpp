// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/market_enums.hpp"
#include "qd/events/order_source.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owning source-specific auction imbalance; one latest value per symbol/source in TICKER. */
struct OrderImbalance {
    std::u16string symbol;              ///< Exact owning event symbol, without a source suffix.
    std::int64_t eventTime{};           ///< Independent FIELD milliseconds, zero when unavailable.
    std::int32_t sourceId{};            ///< Validated named-record source ID, default zero.
    std::int64_t timeSequence{};        ///< Signed Time high32 and complete Sequence low32.
    core::WideDecimal refPrice{0};      ///< Reference price; raw zero denotes NaN.
    core::WideDecimal pairedSize{0};    ///< Auction matched quantity; raw zero denotes NaN.
    core::WideDecimal imbalanceSize{0}; ///< Auction unmatched quantity; raw zero denotes NaN.
    core::WideDecimal nearPrice{0};     ///< Optional near clearing price, absent NaN.
    core::WideDecimal farPrice{0};      ///< Optional far clearing price, absent NaN.
    std::int32_t flags{};               ///< Complete auction/side business flags, including reserved bits.
    std::optional<std::array<detail::BusinessNumericSnapshot, 5>> businessNumericSnapshot{};
    ///< Inline direct business doubles valid while the corresponding raw decimal is unchanged.

    /** @return Current named source ID. */
    [[nodiscard]] std::int32_t getSourceId() const noexcept;
    /** @return Independently owned source resolved from the current ID.
     * @throws std::invalid_argument A manually changed source ID cannot be resolved.
     * @throws std::bad_alloc Source name allocation fails.
     */
    [[nodiscard]] OrderSource getSource() const;
    /** @return Business time in Unix milliseconds, independent of FIELD eventTime. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Business sequence from the low 22 bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Current business reference price. */
    [[nodiscard]] double getRefPrice() const noexcept;
    /** @return Current business paired size. */
    [[nodiscard]] double getPairedSize() const noexcept;
    /** @return Current business imbalance size. */
    [[nodiscard]] double getImbalanceSize() const noexcept;
    /** @return Current business near clearing price, NaN when absent. */
    [[nodiscard]] double getNearPrice() const noexcept;
    /** @return Current business far clearing price, NaN when absent. */
    [[nodiscard]] double getFarPrice() const noexcept;
    /** @return Side from flags bits 0..2; reserved codes map to UNDEFINED. */
    [[nodiscard]] ImbalanceSide getImbalanceSide() const noexcept;
    /** @return Auction type from flags bits 3..5; reserved codes map to UNDEFINED. */
    [[nodiscard]] AuctionType getAuctionType() const noexcept;
};

/** Build the default TICKER logical records for DEFAULT, NTV, ARCA and NUAM sources.
 * @param limits Complete finite schema allowances.
 * @return Owning standard records, or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError>
defaultOrderImbalanceSchema(core::SchemaLimits limits);

/** Immutable mapper for exact source-specific auction imbalance records. */
class PreparedOrderImbalance {
    EventMapping mapping_;
    std::int32_t sourceId_;
    /** @param mapping Complete independently owned selection state.
     * @param sourceId Validated exact record source ID.
     */
    PreparedOrderImbalance(EventMapping mapping, std::int32_t sourceId) noexcept;

    public:
    /** Select required and optional imbalance fields from the exact named record.
     * @param schema Validated source, borrowed only during the call.
     * @param localId Explicit local record ID.
     * @param limits Finite schema shape, selection, name and comparison allowances.
     * @return Complete independently owned mapper or original refusal.
     */
    [[nodiscard]] static std::expected<PreparedOrderImbalance, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed context, valid until assignment, move-from or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Exact validated source ID, independent of publication capabilities. */
    [[nodiscard]] std::int32_t sourceId() const noexcept;
    /** Validate complete owning input and project an owning auction imbalance.
     * @param symbol Exact unsuffixed event symbol, borrowed through return.
     * @param values Complete canonical local-order fields and context.
     * @param limits Finite owned symbol and string bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @return Complete event or original validation/resource/allocation refusal.
     */
    [[nodiscard]] std::expected<OrderImbalance, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0) const;
    /** Validate complete borrowed input and project an owning auction imbalance.
     * @param symbol Exact unsuffixed event symbol, borrowed through return.
     * @param values Complete canonical local-order fields and context, borrowed through return.
     * @param limits Finite owned symbol and string bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @return Complete event or original refusal; no input storage is retained.
     */
    [[nodiscard]] std::expected<OrderImbalance, MappingError> mapView(std::u16string_view symbol,
                                                                      BorrowedRecordValueView values,
                                                                      EventValueLimits limits,
                                                                      std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
