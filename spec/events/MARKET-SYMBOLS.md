# Regional market symbols

[Русская версия](MARKET-SYMBOLS.ru.md). English is authoritative.

The pure `qdcxx_market_event_symbols` library exposes five free functions in
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
at the second `{`. The contents need not contain `=`. This boundary rule is used
solely to preserve the block verbatim; attribute parsing, sorting and normalization
are outside this contract.

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
with independent storage. Queries are `noexcept`; allocating operations propagate
standard `std::bad_alloc` and `std::length_error` without modifying inputs or
exposing a partial result. Size growth uses checked standard string operations,
without unchecked summed lengths. There is no retained state or I/O.

There is no UTF-8 overload. Convert explicitly at the application encoding boundary;
the exact representation matches `Quote::symbol`. No regular-expression selector,
generic attribute API, Candle syntax, source qualifier or subscription integration
is supplied here.

## Evidence

[Oracle fixtures and provenance](../../tests/fixtures/market-symbols/README.md)
cover null/empty, malformed blocks/suffixes, all 65,536 UTF-16 exchange units,
supported/default lists and changes preserving attribute order and duplicates.
[Reproduction tool](../../tools/upstream/market-symbol-oracle/README.md) invokes
the actual pinned production class. [Usage](../../docs/MARKET-SYMBOLS.md) explains
linking and examples.
