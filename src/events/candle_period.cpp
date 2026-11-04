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
    return detail::javaLong(static_cast<double>(TYPES[static_cast<unsigned>(type_)].millis) * value_);
}

std::u16string CandlePeriod::toString() const {
    auto type = candleTypeShortName(type_);

    if (value_ == 1) {
        return std::u16string(type);
    }

    auto value = detail::formatCandleDouble(value_);
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
