# Market enums, raw flags and event value conversions

[Русская версия](EVENT-VALUES.ru.md). English is authoritative.

The installed `qdcxx_event_values` target provides pure, synchronous value operations in
`org::ttldtor::qdcxx::events`. Its headers are `market_enums.hpp`, `event_flags.hpp`,
`time_helpers.hpp`, `short_string.hpp` and `event_value.hpp` under `qd/events/`.
There are no SDK, transport, timezone, scheduling, schema-loading or process-property dependencies.
Inputs are passed by value or borrowed only during the call; no references, callbacks or state survive.
Concurrent calls are independent. An owning decoded string has ordinary value lifetime.
The existing `qdcxx_trade`, `qdcxx_profile` and compatible `qdcxx::feed` package target expose this
component transitively. Existing Trade/Profile getter names and header includes remain usable.

## Business enums and unknown values

All twelve production market business enums have fixed `std::int32_t` representations:

| Type | Explicit codes, starting at zero | Java table size | Reserved entry projection |
| --- | --- | --- | --- |
| Direction | UNDEFINED, DOWN, ZERO_DOWN, ZERO, ZERO_UP, UP | 8 | UNDEFINED |
| TradingStatus | UNDEFINED, HALTED, ACTIVE | 4 | UNDEFINED |
| ShortSaleRestriction | UNDEFINED, ACTIVE, INACTIVE | 4 | UNDEFINED |
| Side | UNDEFINED, BUY, SELL | 4 | UNDEFINED |
| Scope | COMPOSITE, REGIONAL, AGGREGATE, ORDER | 4 | No reserved entries |
| OrderAction | UNDEFINED, NEW, REPLACE, MODIFY, DELETE_ORDER, PARTIAL, EXECUTE, TRADE, BUST | 16 | UNDEFINED |
| IcebergType | UNDEFINED, NATIVE, SYNTHETIC | 4 | UNDEFINED |
| PriceType | REGULAR, INDICATIVE, PRELIMINARY, FINAL | 4 | No reserved entries |
| OtcMarketsPriceType | UNPRICED, ACTUAL, WANTED | 4 | UNPRICED |
| TimeAndSaleType | NEW, CORRECTION, CANCEL | 4 | NEW |
| AuctionType | UNDEFINED, OPENING, CLOSING, REOPENING, OTHER | 8 | UNDEFINED |
| ImbalanceSide | UNDEFINED, BUY, SELL, NO_IMBALANCE, INSUFFICIENT_ORDERS, PAUSED | 8 | UNDEFINED |

`marketEnumFromCode<E>(code)` preserves **every** signed32 code, including negative, reserved and
out-of-table values. `marketEnumCode(value)` returns that exact code. `isKnownMarketEnum(value)`
checks whether the raw value names an explicit enumerator. These operations do not throw or allocate.
The `MarketEnum` concept admits exactly the twelve types above; unrelated enums are rejected at compile time.

`marketEnumValue(value)` separately reproduces Java `valueOf(int)`: a named code returns itself,
a reserved table entry returns the listed sentinel, and a negative or out-of-table code returns
`std::unexpected(EventValueError::INVALID_ENUM_CODE)`. The Java failure is an array-bounds exception;
the native API uses a typed checked result. A sentinel projection is not raw preservation. Retain
the raw value when encoding or inspecting unknown input. Trade/Profile still keep complete raw
`flags` and return their established sentinel projections without overwriting those flags.

```cpp
#include <qd/events/market_enums.hpp>
namespace events = org::ttldtor::qdcxx::events;
const auto raw = events::marketEnumFromCode<events::Direction>(7);
const auto projected = events::marketEnumValue(raw); // Direction::UNDEFINED
const auto preserved = events::marketEnumCode(raw); // 7
```

The new native token `OrderAction::DELETE_ORDER` has Java's `DELETE` code 4. The spelling avoids the Windows SDK `DELETE` macro without undefining or changing it; the Java business name remains `DELETE`. The installed Windows consumer includes all five new headers after `Windows.h` and verifies that macro and numeric identity.

## Bit fields and indexed masks

The signed32/signed64 `getBits(flags, mask, shift)` and `setBits(flags, mask, shift, bits)` overloads
reproduce EventUtil. Extraction uses an arithmetic right shift, then an unshifted mask. Replacement
masks the new bits and preserves every bit outside `mask << shift`, including unknown and sign bits.
Masks can be arbitrary signed patterns. Shift distances use the low five/six bits, including negative
and oversized distances, exactly as Java does. Shifted-out bits are discarded. All calculations use
defined unsigned intermediate arithmetic; no C++ signed overflow or invalid shift is required.

`indexed_event_flags` exposes TX_PENDING=0x01, REMOVE_EVENT=0x02, SNAPSHOT_BEGIN=0x04,
SNAPSHOT_END=0x08, SNAPSHOT_SNIP=0x10 and SNAPSHOT_MODE=0x40. These are indexed-event transaction
and snapshot masks, separate from event-specific business flags and QTP transport metadata. Merely
using these constants does not process transactions, snapshots or construct an indexed event mapper.

## Time, packed sequence and day ID

`getNanosFromMillisAndNanoPart(millis, part)` returns the low signed64 bits of
`millis * 1000000 + part`. Every signed64 millisecond value and signed32 part is accepted;
parts outside 0..999999 are preserved arithmetically. `getMillisFromNanos` and
`getNanoPartFromNanos` use mathematical floor division/modulus, so -1ns splits into -1ms and 999999ns.
Composition wraps at signed64 boundaries; splitting always has a nonnegative remainder.

`getSecondsFromTime(millis)` floors milliseconds to seconds and saturates to signed32 limits.
`getMillisFromTime(millis)` independently returns the nonnegative 0..999 remainder even when seconds
saturate. They reproduce TimeUtil's numeric functions, without its timezone or daily-time functions.

The packed time/sequence word has signed32 seconds in high32, ten millisecond bits in low32 bits31..22,
and a low22 sequence. `getTimeFromTimeSequence` includes all ten bits, even 1000..1023;
`getSequenceFromTimeSequence` returns 0..MAX_EVENT_SEQUENCE (4194303). `withTimeSequenceTime` applies
the saturated seconds/floor remainder conversion and preserves the original low22 sequence.
`withTimeSequenceSequence` replaces only low22 bits, preserving the raw time and reserved millisecond
patterns, or returns INVALID_SEQUENCE outside 0..4194303. No uniqueness/monotonicity is promised.
These are TradeBase-compatible packed business values, not FIELD event metadata or order indices.

`getDayIdByYearMonthDay(year, month, day)` and its signed packed-date overload reproduce DayUtil.
The calendar is proleptic Gregorian with astronomical year zero and signed32 year/day values.
Only month is checked: 1..12 succeeds; other months return INVALID_MONTH. Day zero, negative days
and days beyond the month's usual length normalize by arithmetic, including leap years and century rules.
For example, 2000-02-30 is 2000-03-01. The packed form decomposes
`yearSign * (abs(year)*10000 + month*100 + day)` (+1 for nonnegative years, including zero) with Java signed decimal division and negation,
including the signed-minimum overflow case. No locale, timezone or trading-calendar interpretation occurs.

`getYearMonthDayByDayId(dayId)` accepts every signed32 day count since 1970-01-01 and returns the
signed packed Gregorian date using the production March-based cycle algorithm. All intermediate
Java int wraps are reproduced explicitly in both directions. Extreme values can overflow the
packed representation or the inverse's epoch shift; these results are preserved compatibility
values, not a promise of a round-tripping valid civil date. No arbitrary year restriction is added.

## SHORT_STRING business conversion

`encodeShortString(optional<u16string_view>)` accepts null or at most eight UTF-16 units,
each 0..0xFF. It checks length **before** characters: excess length returns SHORT_STRING_TOO_LONG,
otherwise a unit above 0xFF returns INVALID_SHORT_STRING_CHARACTER. Zero units count toward the
input length and are omitted from packed bytes. Null, empty and all-zero input encode as zero.
Nonzero units pack in source order, including a high sign bit; no allocation occurs.

`decodeShortString(int64)` removes all zero bytes from the complete raw64 word and returns null
for zero, otherwise an independently owned one-to-eight-unit UTF-16 string. This is byte-valued
text, not UTF-8. It performs no caching; allocation can throw `std::bad_alloc` or `std::length_error`.
The optional view is never retained. The existing SHORT_STRING wire identity remains 40 and its
field-level value remains signed low32, as specified in [FIELD-TYPES](../encoding/FIELD-TYPES.md).
The full64 utility does not widen that field. Sign-extension of a negative field raw32 is observable
when it is passed to the full64 decoder; the actual standard field witnesses retain that behavior.
No new wire type, implicit field conversion or typed TimeAndSale mapper is introduced.

## Production evidence and consumer use

The [production oracle](../../tools/upstream/event-values-oracle/README.md) calls the exact locked
[EventUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/impl/EventUtil.java),
[TimeNanosUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/impl/TimeNanosUtil.java),
[DayUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/DayUtil.java),
[TimeUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java),
all twelve enum tables, IndexedEvent constants and Trade time/sequence setters/getters. It also uses
the actual DXFeedScheme TimeAndSale.ExchangeSaleConditions SHORT_STRING field's parse/render methods.
The [capture contract](../../tests/fixtures/event-values/CAPTURES.md) distinguishes full64 helpers
from that signed32 standard field. UTC and Pacific/Honolulu observations must be byte-identical.

The [standalone installed consumer](../../tests/conformance/event_values_consumer/main.cpp) exercises
the public operations and input errors; its CMake project compiles every new header separately.
Link `qdcxx_event_values`, or the existing installed `qdcxx::feed` target when that component is selected.
It needs no private headers or source paths. Native conformance compares each result with actual Java
observations; platform, sanitizer, installation and relocation outcomes remain separate executed checks.
