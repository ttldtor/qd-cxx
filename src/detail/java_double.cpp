// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "java_double.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::detail {
namespace {
constexpr std::uint64_t FRACTION_MASK = (std::uint64_t{1} << 52) - 1;
constexpr std::uint64_t EXPONENT_MASK = 0x7ff;

// Fixed-capacity arithmetic keeps binary64 conversion independent of ambient locales and
// arbitrary-precision dependencies. The largest scaled binary64 operand needs fewer than
// 1200 bits; 1536 bits also cover the final decimal-digit and rounding comparisons.
class DecimalInteger {
    public:
    /** @param value Initial unsigned magnitude; remaining fixed-capacity words are zero. */
    explicit DecimalInteger(std::uint64_t value = 0) {
        words_[0] = static_cast<std::uint32_t>(value);
        words_[1] = static_cast<std::uint32_t>(value >> 32);
    }

    /** @param factor Small unsigned factor; the caller ensures the fixed capacity suffices. */
    void multiply(std::uint32_t factor) {
        std::uint64_t carry = 0;

        for (auto &word : words_) {
            auto product = std::uint64_t{word} * factor + carry;
            word = static_cast<std::uint32_t>(product);
            carry = product >> 32;
        }
    }

    /** @param exponent Nonnegative number of factors of five to apply. */
    void powerFive(int exponent) {
        for (int i = 0; i < exponent; ++i) {
            multiply(5);
        }
    }

    /** @param bits Nonnegative binary left shift within the fixed capacity. */
    void shift(int bits) {
        for (int i = 0; i < bits; ++i) {
            multiply(2);
        }
    }

    /** @param other Unsigned magnitude to compare.
     * @return Negative, zero or positive according to unsigned magnitude order.
     */
    int compare(const DecimalInteger &other) const noexcept {
        for (auto i = words_.size(); i > 0; --i) {
            if (words_[i - 1] != other.words_[i - 1]) {
                return words_[i - 1] < other.words_[i - 1] ? -1 : 1;
            }
        }

        return 0;
    }

    /** @param other Unsigned magnitude not greater than this value. */
    void subtract(const DecimalInteger &other) noexcept {
        std::uint64_t borrow = 0;

        for (std::size_t i = 0; i < words_.size(); ++i) {
            auto right = std::uint64_t{other.words_[i]} + borrow;
            auto left = std::uint64_t{words_[i]};
            words_[i] = static_cast<std::uint32_t>(left - right);
            borrow = left < right ? 1 : 0;
        }
    }

    /** @param other Unsigned magnitude to add within the fixed capacity. */
    void add(const DecimalInteger &other) noexcept {
        std::uint64_t carry = 0;

        for (std::size_t i = 0; i < words_.size(); ++i) {
            auto sum = std::uint64_t{words_[i]} + other.words_[i] + carry;
            words_[i] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32;
        }
    }

    /** @return Low 64 bits, used for explicit modulo arithmetic compatibility. */
    std::uint64_t lowWord() const noexcept {
        return std::uint64_t{words_[0]} | (std::uint64_t{words_[1]} << 32);
    }

    /** @param denominator Positive divisor; the caller bounds the quotient to a decimal digit.
     * @return Quotient digit; this object retains the remainder.
     */
    int digit(const DecimalInteger &denominator) noexcept {
        int value = 0;

        while (compare(denominator) >= 0) {
            subtract(denominator);
            ++value;
        }

        return value;
    }

    private:
    std::array<std::uint32_t, 48> words_{};
};

} // namespace

// Generate decimal digits from the exact rational value and a symmetric binary rounding
// interval. The strict small-operand boundary and two-digit scientific minimum are
// observable compatibility details; see spec/ipf/PROFILE.md and spec/events/CANDLE-PERIOD.md, not host to_chars
// formatting.
DecimalDigits decimalDigits(double value) {
    auto bits = std::bit_cast<std::uint64_t>(value);
    auto fraction = bits & FRACTION_MASK;
    auto encodedExponent = static_cast<int>((bits >> 52) & EXPONENT_MASK);
    int binaryExponent;
    int significantBits;

    if (encodedExponent == 0) {
        significantBits = 64 - std::countl_zero(fraction);
        auto shift = 53 - significantBits;
        fraction <<= shift;
        binaryExponent = -1022 - shift;
    } else {
        fraction |= std::uint64_t{1} << 52;
        binaryExponent = encodedExponent - 1023;
        significantBits = 53;
    }

    auto trailingZeros = std::countr_zero(fraction);
    auto fractionBits = 53 - trailingZeros;
    fraction >>= trailingZeros;
    // The historical conversion selects its arithmetic boundary using a first-order log10
    // estimate around normalized mantissa 1.5, with these compatibility-visible rounded
    // coefficients. An exact log10 changes a few final digits and subnormal representations.
    constexpr double LOG_SLOPE = 0.289529654;
    constexpr double LOG_CENTER = 0.176091259;
    constexpr double LOG_TWO = 0.301029995663981;
    auto normalized = std::ldexp(static_cast<double>(fraction), -(fractionBits - 1));
    int exponent = static_cast<int>(std::floor((normalized - 1.5) * LOG_SLOPE + LOG_CENTER + binaryExponent * LOG_TWO));
    auto numeratorFives = std::max(0, -exponent);
    auto denominatorFives = std::max(0, exponent);
    auto numeratorTwos = binaryExponent - fractionBits + 1 - exponent;
    auto denominatorTwos = 0;
    auto marginTwos = binaryExponent - significantBits - exponent - (fractionBits == 1 ? 1 : 0);
    auto common = std::min({numeratorTwos, denominatorTwos, marginTwos});
    numeratorTwos -= common;
    denominatorTwos -= common;
    marginTwos -= common;
    auto fiveBits = [](int power) {
        return power == 0 ? 0 : static_cast<int>(std::ceil(power * std::log2(5.0)));
    };
    auto numeratorBits = fractionBits + numeratorTwos + fiveBits(numeratorFives);
    auto denominatorBits = denominatorTwos + 1 + fiveBits(denominatorFives + 1);
    auto arithmeticWidth = numeratorBits < 32 && denominatorBits < 32   ? 32
                           : numeratorBits < 64 && denominatorBits < 64 ? 64
                                                                        : 0;
    DecimalInteger numerator(fraction), denominator(1), margin(1);
    numerator.powerFive(numeratorFives);
    numerator.shift(numeratorTwos);
    denominator.powerFive(denominatorFives);
    denominator.shift(denominatorTwos);
    margin.powerFive(numeratorFives);
    margin.shift(marginTwos);
    auto tenDenominator = denominator;
    tenDenominator.multiply(10);
    std::string digits;
    bool low = false, high = false;

    for (int iteration = 0; iteration < 20; ++iteration) {
        auto digit = numerator.digit(denominator);
        numerator.multiply(10);
        margin.multiply(10);
        low = numerator.compare(margin) < 0;
        auto upper = numerator;
        upper.add(margin);
        high = upper.compare(tenDenominator) >= 0;

        if (arithmeticWidth != 0) {
            // Small-operand reference arithmetic wraps signed32/signed64 addition. Use
            // unsigned modulo operations explicitly; signed overflow would be undefined C++.
            auto mask = arithmeticWidth == 32 ? std::uint64_t{0xffffffff} : std::numeric_limits<std::uint64_t>::max();
            auto sign = std::uint64_t{1} << (arithmeticWidth - 1);
            auto wrappedMargin = margin.lowWord() & mask;
            auto wrappedUpper = upper.lowWord() & mask;
            bool positiveMargin = wrappedMargin != 0 && (wrappedMargin & sign) == 0;
            low = positiveMargin && numerator.lowWord() < wrappedMargin;
            high = (wrappedUpper & sign) == 0 && wrappedUpper > tenDenominator.lowWord();

            if (iteration != 0 && !positiveMargin) {
                low = true;
                high = true;
            }
        }

        if (iteration == 0 && digit == 0 && !high) {
            --exponent;
        } else {
            digits.push_back(static_cast<char>('0' + digit));
        }

        if (digits.size() == 1 && (exponent < -3 || exponent >= 8)) {
            low = false;
            high = false;
        }

        if (!digits.empty() && (low || high)) {
            break;
        }
    }

    auto twiceRemainder = numerator;
    twiceRemainder.multiply(2);
    auto half = twiceRemainder.compare(tenDenominator);

    if (high && (!low || half > 0 || (half == 0 && ((digits.back() - '0') % 2 != 0)))) {
        auto position = digits.size();

        while (position > 0 && digits[position - 1] == '9') {
            digits[--position] = '0';
        }

        if (position == 0) {
            digits[0] = '1';
            ++exponent;
        } else {
            ++digits[position - 1];
        }
    }

    return {std::move(digits), exponent};
}

// Compare a positive finite binary64 value with its complete decimal text exactly. This is
// needed only when the shortest representation itself lands on the fractional rounding tie;
// that representation may already have rounded away from the actual binary value.
int compareBinaryToDecimal(double value, std::string_view text) {
    DecimalInteger decimal(0);
    int fractionalDigits = 0;
    bool fractional = false;

    for (auto digit : text) {
        if (digit == '.') {
            fractional = true;
            continue;
        }

        decimal.multiply(10);
        decimal.add(DecimalInteger(static_cast<unsigned>(digit - '0')));

        if (fractional) {
            ++fractionalDigits;
        }
    }

    auto bits = std::bit_cast<std::uint64_t>(value);
    auto significand = (bits & FRACTION_MASK) | (std::uint64_t{1} << 52);
    auto binaryExponent = static_cast<int>((bits >> 52) & EXPONENT_MASK) - 1023 - 52;
    DecimalInteger binary(significand);
    binary.powerFive(fractionalDigits);
    auto binaryShift = binaryExponent + fractionalDigits;

    if (binaryShift >= 0) {
        binary.shift(binaryShift);
    } else {
        decimal.shift(-binaryShift);
    }

    return binary.compare(decimal);
}

std::optional<double> parseJavaDouble(std::u16string_view value) {
    while (!value.empty() && value.front() <= u' ') {
        value.remove_prefix(1);
    }

    while (!value.empty() && value.back() <= u' ') {
        value.remove_suffix(1);
    }

    if (value.empty()) {
        return std::nullopt;
    }

    bool negative = value.front() == '-';

    if (value.front() == '+' || negative) {
        value.remove_prefix(1);
    }

    if (value == u"NaN") {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (value == u"Infinity") {
        return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }

    if (!value.empty() &&
        (value.back() == u'f' || value.back() == u'F' || value.back() == u'd' || value.back() == u'D')) {
        value.remove_suffix(1);
    }

    bool hexadecimal = value.size() >= 2 && value[0] == u'0' && (value[1] == u'x' || value[1] == u'X');

    if (hexadecimal) {
        value.remove_prefix(2);
    }

    std::string ascii;
    ascii.reserve(value.size());

    for (auto unit : value) {
        if (unit > 127) {
            return std::nullopt;
        }

        ascii.push_back(static_cast<char>(unit));
    }

    auto exponentPosition = ascii.find_first_of(hexadecimal ? "pP" : "eE");

    if (hexadecimal && exponentPosition == std::string::npos) {
        return std::nullopt;
    }

    auto significand = std::string_view(ascii).substr(0, exponentPosition);
    std::size_t digits = 0;
    bool pointSeen = false;

    for (auto c : significand) {
        if (c == '.' && !pointSeen) {
            pointSeen = true;
            continue;
        }

        bool valid = c >= '0' && c <= '9';
        valid = valid || (hexadecimal && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));

        if (!valid) {
            return std::nullopt;
        }

        ++digits;
    }

    if (digits == 0) {
        return std::nullopt;
    }

    std::int64_t exponent = 0;
    // An exponent beyond the complete input length plus the binary64 decimal range
    // cannot be cancelled by significand digits; saturation preserves overflow direction.
    auto exponentLimit =
        static_cast<std::int64_t>(std::min<std::size_t>(ascii.size(), std::numeric_limits<std::int64_t>::max() / 16)) +
        4096;

    if (exponentPosition != std::string::npos) {
        auto rest = std::string_view(ascii).substr(exponentPosition + 1);
        bool exponentNegative = !rest.empty() && rest.front() == '-';

        if (!rest.empty() && (rest.front() == '+' || rest.front() == '-')) {
            rest.remove_prefix(1);
        }

        if (rest.empty()) {
            return std::nullopt;
        }

        for (auto c : rest) {
            if (c < '0' || c > '9') {
                return std::nullopt;
            }

            exponent = std::min(exponentLimit, exponent * 10 + c - '0');
        }

        if (exponentNegative) {
            exponent = -exponent;
        }
    }

    double result = 0;
    auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), result,
                                  hexadecimal ? std::chars_format::hex : std::chars_format::general);

    if (parsed.ptr != ascii.data() + ascii.size()) {
        return std::nullopt;
    }

    if (parsed.ec == std::errc::result_out_of_range) {
        auto point = significand.find('.');
        auto firstNonzero = significand.find_first_not_of("0.");

        if (firstNonzero == std::string_view::npos) {
            result = 0;
        } else {
            auto integralDigits = point == std::string_view::npos ? significand.size() : point;
            auto offset = static_cast<std::int64_t>(integralDigits) - static_cast<std::int64_t>(firstNonzero) -
                          (firstNonzero < integralDigits ? 1 : 0);
            auto scale = exponent + offset * (hexadecimal ? 4 : 1);
            result = scale >= 0 ? std::numeric_limits<double>::infinity() : 0;
        }
    } else if (parsed.ec != std::errc{}) {
        return std::nullopt;
    }

    return negative ? -result : result;
}

} // namespace org::ttldtor::qdcxx::detail
