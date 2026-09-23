# Profile event projection

[Русская версия](PROFILE.ru.md). English is authoritative.

QDCXX-025: projection follows the new [production Java evidence](../../tests/fixtures/events/mapping/COVERAGE.md)
and [configured-schema boundary](../schema/CONFIGURED-EVENTS.md). The separate event adapter takes
an explicit prepared schema mapping; source loading and tape delivery remain outside it.

## Fields and defaults

Only Description is required locally. The unchanged default has 16 logical fields, 24 Java integer
slots (including ten VOID tails), two object slots and hasTime=false. Logical order is the table order.
Every numeric field below is full120 in the default and is also measured as exact full8 in numeric-int.

| Local lookup | Property lookup | Event projection | Full type | Absent local default |
| --- | --- | --- | --- | --- |
| Beta | Beta | beta | 120 or 8 | NaN |
| Eps | Eps | earningsPerShare | 120 or 8 | NaN |
| DivFreq | DivFreq | dividendFrequency | 120 or 8 | NaN |
| ExdDiv.Amount | ExdDivAmount | exDividendAmount | 120 or 8 | NaN |
| ExdDiv.Date | ExdDivDate | exDividendDayId | 88 | 0 |
| 52High.Price | HighPrice52 | high52WeekPrice | 120 or 8 | NaN |
| 52Low.Price | LowPrice52 | low52WeekPrice | 120 or 8 | NaN |
| Shares | Shares | shares | 120 or 8 | NaN |
| FreeFloat | FreeFloat | freeFloat | 120 or 8 | NaN |
| HighLimitPrice | HighLimitPrice | highLimitPrice | 120 or 8 | NaN |
| LowLimitPrice | LowLimitPrice | lowLimitPrice | 120 or 8 | NaN |
| Halt.StartTime | HaltStartTime | haltStartTimeMs milliseconds | 56 | 0 |
| Halt.EndTime | HaltEndTime | haltEndTimeMs milliseconds | 56 | 0 |
| Flags | Flags | exact signed business flags | 8 | 0 |
| Description | Description | owned nullable UTF-16 | 10 | required; missing local rejects |
| StatusReason | StatusReason | owned nullable UTF-16 | 10 | null |

Date retains the signed raw day ID without calendar conversion. Halt times multiply signed int32
seconds by 1000 in int64. Full120 retains exact raw decimal bits and existing binary64 projection;
full8 is an exact signed integer numeric value. Missing local decimals and zero raw full120 values
project NaN, while a present wire-omitted full8 value projects zero. The event owns symbol and nullable
strings, preserving empty values, embedded NUL, paired and lone surrogates. Preparation and copying
must apply finite limits before allocating; a rejected projection publishes no partial event.
Canonical FieldValue validation and schema/record context checks are required before projection.

## Prepared API and ownership bounds

`defaultProfileSchema(limits)` constructs local ID 0/Profile under EXTENDED_FIELDS_V1, with the full
table above and explicit local, full-name and property keys. It is a convenience default using the same
LocalSchema validation path as caller-supplied schemas. It does not establish support for arbitrary
records named Profile or perform loading, property resolution or wire conversions.

`PreparedProfile::prepare(schema, localId, limits)` resolves the table's explicit local/property keys
through the shared [EventMapping contract](EVENT-MAPPING.md). Reordering, nonzero local IDs, optional
absence, explicit aliases and extra supported fields use the same path. Distinct matched targets reject
as ambiguous; present fields must have one of the table's exact full types. The immutable plan owns its
context, complete type shape and selections; source names/containers are not retained. `context()`
returns a borrowed reference until the mapping is moved, assigned or destroyed; a copied context keeps
the opaque identity alive independently. Generic extra/custom fields remain unchanged and accessible.

`map(symbol, values, EventValueLimits, eventTime=0)` accepts a context-tagged RecordValueView. It validates
the context first, then every tag and canonical value shape, including unselected fields, before copying.
The result owns the symbol, Description and StatusReason; nullable UTF-16 strings remain distinct from
empty strings. EventValueLimits independently bounds symbolUnits, stringUnits per copied string and
totalUnits across all three values. Null and empty use zero units. Aggregate checks use subtraction
before copying, preventing size overflow. Zero is a real bound. Allocation/length failures become
MappingError::ALLOCATION_FAILURE; budget failures become RESOURCE_LIMIT; neither publishes a partial event.
Projection has no retained counters: a caller retaining several events owns their aggregate budget.
The inspector additionally bounds cumulative copied event units, decoder work and encoded output.

The ten numeric members use WideDecimal. Full120 preserves raw bits; full8 projects the exact signed32
integer with decimal coefficient equal to that integer and rank 128 (`raw = value * 256 + 128`). This
derived event representation preserves binary64 integer semantics without changing the original
generic full8 tag/raw value and without adding a general decimal-composition API. Missing local numeric
members remain NaN. EventTime is explicit caller metadata and defaults to zero; the adapter does not
read a tape clock. `tradingStatus()` and `shortSaleRestriction()` return the named enums described below,
and `isTradingHalted()` / `isShortSaleRestricted()` expose their measured Boolean views.

## Flags and time

Flags remain signed32 record data, including high reserved bits; they are not QTP event flags.
Trading status is bits 0..1: 0 undefined, 1 halted, 2 active, 3 maps to undefined.
Short-sale restriction is bits 2..3: 0 undefined, 1 active, 2 inactive, 3 maps to undefined.
Thus isTradingHalted is status==1; isShortSaleRestricted is restriction==1. Raw flags are preserved
regardless of these projections. The captured 0x76543216 yields active/active; -1 yields undefined/
undefined without becoming raw zero. QTP event flags remain zero in all accepted tapes.

EventTime is separate from halt times, Quote business times and Sequence. STREAM/NONE replay
has eventTime=0. RAW/MESSAGE replay carries supplied 1600000000123 milliseconds through the
existing timing contract. Neither Profile business flags nor halt fields control container timing.

## Related Quote observations

Prepared Quote requires Bid.Price/Bid.Size/Ask.Price/Ask.Size. Bid.Time/Ask.Time and exchange
fields are optional with zero defaults; Sequence(full72) and TimeNanoPart(full8) are independent
optional fields with zero defaults. Sequence retains signed32 packed bits: low22 are public sequence,
unsigned high10 supply milliseconds. Quote time is max(bidTimeMs,askTimeMs)+unsignedHigh10 for
these second-based fields. Negative packed values therefore remain meaningful bit patterns rather
than invalid sequence counts. Direct boundary probes include INT_MIN/-1/0/1/4194303/4194304/INT_MAX;
TimeNanoPart preserves the signed raw value even outside ordinary application ranges.
Legacy eight-field Quote retains its previous contract and output.
