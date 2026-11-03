# Field conversion captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

`oracle.tsv` contains 50,042 LF-delimited raw production observations. The
[provenance](provenance.json) binds the complete TSV SHA-256, pinned upstream sources, actual
JDK executables, harness, launcher and production-class inventory. Do not rewrite or normalize
this dataset; recapture using the [maintained helper](../../../tools/upstream/field-conversion-oracle/README.md).

The actual DXFeedScheme Quote/Profile fields supply names, properties and default types. Each
configured pair reverses physical order and constructs genuine two-slot Java long fields with
VOID tails, then exports one logical field per complete local value. The records are processed by
generated mappings and actual MarketFactoryImpl delegates. Each scenario has its own dense pair
of record IDs; the corpus is not one receiving profile containing every variant simultaneously.

| Scenario family | Observations |
| --- | --- |
| numeric2/4/8/24/104/120 | Exact supported types in all four Quote and ten Profile numeric selections; WideDecimal accessor bits and all 14 actual public Java business-double getters |
| raw2/4/8/24/40/56/72/88/104/120/152 | Exact first-slot behavior for Sequence/nano/exchange/date/flags/halts, including long high/low differences |
| text10/text137 | Null, empty, NUL, paired and unpaired UTF-16; production field serialization/read produces String before actual delegate use |
| missing120 | All optional properties absent; required Quote numbers and Profile Description retained |
| required120 | Required Quote numbers/Profile Description absent; actual factory/delegate refusal |
| bad-numeric40/56/72/88/152/137 | Unsupported representation or object storage in a numeric selection; observed Java failure or optional missing behavior, with stricter explicit native refusal |
| bad-text9/233/249 | Non-String object storage; actual ClassCastException instead of object-to-string coercion |
| factory1/factory3 | Actual production factory returns no local BYTE/SHORT DataIntField |

DECIMAL samples cover every 128 low flag pattern at five signed mantissa boundaries: 2560 cursor
instances across numeric and raw Quote/Profile variants. LONG samples cover both signed 64
extremes, signed 56-bit fitting thresholds, decimal reduction half ties/carry transitions, signed
binary64 exactness boundaries and rounding near both long extremes. WIDE_DECIMAL retains finite,
noncanonical special and arbitrary signed 64 bit patterns. Timezone pairs are byte-identical and
class inventories remain unchanged. Each Java/version/compiler subprocess is bounded to 128MiB
heap and 60 seconds by the helper.

Native tests compare the relevant production accessor results through complete owned and borrowed
records. They separately test exact 72/108 preparation alternatives, one-below refusal, default
registry construction and explicitly unsupported types. The standalone public Feed consumer sends
actual DECIMAL/LONG/INT/String and raw TIME_MILLIS-slot variants through deterministic TCP, checks
owned mixed Quote/Profile delivery against these observations and validates exact ADD/REMOVE tuples.
It is suitable for separate installed and physically relocated package checks on both OS.

Native business getters are checked separately from raw decimal fields, including LONG
36028797018963973 with a one-ULP difference between those views. Manual/legacy aggregates,
copy/move and edited-raw fallback are distinct cases. All ten absent Profile business defaults
are observed and compared individually. Snapshot state is inline owning numbers, without new
heap allocation or borrowed fields; receiving projection byte bounds include its actual sizeof.

Source/configuration evidence, native author checks, independent review and package execution are
distinct. This fixture alone proves no public network service run, whole-client readiness, or
process-memory ceiling. See the [contract](../../../spec/events/FIELD-CONVERSIONS.md).
