# SpreadOrder and AnalyticOrder values and mapping

[Русская версия](ORDER-VARIANTS.ru.md). English is authoritative.

`SpreadOrder` owns the common OrderBase fields plus nullable `spreadSymbol`. Its event `symbol` is the
underlying subscription symbol; `spreadSymbol` describes the spread payload. It is a sibling of ordinary
Order and has no MarketMaker field. `AnalyticOrder` derives from `Order` and independently retains three
iceberg numeric values and complete `icebergFlags`. Exact variant dispatch must preserve this analytic
identity; converting it to an ordinary Order loses its extra fields.

Both adapters require Index, Time, Sequence, Price, Size and Flags. SpreadSymbol is also a required field
declaration, whose value may be null. The admitted text types are exact UTF_CHAR_ARRAY and STRING;
serialized object representations are refused. Optional nano/action/IDs/executed/count/trade fields
retain ordinary [Order semantics](ORDER.md). Only Analytic has optional MMID/MarketMaker, using the same
raw first-Java-int accessor and SHORT_STRING decoding as Order. Missing iceberg values are NaN and
missing icebergFlags is zero. Iceberg sizes admit the same six numeric full types as Order values;
IcebergFlags uses the exact raw first-int forms. Types are validated by full identity without masking.

The low two iceberg flag bits mean UNDEFINED 0, NATIVE 1 and SYNTHETIC 2; reserved 3 reads as UNDEFINED.
Every other bit remains in the owning raw field. Three independent iceberg business-number snapshots
coexist with the inherited five Order snapshots. Direct Java source doubles are preserved separately
from wide-decimal accessors. If a public wide raw value changes, its getter falls back to that current
wide value without changing another field's snapshot. `Count` retains signed32 getAsInt semantics widened
to signed64, including saturation/truncation of tiny-decimal input.

`PreparedSpreadOrder` and `PreparedAnalyticOrder` bind an explicit record ID and own immutable context,
complete shape, selections and source ID. Required/optional selection and source inspection consume the
finite MappingLimits allowances. They retain no source schema declarations, cursor, payload view,
callback span, runtime or native resource. `map`/`mapView` first validate the complete provenance, count,
exact full types and canonical payloads. They then precharge the symbol and every selected nullable text
against symbolUnits/stringUnits/totalUnits before the first output allocation. Subtraction checks avoid
sum overflow. Failure returns the existing MappingError atomically; allocation/length exceptions map to
ALLOCATION_FAILURE. Inputs remain unchanged and the prepared mapper remains reusable.

The source occupies public index high32; unsigned wire Index occupies low32. Signed business Time and
unsigned Sequence form timeSequence independently of FIELD eventTime. Native `IndexedTimeLowInt32`
retains the physical VOID/Index layout; an engaged removalTime owns the full signed64 collector identity,
including its independent high word. Receive projection does not impose putEvent-only publication
constraints on negative low indices or business side/size combinations.

Source publication capability, standard-scheme availability and explicit receive-record admission are
separate. DEFAULT publishes both variants; ISE publishes Spread but not Analytic; NTV supports a full
order book while publication flags for both variants are false. An explicitly supplied AnalyticOrder#NTV
record can still be received. Source-specific standard optional shapes are preserved; DEFAULT fields
must not be copied blindly to every suffix. `defaultSpreadOrderSchema` and `defaultAnalyticOrderSchema`
construct explicit normalized native DEFAULT projections with enabled Count and SpreadSymbol/MMID;
optional iceberg fields are absent. Applications can prepare richer immutable schemas explicitly.

The locked authorities are [upstream.lock](../../upstream.lock), market SpreadOrder/AnalyticOrder API
classes, their generated mappings/delegates and MarketFactoryImpl. The
[production oracle](../../tools/upstream/order-variants-oracle/README.md) retains all 200862 original
observations and 200 QTP files. A separate 51-row FIELD witness covers actual nonzero/negative/full-width
metadata, zero/disabled read/write modes and independent business time. Finite native fixture loading
bounds bytes to 16 MiB and rows to 250000 without trimming observations. Both families are ordinary native
indexed routes; no Quote/MarketMaker derived-source permission or time-series classification is implied.