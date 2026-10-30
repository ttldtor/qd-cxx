# Event flags and removal-time validation

[Русская версия](FLAGS-VALIDATION.ru.md). English is authoritative.

This report qualifies the explicit STREAM/NONE event policy in
[STREAM-DATA](../../../../spec/protocol/STREAM-DATA.md#catalog-event-metadata), supplied-schema
BINARY_CATALOG_V1 and symbol option masks0/1. Old modes and three-argument RAW/STREAM payload
functions remain zero-flags only. Collector semantics, FIELD timing, broader family dispatch,
headerless files and a new output format are outside this change.

## Production observations and provenance

The [manifest](../../../fixtures/encoding/field-types/flags-captures.json) records the exact
production source/helper/runtime/classpath hashes and 105 flags artifacts per pin. Executed pins:
QD3.355 `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and
QD3.356 `f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Source equality does not replace either run.
The [reviewed fixtures](../../../fixtures/encoding/field-types/expected/flags/observations.tsv)
have 104 original binary artifacts and 4300 canonical observation rows. Synthetic symbols and
records are finite; no private recording is included or consumed.

| Surface | Actual observation |
| --- | --- |
| BinaryQTPComposer -> BinaryQTPParser | All 64 known subsets in RAW5/TICKER 10/STREAM 15/HISTORY20. Default/empty/unknown composer options emit mask0; recognized hs, including duplicate/mixed unknown tokens, enables mask 0x5f. Separate unknown/reserved/negative inputs establish writer filtering. Requested flags are never assumed emitted. |
| Designed descriptor/data messages -> production parser | Absent/empty/hs/unknown/duplicate/whitespace options are preserved as exact text; all 64 known flags parse independently of advertisement. Unknown0x20/0x80/0x100/MIN 32/-1 raw flags are accepted by Java and refused by selected C++ policy. Wide compact flags0x100000004 project to4. |
| Six original FileWriter -> FileReader files | Custom indexed pair and full unchanged TimeAndSale, with writer opt not set/empty/hs. Omitted writer setting actually publishes hs. Each file requests1/2/4/8/16/64/95; exact delivered flags, all physical scalar slots, record time, fresh defaults and eventTimeSequence0 are asserted. |
| REMOVE_EVENT -> actual parser | Indexed/nonindexed/zero-field/unknown consumers read raw MIN 64/-1/0/mixed/MAX 64; two successive removals and a following ordinary sentinel remain aligned. readTime instrumentation delegates the production compact-long read and records its raw value/positions. |
| Indexed layouts | Seven PAIR32 full4/8/24/40/56/72/88 and three SINGLE64 full 104/120/152 use actual source slots0/1, verified physical tails and a following ordinary record. Full TimeAndSale retains 15 physical int slots and 10 logical fields from the existing source-proven indexed metadata. No name heuristic or synthetic logical tail is used. |
| Primitive Penta reads | Initial F8/MRU1, new/reused flags, no-prefix zero, flagged FF, explicit-zero F9, undefined first repeat, duplicate prefixes, mid-flags EOF and wide low 32 projection. Exact eventFlagsBytes and examined positions are recorded separately from production-message observations. |
| Reset/error boundaries | Message boundaries reset symbol and MRU1; empty messages are harmless; resetSession removes descriptions. An incomplete physical frame waits without authenticating EOF. Nine complete-frame removal truncations report one Java error and zero callbacks from that message. |
| RAW projection | All three fallback callback families are labelled and compared for equal values. One STREAM projection represents each wire record; callback multiplicity is not record count. |

The clean baseline governed Verify completed2026-10-30 at 17:10:25UTC with the immutable existing
inspector, freshly rebuilt locked upstream and all 287 expected artifacts: the unchanged 182 plus
the 105-file flags child. Separate final-source full capture/repeat ran on both qualified read-only
prebuilt classpaths. All 287 artifacts repeat byte-identically at each pin, and all four full
captures match the reviewed expectations. Each 14-root/1317-file production classpath stayed
unchanged. These prebuilt runs do not claim a fresh current reactor build. Java used Zulu
OpenJDK1.8.0_422,128MiB heap and a 30-second own-process deadline. No capture bytes were normalized.

The existing governed workflow compiles FlagsOracle with FieldTypesOracle and its existing
companions; no second maintained launcher is added. The comparator validates the exact 105-file
child set, finite row grammar/counts, signed raw values, explicit projection multiplicity and every
artifact byte. Eight new mutations target flag value, removal time, flag-byte count, projection,
composer mask, completion, binary bytes and option text. All 42 accumulated mutations were rejected,
then the unchanged full baseline was accepted. Global Java Format then Check passed 33 files with
no source-byte changes; source hashes in the receipts match the executed final helpers.

## C++ checks and intentional differences

`protocol_event_flags` checks all 64 known subsets, unknown bits before ID/time/output work,
physical-high-bit projection, zero/absent removal distinctions, arbitrary pair/single logical
positions, null string/byte defaults, nonindexed/unknown/zero-field records, sentinel alignment,
message-local state, malformed prefixes and exact time-truncation positions/owned prefix.
It exercises zero/below/exact/above payload/record/value/output limits, near-size_t allowances,
zero wireFields with dense local defaults and allocation failures through the first success.
Owned values and provenance survive context/reader destruction and equal-description generations.

The native production comparison reads actual composer files and all six FileWriter recordings.
It compares every STREAM cursor raw value against Java observations, combining physical long
slots into logical 64 values, and independently consumes original files through TapeReader.
The full unchanged TimeAndSale schema and its actual lookup metadata are retained.

Existing symbol/stream/indexed-session and generic/Profile formatter checks remain active.
Old formats refuse nonzero flags or engaged removalTime, including0, before row construction
even with output budget0. Explicit zero-prefix byte counts preserve old output. Default numeric
SymbolValue metadata is zero without changing aggregate order. The actual automatic tape-write
projection refuses engaged removalTime; outgoing view/API remains unchanged and BINARY_FIELDS_V1
stays read-only at writer/admission boundaries.

The explicit policy entry is distinctly named `decodeStreamDataWithEventPolicy`; the original
decodeStreamData/decodeRawData remain unoverloaded. Full Windows compilation exposed ambiguous
old function-address use with the initially proposed overload; the new name fixes that source
compatibility regression without modifying old callers. No unused RAW policy API is introduced.
Final-source author MSVC 14.42 Release focused 8/8 checks passed at 17:13:44UTC, including the
unchanged event_mapping_contract. Uncrustify0.83.0 followed by clang-format20.1.8 Format and Check
passed; header contracts document policy precedence, ownership, defaults and failure.

C++ deliberately preserves its owned completed prefix for malformed removal payloads, whereas
the recorded Java current-message buffer is discarded. Unknown Java-valid options/bits remain
selected UNSUPPORTED refusals. This is bounded wire-value compatibility, not collector/recovery
or identical error-delivery behavior.

Independent final integration passed Windows MSVC19.42 Release 59/59 at 17:21:05UTC and Linux
GCC13.3 Debug ASan/UBSan 59/59 at 17:21:16UTC on 2026-10-30. Linux leak detection was disabled;
no leak-check claim is made. Independent critical review passed on the final corrected sources,
separately copied Windows/Linux binaries and actual current production files; supplemental
checks covered all49 pair-type combinations and cumulative source limits. It rehashed both pins'
287 artifacts against captures/repeats/public bytes and actual production class inventories;
it did not execute Java again or attest a new current reactor build.

Doxygen completed at 17:15:03UTC, exit0 and zero warnings with EXTRACT_ALL enabled; this is
not proof that every declaration is documented. Pinned C++ Format/Check and global Java
Format/Check are recorded above. All six production-only builds passed: minimum, JSON-off wire
CLI and JSON-on profiles on Windows and Linux, completing at17:23:32/33UTC. Disabled dependency
and doctest acquisition checks and the existing JSON-off wire CLI checks also passed.
These build/configuration results are separate from CTest and do not establish live-host behavior.
