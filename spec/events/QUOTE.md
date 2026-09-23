# Prepared Quote event projection

[Русская версия](QUOTE.ru.md). English is authoritative.

The prepared API supplements the unchanged [legacy eight-field contract](../protocol/DEFAULT-QUOTE.md).
It follows [configured Java evidence](../schema/CONFIGURED-EVENTS.md) and uses the shared
[mapping ownership, context and validation rules](EVENT-MAPPING.md). Source loading, wire binding
and event projection remain separate. The record name alone never establishes compatibility.

## Field selection

Preparation searches these explicit local/property keys, without deriving aliases. Both keys may
resolve to the same field; different targets fail as ambiguous. Extra supported fields remain generic.

| Local key | Property key | Exact full types | Required |
| --- | --- | --- | --- |
| Bid.Time | BidTime | 56 | no |
| Bid.Exchange | BidExchangeCode | 2 | no |
| Bid.Price | BidPrice | 120 or 8 | yes |
| Bid.Size | BidSize | 120 or 8 | yes |
| Ask.Time | AskTime | 56 | no |
| Ask.Exchange | AskExchangeCode | 2 | no |
| Ask.Price | AskPrice | 120 or 8 | yes |
| Ask.Size | AskSize | 120 or 8 | yes |
| Sequence | Sequence | 72 | no |
| TimeNanoPart | TimeNanoPart | 8 | no |

These numerical alternatives are measured by the numeric-int production case. They do not authorize
wire conversion or any other representation. Missing required local fields reject preparation.
An absent optional local field projects zero. A present wire-omitted field keeps the decoder's tagged
raw zero: numeric full120 therefore means NaN, whereas numeric full8 means zero.

## Owned values and business time

The result owns an exact UTF-16 symbol. Full120 numeric fields retain their raw WideDecimal bits.
Full8 numeric fields become an exact event decimal with coefficient equal to the signed int32 value
and rank 128 (raw = value * 256 + 128). This derived event representation leaves the original generic
record's full8 tag and raw value unchanged; it is not a general decimal-composition API.
Bid/ask signed seconds multiply by 1000 in int64. Exchange codes retain the low UTF-16 unit.

Sequence is a signed32 packed value: unsigned high ten bits provide the millisecond component and
low 22 bits provide the public sequence. Thus timeMs is max(bidTimeMs, askTimeMs) plus the unsigned
high-ten component; negative packed values are meaningful. TimeNanoPart preserves the signed raw
int32, including the directly measured boundary values outside ordinary application ranges.
EventTime is independent metadata: the caller may provide it explicitly to map, otherwise it is zero.
It is never derived from Sequence, TimeNanoPart or the bid/ask fields.

## Bounds and compatibility

`PreparedQuote::prepare(schema, localId, limits)` returns an immutable plan or MappingError.
`context()` exposes its owned record context. `map(symbol, values, symbolUnits, eventTime=0)`
accepts a RecordValueView and returns an owned Quote or MappingError; it retains no input view.

Preparation owns its field plan and context under finite MappingLimits. Projection validates the
complete record context, all field tags and canonical payloads before reading selected values or
copying the symbol. Its mandatory symbol-unit bound applies before allocation. Allocation/length
exceptions produce ALLOCATION_FAILURE and no partial event escapes. Callers retaining multiple
events must enforce their aggregate retained-unit/work budgets.

Reordering, explicit aliases, supported extra fields, optional absence and nonzero local IDs use the
same prepared path. Neither defaultQuoteSchema, legacy mapQuote, old profiles nor output bytes change.
No loader, new full type, regional/indexed event, wire coercion or broader container support is added.
