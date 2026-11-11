// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/indexed_event_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owning option Greeks values for one symbol and indexed time. */
struct Greeks {
    std::u16string symbol;           ///< Exact event symbol.
    std::int64_t eventTime = 0;      ///< Independent FIELD Unix milliseconds, zero when absent.
    std::int64_t index = 0;          ///< Signed seconds and unsigned sequence words.
    std::int32_t eventFlags = 0;     ///< Indexed snapshot, transaction and removal flags.
    core::WideDecimal price{0};      ///< Option price, or NaN.
    core::WideDecimal volatility{0}; ///< Implied volatility, or NaN.
    core::WideDecimal delta{0};      ///< Price sensitivity to the underlying, or NaN.
    core::WideDecimal gamma{0};      ///< Delta sensitivity to the underlying, or NaN.
    core::WideDecimal theta{0};      ///< Price sensitivity to elapsed time, or NaN.
    core::WideDecimal rho{0};        ///< Price sensitivity to interest rates, or NaN.
    core::WideDecimal vega{0};       ///< Price sensitivity to volatility, or NaN.
    std::optional<std::array<detail::BusinessNumericSnapshot, 7>> businessNumericSnapshot{};
    ///< Inline source doubles and wide validity keys; no source payload is borrowed.

    /** @return Process-lifetime DEFAULT source with ID zero. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Complete signed indexed time/sequence word. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Business Unix milliseconds, including all ten raw millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Low 22 sequence bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Business option price, including NaN when unavailable. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Business implied volatility, including NaN when unavailable. */
    [[nodiscard]] double getVolatility() const noexcept;
    /** @return Business delta, including NaN when unavailable. */
    [[nodiscard]] double getDelta() const noexcept;
    /** @return Business gamma, including NaN when unavailable. */
    [[nodiscard]] double getGamma() const noexcept;
    /** @return Business theta, including NaN when unavailable. */
    [[nodiscard]] double getTheta() const noexcept;
    /** @return Business rho, including NaN when unavailable. */
    [[nodiscard]] double getRho() const noexcept;
    /** @return Business vega, including NaN when unavailable. */
    [[nodiscard]] double getVega() const noexcept;
};

/** Construct the standard Greeks record with Time and Sequence indexed words.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning schema with the standard fields or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultGreeksSchema(core::SchemaLimits limits);

/** Immutable prepared complete-field Greeks mapping, independent of caller storage. */
class PreparedGreeks {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;

    /** @param mapping Validated owning field plan.
     * @param layout Exact optional source indexed-time layout.
     */
    explicit PreparedGreeks(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Resolve the exact optional time words and required numeric properties.
     * @param schema Validated immutable local schema, borrowed during this call.
     * @param localId Exact selected local record ID.
     * @param limits Finite mapping preparation allowances.
     * @return Complete independently owned plan or typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedGreeks, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed immutable context until this plan is assigned, moved from or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owning input and project independent event storage.
     * @param symbol Exact event symbol, borrowed through return.
     * @param values Complete local-order fields and exact immutable context.
     * @param symbolUnits Maximum copied UTF-16 symbol units.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Actual indexed delivery flags.
     * @param indexedRemovalTime Exact raw coordinate when validating an indexed removal.
     * @return Independent event or atomic typed refusal.
     */
    [[nodiscard]] std::expected<Greeks, MappingError>
    map(std::u16string_view symbol, RecordValueView values, std::size_t symbolUnits, std::int64_t eventTime = 0,
        std::int32_t eventFlags = 0, std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Validate borrowed input and project independent event storage.
     * @param symbol Exact event symbol, borrowed through return.
     * @param values Complete borrowed local-order fields and immutable context.
     * @param symbolUnits Maximum copied UTF-16 symbol units.
     * @param eventTime Independent FIELD Unix milliseconds.
     * @param eventFlags Actual indexed delivery flags.
     * @param indexedRemovalTime Exact raw coordinate when validating an indexed removal.
     * @return Independent event or atomic typed refusal; no input storage is retained.
     */
    [[nodiscard]] std::expected<Greeks, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
