# Candle symbol attributes

[Русская версия](CANDLE-ATTRIBUTES.ru.md). English is authoritative.

The C++23 target `qdcxx_candle_values` exports
[`candle_attributes.hpp`](../../include/qd/events/candle_attributes.hpp) in
`org::ttldtor::qdcxx::events`. Installed `qdcxx::feed` exposes it transitively.
The pure target depends on the pure market-symbol operations and private numeric archive,
with no SDK, IPF, runtime, network, schedule calculation or ambient configuration.
CandleSymbol composition, typed Candle events, aggregation, schedule filters, Java serialization
and Java object identity/hash codes are outside this value contract.

The authority is [upstream.lock](../../upstream.lock), revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, production
`dxfeed-api/src/main/java/com/dxfeed/event/candle/` classes CandlePrice, CandleSession,
CandleAlignment, CandlePriceLevel, CandleDataType and CandleExchange. All six are present.

## Values and keys

| Type | Encoding/key | Default | Accepted values |
| --- | --- | --- | --- |
| CandlePrice | price | LAST: last | LAST last, BID bid, ASK ask, MARK mark, SETTLEMENT s |
| CandleSession | tho | ANY: false | ANY false, REGULAR true |
| CandleAlignment | a | MIDNIGHT: m | MIDNIGHT m, SESSION s |
| CandlePriceLevel | pl | NaN | Nonnegative finite binary64 including positive zero; any NaN becomes canonical NaN |
| CandleDataType | dt | UNDEFINED: empty | Any exact UTF-16 text; canonical constants SOURCE source, THO tho, ALL all, RTH rth, ETH eth, UNDEFINED empty |
| CandleExchange | Regional suffix, no attribute key | COMPOSITE: zero code unit | Every UTF-16 code unit; no membership check against supported venues |

Price/session parsers accept a nonempty case-insensitive prefix of their canonical text;
alignment accepts only exact case-insensitive m or s. Price settlement therefore accepts s,
but not settlement. Session accepts t/f prefixes, but not regular/any. Java case equivalence
includes dotted/dotless I, long S and Kelvin sign when matching ASCII letters. No trimming or
Unicode normalization occurs. Null/empty/unknown enum text throws `std::invalid_argument`.
Invalid C++ enum values also throw before symbol mutation.

The session symbol reader differs from its standalone parser: only a complete case-insensitive
true selects REGULAR; missing, empty, prefixes, whitespace and every other string select ANY.
Price/alignment symbol readers parse a present value strictly and use their defaults only
when the attribute is absent. A null symbol has no attributes.

Price level rejects negative zero, negatives and either infinity. Its parser uses the shared
Java double syntax: ASCII control/space trimming through U+0020, decimal/hexadecimal literals,
floating suffixes, case-sensitive NaN/Infinity spellings, overflow and underflow. Invalid or
null text throws `std::invalid_argument`; underflow to negative zero is rejected. The default
constructor and DEFAULT mean NaN. Equality treats all NaNs as equal. Input NaN payloads are
canonicalized, matching Java valueOf. Numeric rendering uses the same integer-first policy and
Java 8 digits as [Candle periods](CANDLE-PERIOD.md); positive 2^63 renders
`9223372036854775807`. NaN renders NaN, positive zero renders 0.

CandleDataType copies all input. Null and empty mean undefined; unknown values, whitespace,
delimiters, case and malformed UTF-16 are preserved. SOURCE and source are distinct values.
`getValue()` borrows from the object until mutation/destruction; `toString()` returns a copy.
CandleExchange renders zero as COMPOSITE and other values as their single exact code unit;
it has no text parser. An explicit regional NUL reads as composite.

## Symbol operations and reference normalization

Enum free functions use the pattern `parseCandlePrice`, `candlePriceText`,
`getCandlePrice`, `changeCandlePrice`, `normalizeCandlePrice`; Session and Alignment
use corresponding names. The three value classes provide `getAttributeForSymbol` and
`changeAttributeForSymbol`; PriceLevel and DataType also provide `parse` and
`normalizeAttributeForSymbol`. Exchange uses its code-unit constructor.

```cpp
#include <qd/events/candle_attributes.hpp>
using namespace org::ttldtor::qdcxx::events;
auto symbol = changeCandlePrice(u"AAPL{=m}", CandlePrice::BID);
auto interval = CandlePriceLevel::parse(u"0.25");
auto grouped = interval.changeAttributeForSymbol(*symbol);
auto custom = CandleDataType(u"source-specific");
```

Changing price/session/alignment to their defaults removes matching keys. NaN price level
and undefined data type also remove their keys. Composite exchange removes the regional
suffix. Other values replace/insert through the existing
[market-symbol contract](MARKET-SYMBOLS.md), preserving its duplicate/malformed-input
behavior and its lack of escaping. Null is represented by `std::nullopt`; changing null
to a nondefault attribute creates an attribute-only symbol, while default removal preserves null.
All output symbols are independent owned copies.

Normalization reproduces actual production code, including its quirks. Price, alignment
and price-level normalization call default removal but discard its result; they retain an
already canonical default attribute and canonicalize aliases without removing the key.
For example price=LAST becomes price=last, a=M becomes a=m, pl=+NaN becomes pl=NaN.
Invalid values remain unchanged. Session normalization similarly retains all values other
than complete true, and canonicalizes TRUE to true. This is deliberately different from
changing an attribute to its default. Data-type normalization really removes explicit empty dt;
all nonempty values remain byte-for-byte unchanged. Exchange has no normalizer.

## Ownership, errors and verification

Inputs are borrowed only for each call. Values/returned symbols retain no input views, callback,
runtime or deferred work. Enum text views point to immutable static text. Const operations
are independent; concurrent mutation of the same object requires external synchronization.
Work/storage is linear in input/output text, with no implicit global size limit. Numeric
conversion uses the existing bounded binary64 engine. Allocation/length errors propagate;
they are never converted into invalid syntax or ignored normalization errors.
Malformed symbol substring ranges propagate `std::out_of_range`.

The [production Java 8 oracle](../../tools/upstream/candle-attributes-oracle/README.md)
captures every single UTF-16 code unit for three parsers and exchange construction, null and
malformed symbols, defaults, aliases, unsupported inputs, numeric boundaries and deterministic
binary64 values. Native tests consume all
[265722 raw observations](../../tests/fixtures/events/candle/attributes-oracle.tsv), with
[provenance](../../tests/fixtures/events/candle/attributes-provenance.json), plus owning-value and
invalid-enum tests. The period fixture remains an independent regression of shared rendering.
UTC and Pacific/Honolulu captures must match. Build/package receipts are working evidence,
not a claim of receiving Candle events.

The canonical DataType constants and PriceLevel NaN default have constant initialization and are safe to read from another translation unit during dynamic initialization. Caller-supplied custom text is still copied; returned text remains independent of caller storage.
