// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/order.hpp"

namespace org::ttldtor::qdcxx::events {
/** Complete owning analytic order, retaining all ordinary Order fields without slicing. */
struct AnalyticOrder : Order {
    core::WideDecimal icebergPeakSize{0};     ///< Optional iceberg peak size; absent NaN.
    core::WideDecimal icebergHiddenSize{0};   ///< Optional iceberg hidden size; absent NaN.
    core::WideDecimal icebergExecutedSize{0}; ///< Optional iceberg executed size; absent NaN.
    std::int32_t icebergFlags = 0;            ///< Complete raw flags, including reserved and unrecognized bits.
    std::optional<std::array<detail::BusinessNumericSnapshot, 3>> icebergNumericSnapshot{};
    ///< Independent direct source doubles with matching wide keys; inherited snapshots remain separate.
    /** @return Direct source peak size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getIcebergPeakSize() const noexcept;
    /** @return Direct source hidden size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getIcebergHiddenSize() const noexcept;
    /** @return Direct source executed size while its wide key matches, otherwise the current wide value. */
    [[nodiscard]] double getIcebergExecutedSize() const noexcept;
    /** @return Low-two-bit iceberg type, mapping reserved code three to UNDEFINED without modifying flags. */
    [[nodiscard]] IcebergType getIcebergType() const noexcept;
};

/** Construct the standard native DEFAULT AnalyticOrder logical projection.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning indexed record with standard Count/MMID and absent optional iceberg fields, or schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultAnalyticOrderSchema(core::SchemaLimits limits);

/** Immutable complete-field analytic mapper, independent of source schema and cursor lifetime. */
class PreparedAnalyticOrder {
    EventMapping mapping_;
    std::int32_t sourceId_;
    /** @param mapping Independently owned complete shape and selections.
     * @param sourceId Validated exact record source identity.
     */
    PreparedAnalyticOrder(EventMapping mapping, std::int32_t sourceId) noexcept;

    public:
    /** Resolve required/optional fields and ordinary source without imposing publication capabilities.
     * @param schema Validated immutable source borrowed only through return.
     * @param localId Explicit selected record identity.
     * @param limits Finite shape, key, type, name and comparison allowances.
     * @return Complete owning preparation or atomic typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedAnalyticOrder, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Immutable context borrowed until assignment, move-from or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Validated ordinary source ID, independent of source publication capabilities. */
    [[nodiscard]] std::int32_t sourceId() const noexcept;
    /** Produce a complete deeply owning analytic value after complete canonical validation.
     * @param symbol Exact borrowed event symbol.
     * @param values Complete local-order values with exact immutable context.
     * @param limits Symbol, per-string and total owned UTF-16 bounds.
     * @param eventTime Independent FIELD milliseconds, default zero.
     * @param eventFlags Indexed delivery flags, separate from both business flag fields.
     * @return Owning exact variant or original typed refusal; no source references survive.
     */
    [[nodiscard]] std::expected<AnalyticOrder, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                                 EventValueLimits limits, std::int64_t eventTime = 0,
                                                                 std::int32_t eventFlags = 0) const;
    /** Project callback-borrowed canonical input; all payload views are copied before return.
     * @param symbol Exact borrowed event symbol.
     * @param values Complete callback-borrowed local values with exact immutable context.
     * @param limits Symbol, per-string and total owned UTF-16 bounds.
     * @param eventTime Independent FIELD milliseconds, default zero.
     * @param eventFlags Actual indexed delivery flags.
     * @return Owning exact variant or original typed refusal, independent of cursor lifetime.
     */
    [[nodiscard]] std::expected<AnalyticOrder, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
