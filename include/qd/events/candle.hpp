// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/candle_symbol.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/indexed_event_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::events {

/** Owning candle values from one candle or trade-history record. */
struct Candle {
    std::optional<CandleSymbol> eventSymbol; ///< Independent CandleSymbol; absent for a default event.
    std::int64_t eventTime = 0;              ///< Independent FIELD Unix milliseconds, zero when not selected.
    std::int32_t eventFlags = 0;             ///< Raw indexed transaction, snapshot and removal flags.
    std::int64_t index = 0;                  ///< Signed seconds in high32, millisecond and sequence bits in low32.
    std::int64_t count = 0;                  ///< Source count after Java numeric-to-long conversion.
    core::WideDecimal open{0};               ///< First price or NaN.
    core::WideDecimal high{0};               ///< Highest price or NaN.
    core::WideDecimal low{0};                ///< Lowest price or NaN.
    core::WideDecimal close{0};              ///< Last price or NaN.
    core::WideDecimal volume{0};             ///< Traded volume or NaN.
    core::WideDecimal vwap{0};               ///< Volume-weighted average price or NaN.
    core::WideDecimal bidVolume{0};          ///< Bid-side volume or NaN.
    core::WideDecimal askVolume{0};          ///< Ask-side volume or NaN.
    core::WideDecimal impVolatility{0};      ///< Implied volatility or NaN.
    core::WideDecimal openInterest{0};       ///< Open interest or NaN.
    std::optional<std::array<detail::BusinessNumericSnapshot, 10>> businessNumericSnapshot{};
    ///< Owning original business-double values and their WideDecimal validity keys.

    /** @return Nullable owning candle symbol, shared independently of the source. */
    [[nodiscard]] std::optional<CandleSymbol> getEventSymbol() const noexcept;
    /** @return Process-lifetime DEFAULT indexed source. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Complete packed time/sequence index. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Timestamp in milliseconds, including all ten packed millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Sequence in the low 22 bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Number of original events in this candle. */
    [[nodiscard]] std::int64_t getCount() const noexcept;
    /** @return First price. */
    [[nodiscard]] double getOpen() const noexcept;
    /** @return Highest price. */
    [[nodiscard]] double getHigh() const noexcept;
    /** @return Lowest price. */
    [[nodiscard]] double getLow() const noexcept;
    /** @return Last price. */
    [[nodiscard]] double getClose() const noexcept;
    /** @return Traded volume as a business double. */
    [[nodiscard]] double getVolumeAsDouble() const noexcept;
    /** @return Traded volume narrowed with Java long conversion. */
    [[nodiscard]] std::int64_t getVolume() const noexcept;
    /** @return Volume-weighted average price. */
    [[nodiscard]] double getVWAP() const noexcept;
    /** @return Bid-side volume as a business double. */
    [[nodiscard]] double getBidVolumeAsDouble() const noexcept;
    /** @return Bid-side volume narrowed with Java long conversion. */
    [[nodiscard]] std::int64_t getBidVolume() const noexcept;
    /** @return Ask-side volume as a business double. */
    [[nodiscard]] double getAskVolumeAsDouble() const noexcept;
    /** @return Ask-side volume narrowed with Java long conversion. */
    [[nodiscard]] std::int64_t getAskVolume() const noexcept;
    /** @return Implied volatility. */
    [[nodiscard]] double getImpVolatility() const noexcept;
    /** @return Open interest as a business double. */
    [[nodiscard]] double getOpenInterestAsDouble() const noexcept;
    /** @return Open interest narrowed with Java long conversion. */
    [[nodiscard]] std::int64_t getOpenInterest() const noexcept;
};

/** Selects the Java delegate's physical value mapping, independent of symbol routing. */
enum class CandleRecordKind {
    CANDLE,       ///< Candle and Trade.<period> complete OHLC records.
    TRADE_HISTORY ///< TradeHistory Price and Size records.
};

/** Construct the standard unsuffixed Candle record with all optional fields enabled.
 * @param limits Complete finite schema limits.
 * @return Owning indexed schema or a structured schema error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultCandleSchema(core::SchemaLimits limits);

/** Immutable prepared candle mapping, independently owned from the input schema. */
class PreparedCandle {
    EventMapping mapping_;
    CandleRecordKind kind_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_;

    /** @param mapping Complete owned field shape and selections.
     * @param kind Explicit physical delegate kind.
     * @param indexedTimeLayout Exact matching physical pair, if the schema provides one.
     */
    explicit PreparedCandle(EventMapping mapping, CandleRecordKind kind,
                            std::optional<core::IndexedTimeLayout> indexedTimeLayout) noexcept;

    public:
    /** Prepare the required and optional fields of one explicitly selected record.
     * @param schema Immutable validated schema, borrowed through the call.
     * @param localId Selected local record.
     * @param kind Explicit Candle or TradeHistory physical delegate.
     * @param limits Finite mapping preparation limits.
     * @return Owned mapper or structured refusal.
     */
    [[nodiscard]] static std::expected<PreparedCandle, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, CandleRecordKind kind, MappingLimits limits);
    /** @return Borrowed immutable context until assignment, move or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owned input and create an independent candle.
     * @param symbol Already resolved immutable Candle symbol, borrowed through return.
     * @param values Complete local values and exact context.
     * @param symbolUnits Maximum UTF-16 units in the canonical symbol.
     * @param eventTime Independent FIELD timestamp.
     * @param eventFlags Raw indexed event flags.
     * @return Independent candle or structured refusal.
     */
    [[nodiscard]] std::expected<Candle, MappingError> map(const CandleSymbol &symbol, RecordValueView values,
                                                          std::size_t symbolUnits, std::int64_t eventTime = 0,
                                                          std::int32_t eventFlags = 0) const;
    /** Validate complete borrowed input and create an independent candle.
     * @param symbol Already resolved immutable Candle symbol, borrowed through return.
     * @param values Complete borrowed local values and exact context.
     * @param symbolUnits Maximum UTF-16 units in the canonical symbol.
     * @param eventTime Independent FIELD timestamp.
     * @param eventFlags Raw indexed event flags.
     * @return Independent candle or structured refusal.
     */
    [[nodiscard]] std::expected<Candle, MappingError> mapView(const CandleSymbol &symbol,
                                                              BorrowedRecordValueView values, std::size_t symbolUnits,
                                                              std::int64_t eventTime = 0,
                                                              std::int32_t eventFlags = 0) const;
    /** Validate and project a removal using the prepared physical Time/Sequence pair.
     * @param symbol Already resolved immutable Candle symbol, borrowed through return.
     * @param values Complete borrowed local values, including the exact physical removal words.
     * @param symbolUnits Maximum UTF-16 units in the canonical symbol.
     * @param eventTime Independent FIELD timestamp.
     * @param eventFlags Raw indexed flags, including REMOVE_EVENT.
     * @param rawTime Exact packed removal coordinate supplied by the collector.
     * @return Independent candle or structured refusal.
     */
    [[nodiscard]] std::expected<Candle, MappingError>
    mapIndexedRemovalView(const CandleSymbol &symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
                          std::int64_t eventTime, std::int32_t eventFlags, std::int64_t rawTime) const;
};

} // namespace org::ttldtor::qdcxx::events
