# Tape inspection

[Русская версия](TAPE-INSPECT.ru.md). English is authoritative.

## Explicit ordinary wire schema and output version 7

`--input PATH --wire-schema ordinary-v1` explicitly discovers
[ordinary exact fields](../spec/schema/WIRE-INVENTORY.md) without an external
schema or business adapter. Exactly one of `--wire-schema ordinary-v1`,
`--schema PATH` and `--profile NAME` is required. Invalid combinations, missing or
duplicate values fail as usage before opening input. Diagnostics default to 2;
both existing explicit tape modes remain available. JSON loading may be disabled.
All `--max-schema-*` record/field/lookup/name limits apply to the complete wire
union before owning local construction. Schema JSON byte/depth limits are
loader-only. `--max-construction` additionally covers temporary discovery work
as defined in WIRE-INVENTORY. No ambient settings or source inference are used.

```powershell
qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-stream.qtp --wire-schema ordinary-v1
qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-raw.qtp --wire-schema ordinary-v1 --tape-mode raw-message-v1 --output-format human
```

TSV starts with `format, qdcxx-tape-inspect, 7, ordinary-v1, MODE`, with TAB
separators. Existing frame/descriptor/heartbeat/timing/marker/completion rows keep
their selected mode grammar. Version 7 adds complete schema/source snapshots
after each successful DESCRIBE_RECORDS, before later data. Generation starts at
1 and advances once per successful event, including an empty description batch.
A returned but rejected frame never advances generation or emits candidate metadata.

| Row | Following TAB-separated cells |
| --- | --- |
| schema | frameIndex, generation, localRecordCount, sourceDescriptionCount |
| schema-record | generation, localId, recordNameHex, 0 (hasTime), localFieldCount |
| schema-field | generation, localId, localFieldId, fieldNameHex, fullTypeId |
| wire-record | generation, descriptionIndex, senderId, localId, recordNameHex, wireFieldCount |
| wire-field | generation, senderId, wireFieldIndex, fieldNameHex, fullTypeId, targetLocalFieldId |
| schema-end | generation |
| wire-data | frameIndex, recordIndex, generation, senderId, localId, recordNameHex, symbolHex, typedValues |

Indices are zero-based. Schema records/fields follow first-seen local order.
Descriptions follow original append order; their fields follow exact wire order.
All names/symbols use lowercase four-hex-digit UTF-16 units; `-` means empty.
The typedValues cell contains comma-separated `fullType:raw:units` in current
local field order. Units are `-` for non-10 fields; type 10 uses `null`, `empty`
or exact UTF-16 hex. Signed decimal raw values are never business conversions.
In raw-message-v1, wire-data adds timestampMs (`-` if absent), eventTimeSequence
and eventTimeMs. These preserve exact wire timestamp versus packed/delivered time;
zero differs from absence. Existing zero-flags/timing/option policies do not expand.

Select each data row's generation/sender binding to recover source field presence
and order from its local values. For `90:R(A,B)` and `7:R(B,C,A)`, a union A/B/C
can contain identical raw-zero/null values for both records; wire-field targets
0/1 versus 1/2/0 distinguish absence and original source order. Sender IDs come
from decoded wire data, never reverse inference from a local ID. Snapshots include
exact names and full types, so no external JSON file is needed to interpret them.
This preserves logical source records, not original primitive/envelope byte encoding.

Each bounded metadata row group is counted against remaining encoded output
before allocation/write; no unbounded snapshot history or output buffer accumulates.
A short physical write can leave an incomplete snapshot. `schema-end` closes only
that snapshot; complete reading still requires final completion and exit 0 after
flush. Parser failures retain earlier complete snapshots/data and emit no later
completion. Human output carries the same metadata with safe quoted names; numeric
metadata rows keep their row tags and use spaces. Ordinary `wire-data` is shown as
`Fields` with generation/sender and exact typed fields, not a mapped business event.

No-argument and standalone `--help` remain identical, stdout-only and free of
input I/O. Existing profiles and external-schema output version 6 remain unchanged.
The stream helper `inspectWire(input, output, diagnostic, limits, format, diagnosticsVersion,
mode)` shares the synchronous inspection loop and returns existing native exit codes.
[Focused validation](../tests/conformance/protocol/wire-inventory/VALIDATION.md)
separates designed cases, production fixtures and integration limits.

## External native JSON schemas and output version 6

Use `--schema PATH` instead of `--profile BUILTIN` to supply an owned local schema without rebuilding.
Exactly one schema source and `--input PATH` are required. The
[native version-1 contract](../spec/schema/NATIVE-JSON.md) accepts `qdcxx-native` version 1,
ordinary records with `hasTime:false`, exact full types 2/8/10/40/56/72/88/120, ordered local fields
and explicit lookup keys. Local IDs are consecutive array positions, independent of sender IDs.
Custom records, reordered fields and extra fields are retained. Names are exact Unicode scalar text
converted to owned UTF-16; no aliases, property names or Java slots are inferred.

Build with the [optional JSON loader](CPP-BUILD.md#native-json-schema-loader). From the repository
root, these commands use the reusable native schema with unchanged Java-derived mixed tapes:

```text
.work/schema-json-msvc/Release/qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-stream.qtp --schema spec/schema/examples/native/field-types.json --output-format human
.work/schema-json-msvc/Release/qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-raw.qtp --schema spec/schema/examples/native/field-types.json --tape-mode raw-message-v1 --output-format tsv
```

For a single-configuration build, use `.work/schema-json-linux/qdcxx-tape-inspect` instead.
The JSON file does not select or expand the tape mode: `legacy` remains the default and
`raw-message-v1` must be explicit. Both external modes emit output version **6**, profile
`external-schema-v1`, with generic typed fields in local order. Type 10 preserves null, empty and
exact UTF-16 values; scalar values retain their exact type/raw representation. Extra fields stay
visible, and unknown wire records retain the existing skip policy. Names such as Quote or Profile
do not request event projection; applications use the [prepared APIs](CPP-BUILD.md#native-json-schema-loader).
The [contract](../spec/schema/NATIVE-JSON.md#external-output-identity-and-rows) defines TSV/human rows.

The schema is read once, with finite bounds enforced during reading and loading. Defaults match
the library; zero is a real limit and equality is accepted. These limits are separate from existing
decoded-record/field, string, input-tape and output budgets:

| Option | Default | Unit |
| --- | --- | --- |
| `--max-schema-bytes` | 1048576 | Entire schema document bytes, including whitespace |
| `--max-schema-depth` | 16 | Nested objects/arrays; root depth is 1 |
| `--max-schema-records` | 1024 | Local records |
| `--max-schema-fields` | 16384 | Total local fields |
| `--max-schema-lookups` | 32768 | Total lookup entries |
| `--max-schema-name-units` | 1024 | UTF-16 units per record/lookup name |
| `--max-schema-total-name-units` | 1048576 | Total record/lookup name UTF-16 units |

Arguments are validated first, then loader availability, then the schema file and complete schema;
the tape is opened only afterward. A bad schema therefore takes precedence over a missing tape,
with no built-in fallback. Schema failures emit no stdout header or data and one bounded stderr row:
`schema-error<TAB>1<TAB>CODE<LF>`. This representation is identical for TSV/human and diagnostic
versions 1/2. External diagnostics default to 2; explicit 1/2 is accepted. Exit codes are usage 2,
schema file I/O 3, unsupported profile/version/type/indexed semantics or disabled loader 4,
malformed JSON/schema 5, and resource/allocation failure 6. The contract lists stable codes;
subsequent tape errors retain existing diagnostics.

With `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF`, valid external requests report `LOADER_DISABLED`
before opening either file. No arguments and standalone `--help` remain identical and perform no
file I/O: stdout, empty stderr, exit 0 if output succeeds. Built-in output/diagnostic defaults remain
unchanged. XML, `qd-3.355-xml-captured`, imports, URLs, environment/Java properties, auto-detection,
indexed records, new wire types/conversions, live schema replacement and transport are unsupported.
Native examples define our source format; they do not establish broader private-tape support.

## Explicit oracle-field-types-v1 and output version 4

`--profile oracle-field-types-v1` selects the fixed diagnostic `FieldTypes` local schema:
Text(10), Short(40), Sequence(72), Date(88), Count(8), Char(2), Seconds(56), Wide(120), Tail(10).
It works with omitted/explicit `--tape-mode legacy` and explicit `--tape-mode raw-message-v1`.
The local policy is `EXTENDED_FIELDS_V1`; container selection remains independent. This adapter
neither discovers a schema nor maps a Profile event. [FIELD-TYPES](../spec/encoding/FIELD-TYPES.md)
defines exact values, schema lookup names, row grammar, charging and Java evidence.

Both TSV and human output use version 4 for this profile. TSV records have seven cells:
`field-record`, frame index, emitted index, local ID, name UTF-16 hex, symbol UTF-16 hex, values.
Each comma-separated value is `type:raw:payload`: scalar payload is `-`; type 10 uses
`null`, `empty` or exact lowercase four-digit UTF-16 units with raw zero.
Human output preserves the same tags/raw values and safely quotes string contents, escaping
controls and lone surrogates. No ShortString, calendar or event-time conversion is applied.
RAW frame timing remains separate from field SEQUENCE. Descriptor/frame/timing/completion rows
keep their existing mode-specific grammar. Diagnostic version defaults to 2; explicit 1/2 remains available.

Five additional unsigned decimal limits bound field strings independently of symbols and names:

| Option | Default | Meaning |
| --- | --- | --- |
| `--field-string-bytes` | 65536 | One string's examined prefix and body bytes |
| `--field-string-units` | 16384 | One string's admitted UTF-16 units |
| `--max-field-bytes` | 16777216 | File-wide examined string bytes |
| `--max-field-units` | 4194304 | File-wide admitted decoding units |
| `--max-owned-field-units` | 4194304 | Completed output plus current/temporary string units |

Zero means zero allowance. Decoding work is charged for skipped/repeated/failed values without refund;
owned units bound logical UTF-16 storage, not allocator overhead. Complete rows fit the remaining
output-byte budget before allocation and first write. Detected output failure suppresses subsequent
completion; a final-flush error can follow an already written completion row. Success requires exit 0.

```powershell
.work/field-types-msvc-024/Release/qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-stream.qtp --profile oracle-field-types-v1 --output-format human
.work/field-types-msvc-024/Release/qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/mixed-raw.qtp --profile oracle-field-types-v1 --tape-mode raw-message-v1 --output-format tsv
```

Existing profiles retain their output/diagnostic versions and behavior except the expanded tape help.
No arguments and standalone `--help` remain identical without input IO. Invalid, missing or duplicate
profile arguments are usage errors before opening input. [Coverage](../tests/conformance/protocol/field-types/COVERAGE.md)
and [author validation](../tests/conformance/protocol/field-types/VALIDATION.md) describe checks and limits;
independent review remains pending. Historical sections below describe their original increments.

## Explicit raw-message-v1 and timed output version 3

`--tape-mode raw-message-v1` selects bounded uncompressed RAW_DATA/MESSAGE independently from
`--profile oracle-sre-v1|default-quote-v1`. The mode accepts only the measured option/header subset,
zero event flags and existing exact field identities. Omitted mode or explicit `legacy` preserves
all previous output, refusals and diagnostic defaults. No mode is inferred from input.

[RAW-TAPE](../spec/protocol/RAW-TAPE.md) defines version-3 TSV/human rows: original frame boundaries,
exact descriptor options/time, every heartbeat including the final one, shared data-frame timing,
raw timestamp absence versus zero, packed cursor sequence and delivered event time. Quote business
timestamps remain independent; its displayed eventTime now reflects cursor timing in this mode.
`--max-timing-events` bounds cumulative explicit heartbeat/data metadata attempts (default 4096).
All previous limits and diagnostic versions 1/2 remain available. Human refusal advice follows the
selected mode; legacy advice is unchanged. No-argument/help equivalence and pre-IO usage validation
remain mandatory. New mode does not wait for replay time.

```powershell
.work/raw-msvc/Release/qdcxx-tape-inspect --input tests/fixtures/protocol/raw-tape/expected/connector-quote.qtp --profile default-quote-v1 --tape-mode raw-message-v1 --output-format human
```

This reads the unchanged production Java fixture. The optional private tape has
[additional field/schema gaps](REAL-TAPE-GAPS.md); successful generated fixtures do not establish
full support. The historical sections below describe prior versions and their original checks.

## Explicit default-quote-v1 and diagnostic version 2

Diagnostic names and human explanations are implemented in tools/tape-inspect/diagnostics.hpp/.cpp
(qdcxx_tape_inspect_tool). Protocol retains TapeRejection/TapeDiagnostic data and exact checkpoints,
without CLI profile names or selection advice. The presentation move preserves stdout/stderr bytes,
format versions and exit codes. Review-fix author checks
are separate from historical validation; independent re-review remains pending.

QDCXX-022 adds --profile default-quote-v1 with all eight unchanged default composite Quote fields.
It uses the same BINARY/uncompressed/NONE/EMPTY_SET/STREAM_DATA transport policy. TSV default and
--output-format human now have a separate version 2 identity for this profile: exact type:raw fields,
mapped binary 64 bits and millisecond times; human prices/sizes use decimal text, never raw integers.
[Complete row grammar, safe explanations and mapping losses](../spec/protocol/DEFAULT-QUOTE.md).

--diagnostics-version 1|2 selects legacy nine-cell or new ten-cell diagnostics. Defaults are 1 for
oracle-sre-v1 and 2 for default-quote-v1. Version 2 adds the static reason name; human mode adds a
fixed explanation while retaining exact offsets. Explicit version 2 also explains unsupported local
profiles before opening a file. All existing S/R/E output and errors remain exact without the opt-in;
the old QTP inspector is unchanged. Help lists both profiles; no-argument/--help equivalence remains.
All prior numeric limits apply, with each typed field charged once. No automatic profile detection.
The stream inspection API checks schema/display compatibility before reading or emitting output;
a mismatch returns 4 with stage profile and version 2 reason LOCAL_PROFILE. Quote display requires
one Quote record with the eight exact ordered types; explicit lookup mappings remain the caller's contract.
[Executed checks](../tests/conformance/protocol/quote/VALIDATION.md); independent review pending.

[Русская версия](TAPE-INSPECT.ru.md). English is authoritative.

The optional qdcxx-tape-inspect consumes [bounded tape](../spec/protocol/TAPE-READER.md).
Required arguments: --input PATH --profile oracle-sre-v1. No stdin, network or autodetection.
TSV is default; --output-format human uses the existing safe quoted UTF-8 presentation for records
and version text. No arguments and standalone --help are identical, with stdout/empty stderr/exit 0
on successful output. Nonempty invalid/incomplete arguments return 2 before opening any file.

Limits use unsigned decimal size_t, no duplicate options, no unlimited sentinel; zero is valid except
chunk-size. --max-input, --max-frame, --max-frames, --max-descriptions, --max-actions,
--max-name-units, --max-total-name-units, --max-records, --max-fields, --max-values,
--max-symbol-units, --symbol-units, --max-output, --max-construction, --max-properties,
--max-messages, --max-descriptor-units and --max-text-units set independent finite limits.
Help lists defaults. Frame+9 and streamsize arithmetic must fit. Local schema limits are
1024 records, 65536 fields/lookups, 1024 units/name, 1048576 aggregate units. The built-in profile
is the diagnostic S/R/E adapter, not default Quote or inferred schema. Both inspectors may be built
independently with BUILD_TESTING=OFF, without test fetch or Python discovery.

Output is LF/no BOM, canonical decimal, exact UTF-16 lowercase hex in TSV, '-' for empty sequences.
The existing frame/record row grammar and human record escaping are reused without changing the
old inspector. New header: `format<TAB>qdcxx-tape-inspect<TAB>1`; human header is
`qdcxx-tape-inspect human 1 profile=oracle-sre-v1 identities=local`.
After an accepted descriptor emit `descriptor, version-present(0|1), version-hex-or--,
time(NONE|absent), send(15|-), receive(-)`, separated by TAB. Type=tape and opt empty are guaranteed
by acceptance. Human descriptor quotes version (or shows absent), states type=tape opt=EMPTY_SET,
time=NONE|absent and send=15|-. Version is never interpreted as support.

Before final completion, emit `markers<TAB>count` (human `Markers: count=N authenticated-footer=no`).
This reports empty-envelope presence, not authenticity, physical EOF or recording completeness.
The final complete row uses the old six-cell counter grammar under the new format identity.
Successful parsing requires exit 0 after final flush; output failure may physically truncate any row.
Each row is counted against remaining encoded-byte budget before expansion/allocation/write.
Completed data prefix survives a later parser failure; no partial current decoded record is emitted.
A detected error emits no later markers/completion. Read/flush errors remain nonzero.

Exits: 0 success/help, 2 usage, 3 IO, 4 unsupported/sequence, 5 malformed, 6 resource/allocation.
Diagnostics use `incomplete, code, stage, detail, frame-start, accepted-file-bytes, payload-start,
payload-consumed, payload-error` (9 TAB-separated cells); unavailable positions are '-'.
Nested stage detail is the relevant existing enum ordinal, otherwise zero. The accepted-file position
is not substituted for payload consumption. Absolute payload examination equals payload-start plus
relative position. IO diagnostics use their operation label without inventing parser positions.
Read only bounded chunks; at exact input limit peek once to distinguish clean EOF from excess.
No continuation after failure, stripping/reordering of input or protocol parsing in presentation.

Configure with -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON; target qdcxx_tape_inspect,
filename qdcxx-tape-inspect. [Author validation](../tests/conformance/protocol/tape/VALIDATION.md)
records the completed checks; independent review remains pending.

Descriptor strings also have a fixed 4096-byte encoded-body cap in the CLI; the library exposes this
limit separately. Independent tool callers default to local-schema display metadata and field[index]
labels; the CLI explicitly selects oracle-sre-v1. On a failed decoded event, frame and completed-prefix
rows are written before reporting the parser error; a failure writing that prefix takes IO/output-budget
precedence. This does not allow processing any subsequent input.

## Record metadata that old formats cannot represent

The existing record formats, including generic versions4/6/7 and Profile output, have no raw
symbol-flags or removalTime columns. Formatter::record and Formatter::profileRecord reject nonzero
SymbolValue.eventFlags or any engaged removalTime, including zero, with invalid_argument before
building a row. An explicit zero-flags prefix remains representable: eventFlagsBytes alone never
causes refusal. Existing zero-flags/absent-removal output bytes remain unchanged. Profile's business
Flags field is independent of raw QTP symbol flags.

The automatic tape-write projection rejects engaged removalTime before constructing a writer view;
it cannot silently turn a deletion into fresh zero values. Writer input formats are unchanged.
[The event metadata contract](../spec/protocol/STREAM-DATA.md#catalog-event-metadata) defines the
explicit read-only policy. No new CLI spelling or output grammar is introduced here.



## Explicit binary discovery and output8

```sh
qdcxx-tape-inspect --input tests/fixtures/encoding/field-types/expected/dispatch/normal/writer-MESSAGE-20.qtp --wire-schema binary-catalog-v1 --tape-mode binary-catalog-v1
```

Both broad selections are required together. This is ordinary discovery of the19 exact binary field
identities, four data families and selected NONE/MESSAGE/headerless policy in
[BINARY-MESSAGES](../spec/protocol/BINARY-MESSAGES.md). It does not infer indexed layout from names,
load a broad schema through native-v1 JSON, run collectors or interpret serialized Java objects.
The existing ordinary-v1/old modes and their version4–7 outputs retain their exact behavior/refusals.

TSV begins `format<TAB>qdcxx-tape-inspect<TAB>8<TAB>binary-catalog-v1<TAB>binary-catalog-v1`.
Schema/source row grammar is the existing v7 grammar above; v8 generation advances only on an actual
successful nonempty immutable publication. Empty description batches emit their frame only and
cannot authenticate headerless completion. Equal nonempty batches create a new generation.
Frame rows retain original family/type; RAW is represented once, without Java collector fanout.

`wire-data-v8` has exactly14 cells including its label: frameIndex, recordIndex, generation,
senderId, localId, recordNameHex, symbolHex, typedValues, timestampMs, eventTimeSequence,
eventTimeMs, flags, removalTime. Name/symbol hex uses the existing lowercase UTF16 spelling.
Each comma-separated value is `fullType:raw:units:bytes`: scalars use `raw:-:-`, types10/137 use
`0:null|empty|utf16hex:-`, and types9/233/249 use `0:-:null|empty|bytehex`. Opaque bytes are lowercase
hex and never executed. Empty value list is an empty cell. Timestamp/removal absence is `-`;
engaged0 is different. The three timing columns preserve raw wire versus packed/delivered time.

`descriptor-v8` has exactly9 cells: label, frameIndex, versionPresent, versionHex, optPresent,
optHex, timePresent, timeHex, effectiveSendMask. Presence is0/1; present-empty text hex is `-`.
Send bits are `(1u<<5)|(1u<<10)|(1u<<15)|(1u<<20)` for actual declared known sends, not authorization.
HUMAN renders the same information with safe quoted text and hex, naming all accepted families and
controls. Canonical values/metadata are preserved; redundant prefix/cipher encoding bytes are not promised.

The tool formatter uses distinct `catalogRecord(record, schema, ..., timing, generation)`: expected
binary schema, local ID, provenance, exact field count/full IDs and canonical shape are checked before
row/output-budget work. The name comes from that schema. Calling the old name-only record formatter
with the broad profile fails; old formatter signatures and metadata/payload refusal stay unchanged.

| Broad-only option | Default | Meaning |
| --- | --- | --- |
| --field-payload-encoded-bytes |65536| Examined prefix/body bytes per new payload value |
| --field-payload-bytes |65536| Admitted declared body bytes per value |
| --max-payload-encoded-bytes |16777216| Aggregate examined new-payload bytes |
| --max-payload-work-bytes |16777216| Aggregate admitted body work, without refunds |
| --max-owned-payload-bytes |8388608| Completed/current/temporary logical payload bytes |
| --max-snapshot-units |16777216| Simultaneous schema/description/context/descriptor/temporary metadata |

Defaults apply only when that broad option was omitted; explicit0 stays0. Old modes refuse these
options before tape I/O. Existing independent string-unit, construction, input/frame/record/value,
timing and output limits still apply; STRING137 uses both string-unit and new payload bounds.
The library reader applies no hidden CLI preset. No-args and standalone --help are identical and
need no working files. Mismatched modes/sources and invalid options fail before input is opened.

Output bytes, including header/schema/timing/completion, are counted before expansion and write.
Exits remain0 success/help,2 usage,3 I/O,4 unsupported,5 malformed,6 resource. Success requires
physical EOF, clean reader.finish, all writes and final flush plus exit0. Already-written bytes
cannot be retracted after a failed flush. Borrowed-stream inspection does not prove owner close;
Java acceptance has a separate instrumented close proof. FIELD, sidecars, compression and source
chains remain excluded. [Validation](../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.md)
states the exact production/native/whole-file scope.