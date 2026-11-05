# Indexed Order events

[Русская версия](ORDER.ru.md). English is authoritative.

`Order` is an owning indexed market event. Its 64-bit business index is the resolved record source ID
in high32 and unsigned wire Index in low32. It is independent of business Time/Sequence and optional
FIELD eventTime metadata. Required fields are Index, Time, Sequence, Price, Size and Flags. Optional
fields retain the production defaults; business Flags and indexed eventFlags remain separate words.
The plain Order adapter does not project AnalyticOrder, SpreadOrder or OtcMarketsOrder fields.

`PreparedOrder` binds one explicit immutable local record and validates full provenance, count, exact
full types and canonical values before projection. Source suffix resolution follows PreparedOrderSource:
last `#`, one to four case-sensitive ASCII alphanumeric units, otherwise DEFAULT. This is schema-bound
selection, not a scan or a source-name subscription capability check. Owning results retain no schema,
cursor, receiver, runtime, callback span or caller payload. Nullable MarketMaker is decoded through the
SHORT_STRING raw accessor and bounded independently from the event symbol.

The actual standard Order physical source declares VOID at integer slot0 and COMPACT_INT Index at
slot1. Native `IndexedTimeLowInt32{field}` explicitly maps the low contributor and omits physical VOID
from logical fields. It accepts only exact COMPACT_INT. Normal collector identity is the unsigned low32
value with zero high32. An engaged raw removalTime, including zero, retains the full signed64 collector
identity; only low32 is restored into logical Index. A high32 removal therefore does not alias a normal
zero-high key, even when their public Order indexes coincide. Source identity belongs to the selected
record partition; it is never inserted into collector time. This layout is native-only; the separately
versioned JSON layout grammar continues to admit only pair-int32 and single-int64.

The locked compatibility authority is `dxfeed-impl/.../market/MarketFactoryImpl.java` (physical fields
and delegate contracts), `OrderDelegate.java` (receive projection), `impl/OrderMapping.java` (accessors)
and `dxfeed-api/.../market/OrderBase.java` / `Order.java` (business values), at [upstream.lock](../../upstream.lock).
Ordinary Order subscriptions use HISTORY; STREAM SUB belongs to the separate stream-only scheme.
The high-level API preserves its TICKER default and requires explicit HISTORY for typed Order.
