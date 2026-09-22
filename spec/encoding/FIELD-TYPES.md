# Bounded additional exact field types

[Русская версия](FIELD-TYPES.ru.md). English is authoritative.

QDCXX-024. This contract follows the initial locked-Java primitive and production FileWriter/FileReader
captures and precedes production C++. The [oracle](../../tools/upstream/field-types-oracle/FieldTypesOracle.java)
and [capture report](../../tests/fixtures/encoding/field-types/CAPTURES.md) distinguish runtime observations,
diagnostic inputs and reproducibility checks. The lock remains unchanged. This is field support,
not a Profile event adapter or evidence of complete private-tape compatibility.

## Identity, storage and binding

`LocalProfile::EXTENDED_FIELDS_V1` accepts exactly full identities 2/8/10/40/56/72/88/120.
COMPACT_INT_V1 remains exactly 8; QUOTE_FIELDS_V1 remains exactly 2/8/56/120. Serialization bits
never authorize another full identity. Only ordinary `hasTime=false` local records are supported.
Logical field positions are independent of Java int/object indices and physical VOID tails.
Binding retains ordered repeated assignments, skips supported unknown fields/records, rejects
known-field full-type mismatches, and owns its plans independently of names and schemas.
Java's broader conversions and recovery do not expand this policy.

`FieldValue` adds `optional<u16string> units` after `type` and `raw`. Type 10 requires raw zero:
disengaged units means null, engaged empty means empty, and all other units remain exact, including
NUL and unpaired surrogates. Scalar tags require disengaged units. Types 8/40/56/72/88 require
sign-extended int32 raw values, type 2 permits 0..0x10ffff, and type 120 preserves all signed64 bits.
Unknown tags and noncanonical manually constructed values are invalid. Equality compares all three
members. The public value validator, Quote mapper and typed formatter enforce these invariants.
Existing scalar aggregate expressions remain valid. Fresh missing fields are tagged raw zero/null;
later matching assignments replace earlier values, including null/empty replacements.

Types 40 (SHORT_STRING), 72 (SEQUENCE) and 88 (DATE) use compact-long reads followed by signed
low-32-bit projection. Redundant/wider encodings remain accepted. No ShortString text conversion,
calendar validation or date-unit conversion is provided. Field SEQUENCE is independent of the
heartbeat's `TapeTiming::eventTimeSequence`. Existing scalar contracts remain in [QUOTE-VALUES](QUOTE-VALUES.md).

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
