// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "aggregation.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {
constexpr std::size_t MAXIMUM_INFO_UNITS = 65536; ///< Selected live-profile maximum property text, never unbounded.
constexpr std::u16string_view INFO_PROPERTY = u"aggregationPeriodInfo";

/** @param amount Actual finite cost. @param budget Remaining caller coordinate. @return Successful debit. */
bool debit(std::size_t amount, std::size_t &budget) noexcept {
    if (amount > budget)
        return false;

    budget -= amount;

    return true;
}

/** Bounded source-pattern recognizer; all backtracking alternatives are memoized, with charged scans. */
struct InfoParser {
    std::u16string_view text; ///< Borrowed complete bounded property.
    protocol::LiveWork &work; ///< Caller work, never replenished or retained beyond this stack.
    bool funded = true;       ///< A short budget never produces a positive report.

    /** @param position Examined source unit. @return Unit or zero after bounds/work refusal. */
    char16_t at(std::size_t position) noexcept {
        if (position >= text.size())
            return 0;

        if (!debit(1, work.visits)) {
            funded = false;

            return 0;
        }

        return text[position];
    }

    /** @param position Current cursor. @param literal Required exact grammar. @return Match and advanced cursor. */
    bool literal(std::size_t &position, std::u16string_view literal) noexcept {
        for (auto unit : literal) {
            if (at(position) != unit)
                return false;

            ++position;
        }

        return funded;
    }

    /** @param unit One ASCII source unit. @return Decimal digit membership. */
    static bool digit(char16_t unit) noexcept {
        return unit >= u'0' && unit <= u'9';
    }

    /** @param unit One source unit. @return Java fixed extension-key leading class. */
    static bool letter(char16_t unit) noexcept {
        return (unit >= u'A' && unit <= u'Z') || (unit >= u'a' && unit <= u'z') || unit == u'_';
    }

    /** @param position Numeric start advanced on success. @return Rounded Java millisecond value or no match. */
    std::optional<std::int64_t> number(std::size_t &position) noexcept {
        const auto start = position;

        if (at(position) == u'-')
            ++position;

        const auto integer = position;

        while (digit(at(position)))
            ++position;

        if (position == integer)
            return {};

        if (at(position) == u'.') {
            ++position;
            const auto fraction = position;

            while (digit(at(position)))
                ++position;

            if (position == fraction)
                return {};
        }

        if (at(position) == u'e' || at(position) == u'E') {
            ++position;

            if (at(position) == u'+' || at(position) == u'-')
                ++position;

            const auto exponent = position;

            while (digit(at(position)))
                ++position;

            if (position == exponent)
                return {};
        }

        const auto size = position - start;

        if (!funded || size > MAXIMUM_INFO_UNITS || !debit(size, work.bytes) || !debit(size, work.visits)) {
            funded = false;

            return {};
        }

        std::array<char, MAXIMUM_INFO_UNITS> ascii; // Fixed stack scratch; only admitted numeric units are initialized.

        for (std::size_t i = 0; i < size; ++i)
            ascii[i] = static_cast<char>(text[start + i]);

        double value = 0;
        const auto result = std::from_chars(ascii.data(), ascii.data() + size, value, std::chars_format::general);

        if (result.ec == std::errc::result_out_of_range) {
            std::size_t i = (ascii[0] == '-');
            int exponent = 0, integerDigits = 0, first = -1, digits = 0;
            bool fraction = false;

            for (; i < size && ascii[i] != 'e' && ascii[i] != 'E'; ++i) {
                if (ascii[i] == '.') {
                    fraction = true;
                    continue;
                }

                if (!fraction)
                    ++integerDigits;

                if (first < 0 && ascii[i] != '0')
                    first = digits;

                ++digits;
            }

            if (i < size) {
                ++i;
                int sign = 1;

                if (i < size && (ascii[i] == '+' || ascii[i] == '-')) {
                    if (ascii[i] == '-')
                        sign = -1;

                    ++i;
                }

                for (; i < size; ++i)
                    exponent = std::min(1000000, exponent * 10 + (ascii[i] - '0'));

                exponent *= sign;
            }

            value = first < 0
                        ? 0
                        : (integerDigits - first - 1 + exponent < 0 ? 0 : std::numeric_limits<double>::infinity());

            if (ascii[0] == '-')
                value = -value;
        } else if (result.ec != std::errc{} || result.ptr != ascii.data() + size)
            return {};

        const double scaled = value * 1000.0;

        if (scaled >= 0x1p63)
            return std::numeric_limits<std::int64_t>::max();

        if (scaled <= -0x1p63)
            return std::numeric_limits<std::int64_t>::min();

        auto rounded = static_cast<std::int64_t>(scaled);
        const auto remainder = scaled - static_cast<double>(rounded);

        if (remainder >= 0.5)
            ++rounded;
        else if (remainder < -0.5)
            --rounded;

        return rounded;
    }

    /** @param start Tail immediately after max number. @return Exact accepted extension suffix. */
    bool tail(std::size_t start) noexcept {
        if (start + 1 == text.size() && at(start) == u'}')
            return funded;

        if (!debit(text.size() + 1, work.bytes)) {
            funded = false;

            return false;
        }

        std::array<unsigned char, MAXIMUM_INFO_UNITS + 1> accepted;
        std::fill_n(accepted.begin(), text.size() + 1, 0);

        if (!text.empty() && at(text.size() - 1) == u'}')
            accepted[text.size() - 1] = 1;

        for (std::size_t p = text.size(); p-- > start;) {
            if (!funded)
                return false;

            if (at(p) != u',')
                continue;

            auto q = p + 1;

            if (at(q++) != u'"' || !letter(at(q)))
                continue;

            ++q;

            while (letter(at(q)) || digit(at(q)))
                ++q;

            if (at(q++) != u'"' || at(q++) != u':')
                continue;

            const auto value = q;
            std::array<std::size_t, 4> ends{};
            std::size_t count = 0;

            while (q < text.size() && at(q) != u',' && at(q) != u'}')
                ++q;

            ends[count++] = q; // Pattern's bare scalar alternative intentionally permits empty/future values.

            if (at(value) == u'"') {
                q = value + 1;

                while (q < text.size() && funded) {
                    const auto unit = at(q++);

                    if (unit == u'"') {
                        ends[count++] = q;
                        break;
                    }

                    if (unit == u'\\') {
                        const auto escaped = at(q++);

                        if (q > text.size() || escaped == u'\n' || escaped == u'\r' || escaped == 0x85 ||
                            escaped == 0x2028 || escaped == 0x2029)
                            break;
                    }
                }
            }

            if (at(value) == u'{') {
                q = value + 1;

                while (q < text.size() && at(q) != u'{' && at(q) != u'}')
                    ++q;

                if (at(q) == u'}')
                    ends[count++] = q + 1;
            }

            if (at(value) == u'[') {
                q = value + 1;

                while (q < text.size() && at(q) != u'[' && at(q) != u']')
                    ++q;

                if (at(q) == u']')
                    ends[count++] = q + 1;
            }

            for (std::size_t i = 0; i < count; ++i)
                if (ends[i] < text.size() && accepted[ends[i]])
                    accepted[p] = 1;

            if (!funded)
                return false;
        }

        return funded && start < text.size() && accepted[start];
    }
};

/** Inspect only property keys; selected values remain borrowed and are parsed with separate actual credits.
 * @param properties Exact ordered local properties. @param key Known property key. @param work Actual work.
 * @param funded False after any refused inspection. @return Last matching nullable value.
 */
std::optional<std::u16string_view> property(std::span<const protocol::LivePropertyView> properties,
                                            std::u16string_view key, protocol::LiveWork &work, bool &funded) noexcept {
    std::optional<std::u16string_view> result;

    for (const auto &entry : properties) {
        if (!debit(1, work.visits) || !debit(entry.key ? entry.key->size() : 0, work.textUnits)) {
            funded = false;

            return {};
        }

        if (entry.key && *entry.key == key)
            result = entry.value;
    }

    return result;
}
} // namespace

AggregationRequestText aggregationRequestText(std::optional<std::chrono::milliseconds> period) noexcept {
    AggregationRequestText result;

    if (!period)
        return result;

    const auto put = [&](char value) {
        result.units[result.size++] = static_cast<char16_t>(value);
    };
    const auto number = [&](std::int64_t value) {
        std::array<char, 20> text;
        auto end = std::to_chars(text.data(), text.data() + text.size(), value).ptr;

        for (auto p = text.data(); p != end; ++p)
            put(*p);
    };
    auto value = period->count();
    const auto millis = value % 1000;
    value /= 1000;
    const auto seconds = value % 60;
    value /= 60;
    const auto minutes = value % 60;
    value /= 60;
    const auto hours = value % 24;
    const auto days = value / 24;
    put('P');

    if (days) {
        number(days);
        put('D');
    }

    put('T');

    if (hours) {
        number(hours);
        put('H');
    }

    if (minutes) {
        number(minutes);
        put('M');
    }

    number(seconds);

    if (millis) {
        put('.');
        put(static_cast<char>('0' + millis / 100));
        put(static_cast<char>('0' + millis / 10 % 10));
        put(static_cast<char>('0' + millis % 10));

        while (result.units[result.size - 1] == u'0')
            --result.size;
    }

    put('S');

    return result;
}

std::optional<AggregationPeriodInfo> parseAggregationPeriodInfo(std::u16string_view text, protocol::LiveWork &work,
                                                                bool *complete) noexcept {
    if (complete)
        *complete = false;

    if (text.size() > MAXIMUM_INFO_UNITS) {
        if (complete)
            *complete = true;

        return {};
    }

    if (!debit(text.size(), work.textUnits))
        return {};

    InfoParser parser{text, work};

    struct Completion {
        bool *result;
        const bool &funded;

        ~Completion() {
            if (result)
                *result = funded;
        }
    } completion{complete, parser.funded};

    std::size_t position = 0;

    if (!parser.literal(position, u"{\"min\":"))
        return {};

    const auto minimum = parser.number(position);

    if (!minimum || !parser.literal(position, u",\"max\":"))
        return {};

    const auto maximum = parser.number(position);

    if (!maximum || *minimum < 0 || *maximum < *minimum || !parser.tail(position) || !parser.funded)
        return {};

    return AggregationPeriodInfo{std::chrono::milliseconds{*minimum}, std::chrono::milliseconds{*maximum}};
}

std::optional<AggregationPeriodInfo> peerAggregationPeriodInfo(protocol::LiveProtocolView descriptor,
                                                               std::array<bool, 3> families, protocol::LiveWork &work,
                                                               bool *complete) noexcept {
    bool funded = true;

    if (complete)
        *complete = false;

    const auto inherited = property(descriptor.properties, INFO_PROPERTY, work, funded);
    constexpr std::array<std::int32_t, 3> ids{10, 15, 20};
    constexpr std::array<std::u16string_view, 3> names{u"TICKER_DATA", u"STREAM_DATA", u"HISTORY_DATA"};
    std::optional<AggregationPeriodInfo> result;

    for (const auto &message : descriptor.send) {
        if (!debit(1, work.visits) || !debit(message.name ? message.name->size() : 0, work.textUnits))
            return {};

        for (std::size_t family = 0; family < 3; ++family) {
            if (!families[family] || message.id != ids[family] || !message.name || *message.name != names[family])
                continue;

            auto value = property(message.properties, INFO_PROPERTY, work, funded);

            if (!funded)
                return {};

            if (!value)
                value = inherited;

            if (value) {
                bool complete = false;
                const auto parsed = parseAggregationPeriodInfo(*value, work, &complete);

                if (!complete)
                    return {};

                if (parsed) {
                    if (!result)
                        result = parsed;
                    else {
                        result->minimum = std::min(result->minimum, parsed->minimum);
                        result->maximum = std::max(result->maximum, parsed->maximum);
                    }
                }
            }
        }
    }

    if (complete)
        *complete = funded;

    return funded ? result : std::nullopt;
}
} // namespace org::ttldtor::qdcxx::transport::detail
