// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {
/** Standard instrument-profile fields; names are exact and case-sensitive. */
enum class InstrumentProfileField {
    TYPE,                   ///< Standard TYPE field.
    SYMBOL,                 ///< Standard SYMBOL field.
    DESCRIPTION,            ///< Standard DESCRIPTION field.
    LOCAL_SYMBOL,           ///< Standard LOCAL_SYMBOL field.
    LOCAL_DESCRIPTION,      ///< Standard LOCAL_DESCRIPTION field.
    COUNTRY,                ///< Standard COUNTRY field.
    OPOL,                   ///< Standard OPOL field.
    EXCHANGE_DATA,          ///< Standard EXCHANGE_DATA field.
    EXCHANGES,              ///< Standard EXCHANGES field.
    CURRENCY,               ///< Standard CURRENCY field.
    BASE_CURRENCY,          ///< Standard BASE_CURRENCY field.
    CFI,                    ///< Standard CFI field.
    ISIN,                   ///< Standard ISIN field.
    SEDOL,                  ///< Standard SEDOL field.
    CUSIP,                  ///< Standard CUSIP field.
    ICB,                    ///< Standard ICB field.
    SIC,                    ///< Standard SIC field.
    MULTIPLIER,             ///< Standard MULTIPLIER field.
    PRODUCT,                ///< Standard PRODUCT field.
    UNDERLYING,             ///< Standard UNDERLYING field.
    SPC,                    ///< Standard SPC field.
    ADDITIONAL_UNDERLYINGS, ///< Standard ADDITIONAL_UNDERLYINGS field.
    MMY,                    ///< Standard MMY field.
    EXPIRATION,             ///< Standard EXPIRATION field.
    LAST_TRADE,             ///< Standard LAST_TRADE field.
    STRIKE,                 ///< Standard STRIKE field.
    OPTION_TYPE,            ///< Standard OPTION_TYPE field.
    EXPIRATION_STYLE,       ///< Standard EXPIRATION_STYLE field.
    SETTLEMENT_STYLE,       ///< Standard SETTLEMENT_STYLE field.
    PRICE_INCREMENTS,       ///< Standard PRICE_INCREMENTS field.
    TRADING_HOURS,          ///< Standard TRADING_HOURS field.
};
/** Standard storage/conversion category. */
enum class FieldKind {
    TEXT,    ///< Exact normalized UTF-16 text.
    INTEGER, ///< Signed32 numeric classification code.
    NUMBER,  ///< Binary64 numeric value, including nonfinite values.
    DATE     ///< Signed32 GMT day ID with calendar text conversion.
};

/** Public, exact standard field metadata. */
struct FieldInfo {
    InstrumentProfileField field; ///< Standard field identifier.
    std::u16string_view name;     ///< Static exact UTF-16 field name.
    FieldKind kind;               ///< Typed conversion category; DATE has a numeric day-ID representation.
};

/** Obtain all 31 standard fields in their specified order.
 * @return Static metadata that remains valid for the process lifetime.
 */
[[nodiscard]] const std::array<FieldInfo, 31> &standardFields() noexcept;
/** Find standard metadata without rejecting custom names.
 * @param name Exact case-sensitive UTF-16 name, borrowed for this call.
 * @return Static metadata, or null for a custom name.
 */
[[nodiscard]] const FieldInfo *findField(std::u16string_view name) noexcept;
/** Obtain the 17 standard type names in display order; other type strings remain valid.
 * @return Static exact UTF-16 names, valid for the process lifetime.
 */
[[nodiscard]] const std::array<std::u16string_view, 17> &standardTypes() noexcept;
/** Compare types for file presentation, with unknown types after known types.
 * @param left First exact type string, borrowed for this call.
 * @param right Second exact type string, borrowed for this call.
 * @return Negative, zero or positive presentation order; not a business classification.
 */
[[nodiscard]] int compareTypes(std::u16string_view left, std::u16string_view right) noexcept;
/** Checked conversion/edit failure; a failed edit leaves its profile unchanged. */
enum class ProfileError {
    INVALID_NUMBER, ///< Text is not a Java-compatible floating-point representation.
    INVALID_DATE,   ///< Text has no accepted year-month-day prefix.
    TEXTUAL_FIELD,  ///< Numeric/date access was requested for a standard textual field.
    LIMIT_EXCEEDED  ///< An explicit input, field-count or retained UTF-16 limit was exceeded.
};

/** Per-instance limits in UTF-16 code units, independent of process settings. */
struct ProfileLimits {
    std::size_t customFields = 256;   ///< Maximum retained nonempty custom fields; empty edits erase them.
    std::size_t nameUnits = 256;      ///< Maximum name units accepted by an operation, including standard names.
    std::size_t valueUnits = 65536;   ///< Maximum units retained in any standard/custom textual value.
    std::size_t totalUnits = 1048576; ///< Maximum combined standard values and custom names/values.
    std::size_t inputUnits = 65536;   ///< Maximum input value units scanned by a conversion or edit.
};

/** Independent owning instrument-profile value with exact UTF-16 names and strings.
 * Copies own all data. No callbacks, I/O, clocks, global configuration or raw parser-presence metadata
 * are retained. Read-only concurrent use is allowed; callers serialize mutation and observation.
 * Borrowed getter views are invalidated by mutation, assignment or destruction of this profile.
 * Checked edit failures preserve the complete value. Allocation failure throws std::bad_alloc and
 * preserves an edited profile; ordinary copy construction may also throw std::bad_alloc.
 */
class InstrumentProfile {
    public:
    /** Create an empty profile with independent finite limits.
     * @param limits Explicit per-instance limits; zero allows only operations fitting zero capacity.
     */
    explicit InstrumentProfile(ProfileLimits limits = {});
    /** Copy all owned values and limits.
     * @param other Source value; no storage is shared with the result.
     */
    InstrumentProfile(const InstrumentProfile &other) = default;
    /** Move all owned values and limits.
     * @param other Source value; it remains valid with its original limits and an empty value.
     */
    InstrumentProfile(InstrumentProfile &&other) noexcept;
    /** Replace this value atomically by an independently owned copy/move.
     * @param other Independent replacement value, constructed before this method starts.
     * @return This profile after replacement.
     */
    InstrumentProfile &operator=(InstrumentProfile other) noexcept;
    /** Get this value's explicit limits.
     * @return Independent limits copied by value.
     */
    [[nodiscard]] ProfileLimits limits() const noexcept;
    /** Count retained nonempty custom values, including a possible empty name.
     * @return Actual custom entry count; no allocation or source view is returned.
     */
    [[nodiscard]] std::size_t customFieldCount() const noexcept;
    /** Measure retained standard text plus custom names/values in UTF-16 units.
     * Configuration, container capacity and allocator overhead are excluded.
     * @return Actual logical retained units; no allocation is performed.
     */
    [[nodiscard]] std::size_t retainedUnits() const noexcept;
    /** Get textual field representation; absent/custom-empty/null values normalize to empty.
     * @param name Exact name borrowed for the call; unknown and empty custom names are supported.
     * @return Borrowed normalized text; no view into the name argument is retained.
     */
    [[nodiscard]] std::u16string_view getField(std::u16string_view name) const noexcept;
    /** Set normalized textual or parsed standard typed data.
     * @param name Exact UTF-16 standard/custom name, copied only for nonempty custom data.
     * @param value Borrowed value; standard numeric/date values are parsed and canonicalized.
     * @return Success, or a conversion/limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setField(std::u16string_view name, std::u16string_view value);
    /** Normalize a null value to empty.
     * @param name Exact standard/custom name, borrowed for the call.
     * @param value Null marker, equivalent to an empty string value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setField(std::u16string_view name, std::nullptr_t value);
    /** Read numeric data, parsing custom text and exact ten-unit date-shaped custom values.
     * @param name Exact field name, borrowed for the call.
     * @return Numeric value (zero for missing custom data), or a conversion/category/limit error.
     */
    [[nodiscard]] std::expected<double, ProfileError> getNumericField(std::u16string_view name) const;
    /** Set numeric data; integral/date standard fields use saturated Java int conversion.
     * @param name Exact standard/custom name, borrowed for the call.
     * @param value Binary64 value; signed zero and nonfinite values are accepted.
     * @return Success, or a category/limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setNumericField(std::u16string_view name, double value);
    /** Read a signed day ID; numeric standard fields use saturated Java int conversion.
     * @param name Exact field name, borrowed for the call.
     * @return Signed days since 1970-01-01 GMT, or a conversion/category/limit error.
     */
    [[nodiscard]] std::expected<std::int32_t, ProfileError> getDateField(std::u16string_view name) const;
    /** Set a signed GMT day ID; custom fields receive canonical date text.
     * @param name Exact standard/custom name, borrowed for the call.
     * @param value Signed days since 1970-01-01 GMT; zero canonicalizes to empty.
     * @return Success, or a category/limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setDateField(std::u16string_view name, std::int32_t value);
    /** Enumerate nonempty custom names in exact UTF-16 lexical order.
     * @return Independent owned names; changing them does not change the profile.
     */
    [[nodiscard]] std::vector<std::u16string> customFieldNames() const;
    /** Compare symbol identity without case folding, normalization or type conversion.
     * @param other Profile whose owned symbol is compared.
     * @return True exactly when the UTF-16 symbol units match, including empty symbols.
     */
    [[nodiscard]] bool sameSymbol(const InstrumentProfile &other) const noexcept;
    /** Compare full profile values, treating NaNs as equal and signed zeros as distinct.
     * @param other Independent value to compare; limits are not part of value equality.
     * @return True when all standard and nonempty custom values match.
     */
    [[nodiscard]] bool operator==(const InstrumentProfile &other) const noexcept;
    /** Compare for file presentation using type/product/underlying/last-trade/strike/symbol.
     * @param other Independent profile to compare; this order is not a business identity.
     * @return Negative, zero or positive presentation order, matching numeric NaN comparisons.
     */
    [[nodiscard]] int compareTo(const InstrumentProfile &other) const noexcept;

    /** Read the exact TYPE text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getType() const noexcept {
        return values_[0].text;
    }

    /** Set the exact TYPE text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setType(std::u16string_view value) {
        return setField(u"TYPE", value);
    }

    /** Clear TYPE using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setType(std::nullptr_t value) {
        return setField(u"TYPE", value);
    }

    /** Read the exact SYMBOL text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getSymbol() const noexcept {
        return values_[1].text;
    }

    /** Set the exact SYMBOL text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSymbol(std::u16string_view value) {
        return setField(u"SYMBOL", value);
    }

    /** Clear SYMBOL using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSymbol(std::nullptr_t value) {
        return setField(u"SYMBOL", value);
    }

    /** Read the exact DESCRIPTION text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getDescription() const noexcept {
        return values_[2].text;
    }

    /** Set the exact DESCRIPTION text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setDescription(std::u16string_view value) {
        return setField(u"DESCRIPTION", value);
    }

    /** Clear DESCRIPTION using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setDescription(std::nullptr_t value) {
        return setField(u"DESCRIPTION", value);
    }

    /** Read the exact LOCAL_SYMBOL text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getLocalSymbol() const noexcept {
        return values_[3].text;
    }

    /** Set the exact LOCAL_SYMBOL text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setLocalSymbol(std::u16string_view value) {
        return setField(u"LOCAL_SYMBOL", value);
    }

    /** Clear LOCAL_SYMBOL using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setLocalSymbol(std::nullptr_t value) {
        return setField(u"LOCAL_SYMBOL", value);
    }

    /** Read the exact LOCAL_DESCRIPTION text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getLocalDescription() const noexcept {
        return values_[4].text;
    }

    /** Set the exact LOCAL_DESCRIPTION text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setLocalDescription(std::u16string_view value) {
        return setField(u"LOCAL_DESCRIPTION", value);
    }

    /** Clear LOCAL_DESCRIPTION using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setLocalDescription(std::nullptr_t value) {
        return setField(u"LOCAL_DESCRIPTION", value);
    }

    /** Read the exact COUNTRY text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getCountry() const noexcept {
        return values_[5].text;
    }

    /** Set the exact COUNTRY text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCountry(std::u16string_view value) {
        return setField(u"COUNTRY", value);
    }

    /** Clear COUNTRY using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCountry(std::nullptr_t value) {
        return setField(u"COUNTRY", value);
    }

    /** Read the exact OPOL text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getOpol() const noexcept {
        return values_[6].text;
    }

    /** Set the exact OPOL text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setOpol(std::u16string_view value) {
        return setField(u"OPOL", value);
    }

    /** Clear OPOL using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setOpol(std::nullptr_t value) {
        return setField(u"OPOL", value);
    }

    /** Read the exact EXCHANGE_DATA text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getExchangeData() const noexcept {
        return values_[7].text;
    }

    /** Set the exact EXCHANGE_DATA text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExchangeData(std::u16string_view value) {
        return setField(u"EXCHANGE_DATA", value);
    }

    /** Clear EXCHANGE_DATA using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExchangeData(std::nullptr_t value) {
        return setField(u"EXCHANGE_DATA", value);
    }

    /** Read the exact EXCHANGES text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getExchanges() const noexcept {
        return values_[8].text;
    }

    /** Set the exact EXCHANGES text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExchanges(std::u16string_view value) {
        return setField(u"EXCHANGES", value);
    }

    /** Clear EXCHANGES using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExchanges(std::nullptr_t value) {
        return setField(u"EXCHANGES", value);
    }

    /** Read the exact CURRENCY text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getCurrency() const noexcept {
        return values_[9].text;
    }

    /** Set the exact CURRENCY text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCurrency(std::u16string_view value) {
        return setField(u"CURRENCY", value);
    }

    /** Clear CURRENCY using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCurrency(std::nullptr_t value) {
        return setField(u"CURRENCY", value);
    }

    /** Read the exact BASE_CURRENCY text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getBaseCurrency() const noexcept {
        return values_[10].text;
    }

    /** Set the exact BASE_CURRENCY text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setBaseCurrency(std::u16string_view value) {
        return setField(u"BASE_CURRENCY", value);
    }

    /** Clear BASE_CURRENCY using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setBaseCurrency(std::nullptr_t value) {
        return setField(u"BASE_CURRENCY", value);
    }

    /** Read the exact CFI text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getCfi() const noexcept {
        return values_[11].text;
    }

    /** Set the exact CFI text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCfi(std::u16string_view value) {
        return setField(u"CFI", value);
    }

    /** Clear CFI using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCfi(std::nullptr_t value) {
        return setField(u"CFI", value);
    }

    /** Read the exact ISIN text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getIsin() const noexcept {
        return values_[12].text;
    }

    /** Set the exact ISIN text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setIsin(std::u16string_view value) {
        return setField(u"ISIN", value);
    }

    /** Clear ISIN using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setIsin(std::nullptr_t value) {
        return setField(u"ISIN", value);
    }

    /** Read the exact SEDOL text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getSedol() const noexcept {
        return values_[13].text;
    }

    /** Set the exact SEDOL text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSedol(std::u16string_view value) {
        return setField(u"SEDOL", value);
    }

    /** Clear SEDOL using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSedol(std::nullptr_t value) {
        return setField(u"SEDOL", value);
    }

    /** Read the exact CUSIP text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getCusip() const noexcept {
        return values_[14].text;
    }

    /** Set the exact CUSIP text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCusip(std::u16string_view value) {
        return setField(u"CUSIP", value);
    }

    /** Clear CUSIP using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setCusip(std::nullptr_t value) {
        return setField(u"CUSIP", value);
    }

    /** Read the typed ICB numeric value.
     * @return Stored numeric value, independent of textual formatting.
     */
    [[nodiscard]] std::int32_t getIcb() const noexcept {
        return static_cast<std::int32_t>(values_[15].number);
    }

    /** Set the typed ICB numeric value.
     * @param value New numeric value; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setIcb(std::int32_t value) {
        return setNumericField(u"ICB", value);
    }

    /** Read the typed SIC numeric value.
     * @return Stored numeric value, independent of textual formatting.
     */
    [[nodiscard]] std::int32_t getSic() const noexcept {
        return static_cast<std::int32_t>(values_[16].number);
    }

    /** Set the typed SIC numeric value.
     * @param value New numeric value; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSic(std::int32_t value) {
        return setNumericField(u"SIC", value);
    }

    /** Read the typed MULTIPLIER numeric value.
     * @return Stored numeric value, independent of textual formatting.
     */
    [[nodiscard]] double getMultiplier() const noexcept {
        return static_cast<double>(values_[17].number);
    }

    /** Set the typed MULTIPLIER numeric value.
     * @param value New numeric value; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setMultiplier(double value) {
        return setNumericField(u"MULTIPLIER", value);
    }

    /** Read the exact PRODUCT text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getProduct() const noexcept {
        return values_[18].text;
    }

    /** Set the exact PRODUCT text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setProduct(std::u16string_view value) {
        return setField(u"PRODUCT", value);
    }

    /** Clear PRODUCT using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setProduct(std::nullptr_t value) {
        return setField(u"PRODUCT", value);
    }

    /** Read the exact UNDERLYING text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getUnderlying() const noexcept {
        return values_[19].text;
    }

    /** Set the exact UNDERLYING text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setUnderlying(std::u16string_view value) {
        return setField(u"UNDERLYING", value);
    }

    /** Clear UNDERLYING using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setUnderlying(std::nullptr_t value) {
        return setField(u"UNDERLYING", value);
    }

    /** Read the typed SPC numeric value.
     * @return Stored numeric value, independent of textual formatting.
     */
    [[nodiscard]] double getSpc() const noexcept {
        return static_cast<double>(values_[20].number);
    }

    /** Set the typed SPC numeric value.
     * @param value New numeric value; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSpc(double value) {
        return setNumericField(u"SPC", value);
    }

    /** Read the exact ADDITIONAL_UNDERLYINGS text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getAdditionalUnderlyings() const noexcept {
        return values_[21].text;
    }

    /** Set the exact ADDITIONAL_UNDERLYINGS text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setAdditionalUnderlyings(std::u16string_view value) {
        return setField(u"ADDITIONAL_UNDERLYINGS", value);
    }

    /** Clear ADDITIONAL_UNDERLYINGS using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setAdditionalUnderlyings(std::nullptr_t value) {
        return setField(u"ADDITIONAL_UNDERLYINGS", value);
    }

    /** Read the exact MMY text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getMmy() const noexcept {
        return values_[22].text;
    }

    /** Set the exact MMY text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setMmy(std::u16string_view value) {
        return setField(u"MMY", value);
    }

    /** Clear MMY using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setMmy(std::nullptr_t value) {
        return setField(u"MMY", value);
    }

    /** Read the typed EXPIRATION signed GMT day ID.
     * @return Stored signed GMT day ID, independent of textual formatting.
     */
    [[nodiscard]] std::int32_t getExpiration() const noexcept {
        return static_cast<std::int32_t>(values_[23].number);
    }

    /** Set the typed EXPIRATION signed GMT day ID.
     * @param value New signed GMT day ID; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExpiration(std::int32_t value) {
        return setNumericField(u"EXPIRATION", value);
    }

    /** Read the typed LAST_TRADE signed GMT day ID.
     * @return Stored signed GMT day ID, independent of textual formatting.
     */
    [[nodiscard]] std::int32_t getLastTrade() const noexcept {
        return static_cast<std::int32_t>(values_[24].number);
    }

    /** Set the typed LAST_TRADE signed GMT day ID.
     * @param value New signed GMT day ID; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setLastTrade(std::int32_t value) {
        return setNumericField(u"LAST_TRADE", value);
    }

    /** Read the typed STRIKE numeric value.
     * @return Stored numeric value, independent of textual formatting.
     */
    [[nodiscard]] double getStrike() const noexcept {
        return static_cast<double>(values_[25].number);
    }

    /** Set the typed STRIKE numeric value.
     * @param value New numeric value; no source storage is retained.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setStrike(double value) {
        return setNumericField(u"STRIKE", value);
    }

    /** Read the exact OPTION_TYPE text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getOptionType() const noexcept {
        return values_[26].text;
    }

    /** Set the exact OPTION_TYPE text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setOptionType(std::u16string_view value) {
        return setField(u"OPTION_TYPE", value);
    }

    /** Clear OPTION_TYPE using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setOptionType(std::nullptr_t value) {
        return setField(u"OPTION_TYPE", value);
    }

    /** Read the exact EXPIRATION_STYLE text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getExpirationStyle() const noexcept {
        return values_[27].text;
    }

    /** Set the exact EXPIRATION_STYLE text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExpirationStyle(std::u16string_view value) {
        return setField(u"EXPIRATION_STYLE", value);
    }

    /** Clear EXPIRATION_STYLE using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setExpirationStyle(std::nullptr_t value) {
        return setField(u"EXPIRATION_STYLE", value);
    }

    /** Read the exact SETTLEMENT_STYLE text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getSettlementStyle() const noexcept {
        return values_[28].text;
    }

    /** Set the exact SETTLEMENT_STYLE text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSettlementStyle(std::u16string_view value) {
        return setField(u"SETTLEMENT_STYLE", value);
    }

    /** Clear SETTLEMENT_STYLE using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setSettlementStyle(std::nullptr_t value) {
        return setField(u"SETTLEMENT_STYLE", value);
    }

    /** Read the exact PRICE_INCREMENTS text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getPriceIncrements() const noexcept {
        return values_[29].text;
    }

    /** Set the exact PRICE_INCREMENTS text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setPriceIncrements(std::u16string_view value) {
        return setField(u"PRICE_INCREMENTS", value);
    }

    /** Clear PRICE_INCREMENTS using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setPriceIncrements(std::nullptr_t value) {
        return setField(u"PRICE_INCREMENTS", value);
    }

    /** Read the exact TRADING_HOURS text.
     * @return Borrowed normalized text, invalidated by this profile's mutation or destruction.
     */
    [[nodiscard]] std::u16string_view getTradingHours() const noexcept {
        return values_[30].text;
    }

    /** Set the exact TRADING_HOURS text.
     * @param value Borrowed text copied into this value.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setTradingHours(std::u16string_view value) {
        return setField(u"TRADING_HOURS", value);
    }

    /** Clear TRADING_HOURS using null normalization.
     * @param value Null marker, equivalent to empty text.
     * @return Success, or a limit error with no change to the profile.
     */
    [[nodiscard]] std::expected<void, ProfileError> setTradingHours(std::nullptr_t value) {
        return setField(u"TRADING_HOURS", value);
    }

    private:
    struct Value {
        std::u16string text;
        double number = 0;
    };

    ProfileLimits limits_;
    std::array<Value, 31> values_;
    std::map<std::u16string, std::u16string, std::less<>> custom_;
    std::size_t totalUnits_ = 0;
    std::expected<void, ProfileError> store(std::u16string_view name, std::u16string text, double number);
};
} // namespace org::ttldtor::qdcxx::ipf
