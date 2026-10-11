# Writer, strict admission and replay contract fixtures

[Русская версия](README.ru.md). English is authoritative.

These are deterministic designed inputs/expected outcomes for
[TAPE-WRITER](../../../../../spec/protocol/TAPE-WRITER.md),
[TAPE-WRITE](../../../../../docs/TAPE-WRITE.md) and
[OFFLINE-REPLAY](../../../../../spec/protocol/OFFLINE-REPLAY.md).
They are not production captures or executed C++ writer/replay results. Existing production
[captures](../CAPTURES.md), [coverage](../COVERAGE.md) and
[validation](../../../../conformance/protocol/tape-write/VALIDATION.md) remain separate evidence.
No historical artifact is regenerated through this packet.

## File grammar

All files use UTF-8 without BOM and LF. TSV has one header, exactly the documented columns,
no quoting/escaping, and one finite case per following line. IDs are unique within each table.
Decimal signed raw/time values are exact mathematical integers, never parsed through floating
point. JSON `raw`/`timestampMs` are decimal **strings** to preserve int64 extrema. JSON numeric
IDs/types/counts are bounded int32 and ordinary JSON integer spelling. All root format/version
identities are exact. These JSON fixtures are test data, not a new public tape/configuration loader.

UTF-16 unit strings are lowercase four-digit hex per unit. Empty JSON string means engaged empty
units; JSON null in FieldValue means disengaged units (type10 null or absent scalar payload).
In TSV `null` is type10 null, `empty` engaged empty, `absent` scalar disengaged; scalar units must
be absent. Symbol JSON units are always nonnull, with empty text admitted. Names in the native
schema/layout use ordinary valid Unicode text; lone-surrogate values use hex units only.

`schema.json` is unchanged native version1 grammar, local records Example0 and Empty1.
`writer-cases.json` supplies immutable sender37→local0 and sender127→local1 layouts.
Local values Count/Text are deliberately serialized in wire order Text/OutgoingCount. Lookup
Count precedes OutgoingCount, making first-alias inference visibly wrong. `descriptor.version=null`
means absent version, `options=""` means present empty opt; descriptor null option means absence,
not an explicit wire null property. Present-null wire property is only a negative admission mutation.

`actions` are complete writer calls. `recordIndices` select the supplied records by zero-based
index; each call produces one envelope. finish produces no envelope. Whole positive output must
contain all empty heartbeat forms, negative and trailing extreme time, explicit marker, appended
late description, empty data and empty description group without coalescing or implicit footer.
Mutation paths use only dotted object members and literal zero-based array indices. Apply each
mutation to a fresh deep copy of the positive fixture; `also` is applied before the same operation.
No general patch language is required. W07 deliberately has both nonzero flags and negative sender
ID: flags wins after record-attempt budget. Failure returns no current chunk, then every later call
returns the same first error; retained earlier chunks are compared unchanged.

W18 rejects sender INT32_MAX at `record-id`: the locked Java parser overflows `id + 1`.
Native layout construction separately accepts INT32_MAX-1 without allocating by that ID;
do not ask Java to allocate this huge dense mapping as a positive test. The corrected
sender127 corpus must pass production Java reading. The raw int32 maximum value and
trailing int64 maximum heartbeat remain unchanged. Reader acceptance is not narrowed.

## Admission mutations

`admission-cases.json` creates a fresh synthetic wire sequence for each listed `change`, preserving
the positive base elsewhere. A02/A03 have no data using the added description: admission must still
refuse them. A04 replaces Count's name with Text and type with10 so type validation succeeds and
repeated target is the actual refusal. A05 removes Count; A07 keeps the existing Example description
and adds an identical local-record name under sender99. A06 repeats sender37 and is rejected by the
existing reader before strict inventory checks. A08/A09 use exact unsupported full IDs24/137.
A21 replaces the record name with one UTFString containing lone unit d800; the raw primitive can
represent it, but the existing description/name validation refuses it.
A23 changes sender37 to INT32_MAX in both its description and data. The unchanged
reader can accept it, but admission must retain the writer's INVALID_LAYOUT/record-id
cause and refuse before an admitted token or output is published; no ID is renumbered.

Malformed or unsupported synthetic descriptions must be assembled with reused primitive writers
and a test-only envelope builder rather than fed through TapeWriter's validated layout factory.
For unused-description mutations no corresponding malformed data payload is added; description
admission is the test. Keep synthetic provenance explicit; these cases are not relabelled FileWriter
observations. Production complete Quote/Profile/custom records remain separate positive inputs.

## Primitive/value boundaries

`primitives.tsv` documents exact canonical bytes. Capacity includes the whole primitive; byteLimit
includes the char-array prefix or complete symbol. P13 follows P12 in the same frame; P14 starts a
fresh frame. For every `unchanged` outcome initialize all output bytes to a sentinel and compare the
whole span after failure. Compare successful bytes and untouched excess output. P04 encodes two
surrogate units separately; P10 proves first empty symbol cannot use undefined FF repeat.
`raw-values.tsv` gives all admitted exact full identities and extrema plus invalid combinations.
Create one explicit local field/layout of the row's supported type; negative unsupported24/137
must fail profile/layout admission rather than becoming a supported generic raw field.

## Replay harness

`replay.tsv` uses a complete admitted tape: for each `frameTimesMs` item emit a bits1 heartbeat
with that exact decimal timestamp then one data frame with its `frameRecordCounts` records.
`absent` emits no prior timestamp heartbeat. Items retain state according to RAW-TAPE; a later
explicit zero is present. Give records distinct Count values and intentionally unrelated business
time fields in a separate real-Quote companion check. Observer receives exact sequence/order.

The fake clock initially reads `startNs`; normal wait advances exactly to the requested slice
deadline and returns READY. `targetsNs` is one scheduling target per data frame, `immediate` before
anchor, `excluded` for empty frames, `error` for a range/clock failure. A listed target does not
assert a wait occurred if the fake clock already reached it. Mutation wait-ready-before-deadline
returns READY without advancing on the first needed wait; clock-decreases returns a decreasing
sample after the first delivered frame; clock negative fails the first anchor sample. maxWaitCalls-1
sets that explicit limit to1. Cancellation/observer `record-1` means before/at second record callback;
final-after-eof means the final poll after actual finish. A throwing poll verifies the documented
invalid-callback exception handling. Reader-processedRecords-1 sets the new reader's record bound
to1 for a two-record frame: its complete prefix must be withheld, not delivered as a valid frame.

R21 empty100→nonempty110 anchors at110 immediately. R22/R23 prove an extreme empty timestamp neither
anchors nor advances the schedule. R24 proves a later genuine nonempty huge difference still fails.
These cases check clock/wait calls are absent for empty frames; parsing/metadata budgets still apply.
No real sleeps or observer windows are used; borrowed data is copied inside the callback for assertions.

## Publication and budget harnesses

`publication.tsv` names the actual owning boundary operation to fail. Inject through the production
file-operation/write wrapper used by the utility; do not replace it with a bypassed standalone
mock. F06 emits a real prefix then fails, F07 reports zero progress, F10 creates a race target
immediately before create-only link. F13 leaves a complete published file when temp unlink fails;
F14 preserves primary write error despite cleanup failure. File names/bytes/owned inventory before
and after must be checked, including alias and dangling-symlink cases. Unsupported symlink/hardlink
test environments report limitations rather than success. Windows case equivalence is platform-scoped.

`budgets.tsv` expands each row into zero/exact/one-below/one-above tests. Determine the independent
expected logical charge from fixture elements/encoded lengths and the specified accounting, not
only the implementation's returned counter. At exact and one-above allowance, the positive operation
succeeds; at one-below needed allowance, RESOURCE_LIMIT occurs with atomic current operation. Zero
must also check a genuinely zero-cost null/empty value where allowed, not assume universal failure.
Inherited SchemaLimits/TapeLimits keep their existing boundary suites and are rerun when composed.

B34 uses `description-heavy.json`: generate i=0..127, localId=i, senderId=37+i,
recordName=`Example` plus exactly three zero-padded ASCII decimal digits (Example000..Example127).
Each ordinary record has one exact type8 field, lookup Count then OutgoingCount, wire name OutgoingCount,
one local target0. Emit one description group for all128 records, then one data batch with one zero-valued
IBM-symbol record per ID, then finish, using the supplied exact descriptor. This produces 1280 record-name
units,1664 wire-field-name units,2944 units per raw-description/outgoing-layout collection and3584 local
schema name/lookup units. All schema/reader/layout/work/output limits use the explicit CLI defaults and
must pass the untightened baseline; writer scratch remains independently fixed at1048585 bytes/32768 units.
Only --max-admission-scratch-units is varied. No destination temporary may exist on admission refusal.

During the eventual runtime baseline, instrument the **actual admission owner**, recording each live
additional description/candidate/old-inventory copy and its counted string lengths at construction,
transfer and release checkpoints. Independently sum these collections (reader/schema permanent ownership
is charged separately) and take peak L; compare L with the implementation's reported scratch ledger.
One complete raw-description collection contributes2944; each simultaneous full outgoing candidate copy
contributes2944. Partial collections contribute their actual independently counted name units. Avoid
assuming a fixed number of copies or claiming an allocator-byte peak from these logical units. Rerun fresh
instances with admission unit allowance0,L−1,L,L+1, preserving all other limits/writer scratch. Expected
RESOURCE_LIMIT at0/L−1, success atL/L+1, unchanged original/destination and no temp on refusal. Save the
complete owner trace and observed L; neither is an executed result in this packet.
B32 similarly varies admission byte scratch
with source-copy size exact/below and writer scratch independent. Separate copy/work counters remain
charged, retained storage is checked, and released temporary memory never refunds cumulative work.

## Required acceptance executions

Writer: focused doctest using JSON/primitive/value/budget inputs, owned result/state/order checks,
two independently configured instances, C++→production Java parse and semantic Java→C++ comparison.
Strict utility: complete production Quote/Profile/custom/native-schema inputs plus all admission/
publication/CLI/help/limits cases, original/target/temp preservation assertions and parse of published bytes.
Replay: all fake-clock rows plus lifecycle/borrowed ownership/output-finalization checks and Java raw
time/cursor comparison. These are required executions; this packet records no runtime passes.

Run project-required pinned formatting, Doxygen diagnostics, full Windows MSVC Release and Linux
GCC Debug ASan+UBSan CTest, optional-dependency/library-only/utility-OFF/JSON-OFF checks as applicable.
Keep actual UTC full logs and independent ownership/time/state review distinct from author structural
fixture checks. Historical matrices do not verify new implementations.
