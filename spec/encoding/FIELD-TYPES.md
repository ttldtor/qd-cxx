# Bounded additional exact field types

[Русская версия](FIELD-TYPES.ru.md). English is authoritative.

The initial QDCXX-024 contract was defined from locked-Java primitive and production
FileWriter/FileReader captures. The binary scalar extension below adds separately qualified
both-pin evidence. The [oracle](../../tools/upstream/field-types-oracle/FieldTypesOracle.java)
and [capture report](../../tests/fixtures/encoding/field-types/CAPTURES.md) distinguish runtime observations,
diagnostic inputs and reproducibility checks. The lock remains unchanged. This is field support,
not a Profile event adapter or evidence of complete private-tape compatibility.

## Explicit binary field profile

`LocalProfile::BINARY_FIELDS_V1` is an opt-in, ordinary, read-only subset of
the [finite binary catalog](../protocol/BINARY-TAPE-COVERAGE.md). It accepts exactly
0/1/2/3/4/8/9/10/24/40/56/72/88/104/120/137/152/233/249. Existing local profiles
retain their exact sets below. No indexed layout, conversion, new tape mode or
inspector profile is added. The broader catalog remains incomplete.

| Wire identity | C++ full identity | Physical read | Canonical `raw` |
| --- | --- | --- | --- |
| 0 VOID | `VOID_FIELD` | Zero bytes | 0 |
| 1 BYTE | `BYTE` | One signed byte | -128..127 |
| 3 SHORT | `SHORT` | Two big-endian bytes | -32768..32767 |
| 4 INT | `INT` | Four big-endian bytes | INT32_MIN..INT32_MAX |
| 24 DECIMAL | `DECIMAL` | Compact long | Signed low 32 raw bits |
| 104 LONG | `LONG` | Compact long | All signed 64 bits |
| 152 TIME_MILLIS | `TIME_MILLIS` | Compact long | All signed 64 raw milliseconds |

`SerialType::VOID_FIELD` and `SerialFieldType::VOID_FIELD` avoid the Windows `VOID`
macro; the numeric wire identity remains zero. All scalar `units` are disengaged.
DECIMAL performs no numeric interpretation; LONG/TIME_MILLIS retain high bits just
as existing WIDE_DECIMAL does. Shared serialization bits never authorize aliases,
reserved168, unknown high bits, negative IDs or unsupported full combinations.

An explicitly declared local VOID is a dense local raw-zero default, including
when absent from the source description. A source binding exists only for an
explicit wire declaration. VOID consumes no value bytes and still charges one
wire-field attempt; skips and repeated bindings obey the same rule. Production
Java elides VOID from descriptor read/write actions. Its independent VoidIntField
cursor slot remains zero; this does not establish a delivered Java logical field
or FileWriter emission. LONG/TIME_MILLIS/WIDE_DECIMAL physical tails are never
manufactured as source declarations or logical values.

The same charged scalar decoder serves complete STREAM and RAW payloads. Fixed
width truncation consumes all available prefix bytes and reports TRUNCATED_PAYLOAD
at FIELD with the primitive-start offset; no partial current record is published.
Compact forms consume their complete physical width before any low32 projection.
Scalars allocate no value payload and consume no string budgets. Existing payload,
record, field, symbol and output limits remain finite, including zero; defaults
are admitted before construction. Completed records remain owned after input,
schema, context or reader destruction and are retained on a later field failure.

A caller-supplied new schema can use existing LEGACY/NONE/STREAM or
RAW_MESSAGE_V1/MESSAGE container grammars. Their descriptor, message-family,
flags, timing and EOF policies remain unchanged; ordinary automatic discovery
still uses EXTENDED_FIELDS_V1. `OutgoingLayouts::create` refuses every
BINARY_FIELDS_V1 schema with typed UNSUPPORTED at `profile`, including empty and
old-type-only layouts, before traversal, copying or allocation. No outgoing codec
is added. [Scalar validation](../../tests/conformance/protocol/field-types/SCALAR-VALIDATION.md)
separates source facts, requested composer frames, FileWriter captures and actual
parser/FileReader callbacks at both pinned revisions.

## Byte, UTFString and opaque-object values

Full identities9 BYTE_ARRAY and233 CUSTOM_OBJECT read a compact LONG length,
validated in -1..INT32_MAX before narrowing. Full249 SERIAL_OBJECT instead uses
the production Marshaller compact INT length: consume the complete compact
encoding, project signed low32, then validate -1..INT32_MAX. All three preserve
nullable owned body bytes, without Java object execution, deserialization, class
loading or content validation. Null and engaged-empty bytes remain distinct.

The [portable attachment contract](../events/ATTACHMENTS.md) preserves this representation
for future standard Message/Configuration APIs. Explicit native codec execution is separate;
opaque preservation alone does not implement Java Object access or a codec registry.

Full137 STRING reads UTFString's compact LONG byte length (-1..4*INT32_MAX), then
exact UTF-16 units. This differs from10's character count. Supplementary scalars
become two units; accepted overlong UTF and CESU/unpaired surrogates are preserved.
Invalid sequences are rejected after the complete attempted tail, including an
attempt crossing the declared byte count, bounded by actual message and work
allowances. Examined-byte charges use actual examination, not declared length.

These exact full-ID grammars are independent of local mapping, including skipped
fields and unknown records: discarded249 still uses projected INT, and discarded137
still validates UTF. Java BinaryRecordDesc selects its reader by local field class;
an unknown object field instead uses byte-array LONG and bypasses UTF validation.
Those Java unknown-field behaviors are deliberately different from this generic
canonical projection. Known MarshalledObjField and StringField(true) observations
establish the selected249/137 grammars; this is not Java local conversion/skip or
full DataScheme compatibility. No existing profile's skip behavior changes.

`FieldValue` appends `optional<vector<byte>> bytes{}` after its existing members.
For9/233/249 raw is0, units are absent, and bytes hold null/empty/content. For137
and10 raw is0, units hold null/empty/content, and bytes are absent. Every scalar
requires both payload members absent. Equality includes type, raw, units and bytes.
Copies own independent storage; moved/retained values survive input, schema,
context and reader destruction. Old two/three-member aggregates keep source
meaning; sizeof/ABI and structured-binding arity are not preserved promises.

## Identity, storage and binding

`LocalProfile::EXTENDED_FIELDS_V1` accepts exactly full identities 2/8/10/40/56/72/88/120.
COMPACT_INT_V1 remains exactly 8; QUOTE_FIELDS_V1 remains exactly 2/8/56/120. Serialization bits
never authorize another full identity. Only ordinary `hasTime=false` local records are supported.
Logical field positions are independent of Java int/object indices and physical VOID tails.
Binding retains ordered repeated assignments, skips supported unknown fields/records, rejects
known-field full-type mismatches, and owns its plans independently of names and schemas.
Java's broader conversions and recovery do not expand this policy.

`FieldValue` retains `optional<u16string> units` after `type` and `raw`. Type 10 requires raw zero:
disengaged units means null, engaged empty means empty, and all other units remain exact, including
NUL and unpaired surrogates. Scalar tags require disengaged units. Types 8/40/56/72/88 require
sign-extended int32 raw values, type 2 permits 0..0x10ffff, and type 120 preserves all signed64 bits.
Unknown tags and noncanonical manually constructed values are invalid. Equality compares type,
raw and both nullable payload members. The public value validator and Quote mapper enforce these invariants.
Existing scalar aggregate expressions remain valid. Fresh missing fields are tagged raw zero/null;
later matching assignments replace earlier values, including null/empty replacements.

Types 40 (SHORT_STRING), 72 (SEQUENCE) and 88 (DATE) use compact-long reads followed by signed
low-32-bit projection. Redundant/wider encodings remain accepted. No ShortString text conversion,
calendar validation or date-unit conversion is provided. Field SEQUENCE is independent of the
heartbeat's `TapeTiming::eventTimeSequence`. Existing scalar contracts remain in [QUOTE-VALUES](QUOTE-VALUES.md).

### Serialization and representation components

The full numeric identity is the bitwise OR of a base serialization type and one representation
selector. The C++ `SerialType` and `FieldRepresentation` enums name the implemented components;
`SerialFieldType` retains the existing full identities, now defined through `composeSerialTypeId`.
For example, `WIDE_DECIMAL` is `COMPACT_INT | WIDE_DECIMAL` (`8 | 0x70 = 120`), not a new base encoding.
The selectors are encoded alternatives, not flags that may be combined with each other.

`REPRESENTATION_MASK = 0xf0` and `SERIAL_TYPE_MASK = ~REPRESENTATION_MASK` follow
[locked SerialFieldType.Bits](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L330).
`serialTypeBits` and `representationBits` return raw numeric components without validation.
In particular, the serialization mask preserves unknown high bits; it is not `0x0f`.
Composition neither validates a pairing nor authorizes decoding. Unknown representation selectors
remain numeric, and unsupported full identities still fail the unchanged exact-ID profile gate.
A shared serialization base does not imply equal logical width: WIDE_DECIMAL retains all 64 bits.

The enums are intentionally incomplete. This readability refactor adds no supported identity,
conversion, wire behavior or fixture expectation. Schema construction and binding continue to use
the single `supportsSerialType` policy; tests retain independent numeric expectations.
See [refactor verification](../../tests/conformance/protocol/field-types/TYPE-MODEL-REFACTOR.md).

## Character-array primitive and message boundary

Type 10 is compact-long character count, then that many UTFChar-encoded BMP values. Legal counts
are -1..INT32_MAX; -1 means null. The canonical Java writer counts UTF-16 units and writes each
surrogate separately. The reader accepts observed overlong forms, including four-byte encodings
of BMP units. A supplementary scalar is rejected after its full encoding; a CESU surrogate pair
is two accepted units. Read a full indicated UTFChar tail before validating its continuation bits.
There is no normalization, replacement or UTFString byte-count framing.

The bounded reader exposes `CharArrayBoundary::PRIMITIVE` (default) and `MESSAGE`. PRIMITIVE
matches IOUtil/StringField over ByteArrayInput or DataInput; incomplete content consumes examined
bytes. MESSAGE additionally applies the production BinaryQTPParser's BufferedInputPart check:
after the prefix a positive count greater than all remaining message bytes is INVALID_LENGTH,
without reading body bytes. Payload decoding always selects MESSAGE. These surfaces differ for
malformed counts; a direct primitive observation alone does not prove production error positions.
Neither surface clips the body to the character count in bytes: a following field is left at the
position reached after exactly the declared number of characters.

`readCharArray` borrows a span, returns an owned optional UTF-16 value, consumed bytes, charged units
and explicit status, and retains no continuation state. Errors publish no partial string.
Allocation/length errors become ALLOCATION_FAILURE with the last examined position and charges.

## Finite limits, charging and precedence

`CharArrayLimits` bounds total encoded bytes (including the prefix) and code units per value.
New trailing zero-initialized `StreamDataLimits` members are `fieldString`, `fieldBytes`,
`fieldUnits`, `ownedFieldUnits`. They respectively bound one string, cumulative examined string
bytes, cumulative reserved decoding units, and owned string units. Zero is zero allowance: zero
encoded bytes disallows even a null prefix; zero units allows null and empty. Scalar fields do
not consume these string budgets. Symbols and descriptor/name text keep their separate budgets.

| Checkpoint | Check/charge | Failure and precedence |
| --- | --- | --- |
| Before field | Existing wireFields attempt | Unchanged FIELD error; rejected attempt is not charged. |
| Prefix/body examination | Read at most min(per-value encodedBytes, remaining fieldBytes) | Charge each consumed byte even on failure. Incomplete at an artificial byte cutoff is RESOURCE_LIMIT; actual input exhaustion is INSUFFICIENT_INPUT. |
| Complete count | Validate -1..INT32_MAX, then MESSAGE remaining-byte check | INVALID_LENGTH before unit/resource checks. Null succeeds with zero units. |
| Before any proportional allocation | Check count against min(per-value codeUnits, remaining fieldUnits, available owned units) | RESOURCE_LIMIT after prefix; rejected unit reservation charges no units. |
| Admitted count | Reserve the entire count as decoding work | chargedUnits remains charged on malformed suffix, allocation failure, skip or replacement; no refunds. |
| Each character | Full tail, continuation validation, BMP check, then append | No partial returned value; bytes already examined stay charged. |
| Assignment | Move decoded value; then release replaced units from current record | Temporary replacement coexists with old value and must fit before decoding. |
| Complete record | Publish atomically, transfer current owned count to completed output | Failed current record is discarded; earlier complete records remain owned. |

Available owned units are the remaining aggregate output allowance minus current-record units.
This bounds completed output plus current record plus one temporary string, including skipped
strings. Replacements/skips release temporary storage but never decoding charges. The bound counts
logical UTF-16 elements, not container capacity, allocator metadata or exact heap bytes.
`StreamDataResult` exposes `fieldBytes`, `fieldUnits` and `ownedFieldUnits`; the last counts only
completed output. Tape carries the two work charges and completed owned units across every message,
including failing attempts, and exposes cumulative counters. It never derives work from final values.
Byte accounting uses subtraction before addition; no untrusted count causes an unchecked reserve.

The existing record/output/flag precedence and primitive-start errorOffset remain unchanged.
Both STREAM and RAW share this field decoder. Append-only description updates, owned completed
prefixes, terminal errors and atomic heartbeat updates follow [TAPE-READER](../protocol/TAPE-READER.md)
and [RAW-TAPE](../protocol/RAW-TAPE.md). A failed string publishes neither a partial record nor timing changes.

### New payload budgets and failure checkpoints

New trailing zero-default `StreamDataLimits` members apply only to9/137/233/249:
`fieldPayloadEncodedBytes` bounds examined prefix/body bytes per value,
`fieldPayloadBytes` bounds admitted declared body bytes per value,
`payloadEncodedBytes` bounds aggregate examination, `payloadWorkBytes` bounds
aggregate admitted body work, and `ownedPayloadBytes` bounds completed+current+
temporary logical payload storage. Zero never means unlimited. Type10 keeps its
existing byte budget and is not charged against these new zero-default bounds.

Length-domain and complete-message remaining-byte checks precede body admission.
A declared body beyond the complete message is INVALID_FIELD after its prefix;
an incomplete compact prefix or UTF tail at the actual message end is
TRUNCATED_PAYLOAD. An artificial encoded-byte cutoff is RESOURCE_LIMIT. The
FIELD errorOffset is the primitive start and consumed includes actual examined
bytes. A malformed UTF sequence may examine beyond its declared byte count before
being rejected: `01 c2 80` examines three bytes, with no returned value. This
examination can include following-field bytes; it never crosses the message or
finite encoded allowance. A normal complete value leaves its sentinel untouched.

Before proportional allocation, admit body work, then string-unit work and peak
ownership. STRING reserves U=min(bodyBytes,INT32_MAX) UTF-16 units, charges the
existing `fieldUnits` work counter, and needs both U available owned string units
and checked2*U temporary payload bytes. Its retained logical storage is twice the
actual decoded unit count. Byte/object storage uses its body byte count. Type10
and137 share existing string-unit work/ownership, preserving those counters'
meanings; `fieldBytes` keeps type10's existing byte accounting.

For a body larger than INT32_MAX, decoded units can exceed the capped reservation.
Before either a one-unit or two-unit append, check the entire decoded point
against remaining admitted units by subtraction. Excess returns RESOURCE_LIMIT
after the attempted tail's actual examination, preserving admitted body/unit work
and publishing no current value. No append or completed ownership subtraction
can cross the reservation; boundary arithmetic is checked without giant allocations.

Replacement must fit while the old target and the new temporary coexist. Skips
need temporary allowance too. Failed, skipped and replaced values never refund
examined bytes, admitted body work or admitted string units. Storage release only
releases current/temporary live ownership. These are logical element/storage
budgets, excluding allocator capacity/metadata and RSS; they do not promise exact
physical heap limits. Completed output remains charged across TapeReader frames
even after the caller releases returned records.

Results expose `payloadEncodedBytes`, `payloadWorkBytes` and completed-only
`ownedPayloadBytes`; TapeReader accumulates all three, including terminal failing
frames. Replay diagnostics copy them without widening admission or producer
support. Allocation/length exceptions in payload construction are translated to
ALLOCATION_FAILURE with examination/admitted work preserved. A failed current
record is discarded while previously completed records stay owned. Ordinary
caller copies of owned values can still throw standard allocation exceptions.

## Explicit diagnostic schema and output version 4

The tool-only `oracle-field-types-v1` adapter supplies record ID 0, name `FieldTypes`, hasTime=false,
with the following logical fields and exact lookup names. No schema is inferred from the tape.

| Position | Name | Full type | Java slot |
| --- | --- | --- | --- |
| 0 | Text | 10 | object 0 |
| 1 | Short | 40 | int 0 |
| 2 | Sequence | 72 | int 1 |
| 3 | Date | 88 | int 2 |
| 4 | Count | 8 | int 3 |
| 5 | Char | 2 | int 4 |
| 6 | Seconds | 56 | int 5 |
| 7 | Wide | 120 | int 6, physical VOID 7 |
| 8 | Tail | 10 | object 1 |

This profile supports both existing container modes and uses output version 4 in either mode.
TSV header is `format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\t<mode>\n`.
Frame/descriptor/heartbeat/timing/markers/complete rows retain their existing grammar for that mode.
Data rows are `field-record`, frame index, emitted index, local ID, name hex, symbol hex, values,
with tabs between the seven cells. Values are comma-separated `type:raw:payload` in logical order.
Scalar payload is `-`; type-10 payload is `null`, `empty`, or lowercase four-digit UTF-16 units.
Null and empty both have raw 0 and are distinct. Decimal numbers are canonical signed integers;
name/symbol empty sentinels retain the existing `-` grammar. Output is LF, no BOM.

Human header is `qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=<mode> identities=local`.
Each data line is `Fields <index>: frame=<frame> local=<id> name=<quoted> symbol=<quoted> fields={...}`.
Entries are `field[i]=(type:T raw:R)` for scalars, or `field[i]=(type:10 raw:0 units:null|<quoted>)`.
Quoted values use the existing lossless safe UTF-8/escape rules, including escaping controls and lone
surrogates. No event mapper or business/calendar/text interpretation is called.

Existing generic formatter versions4/6/7 explicitly throw invalid_argument for
full payload identities9/137/233/249, including null/empty values. They never emit
raw0 while dropping bytes or UTFString identity. Broader inspector selection and
a payload-capable output grammar are deferred; the existing CLI profiles remain
unchanged.

Every complete row is counted against the remaining encoded output budget before allocation and
before its first write. Short writes or detected sink failures return nonzero and emit no subsequent
completion. Final flush may fail after a completion row has reached the sink: only exit 0 establishes
successful output. Existing profiles/output/diagnostic versions remain unchanged except help text.

## Standard-record evidence and limits

The unchanged default Profile has 16 logical fields: ten WIDE_DECIMAL values, ExdDiv.Date (88),
Halt.StartTime/Halt.EndTime (56), Flags (8), Description/StatusReason (10). Its 24 Java int slots include
ten physical VOID tails; two object slots are separate. The library tests supply an explicit local
projection retaining every logical field and observed property-name lookup. Java FileWriter/FileReader
values, original descriptions and raw timing are evidence of field delivery, not Profile event mapping.
SHORT_STRING/SEQUENCE witnesses remain synthetic here; broader standard schemas may require excluded
LONG/indexed semantics. No importer, indexed schema, compression, flags, additional type or transport
is authorized. The private tape is neither read nor an acceptance input.
