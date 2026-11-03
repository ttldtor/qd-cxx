// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_period.hpp"
#include "../detail/java_double.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events {
namespace {
struct TypeInfo {
    std::u16string_view name;
    std::u16string_view shortName;
    std::int64_t millis;
};

constexpr std::array<TypeInfo, 13> TYPES{{{u"TICK", u"t", 0},
                                          {u"SECOND", u"s", 1000},
                                          {u"MINUTE", u"m", 60000},
                                          {u"HOUR", u"h", 3600000},
                                          {u"DAY", u"d", 86400000},
                                          {u"WEEK", u"w", 604800000},
                                          {u"MONTH", u"mo", 2592000000},
                                          {u"OPTEXP", u"o", 2592000000},
                                          {u"YEAR", u"y", 31536000000},
                                          {u"VOLUME", u"v", 0},
                                          {u"PRICE", u"p", 0},
                                          {u"PRICE_MOMENTUM", u"pm", 0},
                                          {u"PRICE_RENKO", u"pr", 0}}};

/** @param type Aggregation unit to validate.
 * @return Static metadata for a valid unit.
 * @throws std::invalid_argument The enum value is invalid.
 */
const TypeInfo &typeInfo(CandleType type) {
    auto index = static_cast<unsigned>(type);

    if (index >= TYPES.size()) {
        throw std::invalid_argument("Invalid candle type");
    }

    return TYPES[index];
}

/** @param unit UTF-16 code unit to compare with an ASCII enum name.
 * @return ASCII uppercase equivalent, including Java's non-ASCII ASCII-letter equivalents.
 */
char16_t foldNameUnit(char16_t unit) noexcept {
    if (unit >= u'a' && unit <= u'z') {
        return static_cast<char16_t>(unit - u'a' + u'A');
    }

    if (unit == u'\u0130' || unit == u'\u0131') {
        return u'I';
    }

    if (unit == u'\u017f') {
        return u'S';
    }

    return unit == u'\u212a' ? u'K' : unit;
}

/** @param name Uppercase ASCII enum name.
 * @param text Borrowed candidate prefix.
 * @return True when text is a Java case-insensitive prefix of name.
 */
bool namePrefix(std::u16string_view name, std::u16string_view text) noexcept {
    if (text.size() > name.size()) {
        return false;
    }

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (foldNameUnit(text[i]) != name[i]) {
            return false;
        }
    }

    return true;
}

/** @param value Any binary64 value.
 * @return Java narrowing conversion: NaN zero, saturated overflow, truncation otherwise.
 */
std::int64_t javaLong(double value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }

    if (value >= 0x1p63) {
        return std::numeric_limits<std::int64_t>::max();
    }

    if (value <= -0x1p63) {
        return std::numeric_limits<std::int64_t>::min();
    }

    return static_cast<std::int64_t>(value);
}

/** @param text ASCII numeric text.
 * @return Independently owned UTF-16 copy.
 * @throws std::bad_alloc Output allocation fails.
 */
std::u16string widen(std::string_view text) {
    return {text.begin(), text.end()};
}

/** @param value Multiplier outside the signed64 rendering branch, including nonfinite values.
 * @return Java Double.toString spelling using the shared reference digit engine.
 * @throws std::bad_alloc Output allocation fails.
 */
std::u16string doubleText(double value) {
    if (std::isnan(value)) {
        return u"NaN";
    }

    if (std::isinf(value)) {
        return value < 0 ? u"-Infinity" : u"Infinity";
    }

    auto magnitude = std::abs(value);
    auto decimal = detail::decimalDigits(magnitude);
    std::string text;

    if (magnitude >= 1e-3 && magnitude < 1e7) {
        auto point = decimal.exponent + 1;

        if (point <= 0) {
            text = "0." + std::string(static_cast<std::size_t>(-point), '0') + decimal.digits;
        } else if (static_cast<std::size_t>(point) >= decimal.digits.size()) {
            text = decimal.digits + std::string(static_cast<std::size_t>(point) - decimal.digits.size(), '0') + ".0";
        } else {
            text = decimal.digits;
            text.insert(static_cast<std::size_t>(point), 1, '.');
        }
    } else {
        text = decimal.digits.substr(0, 1) + ".";
        text += decimal.digits.size() == 1 ? "0" : decimal.digits.substr(1);
        text += "E" + std::to_string(decimal.exponent);
    }

    if (value < 0) {
        text.insert(text.begin(), '-');
    }

    return widen(text);
}
} // namespace

std::u16string_view candleTypeShortName(CandleType type) {
    return typeInfo(type).shortName;
}

CandleType parseCandleType(std::u16string_view text) {
    if (text.empty()) {
        throw std::invalid_argument("Missing candle type");
    }

    for (std::size_t i = 0; i < TYPES.size(); ++i) {
        if (text == TYPES[i].shortName) {
            return static_cast<CandleType>(i);
        }
    }

    for (std::size_t i = 0; i < TYPES.size(); ++i) {
        auto name = TYPES[i].name;
        bool plural =
            text.back() == u's' && text.size() == name.size() + 1 && namePrefix(name, text.substr(0, text.size() - 1));

        if (namePrefix(name, text) || plural) {
            return static_cast<CandleType>(i);
        }
    }

    throw std::invalid_argument("Unknown candle type");
}

std::int64_t candleTypeIntervalMillis(CandleType type) {
    return typeInfo(type).millis;
}

constinit const CandlePeriod CandlePeriod::TICK{};
constinit const CandlePeriod CandlePeriod::DAY{1, CandleType::DAY};
constinit const CandlePeriod CandlePeriod::DEFAULT{};

double CandlePeriod::getValue() const noexcept {
    return value_;
}

CandleType CandlePeriod::getType() const noexcept {
    return type_;
}

std::int64_t CandlePeriod::getPeriodIntervalMillis() const noexcept {
    return javaLong(static_cast<double>(TYPES[static_cast<unsigned>(type_)].millis) * value_);
}

std::u16string CandlePeriod::toString() const {
    auto type = candleTypeShortName(type_);

    if (value_ == 1) {
        return std::u16string(type);
    }

    auto integer = javaLong(value_);
    auto value = value_ == static_cast<double>(integer) ? widen(std::to_string(integer)) : doubleText(value_);
    value += type;

    return value;
}

CandlePeriod CandlePeriod::parse(std::u16string_view text) {
    auto numeric = text.find_first_not_of(u"0123456789.+-eE");

    if (numeric == std::u16string_view::npos) {
        numeric = text.size();
    }

    auto value = numeric == 0 ? std::optional<double>{1} : detail::parseJavaDouble(text.substr(0, numeric));

    if (!value) {
        throw std::invalid_argument("Invalid candle multiplier");
    }

    return CandlePeriod(*value, parseCandleType(text.substr(numeric)));
}

bool CandlePeriod::operator==(const CandlePeriod &other) const noexcept {
    return type_ == other.type_ && ((std::isnan(value_) && std::isnan(other.value_)) ||
                                    std::bit_cast<std::uint64_t>(value_) == std::bit_cast<std::uint64_t>(other.value_));
}
} // namespace org::ttldtor::qdcxx::events
