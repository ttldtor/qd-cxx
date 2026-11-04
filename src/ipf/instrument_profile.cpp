// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile.hpp"
#include "../detail/java_double.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::ipf {
namespace {
constexpr std::array<FieldInfo, 31> FIELDS{{
    {InstrumentProfileField::TYPE, u"TYPE", FieldKind::TEXT},
    {InstrumentProfileField::SYMBOL, u"SYMBOL", FieldKind::TEXT},
    {InstrumentProfileField::DESCRIPTION, u"DESCRIPTION", FieldKind::TEXT},
    {InstrumentProfileField::LOCAL_SYMBOL, u"LOCAL_SYMBOL", FieldKind::TEXT},
    {InstrumentProfileField::LOCAL_DESCRIPTION, u"LOCAL_DESCRIPTION", FieldKind::TEXT},
    {InstrumentProfileField::COUNTRY, u"COUNTRY", FieldKind::TEXT},
    {InstrumentProfileField::OPOL, u"OPOL", FieldKind::TEXT},
    {InstrumentProfileField::EXCHANGE_DATA, u"EXCHANGE_DATA", FieldKind::TEXT},
    {InstrumentProfileField::EXCHANGES, u"EXCHANGES", FieldKind::TEXT},
    {InstrumentProfileField::CURRENCY, u"CURRENCY", FieldKind::TEXT},
    {InstrumentProfileField::BASE_CURRENCY, u"BASE_CURRENCY", FieldKind::TEXT},
    {InstrumentProfileField::CFI, u"CFI", FieldKind::TEXT},
    {InstrumentProfileField::ISIN, u"ISIN", FieldKind::TEXT},
    {InstrumentProfileField::SEDOL, u"SEDOL", FieldKind::TEXT},
    {InstrumentProfileField::CUSIP, u"CUSIP", FieldKind::TEXT},
    {InstrumentProfileField::ICB, u"ICB", FieldKind::INTEGER},
    {InstrumentProfileField::SIC, u"SIC", FieldKind::INTEGER},
    {InstrumentProfileField::MULTIPLIER, u"MULTIPLIER", FieldKind::NUMBER},
    {InstrumentProfileField::PRODUCT, u"PRODUCT", FieldKind::TEXT},
    {InstrumentProfileField::UNDERLYING, u"UNDERLYING", FieldKind::TEXT},
    {InstrumentProfileField::SPC, u"SPC", FieldKind::NUMBER},
    {InstrumentProfileField::ADDITIONAL_UNDERLYINGS, u"ADDITIONAL_UNDERLYINGS", FieldKind::TEXT},
    {InstrumentProfileField::MMY, u"MMY", FieldKind::TEXT},
    {InstrumentProfileField::EXPIRATION, u"EXPIRATION", FieldKind::DATE},
    {InstrumentProfileField::LAST_TRADE, u"LAST_TRADE", FieldKind::DATE},
    {InstrumentProfileField::STRIKE, u"STRIKE", FieldKind::NUMBER},
    {InstrumentProfileField::OPTION_TYPE, u"OPTION_TYPE", FieldKind::TEXT},
    {InstrumentProfileField::EXPIRATION_STYLE, u"EXPIRATION_STYLE", FieldKind::TEXT},
    {InstrumentProfileField::SETTLEMENT_STYLE, u"SETTLEMENT_STYLE", FieldKind::TEXT},
    {InstrumentProfileField::PRICE_INCREMENTS, u"PRICE_INCREMENTS", FieldKind::TEXT},
    {InstrumentProfileField::TRADING_HOURS, u"TRADING_HOURS", FieldKind::TEXT},
}};
constexpr std::array<std::u16string_view, 17> TYPES{
    u"CURRENCY", u"FOREX",  u"BOND",   u"INDEX",   u"STOCK",       u"ETF", u"MUTUAL_FUND", u"MONEY_MARKET_FUND",
    u"PRODUCT",  u"FUTURE", u"OPTION", u"WARRANT", u"CERTIFICATE", u"CFD", u"SPREAD",      u"OTHER",
    u"REMOVED"};
constexpr std::int64_t EPOCH_JULIAN_DAY = 2440588;
constexpr std::int64_t GREGORIAN_CUTOVER = 2299161;
constexpr std::int64_t GREGORIAN_CUTOVER_YEAR = 1582;
constexpr std::int64_t MILLIS_PER_DAY = 86400000;

std::u16string widen(std::string_view text) {
    return {text.begin(), text.end()};
}

std::int32_t javaInt(double value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }

    if (value >= std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }

    if (value <= std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }

    return static_cast<std::int32_t>(value);
}

std::int64_t floorDivide(std::int64_t numerator, std::int64_t denominator) noexcept {
    auto quotient = numerator / denominator;

    return quotient - (numerator % denominator < 0 ? 1 : 0);
}

std::u16string formatNumber(double value) {
    if (value == 0) {
        return {};
    }

    if (std::isnan(value)) {
        return u"NaN";
    }

    if (std::isinf(value)) {
        return value < 0 ? u"-Infinity" : u"Infinity";
    }

    if (value >= -0x1p63 && value <= 0x1p63) {
        auto integral = value >= 0x1p63 ? std::numeric_limits<std::int64_t>::max() : static_cast<std::int64_t>(value);

        if (value == static_cast<double>(integral)) {
            return widen(std::to_string(integral));
        }
    }

    auto magnitude = std::abs(value);
    auto decimal = detail::decimalDigits(magnitude);
    std::string text;

    if (magnitude > 1e-9 && magnitude < 1e12) {
        auto point = decimal.exponent + 1;

        if (point <= 0) {
            text = "0." + std::string(static_cast<std::size_t>(-point), '0') + decimal.digits;
        } else if (static_cast<std::size_t>(point) >= decimal.digits.size()) {
            text = decimal.digits + std::string(static_cast<std::size_t>(point) - decimal.digits.size(), '0');
        } else {
            text = decimal.digits;
            text.insert(static_cast<std::size_t>(point), 1, '.');
        }

        auto dot = text.find('.');

        if (dot != std::string::npos && text.size() - dot - 1 > 20) {
            auto cutoff = dot + 21;
            bool round = text[cutoff] > '5' ||
                         (text[cutoff] == '5' && (text.find_first_not_of('0', cutoff + 1) != std::string::npos ||
                                                  (text[cutoff - 1] - '0') % 2 != 0));

            if (text[cutoff] == '5' && text.find_first_not_of('0', cutoff + 1) == std::string::npos) {
                auto side = detail::compareBinaryToDecimal(magnitude, text);
                round = side > 0 || (side == 0 && (text[cutoff - 1] - '0') % 2 != 0);
            }

            text.resize(cutoff);

            if (round) {
                auto position = text.size();

                while (position > 0) {
                    --position;

                    if (text[position] == '.') {
                        continue;
                    }

                    if (text[position] != '9') {
                        ++text[position];
                        break;
                    }

                    text[position] = '0';
                }
            }
        }

        if (dot != std::string::npos) {
            while (text.back() == '0') {
                text.pop_back();
            }

            if (text.back() == '.') {
                text.pop_back();
            }
        }
    } else {
        text.push_back(decimal.digits.front());
        text.push_back('.');
        text += decimal.digits.size() == 1 ? "0" : decimal.digits.substr(1);
        text += "E" + std::to_string(decimal.exponent);
    }

    if (value < 0) {
        text.insert(text.begin(), '-');
    }

    return widen(text);
}

std::expected<double, ProfileError> parseNumber(std::u16string_view value) {
    auto parsed = detail::parseJavaDouble(value);

    if (!parsed) {
        return std::unexpected(ProfileError::INVALID_NUMBER);
    }

    return *parsed;
}

std::int64_t julianDay(std::int64_t year, std::int64_t month, std::int64_t day, bool gregorian) noexcept {
    auto adjustment = floorDivide(14 - month, 12);
    auto shiftedYear = year + 4800 - adjustment;
    auto shiftedMonth = month + 12 * adjustment - 3;
    auto result = day + floorDivide(153 * shiftedMonth + 2, 5) + 365 * shiftedYear + floorDivide(shiftedYear, 4);

    if (gregorian) {
        return result - floorDivide(shiftedYear, 100) + floorDivide(shiftedYear, 400) - 32045;
    }

    return result - 32083;
}

// java.text date parsing accepts Unicode decimal digits. Keep the Unicode decimal blocks
// explicit rather than depending on the host's changing locale or Unicode database.
int dateDigit(char16_t unit) noexcept {
    constexpr std::array<char16_t, 35> STARTS{u'0',   0x660,  0x6f0,  0x7c0,  0x966,  0x9e6,  0xa66,  0xae6,  0xb66,
                                              0xbe6,  0xc66,  0xce6,  0xd66,  0xe50,  0xed0,  0xf20,  0x1040, 0x1090,
                                              0x17e0, 0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90, 0x1b50, 0x1bb0, 0x1c40,
                                              0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xaa50, 0xabf0, 0xff10};

    for (auto start : STARTS) {
        if (unit >= start && unit <= start + 9) {
            return unit - start;
        }
    }

    return -1;
}

std::expected<std::int32_t, ProfileError> parseDate(std::u16string_view text) {
    std::size_t position = 0;
    auto part = [&]() -> std::optional<std::int64_t> {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
            ++position;
        }

        bool negative = position < text.size() && text[position] == '-';

        if (negative) {
            ++position;
        }

        auto first = position;
        std::uint64_t value = 0;
        bool overflow = false;

        while (position < text.size()) {
            auto digit = dateDigit(text[position]);

            if (digit < 0) {
                break;
            }

            if (value > (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + (negative ? 1U : 0U) -
                         static_cast<unsigned>(digit)) /
                            10) {
                overflow = true;
            } else if (!overflow) {
                value = value * 10 + static_cast<unsigned>(digit);
            }

            ++position;
        }

        if (first == position) {
            return std::nullopt;
        }

        if (overflow) {
            return negative ? std::numeric_limits<std::int32_t>::min() : std::numeric_limits<std::int32_t>::max();
        }

        auto narrowed =
            negative ? std::uint32_t{0} - static_cast<std::uint32_t>(value) : static_cast<std::uint32_t>(value);

        return std::bit_cast<std::int32_t>(narrowed);
    };
    auto year = part();

    if (!year || position == text.size() || text[position++] != '-') {
        return std::unexpected(ProfileError::INVALID_DATE);
    }

    auto month = part();

    if (!month || position == text.size() || text[position++] != '-') {
        return std::unexpected(ProfileError::INVALID_DATE);
    }

    auto day = part();

    if (!day) {
        return std::unexpected(ProfileError::INVALID_DATE);
    }

    auto zeroMonth = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(*month) - 1);
    auto normalizedYear = *year + floorDivide(zeroMonth, 12);
    auto normalizedMonth = zeroMonth - 12 * floorDivide(zeroMonth, 12) + 1;
    auto result = julianDay(normalizedYear, normalizedMonth, *day, true);

    // A pre-cutover input year keeps Julian arithmetic even when an overflowing day
    // carries the result beyond the cutover. The input year is tested before month normalization.
    if (*year < GREGORIAN_CUTOVER_YEAR || result < GREGORIAN_CUTOVER) {
        result = julianDay(normalizedYear, normalizedMonth, *day, false);
    }

    // Date uses signed milliseconds, followed by Java's truncation toward zero and int narrowing.
    auto wrappedMillis = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(result - EPOCH_JULIAN_DAY) *
                                                     static_cast<std::uint64_t>(MILLIS_PER_DAY));
    auto days = wrappedMillis / MILLIS_PER_DAY;

    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(days));
}

std::u16string formatDate(std::int32_t day) {
    if (day == 0) {
        return {};
    }

    auto jdn = static_cast<std::int64_t>(day) + EPOCH_JULIAN_DAY;
    std::int64_t year, month, date;

    if (jdn >= GREGORIAN_CUTOVER) {
        auto a = jdn + 32044;
        auto b = floorDivide(4 * a + 3, 146097);
        auto c = a - floorDivide(146097 * b, 4);
        auto d = floorDivide(4 * c + 3, 1461);
        auto e = c - floorDivide(1461 * d, 4);
        auto m = floorDivide(5 * e + 2, 153);
        date = e - floorDivide(153 * m + 2, 5) + 1;
        month = m + 3 - 12 * floorDivide(m, 10);
        year = 100 * b + d - 4800 + floorDivide(m, 10);
    } else {
        auto c = jdn + 32082;
        auto d = floorDivide(4 * c + 3, 1461);
        auto e = c - floorDivide(1461 * d, 4);
        auto m = floorDivide(5 * e + 2, 153);
        date = e - floorDivide(153 * m + 2, 5) + 1;
        month = m + 3 - 12 * floorDivide(m, 10);
        year = d - 4800 + floorDivide(m, 10);
    }

    auto digits = [](std::int64_t value, std::size_t width) {
        auto text = std::to_string(value);

        if (text.size() < width) {
            text.insert(0, width - text.size(), '0');
        }

        return text;
    };

    return widen(digits(year <= 0 ? 1 - year : year, 4) + "-" + digits(month, 2) + "-" + digits(date, 2));
}
} // namespace

const std::array<FieldInfo, 31> &standardFields() noexcept {
    return FIELDS;
}

const FieldInfo *findField(std::u16string_view name) noexcept {
    auto found = std::ranges::find(FIELDS, name, &FieldInfo::name);

    return found == FIELDS.end() ? nullptr : &*found;
}

const std::array<std::u16string_view, 17> &standardTypes() noexcept {
    return TYPES;
}

int compareTypes(std::u16string_view left, std::u16string_view right) noexcept {
    auto first = std::ranges::find(TYPES, left), second = std::ranges::find(TYPES, right);

    if (first == TYPES.end() && second == TYPES.end()) {
        return left.compare(right);
    }

    return first < second ? -1 : first > second ? 1 : 0;
}

InstrumentProfile::InstrumentProfile(ProfileLimits limits) : limits_(limits) {
}

InstrumentProfile::InstrumentProfile(InstrumentProfile &&other) noexcept(NOTHROW_MOVE_CONSTRUCTION)
    : limits_(other.limits_) {
    static_assert(std::is_nothrow_default_constructible_v<Values> ==
                  std::is_nothrow_default_constructible_v<decltype(Value::text)>);

    values_.swap(other.values_);
    custom_.swap(other.custom_);
    std::swap(totalUnits_, other.totalUnits_);
}

InstrumentProfile &InstrumentProfile::operator=(InstrumentProfile other) noexcept {
    std::swap(limits_, other.limits_);
    values_.swap(other.values_);
    custom_.swap(other.custom_);
    std::swap(totalUnits_, other.totalUnits_);

    return *this;
}

ProfileLimits InstrumentProfile::limits() const noexcept {
    return limits_;
}

std::size_t InstrumentProfile::customFieldCount() const noexcept {
    return custom_.size();
}

std::size_t InstrumentProfile::retainedUnits() const noexcept {
    return totalUnits_;
}

std::u16string_view InstrumentProfile::getField(std::u16string_view name) const noexcept {
    if (auto field = findField(name)) {
        return values_[static_cast<std::size_t>(field->field)].text;
    }

    auto found = custom_.find(name);

    return found == custom_.end() ? std::u16string_view{} : found->second;
}

std::expected<void, ProfileError> InstrumentProfile::store(std::u16string_view name, std::u16string text,
                                                           double number) {
    if (name.size() > limits_.nameUnits || text.size() > limits_.valueUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    auto field = findField(name);
    auto found = custom_.find(name);
    auto oldUnits = field                    ? values_[static_cast<std::size_t>(field->field)].text.size()
                    : found == custom_.end() ? 0
                                             : found->first.size() + found->second.size();
    auto newUnits = text.size();

    if (!field && !text.empty()) {
        if (name.size() > std::numeric_limits<std::size_t>::max() - newUnits) {
            return std::unexpected(ProfileError::LIMIT_EXCEEDED);
        }

        newUnits += name.size();

        if (found == custom_.end() && custom_.size() >= limits_.customFields) {
            return std::unexpected(ProfileError::LIMIT_EXCEEDED);
        }
    }

    auto retained = totalUnits_ - oldUnits;

    if (retained > limits_.totalUnits || newUnits > limits_.totalUnits - retained) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    if (field) {
        auto &destination = values_[static_cast<std::size_t>(field->field)];
        destination.text.swap(text);
        destination.number = number;
    } else if (text.empty()) {
        if (found != custom_.end()) {
            custom_.erase(found);
        }
    } else if (found == custom_.end()) {
        custom_.emplace(std::u16string(name), std::move(text));
    } else {
        found->second.swap(text);
    }

    totalUnits_ = retained + newUnits;

    return {};
}

std::expected<void, ProfileError> InstrumentProfile::setField(std::u16string_view name, std::u16string_view value) {
    if (name.size() > limits_.nameUnits || value.size() > limits_.inputUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    auto field = findField(name);

    if (!field || field->kind == FieldKind::TEXT) {
        if (value.size() > limits_.valueUnits) {
            return std::unexpected(ProfileError::LIMIT_EXCEEDED);
        }

        return store(name, std::u16string(value), 0);
    }

    if (field->kind == FieldKind::DATE) {
        auto parsed = value.empty() ? std::expected<std::int32_t, ProfileError>{0} : parseDate(value);

        if (!parsed) {
            return std::unexpected(parsed.error());
        }

        return store(name, formatDate(*parsed), *parsed);
    }

    auto parsed = value.empty() ? std::expected<double, ProfileError>{0} : parseNumber(value);

    if (!parsed) {
        return std::unexpected(parsed.error());
    }

    return setNumericField(name, *parsed);
}

std::expected<void, ProfileError> InstrumentProfile::setField(std::u16string_view name, std::nullptr_t) {
    return setField(name, std::u16string_view{});
}

std::expected<double, ProfileError> InstrumentProfile::getNumericField(std::u16string_view name) const {
    if (name.size() > limits_.nameUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    if (auto field = findField(name)) {
        if (field->kind == FieldKind::TEXT) {
            return std::unexpected(ProfileError::TEXTUAL_FIELD);
        }

        return values_[static_cast<std::size_t>(field->field)].number;
    }

    auto value = getField(name);

    if (value.empty()) {
        return 0;
    }

    if (value.size() > limits_.inputUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    if (value.size() == 10 && value[4] == '-' && value[7] == '-') {
        auto parsed = parseDate(value);

        if (!parsed) {
            return std::unexpected(parsed.error());
        }

        return *parsed;
    }

    return parseNumber(value);
}

std::expected<void, ProfileError> InstrumentProfile::setNumericField(std::u16string_view name, double value) {
    if (name.size() > limits_.nameUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    if (auto field = findField(name)) {
        if (field->kind == FieldKind::TEXT) {
            return std::unexpected(ProfileError::TEXTUAL_FIELD);
        }

        if (field->kind == FieldKind::INTEGER || field->kind == FieldKind::DATE) {
            value = javaInt(value);
        }

        return store(name, field->kind == FieldKind::DATE ? formatDate(javaInt(value)) : formatNumber(value), value);
    }

    return store(name, formatNumber(value), 0);
}

std::expected<std::int32_t, ProfileError> InstrumentProfile::getDateField(std::u16string_view name) const {
    if (name.size() > limits_.nameUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    if (auto field = findField(name)) {
        if (field->kind == FieldKind::TEXT) {
            return std::unexpected(ProfileError::TEXTUAL_FIELD);
        }

        return javaInt(values_[static_cast<std::size_t>(field->field)].number);
    }

    auto value = getField(name);

    if (value.empty()) {
        return 0;
    }

    if (value.size() > limits_.inputUnits) {
        return std::unexpected(ProfileError::LIMIT_EXCEEDED);
    }

    return parseDate(value);
}

std::expected<void, ProfileError> InstrumentProfile::setDateField(std::u16string_view name, std::int32_t value) {
    if (findField(name)) {
        return setNumericField(name, value);
    }

    return store(name, formatDate(value), 0);
}

std::vector<std::u16string> InstrumentProfile::customFieldNames() const {
    std::vector<std::u16string> result;
    result.reserve(custom_.size());

    for (auto &[name, value] : custom_) {
        result.push_back(name);
    }

    return result;
}

bool InstrumentProfile::sameSymbol(const InstrumentProfile &other) const noexcept {
    return getSymbol() == other.getSymbol();
}

bool InstrumentProfile::operator==(const InstrumentProfile &other) const noexcept {
    if (custom_ != other.custom_) {
        return false;
    }

    for (std::size_t i = 0; i < FIELDS.size(); ++i) {
        if (FIELDS[i].kind == FieldKind::TEXT) {
            if (values_[i].text != other.values_[i].text) {
                return false;
            }
        } else {
            auto left = values_[i].number, right = other.values_[i].number;

            if (std::isnan(left) && std::isnan(right)) {
                continue;
            }

            if (std::bit_cast<std::uint64_t>(left) != std::bit_cast<std::uint64_t>(right)) {
                return false;
            }
        }
    }

    return true;
}

int InstrumentProfile::compareTo(const InstrumentProfile &other) const noexcept {
    if (auto order = compareTypes(getType(), other.getType())) {
        return order;
    }

    if (auto order = getProduct().compare(other.getProduct())) {
        return order;
    }

    if (auto order = getUnderlying().compare(other.getUnderlying())) {
        return order;
    }

    if (getLastTrade() != other.getLastTrade()) {
        return getLastTrade() < other.getLastTrade() ? -1 : 1;
    }

    if (getStrike() < other.getStrike()) {
        return -1;
    }

    if (getStrike() > other.getStrike()) {
        return 1;
    }

    return getSymbol().compare(other.getSymbol());
}
} // namespace org::ttldtor::qdcxx::ipf
