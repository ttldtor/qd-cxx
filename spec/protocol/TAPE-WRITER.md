# Bounded tape writer, RAW/MESSAGE version 1

[Русская версия](TAPE-WRITER.ru.md). English is authoritative.

This contract defines the outgoing subset `RAW_MESSAGE_V1`: uncompressed BINARY,
RAW_DATA (5), MESSAGE timing, zero event flags, and ordinary exact full types
2/8/10/40/56/72/88/120. The in-memory writer implements this bounded API; it does
not provide file I/O or publication.
[RAW-TAPE](RAW-TAPE.md), [FIELD-TYPES](../encoding/FIELD-TYPES.md),
[STATIC-BINDING](STATIC-BINDING.md) and existing reader modes retain their behavior.
No VOID wire field, new representation, indexed record, conversion, event adapter,
STREAM/NONE/FIELD writing, compression, transport or session behavior is admitted.

## Authority and evidence

The locked revision is `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` in
[upstream.lock](../../upstream.lock). The eleven production cases and exact 43-artifact
inventory are in [captures](../../tests/fixtures/protocol/tape-write/CAPTURES.md),
[coverage](../../tests/fixtures/protocol/tape-write/COVERAGE.md) and
[validation](../../tests/conformance/protocol/tape-write/VALIDATION.md).
These establish finite Java FileWriter → FileReader witnesses; they do not establish
this C++ API, arbitrary strict rewriting or filesystem durability.

| Fact | Primary source and retained observation |
| --- | --- |
| Outgoing names are property names, not first lookup keys; VOID tails are omitted. | [BinaryRecordDesc:182–204](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L182); [custom wire scan](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message-wire.tsv). |
| Sender record ID and ordered field descriptions are emitted explicitly. | [BinaryQTPComposer:209](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L209); [Quote](../../tests/fixtures/protocol/tape-write/expected/extras/quote-raw-wire.tsv), [Profile](../../tests/fixtures/protocol/tape-write/expected/extras/profile-raw-wire.tsv). |
| Type 10 counts UTF-16 units and writes individual UTFChar values. | [IOUtil:339–361](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/IOUtil.java#L339); [production values](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message.tsv). |
| Raw string symbol uses FD plus a character array; equal previous raw string uses FF. State resets per message. | [PentaCodec:421–477](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java#L421). |
| Record header is symbol then compact sender ID; fields follow ordered descriptors. | [BinaryQTPComposer:90](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L90), [BinaryRecordDesc:455](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L455). |
| MESSAGE batch/singleton grouping differs; NONE loses timing, FIELD adds fields. | [batch scan](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message-wire.tsv), [singleton scan](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-singletons-wire.tsv), [FIELD scan](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-field-time-wire.tsv). |

API shape, strict bijection, raw-only symbol normalization, finite budgets and terminal
operation atomicity below are deliberate bounded policies. They do not promise Java
exception parity or byte-identical FileWriter output.

Implementation and integration evidence is summarized in
[writer validation](../../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md).
Retained native input/output cases, Java parser observations and platform logs are indexed in
the writer evidence archive.

## Immutable layouts and names

`OutgoingField` owns `wireName: u16string`, `serialTypeId: int32_t`, and
`localFieldId: int32_t`. `OutgoingRecord` owns `senderId: int32_t`,
`localRecordId: int32_t`, `wireName: u16string`, and ordered `fields`.
`OutgoingLayouts::create(const LocalSchema&, span<const OutgoingRecord>, LayoutLimits)`
returns `expected<OutgoingLayouts, WriterDiagnostic>`. It borrows inputs for the call,
owns a bounded schema copy and layout copy, and exposes const views only.

Sender IDs are in 0..INT32_MAX-1 and distinct from local IDs. INT32_MAX is rejected as
INVALID_LAYOUT at the record-ID check. The locked production parser computes `id + 1`
in signed Java int in [BinaryQTPParser:501–515](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java#L501);
INT32_MAX overflows and its later map access fails. Lower large IDs still require a
dense Java array and remain subject to that JVM's array-length, heap and work limits;
the numeric range does not promise universal Java resource compatibility. Choose sender
IDs within the receiving implementation's budgets. The existing reader range is unchanged;
strict rewriting rejects an unrepresentable outgoing ID rather than renumbering it.
No allocation in the native writer is indexed
by the numeric sender ID. Local IDs refer to existing schema records. The record name
must exactly equal that local record's name. Sender IDs, local IDs and record names are
unique across this layout set. Multiple incoming sender IDs for one local record are
deliberately refused by strict rewriting rather than silently coalesced.

Every outgoing record covers every local logical field exactly once, with no skipped,
repeated or missing target. The order can be any permutation. Each field's full type
equals the local full type and is supported by the explicit local profile. Its wire
name is a supplied exact lookup key targeting that field. Field names are unique within
a record. Names must be nonempty well-formed UTF-16; no normalization, case folding,
prefixing, alias selection or property-name inference occurs. Empty schemas/layouts
and zero-field records are valid. Layouts need not cover unused local schema records.
`hasTime=true`, physical Java slots and VOID tails are not representable.

Validation order is layout count/schema bounds, then records in supplied order: ID
ranges and duplicate identities; record name validity/budget and exact match; field
count/budget; fields in wire order: local target range, duplicate target, name
validity/budget/lookup, exact full type; finally complete target coverage. A failed
construction publishes nothing. Allocation/length exceptions become ALLOCATION_FAILURE.

## Exact API surface and ownership

Public namespace is `org::ttldtor::qdcxx::protocol`; headers are
`qd/protocol/tape_writer.hpp` and later `qd/protocol/tape_admission.hpp`.
`qdcxx_protocol` owns composition; `qdcxx_encoding` owns primitives. Public result
types expose project/STL types only. The API is single-threaded, non-reentrant,
move-constructible, noncopyable, with no reset, setters, callbacks or ambient settings.

| Type or method | Contract |
| --- | --- |
| `OutgoingDescriptor` | Owns `optional<u16string> version`, `optional<u16string> options`, exact `u16string time`, and `bool explicitRawSend`. No legacy booleans or unknown properties exist. |
| `OutgoingRecordView` | Borrowed `int32_t senderId`, nonnullable `u16string_view symbol`, `span<const FieldValue> values` in **local** logical order, and `int32_t eventFlags=0`. All valid only for the call. |
| `WriterChunk` | Owns `vector<byte> bytes`. One whole envelope per successful operation; `finish()` owns an empty byte vector. Caller-held chunks are caller responsibility. |
| `TapeWriter::create(const OutgoingLayouts&, const OutgoingDescriptor&, WriterLimits)` | Returns `expected<TapeWriter, WriterDiagnostic>`; validates/copies complete configuration before returning READY. No bytes emitted. |
| `begin()` | READY → ACTIVE; emits the sole type-1 descriptor. Repeated/out-of-order call fails terminally. |
| `describe(span<const int32_t> senderIds)` | ACTIVE only; emits one complete type-2 description group, in supplied order, from previously unannounced immutable layouts. Empty group is valid. Duplicate/group-repeated/already announced/unknown ID fails. Marks all IDs announced only after whole operation succeeds. |
| `heartbeat(const TapeHeartbeat&)` | ACTIVE only; emits one type-0 frame. Accepts exactly `(content absent,timestamp absent)`, `(0,absent)` or `(1,present signed64)`. Invalid combinations fail. |
| `data(span<const OutgoingRecordView>)` | ACTIVE only; emits one type-5 frame, even for an empty batch. Every sender ID must already be announced. Preserves input order and maps local values through the explicit ordered field targets. |
| `marker()` | ACTIVE only; emits the compact zero-length envelope. This does not authenticate EOF. |
| `finish()` | ACTIVE → FINISHED after all checks; emits no implicit marker/heartbeat/description and no bytes. Idempotent FINISHED call returns an empty chunk without charges. READY finish fails. |
| `state()`, `diagnostic()`, `counters()` | Const access to READY/ACTIVE/FAILED/FINISHED, first owning/static diagnostic and logical counters. Borrowed access ends at destruction or move. |

Each operation returns `expected<WriterChunk, WriterDiagnostic>`. Every failed operation
after construction makes the writer FAILED. Later operations return the identical first
diagnostic without inspecting inputs or charging work. Any operation except idempotent
`finish()` after FINISHED returns INVALID_STATE and transitions to FAILED. A failed
operation returns no bytes and commits no announcements/timing/output counters. Earlier
chunks remain independent and may form a valid prefix, never a successful completed tape.
Destroying a writer does not finish it or perform I/O.

The writer owns immutable layouts, descriptor and announcement bits. It retains no
symbol state across `data()` operations; within one frame it retains at most one bounded
previous symbol. Inputs and output storage must not alias implementation-owned state.
Allocation/length exceptions from bounded operations are caught and terminally reported.
No fault-injected allocation guarantee is claimed without real owning-boundary tests.

## Descriptor, frames and metadata

Descriptor magic is DXP3, property map contains exact `type=tape`, optional opaque
non-null `version`, optional `opt` (absent, empty or exact `hs`), and required `time`
exactly `MESSAGE` or `message`. Null version/options/time and other text are refused.
Time and option spelling are preserved. `explicitRawSend=false` emits empty send list;
true emits ID 5/name RAW_DATA with zero properties. Receive list is empty. Canonical
property order is type, version when present, opt when present, time; incoming pair order
and primitive widths are normalized. Any input descriptor information absent from this
API must be refused by admission, never discarded. A conversion from `TapeDescriptor`
must also reject `explicitTimeNone` or `explicitStreamSend`, inconsistent flags or any
future unrepresented member.

Envelope length is compact-long canonical byte length of compact-int message ID plus
payload. Type-2 descriptions are ordered sender ID/name/field-count/name/full-type pairs,
with count/ID bounds from the existing raw description grammar. Primitive count/range
checks precede resource checks. Type-0 zero payload differs from explicit content 0.
Bits 1 carries exact signed compact-long milliseconds. Heartbeats leave data independent;
timestamp/cursor packing follows RAW-TAPE exactly, including absence, zero, negatives,
repetition, decrease and saturation. Consecutive/trailing heartbeats and markers remain
explicit operations. Data does not synthesize time or split a batch when time changes.
The caller emits heartbeat operations at the required original boundaries.

## Raw values and canonical encodings

`FieldValue` validation is exactly FIELD-TYPES: type 10 has raw zero and nullable exact
UTF-16 units; scalar types have no units; 8/40/56/72/88 are sign-extended int32; 120 accepts
all int64; 2 accepts 0..0x10ffff, including surrogate code points. Type and local order
must match the selected schema. Null symbol is unrepresentable; empty symbol and any
exact units, including NUL/lone surrogates, are admitted within limits. Nonzero flags
are rejected after the record-attempt budget and before symbol/ID/value examination for that record. Cipher and flag-prefix byte
metadata are not API inputs or semantic preservation promises.

| Full type | Canonical field writer |
| --- | --- |
| 2 | Existing `writeUtfChar(raw)`; supplementary code point is one field scalar. |
| 8/40/56/72/88 | Existing `writeCompactInt(sign-extended raw)`; compatible with Java compact-long read/low32 projection. |
| 120 | Existing `writeCompactLong(raw)`; no decimal conversion. |
| 10 | New bounded `writeCharArray(optional<u16string_view>, span<byte>, CharArrayLimits)`: compact-int count -1/null, 0/empty or UTF-16 unit count ≤ INT32_MAX, then existing `writeUtfChar` per unit. Pair surrogates remain two units. |

Reuse existing atomic compact/UTFChar/UTFString writers; do not create parallel primitive
encoders. `writeCharArray` preflights count, encoded size, units and output capacity with
checked arithmetic; on failure writes zero and leaves the whole output unchanged.
Its per-value encoded-byte bound includes the prefix. Status vocabulary follows existing
encoding `Status`; invalid count uses INVALID_LENGTH, budget RESOURCE_LIMIT, capacity
INSUFFICIENT_OUTPUT. It performs no allocation and retains no input.

New bounded symbol composition emits FD plus **nonnull** `writeCharArray` for a first or
different symbol, FF for exact unit equality with the previous symbol in that same frame.
The first symbol, even empty, never uses FF. It emits no F8/F9 flags or penta/cipher form.
This is deliberately supported normalization, including wildcard text `*`; production
Java must confirm exact parsed units for empty/wildcard/cipher-origin/long/raw/lone-surrogate
inputs before implementation acceptance. It does not claim shortest Java cipher bytes.
Message-local previous-symbol storage and equality scans are bounded/charged.

## Finite budgets and first failure

All library limits are required finite `size_t` counts, no unlimited sentinel; zero is
a real bound and equality succeeds. Layout/schema bounds are checked before proportional
allocation. No exact heap-byte or allocator-capacity claim is made.

| Limits | Units and ownership/work covered |
| --- | --- |
| `LayoutLimits {schema, records, fields, nameUnits, totalNameUnits}` | Existing SchemaLimits plus copied layouts, all fields, individual/aggregate record+field names; copy/work charges include the schema. |
| `WriterLimits {frameBytes, outputBytes, frames, records, fields, text, textUnits, fieldString, fieldBytes, fieldUnits, symbolUnits, totalSymbolUnits, scratchBytes, scratchUnits, workUnits}` | Frame body bytes; committed entire encoded file bytes; envelopes including markers; cumulative attempted records/fields; per UTFString text and cumulative descriptor/name units; one type-10 encoded bytes/units and cumulative field encoded bytes/unit visits; per symbol and cumulative symbol unit visits; peak private envelope bytes and peak retained/current temporary UTF-16 units; cumulative logical work. |
| `workUnits` | Each examined layout/schema element, each describe ID/field/name unit, record attempt, field attempt, descriptor/field/symbol unit visited during validation, sizing, encoding and symbol equality. Each actual traversal charges separately; repeated attempts/failures do not refund charges. |

`scratchBytes` covers simultaneously owned frame/chunk byte buffers before transfer;
implementation should size then allocate one envelope rather than payload plus envelope.
`scratchUnits` covers any owned temporary strings plus retained previous symbol; immutable
schema/layout/descriptor units are separately bounded. Caller inputs/chunks are excluded.
Records/fields count all attempted input elements, not only emitted bytes. Field byte/unit
work and symbol visits include repeat processing; canonical size determines output charge.
Successful operations transfer their scratch chunk to caller and release temporary state.
Cumulative work charges survive failure; committed output/frame counters change only on
whole operation success. Diagnostics distinguish these counters.

Use subtraction-before-addition for counters and checked representability for encoded
lengths. Before every traversal/allocation check its relevant remaining allowance. Operation
order is state; operation/group/record count budget; each element's structural/profile checks
in input order; text/value validity and per-value budgets; remaining cumulative work;
canonical sizing and frame/scratch/remaining-output limits; allocation; encoding; commit.
Within a data record: record-attempt budget, zero flags, symbol validity/budget, sender
identity/announcement, exact local value count, fields in local order (tag/raw/units), then
wire serialization order. Arithmetic failure is RESOURCE_LIMIT; bad value/layout is
INVALID_VALUE/INVALID_LAYOUT; unsupported profile/flags/type/descriptor is UNSUPPORTED;
lifecycle is INVALID_STATE; allocation is ALLOCATION_FAILURE. Construction errors are
distinct from a successfully built writer's terminal operation error.

Diagnostic owns enums and numeric operation/record/field checkpoints plus static stage
text; it never borrows input names. Since no bytes of a failed chunk escape, no invented
file offset is reported for uncommitted bytes. A caller I/O failure belongs to the caller;
the writer cannot certify sink success.

## Strict admission and implementation checks

[TAPE-WRITE](../../docs/TAPE-WRITE.md) defines preflight, exact descriptor coverage,
one-to-one incoming binding and publication. [OFFLINE-REPLAY](OFFLINE-REPLAY.md) defines
separate clock/cancellation ownership. A decoded record alone never proves admission.

Required implementation checks include independent instances/owned results, all supported
tags and raw extremes, name permutation/explicit ID mapping, null versus empty/type-10
surrogate units, every heartbeat form and descriptor distinction, no implicit finish marker,
state/order/unknown-ID/duplicate failures, each limit at zero/exact/excess, and allocation
handling where an actual testable boundary exists. Production Java must parse C++ chunks;
compare one wire projection against STREAM_DATA callbacks, not Java's triple RAW fanout.
Canonical primitive witnesses may require exact bytes; whole tapes require semantic equality
of admitted frame actions, names/IDs/types/values, symbols and raw time/cursor sequence.
Existing conformance fixtures, modes and output versions must remain unchanged.
