# Trade projection

[Русская версия](TRADE.ru.md). English is authoritative.

This contract covers an owning `events::Trade`, `PreparedTrade`, registered composite and regional
Trade records, typed `Subscription<Trade>` and mixed `RegisteredEvent` delivery.
[TradeETH](TRADE-ETH.md) has its own separate supported type; Summary remains unsupported. Generic records remain available independently.

The reference is Java QD 3.355 at the revision in `upstream.lock`: `TradeBase`, `Trade`, generated
`TradeMapping` and `TradeDelegate`, `MarketEventMapping`, `MarketEventDelegateSet` and `Direction`.
The [capture description](../../tests/fixtures/trade/CAPTURES.md) identifies the actual production
oracle and the raw `oracle.tsv` dataset. These observations are not replacement converter formulas.

## Public values and ownership

`Trade` owns its UTF-16 symbol and all scalar state. No cursor, field span, schema or callback reference
survives mapping. All members have defined default initialization. Five decimal members default to
raw zero (NaN); `eventTime`, packed time, nano part, exchange, TradeId, day and flags default to zero.
The event is an aggregate; its trailing optional five-entry inline snapshot has no separate heap
allocation. The public value has no transport base class or mutable process-global configuration.

The five `WideDecimal` members are the generated mapping's wide-accessor view. The Java-style
`getPrice`, `getSizeAsDouble`, `getChange`, `getDayVolumeAsDouble` and `getDayTurnover` return the
production business-double view captured before LONG-to-wide rounding. For example, source LONG
36028797018963973 has a business binary64 value one ULP above the corresponding wide-to-double view.
Each snapshot includes the corresponding wide raw bits. Editing one decimal member selects that
member's current raw view if it differs, without invalidating other source business values. Resetting
the snapshot selects all current raw views; restoring the original raw bits makes the matching source
value applicable again. Manual aggregates have no snapshot. `WideDecimal::toDouble` is unchanged.
Copying/moving preserves owning state; destinations do not depend on source lifetime.

`getSize` and `getDayVolume` apply Java binary64-to-long narrowing to their business getters:
truncate toward zero, NaN to zero, and saturation at signed64 limits for infinity or out-of-range
values. No undefined native floating-to-integer cast is used. NaN results promise classification,
not the sign or payload produced by floating-point arithmetic.

## Exact keys, types and defaults

All names below are exact case-sensitive explicit local lookup alternatives. Extra fields and records
are retained. The mapper validates the complete context and canonical value shape before projection,
including unselected extra values. No string/number coercion or inferred record-name dispatch occurs.

| Physical key / property alternative | Required | Accepted full local types | Missing value |
|---|---|---|---|
| Last.Time / Time | no | raw-int set | seconds zero |
| Last.Sequence / Sequence | no | raw-int set | packed low word zero |
| Last.TimeNanoPart / TimeNanoPart | no | raw-int set | zero |
| Last.Exchange / ExchangeCode | no | raw-int set | syntactic record exchange, composite zero |
| TradeId | no | 104 LONG, 120 WIDE_DECIMAL, 152 TIME_MILLIS | zero |
| Last.Price / Price | yes | numeric set | preparation error |
| Last.Size / Size | yes | numeric set | preparation error |
| Last.Tick / Tick | no | raw-int set | zero |
| Last.Change / Change | no | numeric set | wide raw zero, business NaN |
| DayId | no | raw-int set | zero |
| Volume / DayVolume | no | numeric set | wide raw zero, business NaN |
| DayTurnover | no | numeric set | wide raw zero, business NaN |
| Last.Flags / Flags | no | raw-int set | zero |

The numeric set is exactly full IDs **2, 4, 8, 24, 104, 120**. The raw-int set is exactly
**2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152**. Their physical representation and numeric semantics
are specified in [FIELD-CONVERSIONS](FIELD-CONVERSIONS.md). A two-slot raw-int getter reads the high
physical int word. Exchange narrows that signed word to its low UTF-16 unit without normalization.
`DayId` is signed days since 1970-01-01, without calendar interpretation.

`TradeId` uses a distinct raw long accessor, preserving both words without decimal/time conversion.
Java's accessor can also read a one-slot field plus its physical neighbor, including an unrelated
field; the oracle retains that observation. The native typed adapter deliberately refuses present
one-slot TradeId with `INCOMPATIBLE_TYPE`: logical `FieldValue` does not borrow another field's bits.
This is an explicit supported-profile boundary, not universal Java parity for that aberrant schema.
Absent TradeId remains the actual Java zero default. Generic decoding is unaffected.

Preparation declares 13 selections, 23 lookup alternatives and 110 type alternatives. A complete
default route fits the existing 128-alternative registration allowance; an explicit allowance109
fails atomically with `RESOURCE_LIMIT`. Schema ceilings remain 16 records / 128 logical fields in the
receiving profile. The built-in registry preserves Quote0 and Profile1 and appends composite Trade2.
`defaultTradeSchema` is an explicit complete native convenience schema with all thirteen logical
fields, including the optional nano part, wide numeric fields and LONG TradeId. It does not assert
that every optional field is enabled in an unconfigured Java scheme.

## Packed time and flags

The delegate combines raw `Last.Time` as signed32 seconds in the high word of `timeSequence` and
all32 bits of raw `Last.Sequence` in the low word. Unlike Quote's declared time converter, Trade's
generated time accessor reads a physical int even when the local field has full type152.

`getTime()` is `signed_high32 * 1000 + unsigned_bits22_31`. Packed milliseconds 1000..1023 are
preserved. `getSequence()` returns low22 bits, without monotonicity or uniqueness guarantees.
`getTimeNanos()` combines those milliseconds with the unmodified signed nano part using Java long
arithmetic; native implementation uses defined unsigned arithmetic for wrapping. Negative nano parts
and values outside0..999999 are not clamped. `eventTime` is independently supplied FIELD metadata
milliseconds and is never synthesized from business time, indexed identity or source ordinal.

Direction occupies flags bits1..3: UNDEFINED0, DOWN1, ZERO_DOWN2, ZERO3, ZERO_UP4, UP5. Reserved6/7
read as UNDEFINED. The production delegate then checks `Last.Tick` only when direction is UNDEFINED:
tick1 replaces direction with ZERO_UP, tick2 with ZERO_DOWN, and other ticks leave all flags intact.
This replacement preserves every other flag bit, including bit0 extended-hours status. Trade itself
can report extended hours; [TradeETH](TRADE-ETH.md) uses its separate record and no Tick fallback.

## Regional demand and delivery

Registration explicitly selects a record ID and `EventType::TRADE`; names alone never activate a
mapper. With exported `Trade` and `Trade&Q` registrations, logical selector `AAPL&Q` selects the real
regional route and submits physical `(Trade&Q, AAPL)`. Its output owns logical `AAPL&Q`; absent
Last.Exchange defaults to Q from that record. The suffix mechanism is general, without a Q constant
in implementation. Recognized trailing attributes are preserved by the existing exchange helper.
If no matching regional record is registered, the composite route retains the logical selector.

Quote and Trade choose their regional fallback independently. Profile keeps its ordinary exact
symbol route. All fifteen nonempty selections of Quote/Profile/Trade/TradeETH are finite immutable plans over
at most16 unique bindings, including nonconsecutive type selections. Fixed plan index arrays and a
temporary16-entry stack binding copy are included in retained/work accounting. Initialization plus
metadata read/write work and both index-validation/copy visits are prepaid before copying; one-below
refusal changes neither output nor credits. Existing queue-owned command copies are charged separately.

The worker also funds a whole atomic STREAM fanout before retrying on a fresh turn. For C client
slots, D actual schema records, E interest entries, A aggregate entries and U maximum symbol units,
actual MuxState select preflight charges V=32×(2+C+D+E+A)×(2+E) visits and V×U text units per query.
A routing chunk can issue routeRecords×C queries; those checked visits/text credits are added to
the existing finite interest-work seed. Operations, byte credits, routing quantum and record caps
are unchanged. Overflow produces the existing actionable RESOURCE_LIMIT before native activation.
The boundary test bills real MuxState selects with exact whole-chunk credits and one text unit below,
without a copied selector implementation. Work units are logical credits, not elapsed CPU or memory.

Selectors that cannot round-trip through the physical regional conversion, such as `AAPL&N&Q` or
attributes before a replaced suffix, fail synchronously with native `INVALID_ARGUMENT` at
`subscription-symbol-representation`, atomically for the desired batch. No silent canonicalization
or post-delivery symbol rewrite repairs a misrouted request. Desired logical membership remains exact.

Receiver projections and callback vectors use their actual event/variant sizes in existing bounded
copy/preparation profiles. The callback owns no source views after return; copied events remain valid
after remove, reconnect, endpoint and runtime retirement. Listener exceptions use the existing
logical-subscription failure boundary. No new polling loop or lifecycle mechanism is required.

This is a finite explicitly registered regional subset, not complete catalog/selector support. A
full receiving catalog exceeding16 records needs the existing integration decision; this leaf does
not expand the cap or claim complete-client readiness.

Primary pinned sources: [TradeBase](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/TradeBase.java), [TradeMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/TradeMapping.java), [TradeDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/TradeDelegate.java).
