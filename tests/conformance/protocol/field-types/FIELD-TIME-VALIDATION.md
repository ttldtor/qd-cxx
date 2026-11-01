# FIELD event-time validation

[Русская версия](FIELD-TIME-VALIDATION.ru.md). English is authoritative.

The explicit READ policy is specified in [TAPE-TIME](../../../../spec/protocol/TAPE-TIME.md).
Existing signatures select DISABLED, old NONE/MESSAGE bytes remain governed expectations, and
existing formatter versions refuse engaged FIELD metadata. No CLI FIELD grammar or writer view is added.

On2026-10-30 Windows x64 MSVC19.42 Release and Linux GCC13.3 Debug ASan/UBSan each passed the
full62-test CTest suite. Linux leak detection was disabled. All six production-only profiles passed:
minimal, wire-discovery-OFF and JSON-ON on both platforms, with no new dependency acquisition.
Final Doxygen completed with zero warnings. Independent review passed the ownership, lifetime,
publication, metadata accounting and error contracts on the final semantic change.

## Production corpus

Separate final-source capture/repeat runs executed QD3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`, with JDK8 and each qualified14-root production
classpath. Before/after inventories preserve all1317 production files per pin. Repository Java
passed pinned google-java-format1.36.1 Format then Check before the final captures.

The [capture manifest](../../../fixtures/encoding/field-types/field-time-captures.json) retains
source/runtime/class/harness and original/repeat artifact hashes. The
[baseline](../../../fixtures/encoding/field-types/expected/field-time/) and
[current](../../../fixtures/encoding/field-types/current-field-time-expected/) each add29 artifacts:
25 generic tapes with616 observation rows and115 canonical source records, plus the actual standard
indexed TimeAndSale tape and27-row physical-slot/projection observations. The standard tape adds
five source records. Each capture has469 labelled production record deliveries, including the
separate parser/config/FileReader projections; these are not469 FileWriter source records.

Five original FileWriter tapes cover all four families and actual standard indexed TimeAndSale.
The generic writer matrix uses five packed values and direct parser read-enable false/true with
external packed time zero/nonzero, followed by FileReader default/NONE/FIELD/MESSAGE. Three designed
heartbeat tapes separate descriptor time NONE/MESSAGE/FIELD and zero/nonzero/negative/saturated
extreme MESSAGE state. Designed raw inputs cover missing/first-only/full pair, wrong full types,
reorder, alias, case, VOID before/between/after metadata, full signed64 physical words and deliberate
nine-byte noncanonical compact encodings with sequence-bit boundaries.

Six actual requested-description BinaryQTPComposer tapes contain normal/REMOVE/normal: recognized
pair, wrong sequence/time types and VOID before/between/after. Ordinary leading EventPrice and
wrong-type EventSequence remain transmitted on REMOVE; later Value defaults to zero. The measured
unknown-consumer FIELD case throws Java NullPointerException with no callbacks. C++ retains its
deliberately stronger safe unknown-skip guarantee. The companion uses production readers/writers;
known physical input words are labelled separately from actual Java cursor projections.

The full638-artifact capture/repeat and cross-pin comparisons passed. Existing609 expectations
remain unchanged. The old57 synthetic endpoint UID ranges remain confined to their established
in-memory comparison view; new FIELD files are compared byte-for-byte, and no raw capture is edited.

## Native ownership and resource checks

The focused Linux GCC13.3 Debug native run passed FIELD and affected existing message-family tests.
The FIELD executable consumes both immutable producer sets, compares all generic raw metadata and
available ordinary business values, and reconstructs the explicitly source-proven TimeAndSale
schema/layout to compare all logical fields. Owned outputs are checked after reader destruction.

Designed native cases additionally check default factory address compatibility, conflicting business
names before lookup, exact packed/millisecond projections, all64 known flag subsets, invalid flags,
every truncated current-record boundary, zero/L-1/L/L+1 output and field-work budgets, SIZE_MAX limits,
unknown skips and source ordinals across all four families, terminal completed prefix, allocation
failure rollback with borrowed-address preservation, every intermediate replacement, fixed plan
construction work and old-plus-candidate snapshot peak boundaries, and retained ordinary names/types
across prefix/business/prefix generations. TSV/HUMAN and all eight old formatter profiles reject
engaged zero metadata before a zero row budget.

Pinned Uncrustify0.83.0 followed by clang-format20.1.8 Format/Check passed for the C++ sources.
The clean locked full-reactor governed Verify passed all638 artifacts and the old tape baseline.
All62 verifier mutations were rejected:50 existing and12 FIELD cases covering raw seconds, optional
sequence, projection, order, business values, completion, unknown consumers, raw bytes, extra files,
standard indexed time and physical slots. The unchanged positive baseline passed afterward.
Independent copied Windows and Linux sanitizer binaries additionally passed FIELD19 cases/6546
assertions, message-family14/1266 and session12 cases (4223 Windows/4101 Linux assertions; allocation
checkpoints differ by platform). Independent inspection rehashed retained production executions,
public fixtures, unchanged classpaths and final source. No independent JVM or verifier-negative rerun
is claimed. The final documentation-only header clarification was independently checked separately
from the executed semantic binaries.

## Limits

READ is an explicit library/protocol selection. No scheduling, replay, source callback framework,
sidecar, compression, chain, business adapter, conversion, Java object execution or arbitrary-version
compatibility follows. Discovery does not infer hasTime. MESSAGE state and physical FIELD ownership
remain separate. Empty timed messages do not authenticate physical EOF or source close completion.
