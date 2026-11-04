// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/event_text.hpp"

#include "../detail/java_double.hpp"
#include "decimal_projection.hpp"
#include "event_text_numeric.hpp"
#include "qd/events/indexed_event_source.hpp"
#include "qd/events/order_source.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/summary.hpp"
#include "qd/events/time_helpers.hpp"
#include "qd/events/trade.hpp"
#include "qd/events/trade_eth.hpp"
#include "qd/time/time_zone_data.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/** Fixed-size logical work charged for each scalar formatter, in addition to actual copied units. */
constexpr std::size_t SCALAR_WORK = 128;
constexpr std::int64_t MILLIS_PER_DAY = 86400000;               ///< Exact epoch-day size.
constexpr std::int64_t MIN_FORMAT_TIME = -62135683200000;       ///< Java's year-one guard minus one day.
constexpr std::int64_t MAX_FORMAT_TIME = 253402387200000;       ///< Java's year-9999 guard plus two days.
constexpr std::u16string_view HEX_DIGITS = u"0123456789abcdef"; ///< Lowercase Java hex alphabet.

/** Internal unwinding value; only the public guard converts it into an atomic typed result. */
struct TextFailure {
    EventTextError error; ///< Exact first failure.
};

/** Decrease a finite allowance without unsigned wrap.
 * @param available Remaining logical allowance.
 * @param amount Required credits.
 */
void consume(std::size_t &available, std::size_t amount) {
    if (amount > available) {
        throw TextFailure{EventTextError::RESOURCE_LIMIT};
    }

    available -= amount;
}

/** Convert exceptions only at the complete result boundary.
 * @tparam Action Synchronous local result producer; no callback is retained.
 * @param action Borrowed producer, invoked exactly once within this call.
 * @return Complete independently owned result or first typed formatting/allocation error.
 */
template <typename Action> auto protect(Action &&action) -> std::expected<decltype(action()), EventTextError> {
    try {
        return action();
    } catch (const TextFailure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(EventTextError::ALLOCATION);
    } catch (const std::length_error &) {
        return std::unexpected(EventTextError::RESOURCE_LIMIT);
    }
}

/** Format one bounded integer without process locale state.
 * @tparam Integer Signed or unsigned standard integer.
 * @param value Complete integer value.
 * @param base Decimal or hexadecimal radix.
 * @return Owned UTF-16 ASCII spelling.
 */
template <typename Integer> std::u16string integerText(Integer value, int base = 10) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, base);

    return {buffer.data(), result.ptr};
}

/** Match a correctly rounded decimal power used by WideDecimal's binary64 composition.
 * @param exponent Decimal exponent in -127..127.
 * @return Nearest IEEE binary64 value; fixed-size conversion cannot overflow this domain.
 */
double decimalPower(int exponent) noexcept {
    std::array<char, 16> buffer{'1', 'e'};
    const auto end = std::to_chars(buffer.data() + 2, buffer.data() + buffer.size(), exponent).ptr;
    double value = 0;
    std::from_chars(buffer.data(), end, value, std::chars_format::scientific);

    return value;
}

/** Exact rounded binary64 product parts for the bounded Profile share scale. */
struct ScaledShare {
    std::uint64_t significand; ///< Positive rounded binary64 significand, with at most 53 bits.
    int exponent;              ///< Value is significand times two to this power.
};

/** Multiply the selected share magnitude/divisor with explicit nearest-even binary64 rounding.
 * The selected rank bounds the scaled result by approximately 2^51. Even the smallest input
 * subnormal scales to a normal binary64, so this operation needs no overflow/subnormal product path.
 * @param magnitude Positive finite source binary64, without a sign bit.
 * @param divisor Positive normal binary64 decimal power for the selected rank.
 * @return Rounded product parts, independent of the caller thread's floating rounding mode.
 */
ScaledShare scaledShare(double magnitude, double divisor) noexcept {
    constexpr unsigned FRACTION_BITS = 52;
    constexpr unsigned WORD_BITS = 32;
    constexpr unsigned WIDE_WORD_BITS = 64;
    constexpr unsigned PRODUCT_BITS = 2 * WIDE_WORD_BITS;
    constexpr int EXPONENT_BIAS = 1023;
    constexpr int SUBNORMAL_EXPONENT = -1074;
    constexpr std::uint64_t EXPONENT_MASK = 0x7ff;
    constexpr unsigned PRECISION = FRACTION_BITS + 1;
    constexpr std::uint64_t FRACTION_MASK = (std::uint64_t{1} << FRACTION_BITS) - 1;
    constexpr std::uint64_t HIDDEN_BIT = std::uint64_t{1} << FRACTION_BITS;
    constexpr std::uint64_t LOW_WORD_MASK = (std::uint64_t{1} << WORD_BITS) - 1;
    const auto valueBits = std::bit_cast<std::uint64_t>(magnitude);
    const auto divisorBits = std::bit_cast<std::uint64_t>(divisor);
    const auto valueExponent = static_cast<int>((valueBits >> FRACTION_BITS) & EXPONENT_MASK);
    const auto divisorExponent = static_cast<int>((divisorBits >> FRACTION_BITS) & EXPONENT_MASK);
    const auto valueSignificand = (valueBits & FRACTION_MASK) | (valueExponent == 0 ? 0 : HIDDEN_BIT);
    const auto divisorSignificand = (divisorBits & FRACTION_MASK) | HIDDEN_BIT;
    const auto valueLow = valueSignificand & LOW_WORD_MASK;
    const auto valueHigh = valueSignificand >> WORD_BITS;
    const auto divisorLow = divisorSignificand & LOW_WORD_MASK;
    const auto divisorHigh = divisorSignificand >> WORD_BITS;
    const auto crossLeft = valueHigh * divisorLow;
    const auto crossRight = valueLow * divisorHigh;
    auto low = valueLow * divisorLow;
    auto next = low + (crossLeft << WORD_BITS);
    std::uint64_t carry = next < low;
    low = next;
    next = low + (crossRight << WORD_BITS);
    carry += next < low;
    low = next;
    const auto high = valueHigh * divisorHigh + (crossLeft >> WORD_BITS) + (crossRight >> WORD_BITS) + carry;
    const auto length = high == 0 ? WIDE_WORD_BITS - std::countl_zero(low) : PRODUCT_BITS - std::countl_zero(high);
    const auto discarded = static_cast<unsigned>(length) - PRECISION;
    auto significand = discarded == 0 ? low : (low >> discarded) | (high << (WIDE_WORD_BITS - discarded));
    auto exponent =
        (valueExponent == 0 ? SUBNORMAL_EXPONENT : valueExponent - EXPONENT_BIAS - static_cast<int>(FRACTION_BITS)) +
        divisorExponent - EXPONENT_BIAS - static_cast<int>(FRACTION_BITS) + static_cast<int>(discarded);

    if (discarded != 0) {
        const auto tail = low & ((std::uint64_t{1} << discarded) - 1);
        const auto half = std::uint64_t{1} << (discarded - 1);

        if (tail > half || (tail == half && (significand & 1) != 0)) {
            ++significand;
        }
    }

    if (significand == (std::uint64_t{1} << PRECISION)) {
        significand >>= 1;
        ++exponent;
    }

    return {significand, exponent};
}

/** Perform Java's rounded positive product-plus-half followed by signed64 truncation.
 * @param magnitude Positive finite business value admitted by the selected decimal rank.
 * @param divisor Corresponding normal binary64 decimal power.
 * @return Nonnegative integer significand, bounded by approximately 2^51.
 */
std::int64_t shareSignificand(double magnitude, double divisor) noexcept {
    auto scaled = scaledShare(magnitude, divisor);
    constexpr unsigned BINARY64_PRECISION = 53;
    constexpr int MIN_HALF_ADDITION_EXPONENT = -static_cast<int>(BINARY64_PRECISION) - 1;
    constexpr std::uint64_t NORMALIZATION_LIMIT = std::uint64_t{1} << BINARY64_PRECISION;

    if (scaled.exponent < MIN_HALF_ADDITION_EXPONENT) {
        // A product below one quarter still truncates to zero after rounded addition of one half.
        return 0;
    }

    // Here exponent is in -54..-1: one half aligns into at most 54 integer bits.
    auto sum = scaled.significand + (std::uint64_t{1} << static_cast<unsigned>(-1 - scaled.exponent));

    if (sum >= NORMALIZATION_LIMIT) {
        const auto discarded = sum & 1;
        sum >>= 1;

        if (discarded != 0 && (sum & 1) != 0) {
            ++sum;
        }

        ++scaled.exponent;
    }

    return static_cast<std::int64_t>(sum >> static_cast<unsigned>(-scaled.exponent));
}

/** Compose the specific Java Profile shares/freeFloat presentation value.
 * @param value Business accessor binary64; its raw source decimal remains untouched.
 * @return WideDecimal presentation bits, including canonical zero and nonfinite values.
 */
core::WideDecimal profileShareDecimal(double value) noexcept {
    const auto integer = org::ttldtor::qdcxx::detail::javaLong(value);

    constexpr double JAVA_LONG_BOUND = 0x1p63;

    if (std::isfinite(value) && std::abs(value) <= JAVA_LONG_BOUND && std::trunc(value) == value) {
        return detail::composeEventDecimal(integer, 0);
    }

    if (std::isnan(value)) {
        return {0};
    }

    const auto magnitude = std::abs(value);
    constexpr double COMPOSITION_MAGNITUDE = 0x1p51;
    int lower = 0;
    int upper = 256;

    while (lower + 1 < upper) {
        const auto rank = (lower + upper) / 2;

        if (magnitude <= COMPOSITION_MAGNITUDE * decimalPower(128 - rank)) {
            lower = rank;
        } else {
            upper = rank;
        }
    }

    if (lower == 0) {
        return {std::signbit(value) ? -256 : 256};
    }

    const auto significand = shareSignificand(magnitude, decimalPower(lower - 128));

    return detail::composeEventDecimal(std::signbit(value) ? -significand : significand, lower - 128);
}

/** @param value Known projected direction.
 * @return Program-lifetime Java enum name.
 */
std::u16string_view directionName(Direction value) noexcept {
    constexpr std::array names{u"UNDEFINED", u"DOWN", u"ZERO_DOWN", u"ZERO", u"ZERO_UP", u"UP"};
    const auto code = static_cast<std::int32_t>(value);

    return code >= 0 && code < static_cast<std::int32_t>(names.size()) ? names[static_cast<std::size_t>(code)]
                                                                       : u"UNDEFINED";
}
} // namespace

namespace detail {
/** Independently retained immutable provider binding; no caller pointer or ID view survives construction. */
struct EventTextZone {
    time::ZoneBinding binding; ///< Owns the selected identity/reference/profile backing.

    /** @param value Complete immutable binding to retain independently of its original owner. */
    explicit EventTextZone(time::ZoneBinding value) noexcept : binding(std::move(value)) {
    }
};

/** Per-call owned output and finite credits; no borrowed input survives the synchronous operation. */
class EventTextWriter {
    EventTextLimits limits_;
    std::u16string result_;

    public:
    /** @param limits Explicit finite limits copied into this call's independent state. */
    explicit EventTextWriter(EventTextLimits limits) : limits_(limits) {
    }

    /** @param text Literal/generated UTF-16 fragment, borrowed only during this append. */
    void append(std::u16string_view text) {
        consume(limits_.outputUnits, text.size());
        consume(limits_.work, text.size());
        result_.append(text);
    }

    /** @param text Exact source string, borrowed only during this copy. */
    void source(std::u16string_view text) {
        consume(limits_.inputUnits, text.size());
        consume(limits_.work, text.size());
        append(text);
    }

    /** @param text Exact nullable source string; null is rendered literally as null. */
    void nullable(const std::optional<std::u16string> &text) {
        if (text) {
            source(*text);
        } else {
            append(u"null");
        }
    }

    /** @tparam Integer Standard integer.
     * @param value Complete raw value.
     * @param base Decimal or hexadecimal radix.
     */
    template <typename Integer> void integer(Integer value, int base = 10) {
        consume(limits_.work, SCALAR_WORK);
        append(integerText(value, base));
    }

    /** @param value Complete binary64 business value. */
    void number(double value) {
        consume(limits_.work, SCALAR_WORK);
        append(eventDoubleText(value));
    }

    /** @param value Profile shares/freeFloat binary64 business value. */
    void share(double value) {
        consume(limits_.work, SCALAR_WORK);
        const auto text = profileShareDecimal(value).toString();
        std::array<char16_t, 32> units{};

        for (std::size_t i = 0; i < text.size(); ++i) {
            units[i] = static_cast<unsigned char>(text[i]);
        }

        append({units.data(), text.size()});
    }

    /** @param value Exact UTF-16 exchange code, including unpaired surrogates. */
    void character(char16_t value) {
        consume(limits_.work, SCALAR_WORK);

        if (value >= u' ' && value <= u'~') {
            append({&value, 1});
        } else if (value == 0) {
            append(u"\\0");
        } else {
            std::array<char16_t, 6> units{u'\\',
                                          u'u',
                                          HEX_DIGITS[(value >> 12) & 15],
                                          HEX_DIGITS[(value >> 8) & 15],
                                          HEX_DIGITS[(value >> 4) & 15],
                                          HEX_DIGITS[value & 15]};
            append({units.data(), units.size()});
        }
    }

    /** @param timeMillis Any signed64 epoch milliseconds.
     * @param withMillis Whether to append three millisecond digits.
     * @param context Borrowed immutable explicit timezone.
     */
    void time(std::int64_t timeMillis, bool withMillis, const EventTextContext &context) {
        consume(limits_.work, SCALAR_WORK);

        if (timeMillis == 0) {
            append(u"0");

            return;
        }

        if (timeMillis < MIN_FORMAT_TIME || timeMillis > MAX_FORMAT_TIME) {
            append(integerText(timeMillis));

            return;
        }

        const auto offset = context.offsetAt(timeMillis);

        if (!offset) {
            throw TextFailure{offset.error()};
        }

        const auto local = timeMillis + *offset;
        auto day = local / MILLIS_PER_DAY;
        auto dayTime = local % MILLIS_PER_DAY;

        if (dayTime < 0) {
            --day;
            dayTime += MILLIS_PER_DAY;
        }

        const auto ymd = getYearMonthDayByDayId(static_cast<std::int32_t>(day));

        if (ymd < 10101 || ymd > 99991231) {
            append(integerText(timeMillis));

            return;
        }

        std::array<char16_t, 19> units{};
        auto position = std::size_t{0};
        const auto digits = [&](std::int64_t number, std::size_t count) {
            for (auto i = count; i != 0; --i) {
                units[position + i - 1] = static_cast<char16_t>(u'0' + number % 10);
                number /= 10;
            }

            position += count;
        };

        digits(ymd, 8);
        units[position++] = u'-';
        digits(dayTime / 3600000, 2);
        digits(dayTime / 60000 % 60, 2);
        digits(dayTime / 1000 % 60, 2);

        if (withMillis) {
            units[position++] = u'.';
            digits(dayTime % 1000, 3);
        }

        append({units.data(), position});
    }

    /** @return Complete independently owned UTF-16 output; this writer is consumed. */
    std::u16string finish() {
        return std::move(result_);
    }

    /** @param malformed Explicit policy for unpaired source surrogates.
     * @return Complete owned UTF-8 bytes using the same call's remaining work credits.
     */
    std::string utf8(MalformedUtf16 malformed) {
        return encode(result_, malformed);
    }

    /** @param text Borrowed exact source, unchanged during this synchronous call.
     * @param malformed Explicit unpaired-surrogate policy.
     * @return Complete owned UTF-8 output without retaining source storage.
     */
    std::string sourceUtf8(std::u16string_view text, MalformedUtf16 malformed) {
        consume(limits_.inputUnits, text.size());

        return encode(text, malformed);
    }

    private:
    /** @param text Exact borrowed UTF-16 text.
     * @param malformed Explicit unpaired-surrogate policy.
     * @return Complete owned standard UTF-8 bytes or an exception converted at the public boundary.
     */
    std::string encode(std::u16string_view text, MalformedUtf16 malformed) {
        if (malformed != MalformedUtf16::REJECT && malformed != MalformedUtf16::REPLACE) {
            throw TextFailure{EventTextError::INVALID_CONTEXT};
        }

        std::string output;

        for (std::size_t i = 0; i < text.size(); ++i) {
            consume(limits_.work, 1);
            std::uint32_t scalar = text[i];

            if (scalar >= 0xd800 && scalar <= 0xdfff) {
                if (scalar <= 0xdbff && i + 1 < text.size() && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
                    consume(limits_.work, 1);
                    scalar = 0x10000 + ((scalar - 0xd800) << 10) + (text[++i] - 0xdc00);
                } else if (malformed == MalformedUtf16::REPLACE) {
                    scalar = 0xfffd;
                } else {
                    throw TextFailure{EventTextError::INVALID_UTF16};
                }
            }

            std::array<char, 4> bytes{};
            const auto count = scalar <= 0x7f ? 1U : scalar <= 0x7ff ? 2U : scalar <= 0xffff ? 3U : 4U;
            auto remaining = scalar;

            for (auto j = count; j > 1; --j) {
                bytes[j - 1] = static_cast<char>(0x80 | (remaining & 0x3f));
                remaining >>= 6;
            }

            constexpr std::array<unsigned, 5> PREFIX{0, 0, 0xc0, 0xe0, 0xf0};
            bytes[0] = static_cast<char>(remaining | PREFIX[count]);
            consume(limits_.utf8Bytes, count);
            consume(limits_.work, count);
            output.append(bytes.data(), count);
        }

        return output;
    }
};
} // namespace detail

EventTextContext::EventTextContext(std::int32_t offsetMillis) noexcept : fixedOffsetMillis_(offsetMillis) {
}

EventTextContext::EventTextContext(EventTextContext &&other) noexcept
    : fixedOffsetMillis_(other.fixedOffsetMillis_), zone_(other.zone_) {
}

EventTextContext &EventTextContext::operator=(EventTextContext &&other) noexcept {
    fixedOffsetMillis_ = other.fixedOffsetMillis_;
    zone_ = other.zone_;

    return *this;
}

EventTextContext EventTextContext::utc() noexcept {
    return EventTextContext(0);
}

EventTextContext EventTextContext::fixedOffset(std::int32_t offsetMillis) noexcept {
    return EventTextContext(offsetMillis);
}

namespace {
/** Preserve the provider's actionable failure categories at the presentation boundary.
 * @param error Complete typed provider/binding failure.
 * @return Corresponding event-text refusal; no fallback or data substitution occurs.
 */
EventTextError timeZoneError(time::TimeZoneError error) noexcept {
    switch (error) {
    case time::TimeZoneError::INVALID_ID:
        return EventTextError::UNKNOWN_TIME_ZONE;
    case time::TimeZoneError::NOT_READY:
        return EventTextError::TIME_ZONE_NOT_READY;
    case time::TimeZoneError::OUT_OF_RANGE:
        return EventTextError::TIME_ZONE_RANGE;
    case time::TimeZoneError::RESOURCE_LIMIT:
        return EventTextError::RESOURCE_LIMIT;
    case time::TimeZoneError::ALLOCATION:
        return EventTextError::ALLOCATION;
    case time::TimeZoneError::DATA:
        return EventTextError::TIME_ZONE_DATA;
    case time::TimeZoneError::INVALID_ARGUMENT:
        return EventTextError::INVALID_CONTEXT;
    }

    return EventTextError::INVALID_CONTEXT;
}
} // namespace

std::expected<EventTextContext, EventTextError> EventTextContext::timeZone(const time::TimeZoneData *data,
                                                                           std::u16string_view zoneId,
                                                                           std::int64_t rawOffsetReferenceMillis,
                                                                           std::size_t maxIdUnits) {
    return protect([&] {
        auto binding = time::ZoneBinding::create(zoneId, data, {rawOffsetReferenceMillis, maxIdUnits});

        if (!binding) {
            throw TextFailure{timeZoneError(binding.error())};
        }

        EventTextContext context(0);
        context.zone_ = std::make_shared<const detail::EventTextZone>(std::move(*binding));

        return context;
    });
}

std::expected<std::int32_t, EventTextError> EventTextContext::offsetAt(std::int64_t timeMillis) const {
    if (!zone_) {
        return fixedOffsetMillis_;
    }

    const auto offset = zone_->binding.getOffset(timeMillis);

    if (!offset) {
        return std::unexpected(timeZoneError(offset.error()));
    }

    constexpr std::int64_t MILLIS_PER_SECOND = 1000;
    const auto millis = static_cast<std::int64_t>(offset->totalSeconds) * MILLIS_PER_SECOND;

    if (millis < (std::numeric_limits<std::int32_t>::min)() || millis > (std::numeric_limits<std::int32_t>::max)()) {
        return std::unexpected(EventTextError::TIME_ZONE_RANGE);
    }

    return static_cast<std::int32_t>(millis);
}

std::expected<std::u16string, EventTextError> formatEventDouble(double value, EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        writer.number(value);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> encodeEventChar(char16_t value, EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        writer.character(value);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> formatEventFlags(std::int32_t flags, EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        writer.integer(static_cast<std::uint32_t>(flags), 16);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> formatEventTime(std::int64_t timeMillis, bool withMillis,
                                                              const EventTextContext &context, EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        writer.time(timeMillis, withMillis, context);

        return writer.finish();
    });
}

std::expected<std::string, EventTextError> toUtf8(std::u16string_view text, EventTextLimits limits,
                                                  MalformedUtf16 malformed) {
    return protect([&] {
        detail::EventTextWriter writer(limits);

        return writer.sourceUtf8(text, malformed);
    });
}

namespace {
/** @param writer Per-call output owner.
 * @param value Unchanged borrowed Quote.
 * @param context Explicit immutable timezone.
 */
void render(detail::EventTextWriter &writer, const Quote &value, const EventTextContext &context) {
    writer.append(u"Quote{");
    writer.source(value.symbol);
    writer.append(u", eventTime=");
    writer.time(value.eventTime, true, context);
    writer.append(u", time=");
    writer.time(value.timeMs, true, context);
    writer.append(u", timeNanoPart=");
    writer.integer(value.timeNanoPart);
    writer.append(u", sequence=");
    writer.integer(value.sequence);
    writer.append(u", bidTime=");
    writer.time(value.bidTimeMs, true, context);
    writer.append(u", bidExchange=");
    writer.character(value.bidExchangeCode);
    writer.append(u", bidPrice=");
    writer.number(value.getBidPrice());
    writer.append(u", bidSize=");
    writer.number(value.getBidSizeAsDouble());
    writer.append(u", askTime=");
    writer.time(value.askTimeMs, true, context);
    writer.append(u", askExchange=");
    writer.character(value.askExchangeCode);
    writer.append(u", askPrice=");
    writer.number(value.getAskPrice());
    writer.append(u", askSize=");
    writer.number(value.getAskSizeAsDouble());
    writer.append(u"}");
}

/** @param writer Per-call owned output.
 * @param value Unchanged borrowed Profile.
 * @param context Explicit immutable timezone.
 */
void render(detail::EventTextWriter &writer, const Profile &value, const EventTextContext &context) {
    writer.append(u"Profile{");
    writer.source(value.symbol);
    writer.append(u", eventTime=");
    writer.time(value.eventTime, true, context);
    writer.append(u", description='");
    writer.nullable(value.description);
    writer.append(u"', SSR=");
    constexpr std::array ssr{u"UNDEFINED", u"ACTIVE", u"INACTIVE"};
    writer.append(ssr[static_cast<std::size_t>(value.shortSaleRestriction())]);
    writer.append(u", status=");
    constexpr std::array status{u"UNDEFINED", u"HALTED", u"ACTIVE"};
    writer.append(status[static_cast<std::size_t>(value.tradingStatus())]);
    writer.append(u", statusReason='");
    writer.nullable(value.statusReason);
    writer.append(u"', haltStartTime=");
    writer.time(value.haltStartTimeMs, false, context);
    writer.append(u", haltEndTime=");
    writer.time(value.haltEndTimeMs, false, context);
    writer.append(u", highLimitPrice=");
    writer.number(value.getHighLimitPrice());
    writer.append(u", lowLimitPrice=");
    writer.number(value.getLowLimitPrice());
    writer.append(u", high52WeekPrice=");
    writer.number(value.getHigh52WeekPrice());
    writer.append(u", low52WeekPrice=");
    writer.number(value.getLow52WeekPrice());
    writer.append(u", beta=");
    writer.number(value.getBeta());
    writer.append(u", earningsPerShare=");
    writer.number(value.getEarningsPerShare());
    writer.append(u", dividendFrequency=");
    writer.number(value.getDividendFrequency());
    writer.append(u", exDividendAmount=");
    writer.number(value.getExDividendAmount());
    writer.append(u", exDividendDay=");
    writer.integer(getYearMonthDayByDayId(value.exDividendDayId));
    writer.append(u", shares=");
    writer.share(value.getShares());
    writer.append(u", freeFloat=");
    writer.share(value.getFreeFloat());
    writer.append(u"}");
}

/** @param writer Per-call owned output.
 * @param value Unchanged borrowed Summary business value.
 * @param context Explicit immutable timezone, used only for independent eventTime.
 */
void render(detail::EventTextWriter &writer, const Summary &value, const EventTextContext &context) {
    constexpr std::array priceTypes{u"REGULAR", u"INDICATIVE", u"PRELIMINARY", u"FINAL"};
    writer.append(u"Summary{");
    writer.source(value.symbol);
    writer.append(u", eventTime=");
    writer.time(value.eventTime, true, context);
    writer.append(u", day=");
    writer.integer(getYearMonthDayByDayId(value.getDayId()));
    writer.append(u", dayOpen=");
    writer.number(value.getDayOpenPrice());
    writer.append(u", dayHigh=");
    writer.number(value.getDayHighPrice());
    writer.append(u", dayLow=");
    writer.number(value.getDayLowPrice());
    writer.append(u", dayClose=");
    writer.number(value.getDayClosePrice());
    writer.append(u", dayCloseType=");
    writer.append(priceTypes[static_cast<std::size_t>(value.getDayClosePriceType())]);
    writer.append(u", prevDay=");
    writer.integer(getYearMonthDayByDayId(value.getPrevDayId()));
    writer.append(u", prevDayClose=");
    writer.number(value.getPrevDayClosePrice());
    writer.append(u", prevDayCloseType=");
    writer.append(priceTypes[static_cast<std::size_t>(value.getPrevDayClosePriceType())]);
    writer.append(u", prevDayVolume=");
    writer.number(value.getPrevDayVolume());
    writer.append(u", openInterest=");
    writer.integer(value.getOpenInterest());
    writer.append(u"}");
}

/** @tparam Event Trade or TradeETH with the common supported business accessor surface.
 * @param writer Per-call owned output.
 * @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone.
 * @param type Program-lifetime literal event class name.
 */
template <typename Event>
void renderTrade(detail::EventTextWriter &writer, const Event &value, const EventTextContext &context,
                 std::u16string_view type) {
    writer.append(type);
    writer.append(u"{");
    writer.source(value.symbol);
    writer.append(u", eventTime=");
    writer.time(value.eventTime, true, context);
    writer.append(u", time=");
    writer.time(value.getTime(), true, context);
    writer.append(u", timeNanoPart=");
    writer.integer(value.timeNanoPart);
    writer.append(u", sequence=");
    writer.integer(value.getSequence());
    writer.append(u", exchange=");
    writer.character(value.exchangeCode);
    writer.append(u", tradeId=");
    writer.integer(value.tradeId);
    writer.append(u", price=");
    writer.number(value.getPrice());
    writer.append(u", change=");
    writer.number(value.getChange());
    writer.append(u", size=");
    writer.number(value.getSizeAsDouble());
    writer.append(u", day=");
    writer.integer(getYearMonthDayByDayId(value.dayId));
    writer.append(u", dayVolume=");
    writer.number(value.getDayVolumeAsDouble());
    writer.append(u", dayTurnover=");
    writer.number(value.getDayTurnover());
    writer.append(u", direction=");
    writer.append(directionName(value.getTickDirection()));
    writer.append(u", ETH=");
    writer.append(value.isExtendedTradingHours() ? u"true" : u"false");
    writer.append(u"}");
}

/** @param writer Per-call owned output.
 * @param value Unchanged borrowed Trade.
 * @param context Explicit immutable timezone.
 */
void render(detail::EventTextWriter &writer, const Trade &value, const EventTextContext &context) {
    renderTrade(writer, value, context, u"Trade");
}

/** @param writer Per-call owned output.
 * @param value Unchanged borrowed TradeETH.
 * @param context Explicit immutable timezone.
 */
void render(detail::EventTextWriter &writer, const TradeETH &value, const EventTextContext &context) {
    renderTrade(writer, value, context, u"TradeETH");
}

/** @tparam Event One supported event value.
 * @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone.
 * @param limits Per-call finite allowances.
 * @return Complete owned UTF-16 event text or atomic typed error.
 */
template <typename Event>
std::expected<std::u16string, EventTextError> eventText(const Event &value, const EventTextContext &context,
                                                        EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        render(writer, value, context);

        return writer.finish();
    });
}

/** @tparam Event One supported event value.
 * @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone.
 * @param limits Shared finite rendering and encoding allowances.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Complete owned UTF-8 event text or atomic typed error.
 */
template <typename Event>
std::expected<std::string, EventTextError> eventUtf8(const Event &value, const EventTextContext &context,
                                                     EventTextLimits limits, MalformedUtf16 malformed) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        render(writer, value, context);

        return writer.utf8(malformed);
    });
}
} // namespace

std::expected<std::u16string, EventTextError> toString(const Quote &value, const EventTextContext &context,
                                                       EventTextLimits limits) {
    return eventText(value, context, limits);
}

std::expected<std::u16string, EventTextError> toString(const Profile &value, const EventTextContext &context,
                                                       EventTextLimits limits) {
    return eventText(value, context, limits);
}

std::expected<std::u16string, EventTextError> toString(const Trade &value, const EventTextContext &context,
                                                       EventTextLimits limits) {
    return eventText(value, context, limits);
}

std::expected<std::u16string, EventTextError> toString(const TradeETH &value, const EventTextContext &context,
                                                       EventTextLimits limits) {
    return eventText(value, context, limits);
}

std::expected<std::string, EventTextError> toStringUtf8(const Quote &value, const EventTextContext &context,
                                                        EventTextLimits limits, MalformedUtf16 malformed) {
    return eventUtf8(value, context, limits, malformed);
}

std::expected<std::string, EventTextError> toStringUtf8(const Profile &value, const EventTextContext &context,
                                                        EventTextLimits limits, MalformedUtf16 malformed) {
    return eventUtf8(value, context, limits, malformed);
}

std::expected<std::string, EventTextError> toStringUtf8(const Trade &value, const EventTextContext &context,
                                                        EventTextLimits limits, MalformedUtf16 malformed) {
    return eventUtf8(value, context, limits, malformed);
}

std::expected<std::string, EventTextError> toStringUtf8(const TradeETH &value, const EventTextContext &context,
                                                        EventTextLimits limits, MalformedUtf16 malformed) {
    return eventUtf8(value, context, limits, malformed);
}

std::expected<std::u16string, EventTextError> toString(const Summary &value, const EventTextContext &context,
                                                       EventTextLimits limits) {
    return eventText(value, context, limits);
}

std::expected<std::string, EventTextError> toStringUtf8(const Summary &value, const EventTextContext &context,
                                                        EventTextLimits limits, MalformedUtf16 malformed) {
    return eventUtf8(value, context, limits, malformed);
}

std::expected<std::u16string, EventTextError> toString(Direction value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"DOWN", u"ZERO_DOWN", u"ZERO", u"ZERO_UP", u"UP"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(TradingStatus value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"HALTED", u"ACTIVE"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(ShortSaleRestriction value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"ACTIVE", u"INACTIVE"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(Side value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"BUY", u"SELL"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(Scope value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"COMPOSITE", u"REGIONAL", u"AGGREGATE", u"ORDER"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(OrderAction value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"NEW",     u"REPLACE", u"MODIFY", u"DELETE",
                                   u"PARTIAL",   u"EXECUTE", u"TRADE",   u"BUST"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(IcebergType value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"NATIVE", u"SYNTHETIC"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(PriceType value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"REGULAR", u"INDICATIVE", u"PRELIMINARY", u"FINAL"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(OtcMarketsPriceType value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNPRICED", u"ACTUAL", u"WANTED"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(TimeAndSaleType value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"NEW", u"CORRECTION", u"CANCEL"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(AuctionType value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"OPENING", u"CLOSING", u"REOPENING", u"OTHER"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(ImbalanceSide value, EventTextLimits limits) {
    return protect([&] {
        constexpr std::array names{u"UNDEFINED", u"BUY", u"SELL", u"NO_IMBALANCE", u"INSUFFICIENT_ORDERS", u"PAUSED"};
        const auto code = static_cast<std::int32_t>(value);

        if (code < 0 || static_cast<std::size_t>(code) >= names.size()) {
            throw TextFailure{EventTextError::INVALID_ENUM_CODE};
        }

        detail::EventTextWriter writer(limits);
        writer.append(names[static_cast<std::size_t>(code)]);

        return writer.finish();
    });
}

std::expected<std::optional<std::u16string>, EventTextError> toString(const IndexedEventSource &value,
                                                                      EventTextLimits limits) {
    return protect([&]() -> std::optional<std::u16string> {
        const auto name = value.name();

        if (!name) {
            return std::nullopt;
        }

        detail::EventTextWriter writer(limits);
        writer.source(*name);

        return writer.finish();
    });
}

std::expected<std::u16string, EventTextError> toString(const OrderSource &value, EventTextLimits limits) {
    return protect([&] {
        detail::EventTextWriter writer(limits);
        writer.source(value.name());

        return writer.finish();
    });
}
} // namespace org::ttldtor::qdcxx::events
