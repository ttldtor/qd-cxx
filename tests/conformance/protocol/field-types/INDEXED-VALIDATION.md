# Indexed schema and description-session validation

[Русская версия](INDEXED-VALIDATION.ru.md). English is authoritative.

This report qualifies the explicit read-only [indexed contract](../../../../spec/schema/INDEXED-SCHEMAS.md)
and partial BINARY_CATALOG_V1 supplied-schema mode. It does not qualify the complete
binary catalog, broad discovery, flags, other families, FIELD timing or source chains.
Old local profiles, tape modes, outgoing/admission/replay gates and inspector output are unchanged.

## Actual production evidence

The [manifest](../../../fixtures/encoding/field-types/indexed-captures.json) records exact
source/helper/runtime/classpath hashes and26 original artifacts per pin: QD3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Both runtimes executed independently.
The baseline used complete clean governed Capture and fresh Verify; the current pin used
qualified prebuilt production classes and fresh oracle compilation/capture/repeat.
Neither source-byte equality nor a descriptor version substitutes for either execution.

| Surface | Actual observation |
| --- | --- |
| Original FileWriter/FileReader | Eleven cases, each five signed64 values: Pair4/8/24/40/56/72/88; Single104/120/152; actual standard TimeAndSale. BINARY/NONE/EMPTY_SET/STREAM_DATA/zero flags. |
| Storage/projection | High32 at physical int0, low32 at1; full logical64 retains both words. Actual slots/full types/lookups and verified omitted VOID-tail owners are separate from logical values. No reduced standard schema. |
| Production parser | The same11 original files parsed independently; aliases, reordered/missing fields, equal/new/redefined sender IDs and actual reset are observed. EventTimeSequence remains0. |
| Successful same-message batches | Four cases with3/4 entries for sender0: equal descriptions, reorder/subset, full4 change, record-name change. Actual remaps and final callback record/time/sentinel are asserted. |
| Failure policy | A late structural error retains two valid Java prefix changes; a middle error retains one change and prevents later installation. C++ deliberately rolls back the whole batch and refuses Java wire4-to-local8 conversion, including overwritten intermediates. |
| Exporter boundary | The unchanged native-v1 exporter actually refuses all11 indexed selections with UNSUPPORTED_INDEXED. No native format/importer expansion is inferred. |
| Reproducibility | Each pin's26 original artifacts match its repeat byte-for-byte. Canonical572-row observations match across pins. LF is written by the producer; no capture normalization occurs. |

Assembled description mutations use unchanged actual composer data; they are not original
FileWriter recordings. The full clean Verify also preserves the historical82 root,25 scalar
and49 object artifacts. The current indexed run does not claim a fresh whole-catalog/current
reactor verification. Public fixtures are in
[baseline indexed](../../../fixtures/encoding/field-types/expected/indexed/observations.tsv)
and [current indexed](../../../fixtures/encoding/field-types/current-indexed-expected/observations.tsv).

The existing comparator validates finite artifact/row grammars, exact word composition,
source indexing, actual callback records, reset and batch completion, then compares original
bytes. Six indexed mutations test time, slot, lookup, last-winner, completion and binary
corruption; all34 accumulated negative mutations are rejected, followed by acceptance of the
unchanged public baseline.

## C++ and final-source verification

`indexed_schema_layout` checks finite contributor identities, presence/index errors, old-profile
refusals, explicit mode selection, reordered logical positions, owned plans and missing defaults.
`tape_session_generations` checks sparse INT_MAX senders, stable active ordering, equal-shape
fresh provenance, every intermediate entry, empty-batch no-op, old borrowed/owned lifetime,
parse/type/peak/work/allocation rollback, independent new-reader reset and payload counters.
It exercises zero/below/exact/above storage/work limits,100 equal replacements, all incomplete
update envelopes and public UTFString diagnostic parity of allocation-free name admission.
Ordinary allocation failure is injected at every checkpoint through the first successful
replacement; no prior published address or identity changes on failure.

`indexed_schema_java` and `indexed_schema_java_current` consume their pin's original files
under chunks1/7/65536, retain all ten TimeAndSale logical fields, compare raw words/sentinels,
exercise same-message alternatives and assert the selected stronger C++ refusal/rollback.
No indexedTime duplicate or synthetic tail is used to satisfy the comparison.

Final-source MSVC14.42 focused checks passed. Independent final integration passed
Windows MSVC19.42 Release58/58 at16:44:20UTC and Linux GCC13.3 Debug ASan/UBSan58/58 at16:45:33UTC;
Linux leak detection was disabled. All three production-only profiles on each OS passed,
including JSON-off wire CLI and disabled dependency/test-only fetching boundaries.
Independent critical review passed with separately copied Windows/Linux focused binaries
and unchanged reviewed source hashes.

Pinned Uncrustify0.83.0 then clang-format20.1.8 Format/Check passed for11 authored C++ files.
Global google-java-format1.36.1 Format/Check passed for32 Java sources before final captures.
Doxygen exited0 with an empty warning log on frozen headers. EXTRACT_ALL suppresses missing-member
warnings, so that run alone does not prove complete API documentation; ownership/error/parameter
comments were also inspected. Paired documentation and evidence checks are recorded separately.
These scoped results establish no all-QD, business-adapter, transport or host-runtime acceptance.
