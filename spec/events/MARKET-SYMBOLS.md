# Market symbols and generic attributes

[Русская версия](MARKET-SYMBOLS.ru.md). English is authoritative.

The pure `qdcxx_market_event_symbols` library exposes nine free functions in
`org::ttldtor::qdcxx::events::market_symbols`, declared in
[`market_event_symbols.hpp`](../../include/qd/events/market_event_symbols.hpp).
The reference is QD 3.355 at the revision in [upstream.lock](../../upstream.lock),
`dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java`.
`Quote.java` documents separate `Quote` and `Quote&X` records. A helper result does
not enable regional Feed routing or a new record mapping.

## Exact syntax and operations

All positions and exchange codes are UTF-16 code units, including NUL and lone
surrogates. No Unicode validation, decoding, normalization or venue lookup occurs.
`SUPPORTED_EXCHANGES` is the upstream ASCII-ordered `0-9A-Za-z` list;
`DEFAULT_EXCHANGES` is `A-Z`. These are descriptive constants: parsing and composition
accept every code unit. `Q` remains a code; it does not establish a venue identity.

A trailing attribute block exists only when the input has at least three units,
ends in `}`, and its last `{` before that final unit has at least one intervening
unit. Thus `{x}` is a block, `{}` is ordinary text, and in `{a{b}` the block starts
at the second `{`. The contents need not contain `=`. Base/exchange operations preserve the block verbatim; attribute operations below interpret it.

Ignoring that block, a regional suffix is exactly `&` followed by one final unit.
`SPY&Q` is regional; `SPY` and `SPY&QQ` are composite syntactically. A surrogate
pair after `&` has two units and is not a regional suffix.

| Operation | Result |
| --- | --- |
| `hasExchangeCode` | Suffix presence, including an explicit NUL code; false for null |
| `getExchangeCode` | Suffix code or zero when absent/null |
| `getBaseSymbol` | Owned base without suffix or block; null stays null |
| `changeExchangeCode` | Replace/add code, or remove it for zero; preserve block |
| `changeBaseSymbol` | Replace base without interpreting it; preserve old suffix and block |

Null and empty inputs differ. `changeExchangeCode(null, 'Q')` produces `&Q`,
whereas a zero replacement keeps null. `changeBaseSymbol(null, base)` returns a
copy of `base`. A null replacement base remains null when no suffix/block must
be appended; otherwise Java concatenation produces the literal `null` followed
by that suffix/block. This measured behavior takes precedence over inconsistent
upstream Javadoc statements about null results.

## Ownership, failure and boundaries

Inputs are `std::optional<std::u16string_view>`, borrowed only during the call;
`std::nullopt` represents Java null. Results are `std::optional<std::u16string>`
with independent storage. Presence/code queries are `noexcept`; allocating operations propagate
standard `std::bad_alloc` and `std::length_error` without modifying inputs or
exposing a partial result. Size growth uses checked standard string operations,
without unchecked summed lengths. There is no retained state or I/O.

There is no UTF-8 overload. Convert explicitly at the application encoding boundary;
the exact representation matches `Quote::symbol`. No regular-expression selector,
Candle syntax, source qualifier or subscription integration
is supplied here.

## Evidence

[Oracle fixtures and provenance](../../tests/fixtures/market-symbols/README.md)
cover null/empty, malformed blocks/suffixes, all 65,536 UTF-16 exchange units,
supported/default lists and changes preserving attribute order and duplicates.
[Reproduction tool](../../tools/upstream/market-symbol-oracle/README.md) invokes
the actual pinned production class. [Usage](../../docs/MARKET-SYMBOLS.md) explains
linking and examples.

## Generic attributes

`hasAttributes` applies the same trailing-block rule and returns false for null.
`getAttributeStringByKey` returns the first exact matching key's value, or null for
an absent key/null symbol. Empty keys and values are valid and distinct from null.
All keys are required: a null key throws `std::invalid_argument`, corresponding to
Java `NullPointerException`, before symbol inspection or result allocation.

`changeAttributeStringByKey(symbol, key, value)` inserts/replaces a value; a null
value removes all matching keys, exactly like `removeAttributeStringByKey`.
Null symbol plus nonnull value produces `{key=value}`; null symbol plus null
value remains null. Removing an absent key returns an owned unchanged copy.
Removing the only attribute removes the braces. Exchange/base prefixes are preserved.

Scanning starts after the recognized opening brace. Each key extends to the next
`=`, even across commas. Its value extends to the next comma or the final `}`.
Scanning stops when no subsequent `=` exists. Keys and values are not validated,
escaped, decoded or normalized; embedded delimiters affect subsequent parsing.
No equality/hash API is added: returned strings retain exact UTF-16 equality.

Replacement retains the first encountered matching position and removes later
duplicates. Insertion occurs before the first encountered key lexically greater
than the new key, using unsigned UTF-16 units; otherwise it appends at the scan
endpoint. Unrelated keys retain their order. Thus changing `a` to `x` in
`S&Q{z=2,a=1}` yields `S&Q{a=x,z=2}`; changing `z` yields `S&Q{z=x,a=1}`.
This is the production algorithm's local ordering, not a full sort of malformed
or unsorted input. A block without `=` retains its unusual upstream behavior:
adding `a=x` to `S&Q{junk}` yields `S&Q,a=x{junk}`.

Allocation/size failures remain `std::bad_alloc`/`std::length_error`.
If malformed input causes a Java invalid substring range, the C++ contract is
`std::out_of_range`; there is no unchecked indexing or truncating range recovery.
The bounded [attribute corpus](../../tests/fixtures/market-attributes/README.md)
contains 276,958 production observations, including 9,672 null-key exceptions;
it contains no observed Java substring exception and is not exhaustive for all strings.
[Reproduction](../../tools/upstream/market-attribute-oracle/README.md) is separate
from the immutable regional/base fixture.
