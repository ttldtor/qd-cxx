# Bounded DESCRIBE_RECORDS payload parser

[Русская версия](DESCRIBE-RECORDS.ru.md). English is authoritative.

QDCXX-013 defines the provisional raw description layer of qdcxx_protocol. Input is exactly one
complete type-2 payload, excluding length and type. No schema binding, event decoding, incoming-ID
map, conversion, transport or session recovery is implemented.

## Measured Java facts and layer projection

QD 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` is the oracle. The
[source index](SOURCES.md) identifies BinaryQTPParser.parseDescribeRecords, BinaryRecordDesc's
reading constructor/field2Desc and SerialFieldType.Bits. [P08 evidence](../../tests/fixtures/protocol/p08/COVERAGE.md)
separates a diagnostic walk through locked Java primitives from actual parser installation and errors.
The diagnostic walk is source-checked instrumentation, not a callback enumerating raw descriptions.
It emits a description only after all its fields pass structural checks. Actual remapRecord hooks
call super and observe only constructed mappings; reflection reads the resulting map without mutation.
The full parser receives a newly complete frame for every input, including strict payload prefixes.
Its frame-end position is not the diagnostic primitive position.

The payload repeats `compactInt senderId, UTFString recordName, compactInt fieldCount`, followed by
`fieldCount` pairs `UTFString fieldName, compactInt fullSerialTypeId`. There is no record count.
All numeric checks use readCompactInt's signed low 32 bits, accepting overlong/cross-width forms.
Read the entire record header before rejecting a negative ID/count or null/empty record name.
Read both field components before rejecting a null/empty field name or type outside 0..255.
Primitive errors can therefore precede those validation checks. UTFString preserves exact UTF-16
units, including isolated surrogates and NUL; it supplies the existing primitive validity rules.
Duplicates are structurally allowed. Order and full type IDs are significant.

Three distinct questions must not be conflated:

1. Structural validity accepts full type IDs 0..255 and the names/counts above.
2. Java BinaryRecordDesc.field2Desc dispatches serialization codes 0,1,2,3,4,8,9,10 after masking
   representation bits 0xf0. Other codes cause InvalidDescException, caught without message corruption
   or installation. P08 measures 5 and 255, preservation of an earlier mapping and continued parsing.
   This parser retains such raw fields without claiming operational decoder support. VOID is retained
   here although Java's constructed descriptor omits it. No serialization operation is implemented here.
3. Local field construction depends on schema factories. Wire BYTE/SHORT can construct skip mappings
   in P08's empty local scheme, while historical S15 local field construction fails. Accepted
   [ADR-0001](../../docs/decisions/ADR-0001-normalized-model-boundaries.md) keeps those boundaries separate.

P08 also measures duplicates, empty payload, zero fields, successful descriptions before a later
malformed/truncated description, surrogate names, numeric width projection and existing P01/P03/P04/P05
payloads. C++ returns all structurally completed descriptions, including unsupported types; this is a
raw-layer projection, not parity with Java's final map or callback list. It never sorts or deduplicates.

## Ownership, results and positions

`parseDescribeRecords(payload, limits)` borrows the span only during the call. The result owns ordered
record/field vectors and UTF-16 strings; senderId is not a local ID and field index is not a wire ID.
No allocation is indexed by senderId. No partial current description escapes on any error. Earlier
completed descriptions remain owned by the result, including after input/frame destruction.

Status is OK, TRUNCATED_PAYLOAD, INVALID_RECORD, INVALID_FIELD, INVALID_STRING, RESOURCE_LIMIT or
ALLOCATION_FAILURE. `consumed` is the exact payload-relative committed examination position.
`stage` identifies the primitive or validation/budget checkpoint. `errorOffset` is the primitive's
start for primitive errors, or the current position for validation/resource checkpoints; on OK it
is payload.size() and stage is END. `primitiveStatus` preserves the codec cause (OK for nonprimitive
failures). No Java exception-message parity or resumable descriptor is promised.

Incomplete primitives in a complete payload mean TRUNCATED_PAYLOAD. The string codec receives only
the remaining payload, so even its logical body overrun cannot cross the frame boundary. Parsing an
invalid payload never changes FrameReader state. Tests explicitly continue with the next frame;
connection recovery/closure policy belongs elsewhere.

## C++ budget and allocation policy

All caller budgets are mandatory finite size_t values, with no unlimited default/sentinel: payload
bytes, descriptions, fields per description, total fields, per-name encoded body bytes, per-name
UTF-16 units, and aggregate decoded UTF-16 units (record plus field names, including duplicates).
Zero is valid. These bound logical elements, not allocator metadata or exact heap capacity.

Precedence and positions:

1. Reject payload size excess at PAYLOAD, position zero, before reading or allocating.
2. At each nonempty description start check description count at DESCRIPTION, before reading its ID.
3. Read/validate the record header in Java order. Then check per-record and remaining total-field
   budgets at RECORD_HEADER, after fieldCount. Do not reserve by the untrusted count.
4. For each name use the existing UTFString per-name limits with codeUnits further limited to the
   remaining aggregate units. This preserves primitive byte-limit/encoding/unit-limit precedence.
   Budget failure reports RESOURCE_LIMIT with primitiveStatus RESOURCE_LIMIT and the primitive start;
   consumed includes its measured examination. Charge successful name units with subtraction-checked
   remaining arithmetic, even within a later failing description. This parser returns on first error.
5. Validate each complete field pair, then append it. After all fields append the completed record.
   Counts are incremented only after checked capacity; no arithmetic wraps or speculative count-sized
   reserve occurs. Earlier results are never discarded on a later resource error.

Catch bad_alloc and length_error, return ALLOCATION_FAILURE and the completed prefix. UTFString may
throw without a consumption result: its call is an examination transaction, so consumed/errorOffset
remain at that primitive's start. A field/record vector append failure reports the position after
its fully examined bytes at FIELD_HEADER/DESCRIPTION respectively. Container moves are nonthrowing;
no partially published description remains. Allocation-failure injection and huge allocations are
not claimed as executed tests.

## Conformance

[Coverage](../../tests/conformance/protocol/descriptions/COVERAGE.md) and
[validation](../../tests/conformance/protocol/descriptions/VALIDATION.md) record direct projections,
frame composition at whole/every split/bytewise schedules, small budget boundaries, sparse IDs,
lifetime and negative fixture checks. No production dependency or schema/event/transport functionality
is added. Public API names remain provisional.
