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
/** Owning option-series values for one underlying symbol and one indexed series. */
struct Series {
    std::u16string symbol;             ///< Exact underlying symbol.
    std::int64_t eventTime = 0;        ///< Independent FIELD Unix milliseconds; zero when not selected.
    std::int64_t index = 0;            ///< Signed wire Index widened to 64 bits; distinct from the collector key.
    std::int32_t eventFlags = 0;       ///< Raw indexed snapshot, transaction and removal flags.
    std::int64_t timeSequence = 0;     ///< Signed business Time seconds in high32, unsigned Sequence in low32.
    std::int32_t expiration = 0;       ///< Expiration day ID, in days since 1970-01-01.
    core::WideDecimal volatility{0};   ///< Implied volatility index, or NaN.
    core::WideDecimal callVolume{0};   ///< Call options traded volume for the day, or NaN.
    core::WideDecimal putVolume{0};    ///< Put options traded volume for the day, or NaN.
    core::WideDecimal putCallRatio{0}; ///< Ratio of put to call options traded volume, or NaN.
    core::WideDecimal forwardPrice{0}; ///< Implied forward price of this series, or NaN.
    core::WideDecimal dividend{0};     ///< Implied simple dividend return, or NaN.
    core::WideDecimal interest{0};     ///< Implied simple interest return, or NaN.
    std::optional<std::array<detail::BusinessNumericSnapshot, 7>> businessNumericSnapshot{};
    ///< Inline source doubles and matching wide validity keys; no source payload is borrowed.

    /** @return The process-lifetime DEFAULT source, with ID zero. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Signed public series index; a manually assigned value retains all 64 bits. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return Business Unix milliseconds, using all ten raw millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Business sequence in the low 22 bits; no uniqueness guarantee. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
    /** @return Expiration as a signed day ID since the Unix epoch. */
    [[nodiscard]] std::int32_t getExpiration() const noexcept;
    /** @return Business implied volatility, including NaN when unavailable. */
    [[nodiscard]] double getVolatility() const noexcept;
    /** @return Business call volume for the day, including NaN when unavailable. */
    [[nodiscard]] double getCallVolume() const noexcept;
    /** @return Business put volume for the day, including NaN when unavailable. */
    [[nodiscard]] double getPutVolume() const noexcept;
    /** @return Sum of available daily call and put volumes, or NaN if both are NaN. */
    [[nodiscard]] double getOptionVolume() const noexcept;
    /** @return Business put-to-call volume ratio, including NaN when unavailable. */
    [[nodiscard]] double getPutCallRatio() const noexcept;
    /** @return Business implied forward price, including NaN when unavailable. */
    [[nodiscard]] double getForwardPrice() const noexcept;
    /** @return Business implied simple dividend return, including NaN when unavailable. */
    [[nodiscard]] double getDividend() const noexcept;
    /** @return Business implied simple interest return, including NaN when unavailable. */
    [[nodiscard]] double getInterest() const noexcept;
};

/** Construct the native default Series record with the physical VOID high word omitted.
 * @param limits Complete finite schema allowances, including zero.
 * @return Owning indexed schema with the standard optional fields enabled, or schema refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultSeriesSchema(core::SchemaLimits limits);

/** Immutable prepared complete-field Series mapping. It retains no caller schema or payload storage. */
class PreparedSeries {
    EventMapping mapping_;

    /** @param mapping Independently owned complete field shape and selections. */
    explicit PreparedSeries(EventMapping mapping) noexcept;

    public:
    /** Resolve an explicitly selected record's required and optional Series fields.
     * @param schema Validated immutable local schema, borrowed through this call.
     * @param localId Explicit selected record ID.
     * @param limits Finite mapping preparation allowances.
     * @return Complete independently owned mapping or original structured refusal.
     */
    [[nodiscard]] static std::expected<PreparedSeries, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed immutable context until this mapping is assigned, moved from or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owning input and project independent event storage.
     * @param symbol Exact underlying symbol, borrowed only through this call.
     * @param values Complete local-order values and exact immutable context.
     * @param symbolUnits Maximum number of copied UTF-16 symbol units.
     * @param eventTime Independent selected FIELD Unix milliseconds, default zero.
     * @param eventFlags Actual indexed delivery flags, default zero.
     * @return Complete owning Series or original context, value, resource or allocation refusal.
     */
    [[nodiscard]] std::expected<Series, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                          std::size_t symbolUnits, std::int64_t eventTime = 0,
                                                          std::int32_t eventFlags = 0) const;
    /** Validate complete borrowed input and project independent event storage.
     * @param symbol Exact underlying symbol, borrowed only through this call.
     * @param values Complete local-order views with all storage alive through return.
     * @param symbolUnits Maximum number of copied UTF-16 symbol units.
     * @param eventTime Independent selected FIELD Unix milliseconds, default zero.
     * @param eventFlags Actual indexed delivery flags, default zero.
     * @return Complete owning Series or original refusal; no caller payload is retained.
     */
    [[nodiscard]] std::expected<Series, MappingError> mapView(std::u16string_view symbol,
                                                              BorrowedRecordValueView values, std::size_t symbolUnits,
                                                              std::int64_t eventTime = 0,
                                                              std::int32_t eventFlags = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
