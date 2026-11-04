// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace org::ttldtor::qdcxx::events {
/** Candle input price selection; this value does not aggregate events. */
enum class CandlePrice {
    LAST,      ///< Last trade.
    BID,       ///< Quote bid.
    ASK,       ///< Quote ask.
    MARK,      ///< Average bid and ask.
    SETTLEMENT ///< Settlement, or last trade when unavailable.
};
/** Candle input session selection; no schedule calculations are performed. */
enum class CandleSession {
    ANY,    ///< All sessions.
    REGULAR ///< Regular session only.
};
/** Candle boundary alignment; this value does not calculate boundaries. */
enum class CandleAlignment {
    MIDNIGHT, ///< Midnight alignment.
    SESSION   ///< Trading-session alignment.
};
inline constexpr std::u16string_view CANDLE_PRICE_ATTRIBUTE_KEY = u"price";            ///< Price key.
inline constexpr std::u16string_view CANDLE_SESSION_ATTRIBUTE_KEY = u"tho";            ///< Trading-hours-only key.
inline constexpr std::u16string_view CANDLE_ALIGNMENT_ATTRIBUTE_KEY = u"a";            ///< Alignment key.
inline constexpr CandlePrice DEFAULT_CANDLE_PRICE = CandlePrice::LAST;                 ///< Default price.
inline constexpr CandleSession DEFAULT_CANDLE_SESSION = CandleSession::ANY;            ///< Default session.
inline constexpr CandleAlignment DEFAULT_CANDLE_ALIGNMENT = CandleAlignment::MIDNIGHT; ///< Default alignment.

/** @param value Valid price selection.
 * @return Borrowed static canonical UTF-16 text, valid for the program lifetime.
 * @throws std::invalid_argument The enum is invalid.
 */
[[nodiscard]] std::u16string_view candlePriceText(CandlePrice value);
/** Nonempty case-insensitive canonical prefix; first declaration wins.
 * @param text Borrowed text, not retained or trimmed; nullopt is rejected.
 * @return Parsed price selection.
 * @throws std::invalid_argument Null or invalid text.
 */
[[nodiscard]] CandlePrice parseCandlePrice(std::optional<std::u16string_view> text);
/** Read the attribute; session uses complete Boolean true recognition, unlike its parser.
 * @param symbol Borrowed symbol; nullopt represents Java null.
 * @return Attribute value, or its default when absent.
 * @throws std::invalid_argument Invalid price/alignment attribute.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Attribute-copy allocation fails.
 */
[[nodiscard]] CandlePrice getCandlePrice(std::optional<std::u16string_view> symbol);
/** Replace the attribute; the default removes every matching key.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @param value Valid price selection.
 * @return Independent owned symbol, possibly nullopt.
 * @throws std::invalid_argument The enum is invalid.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> changeCandlePrice(std::optional<std::u16string_view> symbol,
                                                              CandlePrice value);
/** Preserve reference normalization, including retained explicit default attributes.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @return Independent owned normalized symbol; invalid values remain unchanged.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> normalizeCandlePrice(std::optional<std::u16string_view> symbol);

/** @param value Valid session selection.
 * @return Borrowed static canonical UTF-16 text, valid for the program lifetime.
 * @throws std::invalid_argument The enum is invalid.
 */
[[nodiscard]] std::u16string_view candleSessionText(CandleSession value);
/** Nonempty case-insensitive prefix of false or true.
 * @param text Borrowed text, not retained or trimmed; nullopt is rejected.
 * @return Parsed session selection.
 * @throws std::invalid_argument Null or invalid text.
 */
[[nodiscard]] CandleSession parseCandleSession(std::optional<std::u16string_view> text);
/** Read the attribute; session uses complete Boolean true recognition, unlike its parser.
 * @param symbol Borrowed symbol; nullopt represents Java null.
 * @return Attribute value, or its default when absent.
 * @throws std::invalid_argument Invalid price/alignment attribute.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Attribute-copy allocation fails.
 */
[[nodiscard]] CandleSession getCandleSession(std::optional<std::u16string_view> symbol);
/** Replace the attribute; the default removes every matching key.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @param value Valid session selection.
 * @return Independent owned symbol, possibly nullopt.
 * @throws std::invalid_argument The enum is invalid.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> changeCandleSession(std::optional<std::u16string_view> symbol,
                                                                CandleSession value);
/** Preserve reference normalization, including retained explicit default attributes.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @return Independent owned normalized symbol; invalid values remain unchanged.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> normalizeCandleSession(std::optional<std::u16string_view> symbol);

/** @param value Valid alignment selection.
 * @return Borrowed static canonical UTF-16 text, valid for the program lifetime.
 * @throws std::invalid_argument The enum is invalid.
 */
[[nodiscard]] std::u16string_view candleAlignmentText(CandleAlignment value);
/** Case-insensitive exact m or s; names are not accepted.
 * @param text Borrowed text, not retained or trimmed; nullopt is rejected.
 * @return Parsed alignment selection.
 * @throws std::invalid_argument Null or invalid text.
 */
[[nodiscard]] CandleAlignment parseCandleAlignment(std::optional<std::u16string_view> text);
/** Read the attribute; session uses complete Boolean true recognition, unlike its parser.
 * @param symbol Borrowed symbol; nullopt represents Java null.
 * @return Attribute value, or its default when absent.
 * @throws std::invalid_argument Invalid price/alignment attribute.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Attribute-copy allocation fails.
 */
[[nodiscard]] CandleAlignment getCandleAlignment(std::optional<std::u16string_view> symbol);
/** Replace the attribute; the default removes every matching key.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @param value Valid alignment selection.
 * @return Independent owned symbol, possibly nullopt.
 * @throws std::invalid_argument The enum is invalid.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> changeCandleAlignment(std::optional<std::u16string_view> symbol,
                                                                  CandleAlignment value);
/** Preserve reference normalization, including retained explicit default attributes.
 * @param symbol Borrowed symbol, not retained; nullopt represents Java null.
 * @return Independent owned normalized symbol; invalid values remain unchanged.
 * @throws std::out_of_range Malformed symbol produces an invalid substring range.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> normalizeCandleAlignment(std::optional<std::u16string_view> symbol);

/** Independent nonnegative finite price interval or canonical NaN default.
 * Negative zero, negatives and infinities are rejected. No aggregation/runtime state is retained.
 * Const operations are independent; concurrent mutation of one object requires synchronization.
 */
class CandlePriceLevel final {
    static constexpr std::uint64_t SIGN_MASK = 0x8000000000000000ULL;
    static constexpr std::uint64_t INFINITY_BITS = 0x7ff0000000000000ULL;
    double value_;

    public:
    static constexpr std::u16string_view ATTRIBUTE_KEY = u"pl"; ///< Price-level key.
    static const CandlePriceLevel DEFAULT;                      ///< Canonical NaN, meaning no price grouping.

    /** @param value Nonnegative finite interval, or any NaN (canonicalized).
     * @throws std::invalid_argument Negative zero, a negative value or infinity.
     */
    constexpr explicit CandlePriceLevel(double value = std::numeric_limits<double>::quiet_NaN()) : value_(value) {
        auto bits = std::bit_cast<std::uint64_t>(value);
        auto magnitude = bits & ~SIGN_MASK;

        if (magnitude > INFINITY_BITS) {
            value_ = std::numeric_limits<double>::quiet_NaN();
        } else if (magnitude == INFINITY_BITS || (bits & SIGN_MASK) != 0) {
            throw std::invalid_argument("Invalid candle price level");
        }
    }

    /** @return Stored interval, or canonical quiet NaN. */
    [[nodiscard]] double getValue() const noexcept;
    /** @return Independent Java numeric spelling; integer rendering takes precedence.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::u16string toString() const;
    /** @param text Borrowed Java double text, not retained; nullopt is invalid.
     * @return Independent interval; NaN selects the default.
     * @throws std::invalid_argument Null, invalid numeric text or rejected interval.
     * @throws std::bad_alloc Numeric parsing allocation fails.
     * @throws std::length_error Input exceeds string capacity.
     */
    [[nodiscard]] static CandlePriceLevel parse(std::optional<std::u16string_view> text);
    /** @param symbol Borrowed symbol; nullopt represents Java null.
     * @return Independent interval; absent attribute selects the default.
     * @throws std::invalid_argument Invalid interval text.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Attribute-copy allocation fails.
     */
    [[nodiscard]] static CandlePriceLevel getAttributeForSymbol(std::optional<std::u16string_view> symbol);
    /** @param symbol Borrowed symbol, not retained.
     * @return Independent symbol; the NaN default removes every matching key.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::optional<std::u16string>
    changeAttributeForSymbol(std::optional<std::u16string_view> symbol) const;
    /** @param symbol Borrowed symbol, not retained.
     * @return Independent normalized symbol; invalid values and canonical NaN remain unchanged.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] static std::optional<std::u16string>
    normalizeAttributeForSymbol(std::optional<std::u16string_view> symbol);
    /** @param other Independent interval.
     * @return Equal intervals; all NaNs compare equal.
     */
    [[nodiscard]] bool operator==(const CandlePriceLevel &other) const noexcept;
};

/** Open-ended, case-sensitive owned UTF-16 candle input-data selection.
 * Unknown strings are supported and preserved, including malformed UTF-16 and delimiters.
 * No schedule, runtime or input storage is retained. Const calls are independent; concurrent
 * mutation of one object requires synchronization.
 */
class CandleDataType final {
    // Canonical constants use program-lifetime literals; all caller-supplied values own a copy.
    struct CanonicalTag {};

    std::variant<std::u16string_view, std::u16string> value_;

    /** @param text Program-lifetime canonical literal, never caller storage. */
    constexpr CandleDataType(CanonicalTag, std::u16string_view text) noexcept : value_(std::in_place_index<0>, text) {
    }

    public:
    static constexpr std::u16string_view ATTRIBUTE_KEY = u"dt"; ///< Data-type key.
    static const CandleDataType SOURCE;                         ///< Source-specific rules.
    static const CandleDataType THO;                            ///< Regular-session selection.
    static const CandleDataType ALL;                            ///< All valid data.
    static const CandleDataType RTH;                            ///< Regular trading-hours data.
    static const CandleDataType ETH;                            ///< Extended trading-hours data.
    static const CandleDataType UNDEFINED;                      ///< Server-selected rules; empty string.
    static const CandleDataType DEFAULT;                        ///< Same value as UNDEFINED.
    /** Copy the value; null and empty both mean undefined.
     * @param value Borrowed text, not retained; nullopt is accepted.
     * @throws std::bad_alloc Owned-copy allocation fails.
     * @throws std::length_error Input exceeds string capacity.
     */
    explicit CandleDataType(std::optional<std::u16string_view> value = std::nullopt);
    /** @return Borrowed owned text, valid until this object's mutation or destruction. */
    [[nodiscard]] std::u16string_view getValue() const noexcept;
    /** @return Independent owned copy of the exact text.
     * @throws std::bad_alloc Copy allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::u16string toString() const;
    /** @param value Borrowed text, not retained; null and empty select undefined.
     * @return Independent data-type value.
     * @throws std::bad_alloc Copy allocation fails.
     * @throws std::length_error Input exceeds string capacity.
     */
    [[nodiscard]] static CandleDataType parse(std::optional<std::u16string_view> value);
    /** @param symbol Borrowed symbol; nullopt represents Java null.
     * @return Independent value; absent attribute selects undefined.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Copy allocation fails.
     */
    [[nodiscard]] static CandleDataType getAttributeForSymbol(std::optional<std::u16string_view> symbol);
    /** @param symbol Borrowed symbol, not retained.
     * @return Independent symbol; undefined removes every matching key.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::optional<std::u16string>
    changeAttributeForSymbol(std::optional<std::u16string_view> symbol) const;
    /** @param symbol Borrowed symbol, not retained.
     * @return Independent normalized symbol; explicit empty data type is removed.
     * @throws std::out_of_range Malformed symbol produces an invalid substring range.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] static std::optional<std::u16string>
    normalizeAttributeForSymbol(std::optional<std::u16string_view> symbol);
    /** @param other Independent data-type value.
     * @return Exact case-sensitive UTF-16 equality.
     */
    [[nodiscard]] bool operator==(const CandleDataType &other) const noexcept;
};

/** Any UTF-16 exchange code unit; zero means composite. No venue validation is performed. */
class CandleExchange final {
    char16_t exchangeCode_;

    public:
    static const CandleExchange COMPOSITE; ///< Zero code unit.
    static const CandleExchange DEFAULT;   ///< Composite exchange.

    /** @param exchangeCode Any code unit, including surrogates; zero removes the regional suffix. */
    constexpr explicit CandleExchange(char16_t exchangeCode = 0) noexcept : exchangeCode_(exchangeCode) {
    }

    /** @return Stored code unit; zero means composite. */
    [[nodiscard]] char16_t getExchangeCode() const noexcept;
    /** @return Independently owned COMPOSITE for zero, otherwise the single exact code unit.
     * @throws std::bad_alloc Output allocation fails.
     */
    [[nodiscard]] std::u16string toString() const;
    /** @param symbol Borrowed symbol; nullopt represents Java null.
     * @return Independent exchange value; absent suffix means composite.
     */
    [[nodiscard]] static CandleExchange getAttributeForSymbol(std::optional<std::u16string_view> symbol) noexcept;
    /** @param symbol Borrowed symbol, not retained.
     * @return Independent symbol with regional code replaced, or removed for composite.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::optional<std::u16string>
    changeAttributeForSymbol(std::optional<std::u16string_view> symbol) const;
    /** @param other Independent exchange value.
     * @return Exact code-unit equality.
     */
    [[nodiscard]] bool operator==(const CandleExchange &other) const noexcept = default;
};
} // namespace org::ttldtor::qdcxx::events
