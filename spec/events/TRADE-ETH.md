# TradeETH projection

[Русская версия](TRADE-ETH.ru.md). English is authoritative.

`events::TradeETH` is a distinct owning aggregate for the last extended-hours trade and its day totals,
delivered by `Subscription<TradeETH>` and `RegisteredEvent`. Regular Trade keeps its independent
type, API and routes. Summary remains unsupported as a typed event. The reference is locked Java
QD3.355 TradeETH, TradeBase, generated TradeETHMapping and TradeETHDelegate; see the
[capture layout](../../tests/fixtures/trade-eth/CAPTURES.md).

## Owned values and getters

All scalars have defined defaults. Five decimals start at raw0 (NaN). The trailing optional five-entry
inline snapshot owns business doubles and corresponding wide bits without a separate heap allocation.
The symbol is owned; copies retain no cursor, schema or callback references. As in [Trade](TRADE.md),
getPrice/getSizeAsDouble/getChange/getDayVolumeAsDouble/getDayTurnover preserve direct Java business
values before LONG-to-wide rounding. Editing a raw member independently selects its current raw view.
Manual/default aggregates have no snapshot. getSize/getDayVolume use Java truncation, saturation and
NaN-to-zero narrowing. The existing Trade aggregate layout and public API are unchanged.

TimeSequence combines signed32 seconds and all32 Sequence bits. getTime uses unsigned bits22..31 as
milliseconds, including1000..1023. getSequence returns low22 bits. getTimeNanos uses Java long wrapping
and the unmodified signed nano part. EventTime is independent FIELD metadata milliseconds, default0.
No time-zone or calendar filter is synthesized.

Flags bit0 comes from data, not the type. It may become false when the regular session starts while
the ETH price, size, change and totals remain unchanged. Direction uses bits1..3 and the existing
Direction enum; reserved6/7 return UNDEFINED. The production ETH delegate has **no Last.Tick fallback**.
Required ETHLast.Flags is copied without synthesized bits. Transition and extra-tick oracle rows cover
both distinctions.

## Exact mapping profile

| Physical key / property alternative | Required | Types | Missing value |
|---|---|---|---|
| ETHLast.Time / Time | no | raw-int | seconds0 |
| ETHLast.Sequence / Sequence | no | raw-int | low word0 |
| **Last.TimeNanoPart** / TimeNanoPart | no | raw-int |0 |
| ETHLast.Exchange / ExchangeCode | no | raw-int | record exchange, composite0 |
| TradeId | no |104,120,152 | raw64 zero |
| ETHLast.Price / Price | yes | numeric | preparation error |
| ETHLast.Size / Size | yes | numeric | preparation error |
| ETHLast.Change / Change | no | numeric | wide0/business NaN |
| DayId | no | raw-int | signed day0 |
| ETHVolume / DayVolume | no | numeric | wide0/business NaN |
| ETHDayTurnover / DayTurnover | no | numeric | wide0/business NaN |
| ETHLast.Flags / Flags | yes | raw-int | preparation error |

Numeric full IDs:2,4,8,24,104,120. Raw-int full IDs:2,4,8,24,40,56,72,88,104,120,152.
[Field conversions](FIELD-CONVERSIONS.md) define exact physical semantics. Two-slot raw int reads its
high physical word; exchange narrows to its low UTF-16 unit. TradeId preserves both words104/120/152.
Java one-slot TradeId can read an unrelated neighboring slot: the corpus records this fact, while
the native typed adapter explicitly refuses present one-slot TradeId with INCOMPATIBLE_TYPE.
Missing TradeId is0. Complete context/shape validation includes unselected extra fields before output.

The nano key lacks the ETH prefix. Only ETHLast.TimeNanoPart without the actual key/property alias
gives production missing default0. Preparation requires12 selections,22 lookup alternatives and99
type alternatives:99 succeeds;98 fails atomically with RESOURCE_LIMIT. DefaultTradeETHSchema provides
twelve explicit native fields including nano, wide numerics and LONG TradeId. It does not assert all
optional fields are enabled in an unconfigured production scheme.

## Registry, routing and bounded delivery

Builtin keeps Quote0/Profile1/Trade2 and appends TradeETH3:4records/49fields. Explicit TradeETH and
TradeETH&Q registrations bind logical AAPL&Q to physical(TradeETH&Q,AAPL) and owning AAPL&Q output.
Suffix conversion is general; Q is a fixture. Without a matching regional registration the composite
route retains the selector. Quote/Trade/TradeETH regional fallback is independent; Profile keeps exact
symbols. Unrepresentable regional selectors fail before demand publication under [Trade](TRADE.md).
Renaming a generic record after decoding does not establish this binding.

All15 nonempty Quote/Profile/Trade/TradeETH sets have immutable plans with at most16 binding indices.
Existing sizeof-based preparation/storage accounting includes fifteen plans and temporary copies.
The checked whole-atomic-fanout grant uses configured clients and actual records. Public defaults
remain8subscriptions/16records/128fields. The all-mask fixture explicitly configures17subscriptions
(15mixed, typed ETH, thrower),2symbols and6records/72fields. Actual MuxState exact/one-below tests prove
whole-query progress without raising public caps.

Callback spans live only during invocation; copied events survive retirement. Existing error
atomicity, listener isolation and quiescent close guarantees apply. Installed/moved consumers cover
physical composite/regional delivery,15masks, negative record/symbol routes, attributes, remove,
throw and explicit reconnect. A no-data wire marker fences the last desired replacement; ACTIVE
alone does not. A live probe fences the first withdrawal. Final removal matches the exact eight-tuple
multiset across complete frames. Ordinary close stays drained; explicit reconnect preserves its
distinct gap result. These deterministic local checks are distinct from a public service run.

Pinned primary sources: [TradeETH](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/TradeETH.java), [TradeETHMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/TradeETHMapping.java), [TradeETHDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/TradeETHDelegate.java).
