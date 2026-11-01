# Exact binary scalar validation

[Русская версия](SCALAR-VALIDATION.ru.md). English is authoritative.

This report qualifies the ordinary read-only scalar extension in
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.md). The broader 19-type catalog,
object payloads, indexed layouts, new container policies, flags and business
conversions remain outside this result. Implementation review and final integration
results are separate from the focused recorded checks below.

## Production evidence

Both Java QD pins were executed: 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and 3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. The lock remains unchanged.
The [capture manifest](../../../fixtures/encoding/field-types/scalar-captures.json)
records 16 relevant production source hashes per pin, source/helper hashes, runtime
hashes, complete classpath-inventory digests, settings and every new artifact hash.
The baseline uses a clean reactor build and the original full Verify workflow;
the second pin reuses verified clean compiled classes and runs a fresh scalar
capture and repeat. These build qualifications are deliberately distinct.

| Evidence | Actual observation |
| --- | --- |
| Default factories | BYTE1 and SHORT3 return null. No synthetic field implementation is substituted. |
| Requested composer | Fourteen original captures per pin: seven identities in STREAM and RAW. BYTE1/SHORT3 bind production CompactIntField8; other non-VOID identities retain their own consumers. Wire and consumer IDs are separate columns. |
| FileWriter/FileReader | Ten original recordings per pin: 0/4/24/104/152, each in STREAM/NONE and RAW/MESSAGE, compression NONE, EMPTY_SET and zero flags. |
| Production parser | 1106 samples per pin cover positive and incomplete widths, signed bounds, compact overlong/high32 forms and following sentinel913. |
| RAW projection | Three actual callback families per successful RAW wire record; exactly one STREAM callback is required and its values are retained. All callback families must agree on the scalar and sentinel. C++ emits one source wire record. |
| VOID | Requested descriptor and FileWriter elide VOID. A manually encoded explicit VOID description is accepted by BinaryQTPParser and leaves the independent VoidIntField0 cursor slot at default zero, with sentinel alignment. No delivered logical VOID or FileWriter VOID emission is claimed. |
| Reproducibility | Each pin has 25 reviewed artifacts and a fresh byte-identical repeat. The 82 inherited baseline root artifacts remain unchanged. |

Primary implementation sources are
[SerialFieldType 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L283-L307),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L283-L307),
[BinaryRecordDesc reads and storage 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L387),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L387),
and [VOID elision 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L606-L607),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L606-L607).
Source inspection explains these observations; it does not replace either runtime.

The baseline fixtures are in [expected/scalars](../../../fixtures/encoding/field-types/expected/scalars/samples.tsv),
and the second pin in [current-scalar-expected](../../../fixtures/encoding/field-types/current-scalar-expected/samples.tsv).
`samples.tsv` records `sample`, case ID, full wire type, source family, exact complete
data payload hex, callback count, error count, STREAM raw value, sentinel and examined
field bytes. The last position is captured around the actual production
BinaryRecordDesc.readRecord call, before the parser drains a corrupt message. It
excludes the three symbol/record-ID bytes. Failed records publish no callback value.
The two pins have identical sample rows. Complete original artifacts are retained
and hashed separately per pin, without header normalization.

## C++ regression and scope

`field_type_java_values` exhaustively checks exact numeric policy, old profile
refusals, signed range invariants and unknown full identities through 512.
`field_type_java_tape` reads both pins' 1106 samples, compares raw bits, consumed
positions, exact FIELD error offsets, completed-prefix behavior and zero scalar
string charges. It separately tests skipped/replaced values, malformed skipped
SHORT, explicit versus absent wire VOID metadata, two-record alignment and finite
zero/below/exact/above payload/record/field/output/symbol budgets.

Container tests read original FileWriter tapes under the new explicit schema in
the existing LEGACY and RAW_MESSAGE_V1 grammars with chunks 1/7/65536. BYTE/SHORT
container tests combine an unchanged production FileWriter INT header with the
unchanged actual requested composer frames; these assembled inputs are not called
original FileWriter BYTE/SHORT recordings. LONG/TIME_MILLIS have one logical value
each and no synthetic physical tails. Owned records survive reader/schema/input
lifetime boundaries. Ordinary automatic discovery remains unchanged.

`protocol_tape_writer` arms its existing allocation failure instrumentation with
zero allowance and verifies early typed UNSUPPORTED for every new-profile layout,
including empty schemas, empty declarations, invalid declarations and old-type-only
schemas. This prevents the existing outgoing compact-int codec from accidentally
accepting the read-only profile.

Windows MSVC19.42 Release focused build and all three named CTest suites
passed. The first build exposed the Windows VOID macro conflict; `VOID_FIELD` fixes
the public spelling without changing macros. The resulting consumer tests compile
references after doctest's Windows headers. Pinned Uncrustify0.83.0 followed by
clang-format20.1.8 Format and Check passed for all ten authored C++ files, using
the nine-file implementation selection plus the focused I/O fixture correction.
The oracle comparator rejects 23 deliberate mutations, including six scalar
value/type/position/byte/completion/fanout mutations, then accepts the unchanged
baseline. The first full regression exposed an obsolete I/O
test fixture that called full24/raw0 invalid; the fixture now uses unsupported25.
This correction changes no production contract. Generic v4/v6/v7 presentation
continues to represent canonical full tags/raw/units, including the newly valid
scalars. Version5 Profile keeps its fixed shape/tags; existing CLI schema gates
still require their previous profiles.

Final integration on 2026-10-30 ran all 53 CTest suites on Windows MSVC19.42 Release
and Linux GCC13.3 Debug with ASan/UBSan. Each run passed 52 suites and failed only
the stale I/O fixture above. After that test-only correction, the affected suite
passed 1/1 on Windows at 14:17:54UTC and Linux at 14:19:03UTC. Production code was
unchanged; the other 52 results remain valid. This is a complete aggregate result,
not a second single 53-suite run.

All three existing production-only profiles rebuilt successfully on both systems;
JSON-OFF wire CLI checks passed on both. None of the six production-only trees
acquired doctest sources; the four JSON-OFF trees had no `_deps` directory.
Required global Java Format then Check passed for 30 source files at 14:21:40–47UTC,
with zero source-byte changes. Doxygen exited 0 at 14:21:41UTC with an empty warning
log. Its EXTRACT_ALL=YES configuration alone does not establish complete member
documentation; public comments were reviewed manually. Independent final-source
review passed after reproducing and correcting the stale fixture, with no remaining
findings.

## Reproduction and remaining limits

Use the existing [oracle invocation](../../../../tools/upstream/field-types-oracle/README.md)
for the baseline clean Verify. For the second pin, build the same locked production
module set with JDK8, compile FieldTypesOracle.java against that explicit classpath,
run its internal `scalar-only` entry into two new directories and compare the
complete 25-file sets against `current-scalar-expected`. Keep exact pin, source,
classpath and runtime digests; a current branch or source-identical second version
alone does not count as executed evidence.

No private tape, live feed, business event mapper, object decoding, indexed time,
flags, additional family or outgoing scalar support is qualified. The three old
schema policies, ordinary discovery and existing container grammars retain their
prior acceptance/refusal behavior. New combinations require explicit
BINARY_FIELDS_V1 selection; neither default construction nor old profiles widen.
