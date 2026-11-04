// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/market_enums.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::time {
class TimeZoneData;
}

namespace org::ttldtor::qdcxx::events {
struct Quote;
struct Profile;
struct Trade;
struct TradeETH;
struct Summary;
class IndexedEventSource;
class OrderSource;

namespace detail {
struct EventTextZone;
class EventTextWriter;
} // namespace detail

/** Atomic formatting failures; an error never exposes a partial result. */
enum class EventTextError {
    RESOURCE_LIMIT,      ///< An explicit input, output or logical-work allowance was exceeded.
    ALLOCATION,          ///< An owning allocation failed.
    INVALID_ENUM_CODE,   ///< A raw C++ enum does not name a Java constant.
    INVALID_UTF16,       ///< Strict UTF-8 conversion found an unpaired UTF-16 surrogate.
    TIME_ZONE_NOT_READY, ///< A named-zone context requires an initialized explicit provider.
    UNKNOWN_TIME_ZONE,   ///< The exact named zone is absent from the selected provider profile.
    TIME_ZONE_RANGE,     ///< The qualified binding or offset cannot represent this requested operation.
    TIME_ZONE_DATA,      ///< The explicit immutable provider reported a data failure.
    INVALID_CONTEXT      ///< Context, encoding policy or the qualified timezone query is invalid.
};

/** Per-call finite allowances; zero is meaningful for every member. */
struct EventTextLimits {
    std::size_t inputUnits;  ///< Maximum combined borrowed UTF-16 source units inspected/copied.
    std::size_t outputUnits; ///< Maximum UTF-16 result units, including literal punctuation.
    std::size_t utf8Bytes;   ///< Maximum UTF-8 result bytes; unused by UTF-16-only functions.
    std::size_t work;        ///< Logical scalar, source inspection and output-copy credits; see EVENT-TEXT.
};

/** Policy for unpaired surrogates when converting literal Java UTF-16 text to UTF-8. */
enum class MalformedUtf16 {
    REJECT, ///< Return INVALID_UTF16 without a result.
    REPLACE ///< Encode each unpaired surrogate as U+FFFD; valid pairs remain one scalar.
};

/** Explicit immutable formatting timezone; copies retain any owned provider binding.
 * No ambient timezone, locale, filesystem, network or mutable formatter cache is consulted.
 * Independent const calls may run concurrently while callers keep input events unchanged.
 */
class EventTextContext final {
    std::int32_t fixedOffsetMillis_ = 0;
    std::shared_ptr<const detail::EventTextZone> zone_;

    /** @param offsetMillis Exact Java signed32 raw offset, including sub-minute offsets. */
    explicit EventTextContext(std::int32_t offsetMillis) noexcept;
    /** Query the retained immutable binding or the fixed offset.
     * @param timeMillis Signed milliseconds since the Unix epoch.
     * @return Offset milliseconds or an explicit provider/context error.
     */
    [[nodiscard]] std::expected<std::int32_t, EventTextError> offsetAt(std::int64_t timeMillis) const;
    friend class detail::EventTextWriter;

    public:
    /** @param other Immutable context to share without allocation. */
    EventTextContext(const EventTextContext &other) noexcept = default;
    /** @param other Immutable context; the source remains usable after this copy-like move. */
    EventTextContext(EventTextContext &&other) noexcept;
    /** @param other Immutable context to share.
     * @return This independently owned handle after replacement.
     */
    EventTextContext &operator=(const EventTextContext &other) noexcept = default;
    /** @param other Immutable context; the source remains usable.
     * @return This independently owned handle after replacement.
     */
    EventTextContext &operator=(EventTextContext &&other) noexcept;
    /** @return Explicit UTC context; no provider initialization or allocation. */
    [[nodiscard]] static EventTextContext utc() noexcept;
    /** @param offsetMillis Any signed32 Java SimpleTimeZone raw offset, in milliseconds.
     * @return Explicit fixed-offset context without provider initialization or allocation.
     */
    [[nodiscard]] static EventTextContext fixedOffset(std::int32_t offsetMillis) noexcept;
    /** Bind an exact named timezone to the explicitly selected immutable provider.
     * @param data Optional ready capability, borrowed only during binding; success retains its own immutable pin.
     * @param zoneId Exact UTF-16 zone identity, borrowed only during binding.
     * @param rawOffsetReferenceMillis Explicit instant selecting Java's raw-offset convention.
     * @param maxIdUnits Maximum accepted copied UTF-16 identity units, default 128; zero is meaningful.
     * @return Independently owned context or typed unavailable/identity/resource/allocation error.
     */
    [[nodiscard]] static std::expected<EventTextContext, EventTextError> timeZone(const time::TimeZoneData *data,
                                                                                  std::u16string_view zoneId,
                                                                                  std::int64_t rawOffsetReferenceMillis,
                                                                                  std::size_t maxIdUnits = 128);
};

/** Format the current Java Double.toString numeric profile, independent of Candle/IPF formats.
 * @param value Any IEEE 754 binary64 value, including signed zero and nonfinite values.
 * @param limits Finite result/work allowances; no borrowed source units are consumed.
 * @return Owned UTF-16 result or atomic resource/allocation failure.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> formatEventDouble(double value, EventTextLimits limits);
/** Encode one UTF-16 code unit as EventUtil.encodeChar, including individual surrogates.
 * @param value Any UTF-16 unit.
 * @param limits Finite result/work allowances; no borrowed source units are consumed.
 * @return Literal printable ASCII, backslash-zero, or lowercase backslash-u plus four hex digits.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> encodeEventChar(char16_t value, EventTextLimits limits);
/** Render every raw flag bit using Java's unsigned lowercase hexadecimal spelling.
 * @param flags Complete signed32 word; unknown/reserved bits are retained.
 * @param limits Finite result/work allowances.
 * @return Owned hexadecimal text without a prefix, or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> formatEventFlags(std::int32_t flags,
                                                                             EventTextLimits limits);
/** Format a TimeFormat timestamp with explicit timezone rules.
 * @param timeMillis Any signed64 epoch milliseconds; zero is rendered as zero.
 * @param withMillis Whether to include the three-digit millisecond suffix.
 * @param context Immutable explicit timezone; borrowed only during the call.
 * @param limits Finite result/work allowances.
 * @return Owned timestamp, raw signed64 fallback outside the Java date range, or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
formatEventTime(std::int64_t timeMillis, bool withMillis, const EventTextContext &context, EventTextLimits limits);
/** Convert exact UTF-16 source text to standard UTF-8 without process encoding state.
 * @param text Borrowed source, unchanged for this call; nothing is retained.
 * @param limits Finite input/output-byte/work allowances; outputUnits is unused.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Owned UTF-8 bytes or atomic input/resource/allocation error.
 */
[[nodiscard]] std::expected<std::string, EventTextError> toUtf8(std::u16string_view text, EventTextLimits limits,
                                                                MalformedUtf16 malformed);

/** Render a complete supported event with Java field order and explicit formatting context.
 * @param value Borrowed event; all fields remain unchanged for this synchronous call.
 * @param context Borrowed immutable explicit timezone context.
 * @param limits Finite combined input, UTF-16 output and logical-work allowances.
 * @return Complete independently owned UTF-16 text or atomic error; no generic parsing is promised.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
toString(const Quote &value, const EventTextContext &context, EventTextLimits limits);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Finite per-call allowances.
 * @return Complete owned Java-compatible Profile text or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
toString(const Profile &value, const EventTextContext &context, EventTextLimits limits);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Finite per-call allowances.
 * @return Complete owned Java-compatible Trade text or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
toString(const Trade &value, const EventTextContext &context, EventTextLimits limits);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Finite per-call allowances.
 * @return Complete owned Java-compatible TradeETH text or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
toString(const TradeETH &value, const EventTextContext &context, EventTextLimits limits);

/** Render the complete Summary business value without narrowing OpenInterest.
 * @param value Unchanged borrowed Summary, including its full signed64 OpenInterest.
 * @param context Explicit immutable timezone for independent eventTime metadata.
 * @param limits Finite per-call input/output/work allowances.
 * @return Complete owned Java-compatible Summary text or atomic error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError>
toString(const Summary &value, const EventTextContext &context, EventTextLimits limits);
/** Render and encode a complete event directly for UTF-8 streams such as std::cout.
 * @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Shared finite allowances for rendering and encoding, including UTF-8 bytes.
 * @param malformed Explicit unpaired-surrogate policy; no process encoding is inferred.
 * @return Complete owned UTF-8 text or atomic error; no partial stream write occurs here.
 */
[[nodiscard]] std::expected<std::string, EventTextError>
toStringUtf8(const Quote &value, const EventTextContext &context, EventTextLimits limits, MalformedUtf16 malformed);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Shared render/encoding allowances.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Complete owned UTF-8 text or atomic error.
 */
[[nodiscard]] std::expected<std::string, EventTextError>
toStringUtf8(const Profile &value, const EventTextContext &context, EventTextLimits limits, MalformedUtf16 malformed);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Shared render/encoding allowances.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Complete owned UTF-8 text or atomic error.
 */
[[nodiscard]] std::expected<std::string, EventTextError>
toStringUtf8(const Trade &value, const EventTextContext &context, EventTextLimits limits, MalformedUtf16 malformed);
/** @param value Unchanged borrowed event.
 * @param context Explicit immutable timezone context.
 * @param limits Shared render/encoding allowances.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Complete owned UTF-8 text or atomic error.
 */
[[nodiscard]] std::expected<std::string, EventTextError>
toStringUtf8(const TradeETH &value, const EventTextContext &context, EventTextLimits limits, MalformedUtf16 malformed);
/** @param value Unchanged borrowed Summary, retaining full signed64 OpenInterest.
 * @param context Explicit immutable timezone.
 * @param limits Shared render/encoding allowances.
 * @param malformed Explicit unpaired-surrogate policy.
 * @return Complete owned UTF-8 Summary text or atomic error.
 */
[[nodiscard]] std::expected<std::string, EventTextError>
toStringUtf8(const Summary &value, const EventTextContext &context, EventTextLimits limits, MalformedUtf16 malformed);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known Direction constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(Direction value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known TradingStatus constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(TradingStatus value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known ShortSaleRestriction constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(ShortSaleRestriction value,
                                                                     EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known Side constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(Side value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known Scope constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(Scope value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known OrderAction constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(OrderAction value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known IcebergType constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(IcebergType value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known PriceType constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(PriceType value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known OtcMarketsPriceType constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(OtcMarketsPriceType value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known TimeAndSaleType constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(TimeAndSaleType value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known AuctionType constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(AuctionType value, EventTextLimits limits);
/** Render the exact Java constant name without normalizing unknown raw codes.
 * @param value Known ImbalanceSide constant; raw unnamed codes are rejected.
 * @param limits Finite result/work allowances.
 * @return Owned UTF-16 Java name or INVALID_ENUM_CODE/resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(ImbalanceSide value, EventTextLimits limits);
/** Preserve the source's exact nullable Java toString value.
 * @param value Borrowed owned source identity, unchanged during the call.
 * @param limits Finite name-copy/input/output/work allowances.
 * @return Independently owned nullable UTF-16 name; null differs from the literal string null.
 */
[[nodiscard]] std::expected<std::optional<std::u16string>, EventTextError> toString(const IndexedEventSource &value,
                                                                                    EventTextLimits limits);
/** @param value Borrowed resolved order source, unchanged during the call.
 * @param limits Finite name-copy/input/output/work allowances.
 * @return Owned exact UTF-16 source name or atomic resource/allocation error.
 */
[[nodiscard]] std::expected<std::u16string, EventTextError> toString(const OrderSource &value, EventTextLimits limits);
} // namespace org::ttldtor::qdcxx::events
