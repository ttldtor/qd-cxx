# Explicit indexed schemas and immutable description generations

[Русская версия](INDEXED-SCHEMAS.ru.md). English is authoritative.

This read-only extension requires explicit `LocalProfile::BINARY_FIELDS_V1`.
The caller supplies a validated local schema; wire names do not create indexing.
The partial `TapeMode::BINARY_CATALOG_V1` adds active sender replacement while retaining
restricted BINARY/NONE/EMPTY_SET/STREAM_DATA/zero-flags grammar. Headerless input,
other data families, options/flags, FIELD time and source chains remain separate work.
[Validation](../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.md) identifies
executed checks and remaining qualifications.

## Source storage and local values

Java QD3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19` define indexed cursor time through the
first two physical integer slots: signed high32 first, then low32 bits.
`getTime`/`setTime` are independent of cursor EventTimeSequence and tape MESSAGE/FIELD
timing. See [cursor time](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordCursor.java#L511-L534),
[word order](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordCursor.java#L1331-L1342)
and [the second pin](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/ng/RecordCursor.java#L1331-L1342).

`LocalRecord.hasTime` remains explicit. Its trailing optional `indexedTimeLayout` owns:

| Alternative | Logical field IDs | Admitted exact identities |
| --- | --- | --- |
| `IndexedTimePairInt32` | `firstField`, distinct `secondField` | Each full4/8/24/40/56/72/88 |
| `IndexedTimeSingleInt64` | `field` | Full104/120/152 |

An indexed record requires a layout; an ordinary record requires its absence.
Inconsistent presence, duplicate pair IDs and out-of-range IDs are INVALID_SCHEMA.
Narrow BYTE/SHORT/UTF_CHAR, VOID and object fields cannot preserve arbitrary time words;
using them as contributors is UNSUPPORTED_PROFILE. Older local profiles still refuse
indexing and indexed metadata. Ordinary fields retain their exact existing type policy.

Physical source slots, logical local field IDs, wire field order, sender IDs,
local record IDs and decoded positions are distinct domains. Context construction resolves
the local layout into trailing owned `StreamRecordPlan.indexedTimeLayout` using
`ResolvedIndexedTimePairInt32` or `ResolvedIndexedTimeSingleInt64`. It survives schema
destruction. Current local field IDs equal validated positions; that does not authorize
using a Java slot or sparse sender ID as a value index.

Unsigned word composition followed by signed64 bit-cast preserves all bits without signed
shift overflow. A single64 field already retains them. No Decimal/time-unit/business
conversion occurs. Missing wire fields retain fresh dense defaults; repeated assignments
remain ordered last-write wins. Wire reorder and explicit aliases affect binding actions,
not the resolved positions. There is no duplicate indexedTime member in delivered records.

Production LONG104/WIDE_DECIMAL120/TIME_MILLIS152 occupy two Java integer slots.
Their verified physical VOID tail is omitted from logical values and wire descriptions;
standalone explicit VOID retains its separate [field contract](../encoding/FIELD-TYPES.md).
[BinaryRecordDesc](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L162-L216)
uses actual property names and elides tails. DESCRIBE_RECORDS has no hasTime bit.
The existing native-v1 [Java exporter](JAVA-EXPORT.md) and loader still refuse indexed
input. Manual validated metadata and source observations do not widen that format.

## Explicit protocol selection

Use the supplied-schema TapeReader constructor with BINARY_FIELDS_V1 and BINARY_CATALOG_V1.
The ordinary discovery constructor refuses the new mode. Older tape modes retain ordinary
acceptance, repeated-sender refusal, empty-batch behavior and output; they refuse indexed
schemas. No default constructor or inspector selection changes.

The new mode maintains one sparse active table in stable first-seen sender order.
A replacement keeps its position; a new sender appends. INT_MAX is a key, never an
allocation index. Local IDs remain supplied-schema IDs. Name changes can bind another local
record or become supported skip plans. Descriptions() exposes active entries, not history.

For every complete nonempty batch, validate all entries in wire order, including overwritten
intermediates. The last valid entry for a sender defines the candidate. Build fresh schema
identity and context, then publish schema/descriptions/context through statically checked
nonthrowing moves/swaps. Equal numeric shapes get fresh identity. An empty batch is a no-op.
No user callbacks run between publications; this is a single-thread guarantee.

Parse, type, resource and allocation failures publish none of the batch. Old borrowed
addresses, values and provenance remain intact; work counters and terminal diagnostics can
advance. Owned records, copied contexts and schema snapshots remain independent. Borrowed
views expire on successful publication, owner move/assignment or destruction. The reader
retains only the current generation.

Java's [parser loop](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java#L354-L410)
installs complete entries immediately; a late error can retain earlier changes.
C++ deliberately selects whole-batch rollback and exact types, refusing the measured Java
wire4-to-local8 conversion even when a later entry would overwrite it.

For an independent closed source, explicitly construct a fresh reader. It cannot reuse old
sender descriptions; old owned output survives. Java reset remains applicable observed
behavior. No in-place C++ reset, implicit header reset or source-chain policy is introduced.

## Exact finite logical budgets

The new mode requires trailing `TapeLimits.snapshotUnits`; zero is finite.
Old modes ignore it. Existing schema, incoming-field/name and final-context limits remain
independent. `context.descriptions` bounds incoming attempts including duplicates, and
independently bounds unique active senders. Construction work is never refunded.

| Category | Logical units |
| --- | --- |
| S, schema | One identity, records, fields, lookup entries, owned record/key UTF-16 units, one engaged local layout |
| D, raw declarations | Descriptions, declared field elements, owned record/field name units |
| C, context | Plans, actions, default-vector headers/default values, one engaged resolved layout per indexed plan |
| Temporary validation | One plan and its field actions |

Peak storage includes old S+D+C, raw staging, candidate copies, fresh schema/context and
temporary validation simultaneously owned. Staged field slots are admitted from a validated
count before construction. Names are measured without allocation using existing compact/
UTFChar primitives, then their exact units are admitted before owning UTFString decode.
Vector capacity, allocator metadata and external caller copies are not units or RSS claims.
Frame/header/data/output memory retain their separate bounds.

Work charges constructor S+C, admitted description payload bytes, raw metadata construction,
each candidate/schema/context copy and each temporary binding plan. Destroying staging
releases storage, never work. Equal replacement storms reach exact limits without retaining
history. Completed payload/string ownership counters retain aggregate semantics across generations.

The focused reordered Pair example has S28, empty C4, D23 and active C9: constructor32,
first peak92 and replacement peak129. Full72 needs two compact bytes, so first cumulative
work is148 and each equal replacement adds139. These are example-specific checks, not
universal constants or heap measurements. Checks subtract before addition. Allocation
exceptions retain the existing typed terminal failure; no partial candidate is published.
