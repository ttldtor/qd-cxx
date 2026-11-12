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
/** Owning theoretical option values for one symbol and indexed time. */
struct TheoPrice {
    std::u16string symbol;                ///< Exact event symbol.
    std::int64_t eventTime = 0;           ///< Independent FIELD Unix milliseconds, zero when absent.
    std::int64_t index = 0;               ///< Signed seconds and unsigned sequence words.
    std::int32_t eventFlags = 0;          ///< Indexed snapshot, transaction and removal flags.
    core::WideDecimal price{0};           ///< Theoretical option price, or NaN.
    core::WideDecimal underlyingPrice{0}; ///< Underlying price used in calculation, or NaN.
    core::WideDecimal delta{0};           ///< Delta used in calculation, or NaN.
    core::WideDecimal gamma{0};           ///< Gamma used in calculation, or NaN.
    core::WideDecimal dividend{0};        ///< Dividend used in calculation, or NaN.
    core::WideDecimal interest{0};        ///< Interest used in calculation, or NaN.
    std::optional<std::array<detail::BusinessNumericSnapshot, 6>> businessNumericSnapshot{};
    ///< Inline source doubles and matching wide validity keys; no source payload is borrowed.

    /** @return Process-lifetime DEFAULT source with ID zero. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Complete signed indexed time/sequence word. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Business Unix milliseconds, including all ten raw millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Low 22 sequence bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Theoretical option price, including NaN when unavailable. */
    [[nodiscard]] double getPrice() const noexcept;
    /** @return Underlying price, including NaN when unavailable. */
    [[nodiscard]] double getUnderlyingPrice() const noexcept;
    /** @return Delta, including NaN when unavailable. */
    [[nodiscard]] double getDelta() const noexcept;
    /** @return Gamma, including NaN when unavailable. */
    [[nodiscard]] double getGamma() const noexcept;
    /** @return Dividend, including NaN when unavailable. */
    [[nodiscard]] double getDividend() const noexcept;
    /** @return Interest, including NaN when unavailable. */
    [[nodiscard]] double getInterest() const noexcept;
};

/** Construct the standard TheoPrice record with Time and Sequence indexed words.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning schema with standard optional fields enabled or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultTheoPriceSchema(core::SchemaLimits limits);

/** Immutable complete-field TheoPrice mapping, independent of caller storage. */
class PreparedTheoPrice {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;

    /** @param mapping Validated owning field plan.
     * @param layout Exact optional source indexed-time layout.
     */
    explicit PreparedTheoPrice(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Resolve the exact required and optional time and numeric properties.
     * @param schema Validated immutable local schema, borrowed during this call.
     * @param localId Exact selected local record ID.
     * @param limits Finite mapping preparation allowances.
     * @return Complete independently owned plan or typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedTheoPrice, MappingError>
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
    [[nodiscard]] std::expected<TheoPrice, MappingError>
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
    [[nodiscard]] std::expected<TheoPrice, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
