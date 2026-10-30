# Exact field-type oracle coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The separate owned-payload extension9/137/233/249 is documented in
[object validation](../../../conformance/protocol/field-types/OBJECT-VALIDATION.md)
and the [both-pin capture manifest](object-captures.json). Its49 artifacts and803
observations per pin cover requested composer, FileWriter/FileReader, known/
unknown-field/unknown-record/repeated-target parser paths, separate FieldReader
positions and supporting primitives. The historical bounded-profile coverage
below retains its original scope and receipts; it does not itself qualify the
new profile, Java object semantics or broad binary-container behavior.

The [82 expected artifacts](expected/) contain 40 complete binary inputs, 40 production
observations, one schema inventory and one primitive table. Evidence uses locked upstream
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; see [captures](CAPTURES.md) and
[validation](VALIDATION.md). Initial measurements preceded the C++ implementation.

| Evidence | Observed boundary |
| --- | --- |
| `primitives.tsv`: 9 writes, 219 string reads, 324 integer reads | Production StringField/IOUtil and ShortStringField/SequenceField/DateField; exact bytes, units, status and consumption |
| `mixed-stream`, `mixed-raw` | Original FileWriter output replayed by FileReader, three singleton records/data frames each; separate Java RAW callback fanout |
| `profile-stream`, `profile-raw`, `schema.tsv` | Unchanged default Profile, all 24 int slots (ten VOID tails), two objects, 16 logical fields; default scheme has 331 records and no overrides |
| `composer` | Direct production BinaryQTPComposer descriptions/data and BinaryQTPParser delivery, complete input consumption |
| `missing`, `repeat-0..2`, `mixed-order` | Null object defaults, zero scalars, mixed int/object logical order, last repeated assignment including null/empty/content |
| `unknown-field`, `unknown-record` | String decoding before skipping; a following known record proves successful alignment |
| `malformed-skipped`, `malformed-unknown-record`, `oversized-skipped` | Malformed string or invalid message-boundary count causes production corruption; Java subsequently delivers the sentinel |
| `mismatch-int`, `mismatch-object` | Java converts integer representation mismatches and leaves incompatible object default null; C++ exact-match refusal is a narrower explicit policy |
| `string-wire-0..10` | Production parsing of null/empty/ASCII, overlong BMP, supplementary scalar rejection, malformed continuation, CESU surrogate pair, negative count and redundant count |
| `wide-40-*`, `wide-72-*`, `wide-88-*` | Production compact-long input projected to signed int32 and followed by known scalar and next record |

String primitive inputs cover NUL, ASCII, multibyte BMP, paired/lone surrogates and control units;
every strict prefix of each writer vector is retained. Overlong 2/3/4-byte encodings of BMP values
are accepted; a four-byte supplementary scalar is rejected after all four bytes. IOUtil reads the
entire required continuation tail before validating continuation bytes. BufferedInputPart rejects
character count greater than remaining message bytes at the count prefix, unlike ordinary
ByteArrayInput. Valid INT_MAX count is tested only through BufferedInputPart before allocation;
negative counts below -1 and counts above INT_MAX are rejected without allocating huge arrays.
No successful primitive returns a partial string, and failures retain status/consumption separately.

Full tags 40/72/88 retain signed raw int32 values even when compact input is wider or redundant.
The primitive table includes signed limits, sign/width boundaries, all strict prefixes of redundant
and wider forms, and low-32-bit projections of signed int64 extremes. No text/calendar conversion
is inferred. Field Sequence values differ deliberately from RAW heartbeat eventTimeSequence.

The Profile witness sets distinct raw fields directly on its production RecordCursor and uses the
unchanged production writer/parser/file paths. It is field-level evidence, not a ProfileDelegate
business-event round trip. Physical VOID slots are retained in the schema and omitted only from
the logical value projection. Production files retain their original descriptors, descriptions,
message grouping, timing and footer. Diagnostic wire files are separately named and manually
construct envelopes; their observations still come from the production FileReader.

Java can recover after a malformed complete message, and permits some field conversions. Those
observations do not weaken the bounded terminal/exact-match C++ policy in
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.md). Additional real witnesses for 40/72 are not
required: standard Trade includes excluded LONG and several SHORT_STRING witnesses are indexed.
No schema is weakened to claim support. Private-file completeness, compression, flags, mutable
sessions, indexed records, other full serial types and Profile/event adapters remain outside scope.

Source traceability: upstream `qd-core/.../kit/{StringField,ShortStringField,SequenceField,DateField}.java`,
`qd-core/.../qtp/BinaryRecordDesc.java`, `dxlib/.../io/{IOUtil,BufferedInputPart}.java`, and
`dxfeed-impl/.../market/MarketFactoryImpl.java`; exact revision/classpath hashes are in run metadata.
The harness source and reproduction commands are in the [oracle README](../../../../tools/upstream/field-types-oracle/README.md).

The separate [flags extension](../../../conformance/protocol/field-types/FLAGS-VALIDATION.md)
adds known STREAM/NONE event metadata under an explicit policy, while keeping this historical
ordinary corpus unchanged. It does not extend old modes, collectors, FIELD or writer formats.


The separate [finite dispatch validation](../../../conformance/protocol/field-types/DISPATCH-VALIDATION.md)
adds322 synthetic artifacts and output8/family/descriptor checks while preserving all287 earlier
files. [Dispatch provenance](dispatch-captures.json) records each original raw hash, the exact57
UID-only comparison view and independent headerless bytes. Earlier subset results above stay historical.
