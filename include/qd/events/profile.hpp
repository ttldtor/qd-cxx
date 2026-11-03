// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Exact default Profile logical names; Java physical VOID tails are not fields. */
inline constexpr std::array<std::string_view, 16> PROFILE_FIELD_NAMES{
    "Beta",         "Eps",    "DivFreq",     "ExdDiv.Amount",  "ExdDiv.Date",   "52High.Price",
    "52Low.Price",  "Shares", "FreeFloat",   "HighLimitPrice", "LowLimitPrice", "Halt.StartTime",
    "Halt.EndTime", "Flags",  "Description", "StatusReason"};
/** Full identities in the default Profile logical order. */
inline constexpr std::array<core::SerialFieldType, 16> PROFILE_FIELD_TYPES{
    core::SerialFieldType::WIDE_DECIMAL,  core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::WIDE_DECIMAL,
    core::SerialFieldType::WIDE_DECIMAL,  core::SerialFieldType::DATE,         core::SerialFieldType::WIDE_DECIMAL,
    core::SerialFieldType::WIDE_DECIMAL,  core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::WIDE_DECIMAL,
    core::SerialFieldType::WIDE_DECIMAL,  core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::TIME_SECONDS,
    core::SerialFieldType::TIME_SECONDS,  core::SerialFieldType::COMPACT_INT,  core::SerialFieldType::UTF_CHAR_ARRAY,
    core::SerialFieldType::UTF_CHAR_ARRAY};

/** Public interpretation of Profile business flag bits zero and one. */
enum class TradingStatus {
    UNDEFINED = 0, ///< Unspecified or reserved raw code.
    HALTED = 1,    ///< Trading is halted.
    ACTIVE = 2     ///< Trading is active.
};

/** Public interpretation of Profile business flag bits two and three. */
enum class ShortSaleRestriction {
    UNDEFINED = 0, ///< Unspecified or reserved raw code.
    ACTIVE = 1,    ///< Short-sale restriction is active.
    INACTIVE = 2   ///< Short-sale restriction is inactive.
};

/** Mandatory logical UTF-16 ownership bounds for one complete event. */
struct EventValueLimits {
    std::size_t symbolUnits; ///< Maximum copied symbol units.
    std::size_t stringUnits; ///< Maximum units in each copied nullable field string.
    std::size_t totalUnits;  ///< Maximum combined symbol, Description and StatusReason units.
};

/** Owned Profile business projection, independent of schema and decoder storage. */
struct Profile {
    std::u16string symbol;                      ///< Exact owned UTF-16 symbol.
    core::WideDecimal beta{0};                  ///< Exact decimal projection; absent local field means NaN.
    core::WideDecimal earningsPerShare{0};      ///< Exact earnings-per-share projection.
    core::WideDecimal dividendFrequency{0};     ///< Exact dividend-frequency projection.
    core::WideDecimal exDividendAmount{0};      ///< Exact ex-dividend amount projection.
    std::int32_t exDividendDayId = 0;           ///< Raw signed day identifier without calendar conversion.
    core::WideDecimal high52WeekPrice{0};       ///< Exact 52-week high price projection.
    core::WideDecimal low52WeekPrice{0};        ///< Exact 52-week low price projection.
    core::WideDecimal shares{0};                ///< Exact shares projection.
    core::WideDecimal freeFloat{0};             ///< Exact free-float projection.
    core::WideDecimal highLimitPrice{0};        ///< Exact high-limit price projection.
    core::WideDecimal lowLimitPrice{0};         ///< Exact low-limit price projection.
    std::int64_t haltStartTimeMs = 0;           ///< Signed halt-start seconds expanded to milliseconds.
    std::int64_t haltEndTimeMs = 0;             ///< Signed halt-end seconds expanded to milliseconds.
    std::int32_t flags = 0;                     ///< Preserved business flags, including reserved bits; not QTP flags.
    std::optional<std::u16string> description;  ///< Owned exact nullable Description; empty differs from null.
    std::optional<std::u16string> statusReason; ///< Owned exact nullable reason; absent local field means null.
    std::int64_t eventTime = 0; ///< Caller metadata milliseconds from Unix epoch; unspecified means zero.

    /** Interpret the low two business flag bits, preserving the raw member.
     * @return Known trading status, or UNDEFINED for the reserved code.
     */
    [[nodiscard]] TradingStatus tradingStatus() const noexcept;

    /** Interpret business flag bits two and three, preserving the raw member.
     * @return Known restriction status, or UNDEFINED for the reserved code.
     */
    [[nodiscard]] ShortSaleRestriction shortSaleRestriction() const noexcept;

    /** Test the measured halted business status.
     * @return True exactly when tradingStatus() is HALTED.
     */
    [[nodiscard]] bool isTradingHalted() const noexcept;

    /** Test the measured active restriction status.
     * @return True exactly when shortSaleRestriction() is ACTIVE.
     */
    [[nodiscard]] bool isShortSaleRestricted() const noexcept;
};

/** Construct all sixteen default Profile fields through the normal validation boundary.
 * @param limits Mandatory finite schema budgets, including zero.
 * @return Owned EXTENDED_FIELDS_V1 schema with local ID zero, or schema/resource/allocation error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultProfileSchema(core::SchemaLimits limits);

/** Immutable Profile field selections tied to one validated local record context. */
class PreparedProfile {
    EventMapping mapping_;

    /** Take a complete independently owned field plan.
     * @param mapping Validated immutable Profile selections and context.
     */
    explicit PreparedProfile(EventMapping mapping);

    public:
    /** Resolve measured Profile keys and exact full types once.
     * @param schema Explicit validated schema; source containers are not retained.
     * @param localId Selected ordinary local record identity.
     * @param limits Mandatory finite preparation budgets.
     * @return Owned plan or atomic preparation error; Description is required locally.
     */
    [[nodiscard]] static std::expected<PreparedProfile, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);

    /** Inspect the retained schema/record identity.
     * @return Borrowed context until this mapping is moved, assigned or destroyed; a copy owns its identity.
     */
    [[nodiscard]] const core::RecordContext &context() const noexcept;

    /** Validate the whole record, then copy a complete event under explicit ownership budgets.
     * @param symbol Exact UTF-16 symbol, borrowed during this call.
     * @param values Context-tagged complete local values, including extra fields.
     * @param limits Maximum per-symbol, per-string and total copied UTF-16 units.
     * @param eventTime Explicit metadata milliseconds, independent of business fields.
     * @return Complete owned Profile or context/value/resource/allocation error; no partial event escapes.
     */
    [[nodiscard]] std::expected<Profile, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                           EventValueLimits limits, std::int64_t eventTime = 0) const;

    /** Project borrowed complete fields, copying only the final event's symbol and nullable strings.
     * @param symbol Exact UTF-16 symbol, borrowed during this call.
     * @param values Context-tagged complete field views; source fields and payloads outlive this call.
     * @param limits Maximum copied per-symbol, per-string and aggregate UTF-16 units.
     * @param eventTime Explicit metadata milliseconds, independent of indexed identity, halt times and flags.
     * @return Complete independently owned Profile or context/value/resource/allocation error; no partial event
     * escapes.
     */
    [[nodiscard]] std::expected<Profile, MappingError> mapView(std::u16string_view symbol,
                                                               BorrowedRecordValueView values, EventValueLimits limits,
                                                               std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
