// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_attributes.hpp"
#include "../detail/java_double.hpp"
#include "qd/events/market_event_symbols.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events {
namespace {
using Symbol = std::optional<std::u16string_view>;
using OwnedSymbol = std::optional<std::u16string>;
constexpr std::array<std::u16string_view, 5> PRICES{u"last", u"bid", u"ask", u"mark", u"s"};
constexpr std::array<std::u16string_view, 2> SESSIONS{u"false", u"true"};
constexpr std::array<std::u16string_view, 2> ALIGNMENTS{u"m", u"s"};

/** @param unit Candidate UTF-16 code unit.
 * @return Lowercase ASCII equivalent for Java case-insensitive comparison to ASCII.
 */
char16_t foldUnit(char16_t unit) noexcept {
    if (unit >= u'A' && unit <= u'Z') {
        return static_cast<char16_t>(unit - u'A' + u'a');
    }

    if (unit == u'\u0130' || unit == u'\u0131') {
        return u'i';
    }

    if (unit == u'\u017f') {
        return u's';
    }

    return unit == u'\u212a' ? u'k' : unit;
}

/** @param canonical Lowercase ASCII canonical value.
 * @param text Candidate exact or prefix value.
 * @param prefix Whether a nonempty prefix is accepted.
 * @return Java case-insensitive match against the canonical ASCII text.
 */
bool matches(std::u16string_view canonical, std::u16string_view text, bool prefix) noexcept {
    if (text.empty() || text.size() > canonical.size() || (!prefix && text.size() != canonical.size())) {
        return false;
    }

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (foldUnit(text[i]) != canonical[i]) {
            return false;
        }
    }

    return true;
}

/** @tparam Enum Selection enum with contiguous zero-based values.
 * @tparam Size Number of canonical selections.
 * @param value Enum value to validate.
 * @param names Static canonical names.
 * @return Borrowed canonical name.
 * @throws std::invalid_argument The enum is invalid.
 */
template <class Enum, std::size_t Size>
std::u16string_view enumText(Enum value, const std::array<std::u16string_view, Size> &names) {
    auto index = static_cast<unsigned>(value);

    if (index >= names.size()) {
        throw std::invalid_argument("Invalid candle attribute enum");
    }

    return names[index];
}

/** @tparam Enum Selection enum.
 * @tparam Size Number of canonical selections.
 * @param text Borrowed candidate; null is invalid.
 * @param names Static canonical names in selection order.
 * @param prefix Whether canonical prefixes are accepted.
 * @return First matching selection.
 * @throws std::invalid_argument Null or invalid selection text.
 */
template <class Enum, std::size_t Size>
Enum parseEnum(Symbol text, const std::array<std::u16string_view, Size> &names, bool prefix) {
    if (text) {
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (matches(names[i], *text, prefix)) {
                return static_cast<Enum>(i);
            }
        }
    }

    throw std::invalid_argument("Invalid candle attribute text");
}

/** @param symbol Borrowed optional symbol.
 * @return Independent copy, preserving null.
 * @throws std::bad_alloc Copy allocation fails.
 */
OwnedSymbol copySymbol(Symbol symbol) {
    return symbol ? OwnedSymbol(std::in_place, *symbol) : std::nullopt;
}

/** Keep the reference's discarded default-removal call, including its failure behavior.
 * @param symbol Borrowed original symbol.
 * @param key Attribute key.
 * @param original Borrowed original attribute value.
 * @param canonical Canonical parsed spelling.
 * @param isDefault Whether the parsed value is the default.
 * @return Independent normalized symbol.
 * @throws std::out_of_range Invalid substring range in a malformed symbol.
 * @throws std::bad_alloc Output allocation fails.
 */
OwnedSymbol normalize(Symbol symbol, std::u16string_view key, std::u16string_view original,
                      std::u16string_view canonical, bool isDefault) {
    if (isDefault) {
        (void)market_symbols::removeAttributeStringByKey(symbol, key);
    }

    return original == canonical ? copySymbol(symbol)
                                 : market_symbols::changeAttributeStringByKey(symbol, key, canonical);
}
} // namespace

std::u16string_view candlePriceText(CandlePrice value) {
    return enumText(value, PRICES);
}

CandlePrice parseCandlePrice(Symbol text) {
    return parseEnum<CandlePrice>(text, PRICES, true);
}

CandlePrice getCandlePrice(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_PRICE_ATTRIBUTE_KEY);

    return text ? parseCandlePrice(*text) : DEFAULT_CANDLE_PRICE;
}

OwnedSymbol changeCandlePrice(Symbol symbol, CandlePrice value) {
    auto text = candlePriceText(value);

    return value == DEFAULT_CANDLE_PRICE
               ? market_symbols::removeAttributeStringByKey(symbol, CANDLE_PRICE_ATTRIBUTE_KEY)
               : market_symbols::changeAttributeStringByKey(symbol, CANDLE_PRICE_ATTRIBUTE_KEY, text);
}

OwnedSymbol normalizeCandlePrice(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_PRICE_ATTRIBUTE_KEY);

    if (!text) {
        return copySymbol(symbol);
    }

    try {
        auto value = parseCandlePrice(*text);

        return normalize(symbol, CANDLE_PRICE_ATTRIBUTE_KEY, *text, candlePriceText(value),
                         value == DEFAULT_CANDLE_PRICE);
    } catch (const std::invalid_argument &) {
        return copySymbol(symbol);
    }
}

std::u16string_view candleSessionText(CandleSession value) {
    return enumText(value, SESSIONS);
}

CandleSession parseCandleSession(Symbol text) {
    return parseEnum<CandleSession>(text, SESSIONS, true);
}

CandleSession getCandleSession(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_SESSION_ATTRIBUTE_KEY);

    return text && matches(u"true", *text, false) ? CandleSession::REGULAR : DEFAULT_CANDLE_SESSION;
}

OwnedSymbol changeCandleSession(Symbol symbol, CandleSession value) {
    auto text = candleSessionText(value);

    return value == DEFAULT_CANDLE_SESSION
               ? market_symbols::removeAttributeStringByKey(symbol, CANDLE_SESSION_ATTRIBUTE_KEY)
               : market_symbols::changeAttributeStringByKey(symbol, CANDLE_SESSION_ATTRIBUTE_KEY, text);
}

OwnedSymbol normalizeCandleSession(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_SESSION_ATTRIBUTE_KEY);

    if (!text) {
        return copySymbol(symbol);
    }

    bool regular = matches(u"true", *text, false);

    if (!regular) {
        (void)market_symbols::removeAttributeStringByKey(symbol, CANDLE_SESSION_ATTRIBUTE_KEY);

        return copySymbol(symbol);
    }

    return normalize(symbol, CANDLE_SESSION_ATTRIBUTE_KEY, *text, u"true", false);
}

std::u16string_view candleAlignmentText(CandleAlignment value) {
    return enumText(value, ALIGNMENTS);
}

CandleAlignment parseCandleAlignment(Symbol text) {
    return parseEnum<CandleAlignment>(text, ALIGNMENTS, false);
}

CandleAlignment getCandleAlignment(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_ALIGNMENT_ATTRIBUTE_KEY);

    return text ? parseCandleAlignment(*text) : DEFAULT_CANDLE_ALIGNMENT;
}

OwnedSymbol changeCandleAlignment(Symbol symbol, CandleAlignment value) {
    auto text = candleAlignmentText(value);

    return value == DEFAULT_CANDLE_ALIGNMENT
               ? market_symbols::removeAttributeStringByKey(symbol, CANDLE_ALIGNMENT_ATTRIBUTE_KEY)
               : market_symbols::changeAttributeStringByKey(symbol, CANDLE_ALIGNMENT_ATTRIBUTE_KEY, text);
}

OwnedSymbol normalizeCandleAlignment(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, CANDLE_ALIGNMENT_ATTRIBUTE_KEY);

    if (!text) {
        return copySymbol(symbol);
    }

    try {
        auto value = parseCandleAlignment(*text);

        return normalize(symbol, CANDLE_ALIGNMENT_ATTRIBUTE_KEY, *text, candleAlignmentText(value),
                         value == DEFAULT_CANDLE_ALIGNMENT);
    } catch (const std::invalid_argument &) {
        return copySymbol(symbol);
    }
}

constinit const CandlePriceLevel CandlePriceLevel::DEFAULT{};

double CandlePriceLevel::getValue() const noexcept {
    return value_;
}

std::u16string CandlePriceLevel::toString() const {
    return detail::formatCandleDouble(value_);
}

CandlePriceLevel CandlePriceLevel::parse(Symbol text) {
    auto value = text ? detail::parseJavaDouble(*text) : std::nullopt;

    if (!value) {
        throw std::invalid_argument("Invalid candle price level text");
    }

    return CandlePriceLevel(*value);
}

CandlePriceLevel CandlePriceLevel::getAttributeForSymbol(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, ATTRIBUTE_KEY);

    return text ? parse(*text) : CandlePriceLevel{};
}

OwnedSymbol CandlePriceLevel::changeAttributeForSymbol(Symbol symbol) const {
    if (std::isnan(value_)) {
        return market_symbols::removeAttributeStringByKey(symbol, ATTRIBUTE_KEY);
    }

    return market_symbols::changeAttributeStringByKey(symbol, ATTRIBUTE_KEY, toString());
}

OwnedSymbol CandlePriceLevel::normalizeAttributeForSymbol(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, ATTRIBUTE_KEY);

    if (!text) {
        return copySymbol(symbol);
    }

    try {
        auto value = parse(*text);
        auto canonical = value.toString();

        return normalize(symbol, ATTRIBUTE_KEY, *text, canonical, std::isnan(value.value_));
    } catch (const std::invalid_argument &) {
        return copySymbol(symbol);
    }
}

bool CandlePriceLevel::operator==(const CandlePriceLevel &other) const noexcept {
    return value_ == other.value_ || (std::isnan(value_) && std::isnan(other.value_));
}

constinit const CandleDataType CandleDataType::SOURCE{CanonicalTag{}, u"source"};
constinit const CandleDataType CandleDataType::THO{CanonicalTag{}, u"tho"};
constinit const CandleDataType CandleDataType::ALL{CanonicalTag{}, u"all"};
constinit const CandleDataType CandleDataType::RTH{CanonicalTag{}, u"rth"};
constinit const CandleDataType CandleDataType::ETH{CanonicalTag{}, u"eth"};
constinit const CandleDataType CandleDataType::UNDEFINED{CanonicalTag{}, u""};
constinit const CandleDataType CandleDataType::DEFAULT{CanonicalTag{}, u""};

CandleDataType::CandleDataType(Symbol value) : value_(std::in_place_index<1>, value.value_or(u"")) {
}

std::u16string_view CandleDataType::getValue() const noexcept {
    if (auto canonical = std::get_if<std::u16string_view>(&value_)) {
        return *canonical;
    }

    return std::get<std::u16string>(value_);
}

std::u16string CandleDataType::toString() const {
    return std::u16string(getValue());
}

CandleDataType CandleDataType::parse(Symbol value) {
    return CandleDataType(value);
}

CandleDataType CandleDataType::getAttributeForSymbol(Symbol symbol) {
    auto value = market_symbols::getAttributeStringByKey(symbol, ATTRIBUTE_KEY);

    return value ? CandleDataType(*value) : CandleDataType{};
}

OwnedSymbol CandleDataType::changeAttributeForSymbol(Symbol symbol) const {
    return market_symbols::changeAttributeStringByKey(symbol, ATTRIBUTE_KEY,
                                                      getValue().empty() ? Symbol{} : Symbol{getValue()});
}

OwnedSymbol CandleDataType::normalizeAttributeForSymbol(Symbol symbol) {
    auto value = market_symbols::getAttributeStringByKey(symbol, ATTRIBUTE_KEY);

    return value && value->empty() ? market_symbols::removeAttributeStringByKey(symbol, ATTRIBUTE_KEY)
                                   : copySymbol(symbol);
}

bool CandleDataType::operator==(const CandleDataType &other) const noexcept {
    return getValue() == other.getValue();
}

constinit const CandleExchange CandleExchange::COMPOSITE{};
constinit const CandleExchange CandleExchange::DEFAULT{};

char16_t CandleExchange::getExchangeCode() const noexcept {
    return exchangeCode_;
}

std::u16string CandleExchange::toString() const {
    return exchangeCode_ == 0 ? std::u16string(u"COMPOSITE") : std::u16string(1, exchangeCode_);
}

CandleExchange CandleExchange::getAttributeForSymbol(Symbol symbol) noexcept {
    return CandleExchange(market_symbols::getExchangeCode(symbol));
}

OwnedSymbol CandleExchange::changeAttributeForSymbol(Symbol symbol) const {
    return market_symbols::changeExchangeCode(symbol, exchangeCode_);
}
} // namespace org::ttldtor::qdcxx::events
