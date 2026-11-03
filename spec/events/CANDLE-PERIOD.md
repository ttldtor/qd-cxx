# Candle aggregation types and periods

[Русская версия](CANDLE-PERIOD.ru.md). English is authoritative.

The independent C++23 target `qdcxx_candle_values` exports
[`candle_period.hpp`](../../include/qd/events/candle_period.hpp) in
`org::ttldtor::qdcxx::events`. `qdcxx_feed` links it publicly, including through
`qdcxx::feed` in an installed package. The pure target requires no SDK, runtime,
network, ambient configuration or initialization. Its private binary64 implementation
archive is installed for static dependency closure; no numeric header is installed.

This contract covers values only. Candle attributes, CandleSymbol, event mapping,
aggregation and receiving Candle events are separate capabilities.

The reference is [upstream.lock](../../upstream.lock), revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, production
`dxfeed-api/src/main/java/com/dxfeed/event/candle/CandleType.java` and `CandlePeriod.java`.

## Units and parsing

| CandleType | Short name | Estimated milliseconds per unit |
| --- | --- | ---: |
| TICK | t | 0 |
| SECOND | s | 1000 |
| MINUTE | m | 60000 |
| HOUR | h | 3600000 |
| DAY | d | 86400000 |
| WEEK | w | 604800000 |
| MONTH | mo | 2592000000 |
| OPTEXP | o | 2592000000 |
| YEAR | y | 31536000000 |
| VOLUME | v | 0 |
| PRICE | p | 0 |
| PRICE_MOMENTUM | pm | 0 |
| PRICE_RENKO | pr | 0 |

`candleTypeShortName` returns borrowed immutable static UTF-16 text.
`parseCandleType` first recognizes an exact case-sensitive short name, then searches
case-insensitive enum-name prefixes in table order. Thus `m`/`M` means MINUTE,
`mo`/`Mo` means MONTH, and `P` means PRICE. Lowercase `pm` and `pr` are exact
short names; uppercase `PM` is invalid, while `PR`/`Pr` resolves to PRICE as a
prefix of its enum name. A full enum name may end in one additional lowercase `s`:
`Minutes` and `MINUTEs` work, while `MINUTES` does not. Java case equivalence
includes dotless/dotted I, long S and Kelvin sign when they correspond to ASCII
letters in an enum name. No whitespace trimming or Unicode normalization occurs.
Empty, unrelated and malformed names throw `std::invalid_argument`.

The interval is an estimate: MONTH and OPTEXP use thirty days and YEAR uses 365.
It neither selects calendar boundaries nor calculates exchange sessions or option
expiration dates. Non-time units have an undefined duration, represented by zero.

## Period values, rendering and equality

`CandlePeriod(value, type)` stores any binary64 multiplier unchanged, including
negative values, both zeros, NaN payloads and infinities. Invalid enum values throw
`std::invalid_argument` at construction and in the standalone unit queries.
The default constructor, `TICK` and `DEFAULT` represent one tick; `DAY` is one day.
`getValue()` and `getType()` return the stored pair.

`toString()` appends the short name to the multiplier. Exactly one omits the
multiplier. Otherwise, if the value equals its Java signed64 narrowing conversion
converted back to double, the signed integer is used; this includes either zero
rendering as `0` and the positive binary64 value 2^63 rendering as
`9223372036854775807`. Other multipliers use Java 8 `Double.toString` digits,
uppercase E without a positive exponent sign, and fixed notation for magnitudes
from 10^-3 inclusive to 10^7 exclusive. Integer rendering takes precedence over
those notation thresholds. There is no locale-dependent formatting.

Equality uses the same unit and Java `Double.compare` equivalence: all NaNs
compare equal, while positive and negative zero differ. Rendering is therefore
not an injective identity encoding. No Java object-identity or enum-hash promise
is made.

`getPeriodIntervalMillis()` multiplies the double multiplier by the unit estimate,
then narrows as Java does: NaN becomes zero, overflow saturates to the signed64
limits, and other values truncate toward zero. The multiplication itself is
binary64; a zero-duration unit times infinity produces NaN and returns zero.

## Period text

`CandlePeriod::parse` scans a numeric prefix consisting only of ASCII digits,
`.`, `+`, `-`, `e`, and `E`. An absent prefix means one; a present prefix must be
a complete Java decimal double. The suffix follows `parseCandleType` rules.
No whitespace is trimmed by the period scanner. Overflow/underflow yield
infinity/signed zero, as in Java. For example:

```cpp
#include <qd/events/candle_period.hpp>
using namespace org::ttldtor::qdcxx::events;
auto period = CandlePeriod::parse(u"2.5Minutes");
// period.toString() == u"2.5m", interval == 150000 milliseconds.
```

Actual scanner behavior takes precedence over the reference Javadoc's general
round-trip wording. NaN and infinity can be constructed and rendered, but their
rendered strings cannot be parsed by this scanner. Hexadecimal literals and
Java floating suffixes are also invalid. `-0t` parses as negative zero but renders
`0t`; `1e309d` parses as infinity and renders `Infinityd`, which is not parseable.

## Ownership, errors and verification

Values own no shared runtime state or borrowed inputs. Const operations do not
mutate caches and can run concurrently; modifying the same value requires ordinary
external synchronization. No callback, deferred work or process setting is used.
All parsing inputs are borrowed only for the call, and rendered strings are owned.
Parsing and rendering may propagate `std::bad_alloc` or `std::length_error`;
allocation failure is never converted to invalid syntax. Work/storage is linear
in the supplied input length; binary64 digit generation uses fixed-capacity internal
arithmetic and a bounded digit count. There is no implicit process-wide input limit.

The [production oracle](../../tools/upstream/candle-period-oracle/README.md) records
all thirteen units, valid/invalid aliases, every single UTF-16 code unit, numeric
boundaries and deterministic raw binary64 cases in
[the raw TSV fixture](../../tests/fixtures/events/candle/period-oracle.tsv), with
[provenance](../../tests/fixtures/events/candle/period-provenance.json).
The native conformance test consumes every row and separately checks semantic
boundaries, signed-zero/NaN equality and invalid enums. Shared IPF numeric behavior
retains its independent value, reader and writer conformance groups.
