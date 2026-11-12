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
/** Owning option-market statistics for one underlying symbol and indexed time. */
struct Underlying {
    std::u16string symbol;                ///< Exact underlying symbol.
    std::int64_t eventTime = 0;           ///< Independent FIELD Unix milliseconds, zero when absent.
    std::int64_t index = 0;               ///< Signed seconds and unsigned sequence words.
    std::int32_t eventFlags = 0;          ///< Indexed snapshot, transaction and removal flags.
    core::WideDecimal volatility{0};      ///< Implied volatility, or NaN.
    core::WideDecimal frontVolatility{0}; ///< Front-month volatility, or NaN.
    core::WideDecimal backVolatility{0};  ///< Back-month volatility, or NaN.
    core::WideDecimal callVolume{0};      ///< Call options volume for the day, or NaN.
    core::WideDecimal putVolume{0};       ///< Put options volume for the day, or NaN.
    core::WideDecimal putCallRatio{0};    ///< Put-to-call volume ratio, or NaN.
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
    /** @return Implied volatility, including NaN when unavailable. */
    [[nodiscard]] double getVolatility() const noexcept;
    /** @return Front-month volatility, including NaN when unavailable. */
    [[nodiscard]] double getFrontVolatility() const noexcept;
    /** @return Back-month volatility, including NaN when unavailable. */
    [[nodiscard]] double getBackVolatility() const noexcept;
    /** @return Daily call options volume, including NaN when unavailable. */
    [[nodiscard]] double getCallVolume() const noexcept;
    /** @return Daily put options volume, including NaN when unavailable. */
    [[nodiscard]] double getPutVolume() const noexcept;
    /** @return Sum of available call and put volumes, or NaN if both are NaN. */
    [[nodiscard]] double getOptionVolume() const noexcept;
    /** @return Put-to-call volume ratio, including NaN when unavailable. */
    [[nodiscard]] double getPutCallRatio() const noexcept;
};

/** Construct the standard Underlying record with Time and Sequence indexed words.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning schema with standard optional fields enabled or a schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultUnderlyingSchema(core::SchemaLimits limits);

/** Immutable complete-field Underlying mapping, independent of caller storage. */
class PreparedUnderlying {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;

    /** @param mapping Validated owning field plan.
     * @param layout Exact optional source indexed-time layout.
     */
    explicit PreparedUnderlying(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Resolve the exact optional time and numeric properties.
     * @param schema Validated immutable local schema, borrowed during this call.
     * @param localId Exact selected local record ID.
     * @param limits Finite mapping preparation allowances.
     * @return Complete independently owned plan or typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedUnderlying, MappingError>
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
    [[nodiscard]] std::expected<Underlying, MappingError>
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
    [[nodiscard]] std::expected<Underlying, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
