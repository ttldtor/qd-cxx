# Summary projection

[Русская версия](SUMMARY.ru.md). English is authoritative.

This contract covers owning `events::Summary`, `PreparedSummary`, typed subscriptions and mixed
delivery. The pinned production reference is QD 3.355 Summary, generated SummaryMapping and
SummaryDelegate. Summary is a lasting trading-session snapshot, independent of Trade and TradeETH;
it is not a time-series event. [Capture layout](../../tests/fixtures/summary/CAPTURES.md).

## Ownership and business values

The flat aggregate owns its symbol and all scalars. Every numeric member has a defined default:
day IDs, OpenInterest, flags and eventTime are zero; the six WideDecimal members start at raw 0 (NaN).
The trailing optional inline six-entry snapshot owns direct business doubles and their wide raw
validity keys, without a separate allocation. No source cursor, schema, field span, callback or
transport reference survives mapping. Copies remain valid after retirement.

getDayOpenPrice/getDayHighPrice/getDayLowPrice/getDayClosePrice/getPrevDayClosePrice/getPrevDayVolume
return the production delegate's direct business double before LONG-to-wide rounding. The decimal
members expose the generated mapping's wide-accessor view. Source LONG 36028797018963973 is a one-ULP
witness between those views. Editing one raw decimal selects that member's current wide view when
its key differs; other snapshots remain applicable. Manual/default events have no snapshot. Resetting
the optional snapshot selects all current wide views. WideDecimal::toDouble is unchanged.

OpenInterest uses the actual production mapping's **long accessor**, intentionally correcting the
pinned delegate's finite-value loss under [ADR-0010](../../docs/decisions/ADR-0010-summary-open-interest-width.md).
Default WIDE120 can store 3000000000 exactly, but the Java delegate returns -1294967296; 4294967296
becomes 0. Its public long event contract does not document that restriction. The native int64 field
preserves the actual getOpenInterestLong/getAsLong result; this is an explicit Java difference.

| Source numeric representation | OpenInterest business conversion |
|---|---|
| INT (full IDs2,4,8) | signed physical int, extended to int64 |
| LONG104 | complete signed 64-bit source |
| DECIMAL24 | actual tiny-to-double, Java truncation/saturation/NaN-zero into long |
| WIDE120 | exact WideDecimal.toLong without a subsequent int narrowing |

Wide toLong rounds to nearest with half ties toward positive infinity, saturates to signed 64-bit bounds,
and maps NaN to0. Tiny double-to-long truncates instead. LONG/WIDE never pass through double.
Explicit legacyINT8 remains physically 32-bit; missing/regionalOI remains0. Public type/layout,
numeric admission and receiving limits are unchanged. Original Java delegate captures stay raw;
a separate complete, classified actual long reference supplies only the corrected OI expected cell.
## Exact fields, flags and time

| Physical key / property alternative | Required | Supported full type set | Missing |
|---|---|---|---|
| DayId | yes | raw-int11 | preparation error |
| DayOpen.Price / DayOpenPrice | yes | numeric6 | preparation error |
| DayHigh.Price / DayHighPrice | yes | numeric6 | preparation error |
| DayLow.Price / DayLowPrice | yes | numeric6 | preparation error |
| DayClose.Price / DayClosePrice | no | numeric6 | wide0/business NaN |
| PrevDayId | yes | raw-int11 | preparation error |
| PrevDayClose.Price / PrevDayClosePrice | yes | numeric6 | preparation error |
| PrevDayVolume | no | numeric6 | wide0/business NaN |
| OpenInterest | no | numeric6, corrected business-long conversion above |0 |
| Flags | no | raw-int11 |0 |

Numeric6 is exactly2,4,8,24,104,120. Raw-int11 is exactly2,4,8,24,40,56,72,88,104,120,152.
[Field conversions](FIELD-CONVERSIONS.md) describe the existing factory/profile boundary, including
local BYTE1/SHORT3. Present unsupported selected types fail INCOMPATIBLE_TYPE; they are not silently
masked as missing. Java's optional non-int field lookup can itself look missing; that does not
broaden this explicit native profile. Complete count/type/context/value-shape validation includes
unselected extra fields before output publication.

DayId/PrevDayId/Flags use the raw physical first int slot, not getAsInt. A two-slot LONG/WIDE/TIME_MILLIS
therefore exposes its signed high32 word. Day IDs remain signed days since1970-01-01 without calendar
validation or time-zone conversion. Flags are complete business flags, distinct from indexed-event
transaction flags. PriceType uses bits2..3 for day close and bits0..1 for previous close: REGULAR0,
INDICATIVE1, PRELIMINARY2, FINAL3. All high bits are preserved; missing flags yields REGULAR/REGULAR.
The implementation reuses the existing common PriceType enum and bit helper.

Actual default production Summary has 10 logical fields; Summary&X has 9 and omits OpenInterest.
Optional close/previous volume/flags are enabled by default. Scheme-wide wide=true is the actual
default, so all seven numeric selections, including composite OpenInterest, use WIDE_DECIMAL120.
With wide=false, price/volume fall back to DECIMAL24 and OpenInterest to COMPACT_INT8. Per-price/
volume selectors can override relevant doubles; OpenInterest has no per-field selector.
defaultSummarySchema is an explicit complete native ten-field wide convenience schema.

Summary declares no business Time, Sequence or Nano field. Factory time-slot names remain null and
the record hasTime flag is false; there is no invented nullable timestamp. Independent FIELD metadata
still supplies eventTime milliseconds, with 0 when unavailable. Day IDs never synthesize eventTime.
Existing common day/time utilities remain separate presentation/conversion APIs.

Preparation requires 10 selections, 15 lookup alternatives and 75 type alternatives: 75 succeeds, 74 fails
atomically with RESOURCE_LIMIT. Default 128 alternatives and 16 records / 128 fields / 256 lookups remain.
Errors preserve original mapping/resource/allocation causes; no partial event escapes.

## Registry, regional binding and finite mixed delivery

Builtin preserves Quote0/Profile1/Trade2/TradeETH3 and appends Summary4: 5 records / 59 fields. An explicitly
exported Summary/Summary&Q registry binds logical AAPL&Q to physical(Summary&Q,AAPL) and owning
AAPL&Q output. Exchange conversion is general; Q is only a fixture. Missing matching regional routes
fall back independently to composite for Quote/Trade/TradeETH/Summary; Profile retains its exact
symbol. Existing attribute-preserving conversion and nonrepresentable-selector refusal apply.
No posthoc generic-record rename establishes this binding.

All 31 nonempty five-type sets have immutable plans with at most 16 binding indices. Existing checked
sizeof-based preparation/storage accounting includes all 31plans and temporary copies. The explicit
all-mask fixture sets 33 subscriptions (31 mixed, typed Summary and thrower),2 symbols,8 records / 91 fields;
public default 8 subscriptions is unchanged. Real MuxState select exact/one-below tests exercise the
whole atomic-query grant, including a fresh turn. Logical work credits are not RSS or elapsed CPU
measurements. Overflow refuses existing RESOURCE before native activation.

Callbacks retain only owning copies; spans expire on return. Throwing one listener fails only its
subscription. Ordinary clearSymbols scenarios require local DRAINED; explicit reconnect preserves
its separate gap outcome. Subscription close from its callback is an explicit local ABORT request.
The queue retains aborted-registration history, so this isolated case expects aggregate non-drained
with no gap/expiry after proving the typed registration CLOSED, the endpoint CONNECTED/healthy and
causal post-close business delivery to all 31other subscribers. Native cancellation diagnostics are
recorded separately, not used as the cause of that classification. Empty TICKER source withdrawal completes before graceful closing as specified by
[queue drain semantics](../mux/QUEUES.md).

Installed and physically moved public consumers exercise real composite/regional demand, complete
values, all 31memberships, wrong record/symbol drops, attributes, removal, throw, callback close and
reconnect. A distinct no-data ADD marker fences the last desired replacement; ACTIVE is not a
backend-membership fence. A live DATA probe fences withdrawal. Final removal matches an exact
ten-tuple multiset across complete allowed frames, retaining missing/duplicate/extra detection.
These deterministic local TCP checks are distinct from a public live-service demonstration.

Pinned primary sources:
[Summary](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/Summary.java),
[SummaryMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/SummaryMapping.java),
[SummaryDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/SummaryDelegate.java).
