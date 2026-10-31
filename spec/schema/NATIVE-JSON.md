# Native resolved schema JSON, version 1

[Русская версия](NATIVE-JSON.ru.md). English is authoritative.

This is the designed `qdcxx-native` profile for an owned ordinary local schema. It follows
[ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.md) and the
[configuration boundary](../../docs/CONFIGURATION.md). JSON spelling is project policy, not a
Java-observed format. The existing [XML-captured contract](JSON-CONTRACT.md), examples and provenance
remain unchanged; `qd-3.355-xml-captured` is explicitly unsupported by this loader.

For the separately selected `loadBinaryJsonSchema` / `qdcxx-native-binary` profile, see
[Native binary JSON](NATIVE-BINARY-JSON.md). The ordinary profile and requirements below are unchanged.

## Document and object grammar

Input is exactly one UTF-8 JSON document, with optional JSON whitespace before and after it, and
without a byte-order mark. Empty or whitespace-only input, comments, trailing commas, trailing
documents, malformed UTF-8, invalid escapes and unpaired surrogate escapes are malformed JSON.
The root must be an object. All members below are required; there are no optional members,
extensions, inferred defaults or accepted `null` values. Member order is irrelevant; array order is
preserved. Reject unknown members and duplicate **decoded** member names at every native object
level: `name` and `na\u006de` are the same key.

| Object | Exact members and kinds |
| --- | --- |
| Root | `formatVersion`: integer; `profile`: object; `records`: array of record objects |
| Profile | `id`: string; `version`: integer |
| Record | `id`: integer; `name`: string; `hasTime`: boolean; `fields`: array of field objects; `lookups`: array of lookup objects |
| Field | `serialTypeId`: integer |
| Lookup | `name`: string; `fieldId`: integer |

`formatVersion` is `1`; `profile` is exactly `{"id":"qdcxx-native","version":1}`.
The format version governs representation; the profile version governs semantics. Other well-typed
versions or profile IDs are unsupported, never silently migrated. Read the root version/profile
identity before interpreting records. A recognized old XML-captured profile is unsupported before
rejecting its native-unknown root members; this does not validate or execute its separate grammar.

Every numeric member has an integer JSON spelling and a value in `0..2147483647`. Booleans, strings,
fractional or exponent spellings (including `1.0` and `1e0`), negative values and overflow are invalid
schema members. Integer `-0` has value zero and is accepted wherever zero is accepted. A syntactically
invalid JSON number is malformed JSON. Numbers beyond the DOM parser's representable numeric range
also yield `MALFORMED_JSON`; after successful parsing, wrong kinds/ranges yield `INVALID_SCHEMA`.
Both errors use exit 5. No numeric member is accepted through floating-point rounding.

## Local records, exact names and lookups

Record `id` equals its zero-based position in `records`; it is not a wire sender ID. Record names
are nonempty and unique within the document. Each name is exact Unicode scalar text converted to
owned UTF-16, without normalization, trimming, case folding or dot removal. Equivalent escaped and
literal strings compare equally. Supplementary characters occupy two UTF-16 units; escaped NUL is
allowed and occupies one unit. Record and lookup names follow the existing core name policy.

Only `hasTime:false` is supported. `true` requests unsupported indexed semantics; omitted, numeric
or string substitutes are invalid schema members. Fields are ordered logical values, whose implicit
record-local IDs equal their zero-based positions. No Java slots, synthetic VOID tails, aliases,
property names or field order are inferred.

`serialTypeId` is the complete identity, including representation bits. Only **2/8/10/40/56/72/88/120**
are supported, as defined by [FIELD-TYPES](../encoding/FIELD-TYPES.md). Other in-range integer IDs
are unsupported types, even if their serialization bits match. This changes neither wire encodings
nor value/default semantics.

Lookup names are nonempty and unique within the containing record, with exact UTF-16 equality.
Each `fieldId` must refer to an existing field in that record. Multiple distinct lookup names may
refer to one field. Every field requires at least one lookup. Lookup order is preserved, but cannot
resolve duplicate keys or change the target. Keys may repeat in different records. No prefixes or
aliases are manufactured; wire binding and event preparation consume only these explicit keys.

`records:[]` is a valid empty schema. An empty-field record is valid with `fields:[]` and `lookups:[]`;
its nonempty name is still charged. A lookup in an empty-field record is dangling and invalid.
Every successful load uses `LocalProfile::EXTENDED_FIELDS_V1`, including empty and all-int schemas.
Only complete validation publishes an owned immutable `LocalSchema`; neither input nor parser
lifetime escapes. Each independent load creates an independent schema identity. Existing copies,
wire plans and [prepared mappings](../events/EVENT-MAPPING.md) retain their established context rules.

## Finite loading limits

Library defaults and external CLI defaults are identical. All are logical finite limits, including
zero; equality is admitted and exceeding a limit fails. These bounds do not promise an exact heap
cap or linear end-to-end validation time. Core validation remains authoritative for local invariants.

| CLI option | Default | Unit and accounting |
| --- | --- | --- |
| `--max-schema-bytes` | 1048576 | Entire input bytes, including whitespace |
| `--max-schema-depth` | 16 | Simultaneously nested objects/arrays; root object has depth 1 |
| `--max-schema-records` | 1024 | Number of records |
| `--max-schema-fields` | 16384 | Total fields across records |
| `--max-schema-lookups` | 32768 | Total lookup entries across records |
| `--max-schema-name-units` | 1024 | UTF-16 units in one record/lookup name |
| `--max-schema-total-name-units` | 1048576 | Sum of all record/lookup name units, including repeated names in different scopes |

Scalar values do not add container depth. A populated field/lookup object has depth 5. Document byte
and depth checks bound parsing, including rejected structure; name budgets cover semantic names,
not fixed grammar keys or the profile ID. Byte bounds still bound those strings. Check counts and
name units before proportional owned allocations. Temporary duplicate/member collections must be
bounded too. Parser padding and allocation-size arithmetic must not wrap. Allocation failure yields
no partial schema. Neither skipped input nor failure authorizes an unlimited temporary buffer.

The optional `qdcxx_schema_json` target uses privately pinned simdjson DOM. Its public API exposes
project/STL types only. The library accepts supplied bytes and limits, without opening files, looking
up environment/properties, fetching URLs/includes or dereferencing provenance. Core, codecs,
protocol and event mappings remain independent of JSON and simdjson.

## Loading failures and CLI order

Library errors are typed and carry no input echo, parser-owned text or invented source offsets.
For multiple independent defects, no general first-error precedence is promised. Byte/depth and
JSON parsing failures may prevent structural checks; version/profile recognition precedes record
interpretation. No duplicate-key or unknown-member document can succeed under the native grammar.

| Stable code | Meaning | CLI exit |
| --- | --- | --- |
| `MALFORMED_JSON` | Invalid document syntax, encoding, escapes, BOM, empty input or trailing document | 5 |
| `INVALID_SCHEMA` | Wrong member set/kind/range, duplicate decoded keys or invalid local invariants | 5 |
| `UNSUPPORTED_PROFILE` | Another profile ID, including XML-captured | 4 |
| `UNSUPPORTED_VERSION` | Another format or native profile version | 4 |
| `UNSUPPORTED_TYPE` | In-range full type ID outside the exact implemented set | 4 |
| `UNSUPPORTED_INDEXED` | Boolean `hasTime:true` | 4 |
| `RESOURCE_LIMIT` | A finite loading budget or parser size/depth limit is exceeded | 6 |
| `ALLOCATION_FAILURE` | Required owned/parser storage cannot be allocated | 6 |
| `IO_ERROR` | Schema-file open/read failure; CLI only | 3 |
| `LOADER_DISABLED` | External schema requested in a loader-disabled build; CLI only | 4 |

`qdcxx-tape-inspect` requires `--input PATH` and exactly one of `--schema PATH` or
`--profile BUILTIN`. Duplicate/conflicting/missing arguments are usage errors (2). Schema limits use
the existing unsigned decimal CLI limit grammar; they do not change decoded-record/field budgets.
Validate arguments, check loader availability, open/read/load/validate the schema, then open the tape.
A bad schema therefore wins over a missing tape. The schema is read once with bounded reading even
if it grows after an initial size check. No fallback to a built-in profile occurs.

Every schema loading failure writes one bounded stderr row, in both output formats and both
diagnostic versions: `schema-error<TAB>1<TAB>CODE<LF>`, where `CODE` is from the table. No input
path/content, fabricated offset, stdout header or record is emitted. Schema errors are separate from
protocol diagnostic enums. Subsequent tape errors keep the established protocol/IO diagnostics.
External mode defaults to diagnostics version 2 and accepts explicit 1 or 2. Built-in defaults,
diagnostics and error precedence remain unchanged.

`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON` defaults ON. When OFF, configuration/build does not acquire
simdjson; built-in inspection remains available. A valid `--schema` request returns `LOADER_DISABLED`
before opening either file. No arguments and standalone `--help` remain identical: stdout, empty
stderr and exit 0 when help output succeeds, without file IO. Help-output failures remain nonzero.

## External output identity and rows

The separately selected tape mode is `legacy` by default or `raw-message-v1`; JSON never selects or
widens it. Both modes use output version **6**, profile identity **external-schema-v1**, LF and no BOM.
The TSV header is `format<TAB>qdcxx-tape-inspect<TAB>6<TAB>external-schema-v1<TAB>MODE<LF>`.
The human header is `qdcxx-tape-inspect human 6 profile=external-schema-v1 tape-mode=MODE identities=local<LF>`.

Generic data rows reuse the typed [FIELD-TYPES grammar](../encoding/FIELD-TYPES.md): seven TSV cells
`field-record`, frame index, emitted index, local ID, record-name UTF-16 hex, symbol UTF-16 hex, values.
Values follow local field order and are comma-separated `type:raw:payload` entries. Scalar payload
is `-`; type 10 has raw zero and payload `null`, `empty` or exact lowercase four-digit UTF-16 units.
An empty field list has an empty seventh cell. Empty name/symbol sequences use the existing `-`
encoding (schema names themselves cannot be empty).

Human data rows are `Fields INDEX: frame=FRAME local=ID name=QUOTED symbol=QUOTED fields={ENTRIES}`.
Entries use `field[i]=(type:T raw:R)` for scalars and
`field[i]=(type:10 raw:0 units:null|QUOTED)` for strings; empty fields produce `{}`.
Safe quoting preserves exact units, including controls and wire-value lone surrogates. No business,
calendar, price, ShortString or Quote/Profile projection is inferred from a record name.

Descriptor, frame, timing, marker and completion rows keep their existing mode-specific grammar from
[TAPE-READER](../protocol/TAPE-READER.md) and [RAW-TAPE](../protocol/RAW-TAPE.md). RAW timing stays
separate from field SEQUENCE. Generic output retains every decoded local field, including extra
fields and custom records; unknown wire records still follow the existing skip policy. Rows are
bounded before allocation/first write. Only exit 0 after final flush establishes successful output.
Existing built-in profiles retain their output bytes and versions, apart from the help extension.

## Reusable examples and verification boundary

- [empty.json](examples/native/empty.json) supplies a valid empty schema.
- [field-types.json](examples/native/field-types.json) declares the diagnostic `FieldTypes` layout for
  the unchanged [mixed STREAM](../../tests/fixtures/encoding/field-types/expected/mixed-stream.qtp) and
  [mixed RAW](../../tests/fixtures/encoding/field-types/expected/mixed-raw.qtp) tapes.
- [mixed.json](examples/native/mixed.json) retains a custom record with all eight types, reordered
  Quote fields with required prices/sizes, optional Sequence and extra text, and reordered Profile
  fields with required Description, selected optional fields and an extra ShortString. Explicit IDs
  1 and 2 select Quote/Profile preparation; omitted optional fields use existing mapping rules.

Examples define native syntax, not new Java observations. Field/event semantics remain grounded in
the linked contracts and existing oracle fixtures. Loading a recognizable event name does not itself
prove that event preparation succeeds; preparation uses explicit ID, lookups, types and context.
XML, Java-property resolution, auto-detection, new wire types/conversions, live schema replacement,
transport and private-tape support remain outside this boundary. Executed checks and independent
review status belong to the implementation validation report, not to these designed examples.
