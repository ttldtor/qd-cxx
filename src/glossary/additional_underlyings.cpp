// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/glossary/additional_underlyings.hpp"
#include "../detail/java_double.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace org::ttldtor::qdcxx::glossary {
namespace {
constexpr int MAX_POWER = 308;
constexpr unsigned FORMAT_PRECISION = 15;
// Binary64 scientific text needs at most 24 ASCII characters, including sign and exponent.
constexpr std::size_t NUMBER_BUFFER_CAPACITY = 64;
constexpr double FIXED_MIN = 1e-9;
constexpr double FIXED_MAX = 1e10;
constexpr std::array<double, MAX_POWER + 1> POWERS{
    1e0,   1e1,   1e2,   1e3,   1e4,   1e5,   1e6,   1e7,   1e8,   1e9,   1e10,  1e11,  1e12,  1e13,  1e14,  1e15,
    1e16,  1e17,  1e18,  1e19,  1e20,  1e21,  1e22,  1e23,  1e24,  1e25,  1e26,  1e27,  1e28,  1e29,  1e30,  1e31,
    1e32,  1e33,  1e34,  1e35,  1e36,  1e37,  1e38,  1e39,  1e40,  1e41,  1e42,  1e43,  1e44,  1e45,  1e46,  1e47,
    1e48,  1e49,  1e50,  1e51,  1e52,  1e53,  1e54,  1e55,  1e56,  1e57,  1e58,  1e59,  1e60,  1e61,  1e62,  1e63,
    1e64,  1e65,  1e66,  1e67,  1e68,  1e69,  1e70,  1e71,  1e72,  1e73,  1e74,  1e75,  1e76,  1e77,  1e78,  1e79,
    1e80,  1e81,  1e82,  1e83,  1e84,  1e85,  1e86,  1e87,  1e88,  1e89,  1e90,  1e91,  1e92,  1e93,  1e94,  1e95,
    1e96,  1e97,  1e98,  1e99,  1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109, 1e110, 1e111,
    1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119, 1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127,
    1e128, 1e129, 1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139, 1e140, 1e141, 1e142, 1e143,
    1e144, 1e145, 1e146, 1e147, 1e148, 1e149, 1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159,
    1e160, 1e161, 1e162, 1e163, 1e164, 1e165, 1e166, 1e167, 1e168, 1e169, 1e170, 1e171, 1e172, 1e173, 1e174, 1e175,
    1e176, 1e177, 1e178, 1e179, 1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186, 1e187, 1e188, 1e189, 1e190, 1e191,
    1e192, 1e193, 1e194, 1e195, 1e196, 1e197, 1e198, 1e199, 1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207,
    1e208, 1e209, 1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219, 1e220, 1e221, 1e222, 1e223,
    1e224, 1e225, 1e226, 1e227, 1e228, 1e229, 1e230, 1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239,
    1e240, 1e241, 1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249, 1e250, 1e251, 1e252, 1e253, 1e254, 1e255,
    1e256, 1e257, 1e258, 1e259, 1e260, 1e261, 1e262, 1e263, 1e264, 1e265, 1e266, 1e267, 1e268, 1e269, 1e270, 1e271,
    1e272, 1e273, 1e274, 1e275, 1e276, 1e277, 1e278, 1e279, 1e280, 1e281, 1e282, 1e283, 1e284, 1e285, 1e286, 1e287,
    1e288, 1e289, 1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296, 1e297, 1e298, 1e299, 1e300, 1e301, 1e302, 1e303,
    1e304, 1e305, 1e306, 1e307, 1e308};
using Index = std::ptrdiff_t;

Index last(std::u16string_view text, char16_t ch, Index position) noexcept {
    if (position < 0) {
        return -1;
    }

    auto found = text.rfind(ch, static_cast<std::size_t>(position));

    return found == text.npos ? -1 : static_cast<Index>(found);
}

// Java's legacy AU parser deliberately differs from Double.parseDouble: no whitespace,
// exponent-sign-without-digits is accepted, exponent arithmetic wraps signed32, and
// scaling outside [-308,308] becomes signed zero/infinity before multiplication.
double parseNumber(std::u16string_view text) noexcept {
    constexpr auto NAN_VALUE = std::numeric_limits<double>::quiet_NaN();

    if (text.empty()) {
        return NAN_VALUE;
    }

    bool negative = text[0] == u'-';
    std::size_t i = negative || text[0] == u'+' ? 1 : 0;
    double mantissa = 0;
    bool floating = false;
    bool hasDigit = false;
    std::uint32_t divider = 0;

    for (; i < text.size(); ++i) {
        auto ch = text[i];

        if (ch >= u'0' && ch <= u'9') {
            if (floating) {
                ++divider;
            }

            mantissa = mantissa * 10 + (ch - u'0');
            hasDigit = true;
        } else if (ch == u'.') {
            if (floating) {
                return NAN_VALUE;
            }

            floating = true;
        } else {
            break;
        }
    }

    if (!hasDigit) {
        return NAN_VALUE;
    }

    std::uint32_t exponent = 0;

    if (i < text.size()) {
        if ((text[i] != u'E' && text[i] != u'e') || ++i >= text.size()) {
            return NAN_VALUE;
        }

        bool negativeExponent = text[i] == u'-';

        if (negativeExponent || text[i] == u'+') {
            ++i;
        }

        for (; i < text.size(); ++i) {
            auto ch = text[i];

            if (ch < u'0' || ch > u'9') {
                return NAN_VALUE;
            }

            exponent = exponent * 10 + static_cast<std::uint32_t>(ch - u'0');
        }

        if (negativeExponent) {
            exponent = 0u - exponent;
        }

        if (exponent == static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            return NAN_VALUE;
        }
    }

    auto adjusted = std::bit_cast<std::int32_t>(exponent - divider);
    double value;

    if (adjusted > MAX_POWER || adjusted < -MAX_POWER) {
        value = mantissa == 0 || adjusted < -MAX_POWER ? 0 : std::numeric_limits<double>::infinity();
    } else {
        value = adjusted >= 0 ? mantissa * POWERS[static_cast<unsigned>(adjusted)]
                              : mantissa / POWERS[static_cast<unsigned>(-adjusted)];
    }

    return negative ? -value : value;
}

/** Test whether decimal digits denote the excluded upper binary rounding boundary.
 * @param value Positive finite nonzero binary64.
 * @param digits Significant decimal digits, bounded by the binary64 shortest conversion.
 * @param exponent Decimal exponent of the leading digit.
 * @return True when the decimal value is exactly halfway to the next binary64.
 */
bool isUpperRoundingBoundary(double value, std::string_view digits, int exponent) {
    constexpr unsigned FRACTION_BITS = 52;
    constexpr std::uint64_t FRACTION_MASK = (std::uint64_t{1} << FRACTION_BITS) - 1;
    constexpr unsigned EXPONENT_MASK = 0x7ff;
    constexpr int EXPONENT_BIAS = 1023;
    auto bits = std::bit_cast<std::uint64_t>(value);
    auto encodedExponent = static_cast<int>((bits >> FRACTION_BITS) & EXPONENT_MASK);
    auto significand = bits & FRACTION_MASK;

    if (encodedExponent != 0) {
        significand |= std::uint64_t{1} << FRACTION_BITS;
    }

    // The upper midpoint is (2 * significand + 1) * 2^(binaryExponent - 1).
    auto boundary = 2 * significand + 1;
    auto binaryTwos = std::max(encodedExponent, 1) - EXPONENT_BIAS - static_cast<int>(FRACTION_BITS) - 1;
    int binaryFives = 0;
    std::uint64_t decimal = 0;
    std::from_chars(digits.data(), digits.data() + digits.size(), decimal);
    auto decimalTwos = exponent - static_cast<int>(digits.size()) + 1;
    auto decimalFives = decimalTwos;

    // Cancel factors instead of approximating either side with floating-point arithmetic.
    while (decimal % 2 == 0) {
        decimal /= 2;
        ++decimalTwos;
    }

    while (decimal % 5 == 0) {
        decimal /= 5;
        ++decimalFives;
    }

    while (boundary % 5 == 0) {
        boundary /= 5;
        ++binaryFives;
    }

    return decimal == boundary && decimalTwos == binaryTwos && decimalFives == binaryFives;
}

std::u16string formatNumber(double value) {
    if (value == 0) {
        return std::signbit(value) ? u"-0" : u"0";
    }

    bool negative = value < 0;
    auto magnitude = std::abs(value);
    // The precision policy consumes shortest round-trip digits (Ryu in upstream),
    // unlike the older Java Double.toString digit policy used by IPF fields.
    std::array<char, NUMBER_BUFFER_CAPACITY> buffer{};
    auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific);
    std::string_view raw(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    auto e = raw.find('e');
    int exponent = 0;
    auto exponentText = raw.substr(e + 1);

    if (exponentText.front() == '+') {
        exponentText.remove_prefix(1);
    }

    std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);
    std::string digits;

    for (char ch : raw.substr(0, e)) {
        if (ch != '.') {
            digits += ch;
        }
    }

    if (digits.size() == 1 && magnitude < std::numeric_limits<double>::min()) {
        auto decimal = org::ttldtor::qdcxx::detail::decimalDigits(magnitude);
        digits = std::move(decimal.digits);
        exponent = decimal.exponent;
    }

    if (digits.size() > FORMAT_PRECISION) {
        bool roundUp = digits[FORMAT_PRECISION] >= '5';

        // The reference shortest conversion excludes the upper binary midpoint, even
        // for an even significand. A host shortest conversion can select that endpoint
        // and create a false precision-rounding tie; retain the interior result instead.
        // Only a 16-digit sequence ending in 5 can change the 15-digit decision.
        if (digits.size() == FORMAT_PRECISION + 1 && digits.back() == '5' &&
            isUpperRoundingBoundary(magnitude, digits, exponent)) {
            roundUp = false;
        }

        digits.resize(FORMAT_PRECISION);

        if (roundUp) {
            auto i = digits.size();

            while (i > 0 && digits[i - 1] == '9') {
                digits[--i] = '0';
            }

            if (i == 0) {
                digits = "1";
                ++exponent;
            } else {
                ++digits[i - 1];
            }
        }
    }

    while (digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
    }

    std::string output = negative ? "-" : "";

    if (magnitude >= FIXED_MIN && magnitude < FIXED_MAX) {
        auto point = exponent + 1;

        if (point <= 0) {
            output += "0." + std::string(static_cast<unsigned>(-point), '0') + digits;
        } else if (static_cast<unsigned>(point) >= digits.size()) {
            output += digits + std::string(static_cast<unsigned>(point) - digits.size(), '0');
        } else {
            output +=
                digits.substr(0, static_cast<unsigned>(point)) + "." + digits.substr(static_cast<unsigned>(point));
        }
    } else {
        output += digits[0];

        if (digits.size() > 1) {
            output += "." + digits.substr(1);
        }

        output += "E" + std::to_string(exponent);
    }

    return {output.begin(), output.end()};
}

void checkText(std::size_t units, AdditionalUnderlyingLimits limits) {
    if (units > limits.textUnits || units > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::length_error("additional underlying text limit exceeded");
    }
}
} // namespace

AdditionalUnderlyings::AdditionalUnderlyings(AdditionalUnderlyings &&other) noexcept(
    std::is_nothrow_default_constructible_v<std::u16string> && std::is_nothrow_default_constructible_v<Map>) {
    text_.swap(other.text_);
    map_.swap(other.map_);
}

AdditionalUnderlyings &AdditionalUnderlyings::operator=(AdditionalUnderlyings other) noexcept {
    text_.swap(other.text_);
    map_.swap(other.map_);

    return *this;
}

const AdditionalUnderlyings &AdditionalUnderlyings::empty() {
    static const AdditionalUnderlyings value;

    return value;
}

AdditionalUnderlyings AdditionalUnderlyings::valueOf(std::optional<std::u16string_view> input,
                                                     AdditionalUnderlyingLimits limits) {
    AdditionalUnderlyings result;

    if (!input || input->empty()) {
        return result;
    }

    auto text = *input;
    checkText(text.size(), limits);
    std::size_t entries = 0;

    for (Index n = static_cast<Index>(text.size()); n > 0; n = last(text, u';', n - 1)) {
        if (++entries > limits.entries) {
            throw std::length_error("additional underlying entry limit exceeded");
        }

        if (n == static_cast<Index>(text.size()) - 1 ||
            (n < static_cast<Index>(text.size()) - 1 && text[static_cast<std::size_t>(n + 1)] != u' ')) {
            throw std::invalid_argument("inappropriate use of separators");
        }

        auto k = last(text, u' ', n - 1);
        auto j = last(text, u' ', k - 1);

        if (k >= n - 1 || j >= k - 1 || j == 0 || (j > 0 && text[static_cast<std::size_t>(j - 1)] != u';') ||
            last(text, u';', n - 1) > j) {
            throw std::invalid_argument("inappropriate use of separators");
        }

        auto spc = parseNumber(text.substr(static_cast<std::size_t>(k + 1), static_cast<std::size_t>(n - k - 1)));

        if (std::isnan(spc)) {
            throw std::invalid_argument("SPC is missing or infinite");
        }

        result.map_.insert_or_assign(
            std::u16string(text.substr(static_cast<std::size_t>(j + 1), static_cast<std::size_t>(k - j - 1))), spc);
    }

    result.text_ = text;

    return result;
}

AdditionalUnderlyings AdditionalUnderlyings::valueOf(const Map *map, AdditionalUnderlyingLimits limits) {
    if (!map || map->empty()) {
        return {};
    }

    if (map->size() > limits.entries) {
        throw std::length_error("additional underlying entry limit exceeded");
    }

    std::u16string text;

    for (const auto &[symbol, spc] : *map) {
        if (symbol.empty()) {
            throw std::invalid_argument("symbol is empty");
        }

        if (symbol.find_first_of(u" ;") != symbol.npos) {
            throw std::invalid_argument("symbol contains prohibited character");
        }

        if (!std::isfinite(spc)) {
            throw std::invalid_argument("SPC is missing or infinite");
        }

        auto number = formatNumber(spc);
        auto separatorUnits = text.empty() ? 1u : 3u;
        checkText(symbol.size(), limits);

        if (number.size() + separatorUnits > limits.textUnits - symbol.size() ||
            text.size() > limits.textUnits - symbol.size() - number.size() - separatorUnits) {
            throw std::length_error("additional underlying text limit exceeded");
        }

        if (!text.empty()) {
            text += u"; ";
        }

        text += symbol;
        text += u' ';
        text += number;
    }

    return valueOf(std::u16string_view(text), limits);
}

double AdditionalUnderlyings::getSpc(std::optional<std::u16string_view> symbol) const noexcept {
    if (!symbol) {
        return 0;
    }

    auto found = map_.find(*symbol);

    return found == map_.end() ? 0 : found->second;
}

double AdditionalUnderlyings::getSpc(std::optional<std::u16string_view> input,
                                     std::optional<std::u16string_view> symbol) noexcept {
    if (!input || input->empty() || !symbol || symbol->empty()) {
        return 0;
    }

    auto text = *input;

    if (text.size() > static_cast<std::size_t>(std::numeric_limits<Index>::max())) {
        return 0;
    }

    for (Index n = static_cast<Index>(text.size()); n > 0; n = last(text, u';', n - 1)) {
        auto k = last(text, u' ', n - 1);

        if (k >= n - 1) {
            continue;
        }

        auto j = last(text, u' ', k - 1) + 1;

        if (k - j != static_cast<Index>(symbol->size()) ||
            text.substr(static_cast<std::size_t>(j), static_cast<std::size_t>(k - j)) != *symbol) {
            continue;
        }

        auto spc = parseNumber(text.substr(static_cast<std::size_t>(k + 1), static_cast<std::size_t>(n - k - 1)));

        if (!std::isnan(spc)) {
            return spc;
        }
    }

    return 0;
}
} // namespace org::ttldtor::qdcxx::glossary
