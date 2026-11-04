# Candle symbol values

[Русская версия](CANDLE-SYMBOL.ru.md). English is authoritative.

The C++23 `qdcxx_candle_values` target exports
[`candle_symbol.hpp`](../../include/qd/events/candle_symbol.hpp) in
`org::ttldtor::qdcxx::events`. Installed `qdcxx::feed` exposes it transitively.
The [guide](../../docs/CANDLE-SYMBOLS.md) and its complete installed consumer use this public API.
No Candle event projection, aggregation, schedule computation or receiving support is added.

The authority is [upstream.lock](../../upstream.lock), revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, production
`dxfeed-api/src/main/java/com/dxfeed/event/candle/CandleSymbol.java` and its seven attribute types.
Period/type and the other attributes retain their
[period](CANDLE-PERIOD.md), [attribute](CANDLE-ATTRIBUTES.md) and
[market-symbol](MARKET-SYMBOLS.md) contracts.

## Construction and normalization

`CandleSymbol::valueOf(optional UTF-16)` copies the input, normalizes it, then initializes
base/exchange/price/session/period/alignment/price-level/data-type getters. The attribute-list
overload accepts `CandleAttribute`, a closed variant of CandleExchange, CandlePrice,
CandleSession, CandlePeriod, CandleAlignment, CandlePriceLevel and CandleDataType. The variadic
overload is a convenience for these same types. Custom CandleDataType strings remain open-ended.

Normalization calls price, session, period, alignment, price level and data type in exactly
that order. Exchange is syntactic and is not normalized. Each pass uses the existing market-symbol
helper, so a changed known attribute can replace duplicate occurrences and insert its canonical
spelling. There is no separate global key sort, unknown-key validation, deduplication or escaping.
Already canonical attributes, unknown keys, case-sensitive keys, malformed blocks and ordering
can remain unchanged. For example `A{z=1,a=m}` and `A{a=m,z=1}` remain distinct identities.
The actual algorithm takes precedence over the Java Javadoc's general sorting description.

The discarded default-removal result from the attribute normalizers also applies to period:
`A{=1T}` becomes `A{=t}`, retaining the explicit default key. Session uses complete true
recognition when reading a symbol. Undefined data type really removes an empty dt. Invalid known
values can survive a normalization pass, but construction then rejects them while initializing
the remaining getters. Unknown attributes are preserved.

An attribute-taking factory first applies all supplied changes in order, then normalizes,
then installs explicit getter values and reads only the remaining getters from the text.
Repeating a kind throws `std::invalid_argument`, including equal duplicates. Invalid enum
values and valueless C++ attribute variants are refused. Factory failure exposes no partial value.

Explicit getters are significant: a negative-zero tick period renders `A{=0t}` but its getter
retains negative zero; plain parsing of that same text produces positive zero. An explicitly
supplied infinite or NaN period can create a symbol whose text cannot be parsed without that
explicit period. For example an infinite daily period yields `A{=Infinityd}`; the getter remains
infinite, whereas plain valueOf of that text throws. Equality and hashing use canonical text,
not the parsed getter tuple. No attempt is made to repair these reference quirks.

## Identity, null and ownership

`getSymbol()` and `getBaseSymbol()` return borrowed optional UTF-16 views; `toString()`
returns an independently owned optional copy. Null input is preserved, has a null base and
default getters unless explicit changes create text. Empty and null are distinct.

`hashCode()` implements Java String's signed32 UTF-16 hash with unsigned wraparound and the
multiplier 31. Null text throws `std::invalid_argument`, safely representing Java's null-domain
failure. `equals(other)` compares exact canonical text for a nonnull left operand; a null left
operand throws unless comparing the same C++ object to itself. A nonnull left operand compared
with a null symbol returns false. These are the observed Java-like equals/hashCode rules.

`operator==` is a deliberate C++ convenience adaptation: it is total and symmetric over the
optional canonical text, so two null values compare equal. It is not presented as exact Java
equals parity on null values. No std::hash specialization or implicit string conversion is supplied.

Values share immutable owned storage. Copy and move operations do not allocate; moves also leave
the source usable. Assignment replaces the complete shared value without allocation or partial getter-field updates.
Getter views/references are guaranteed until that object's replacement/destruction; another copy
may keep the storage alive longer. Caller text/attribute storage is never retained. Concurrent
const inspection of independent values is supported; mutation/destruction of the same wrapper
requires ordinary synchronization. Global-initializer factory use is checked against the existing
constant-initialized attributes and periods. There is no mutable cache, process setting, SDK or
hidden progress in value operations.

Allocation and capacity failures propagate as `std::bad_alloc`/`std::length_error`; malformed
market-symbol ranges propagate `std::out_of_range`. Input work follows a fixed six normalization
passes plus the explicit attribute changes; storage follows the owned symbol/base and explicit
string values. There is no ambient input limit or implicit network work.

## Subscription identity bridge

The existing public `Subscription<Event>` accepts one CandleSymbol in addSymbols/removeSymbols
and nonempty CandleSymbol initializer lists in addSymbols/setSymbols. Constrained list overloads
preserve existing empty-brace calls, which continue to choose the string overloads.

The object batch is validated and copied before one existing mutateSymbols transaction. It uses
the canonical text as the actual desired subscription key; equivalent object texts coalesce and
an equivalent object removes the same key. Null, nonprintable/non-ASCII, leading wildcard and
oversized canonical text are rejected through the existing subscription-symbol validation path.
A failed batch preserves the complete previous desired set. Distinct prepared storage stays within
the existing subscription count and symbol-size limits; no callback or view is retained.

This bridge uses currently supported event projections and their existing route checks. It does
not infer a Candle event type from text, and does not add Candle receiving or aggregation.
The ordinary string overloads retain their exact-text behavior.

The [production oracle](../../tools/upstream/candle-symbol-oracle/README.md) records 9822 factory,
getter, malformed/duplicate/unknown-key and equality observations in
[the fixture](../../tests/fixtures/events/candle/symbol-oracle.tsv), with
[provenance](../../tests/fixtures/events/candle/symbol-provenance.json). UTC/Honolulu bytes must match.
Native tests consume all rows, including raw getter bits, and cover owning copies, null adaptation,
explicit getter differences and startup use. The real inactive feed test uses a one-symbol limit to
prove canonical-object dedup/removal and atomic invalid-batch refusal in the public desired map.
