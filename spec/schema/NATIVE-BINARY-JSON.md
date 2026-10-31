# Native binary schema JSON, version 1

[Русская версия](NATIVE-BINARY-JSON.ru.md). English is authoritative.

This separately selected format supplies owned `LocalProfile::BINARY_FIELDS_V1` declarations with
explicit indexed/time layouts. Include `qd/schema/json_schema.hpp` and call
`org::ttldtor::qdcxx::schema::loadBinaryJsonSchema(input, limits)` from optional `qdcxx_schema_json`.
The [ordinary native JSON](NATIVE-JSON.md), `loadJsonSchema`, `JsonSchemaLimits`, error ordinals and
version-2 rejection remain unchanged. Neither entry auto-detects or migrates profiles.

## Document grammar

Input is exactly one UTF-8 JSON document with optional surrounding JSON whitespace and no BOM.
Empty input, malformed UTF-8/escapes, unpaired surrogate escapes, comments, trailing commas and
trailing documents yield `MALFORMED_JSON`. All members below are required. Objects reject unknown
and duplicate decoded member names, including differently escaped spellings of one key. Member
order is irrelevant; array order is preserved.

| Object | Exact members |
| --- | --- |
| Root | `formatVersion`: integer; `profile`: object; `records`: array |
| Profile | `id`: string; `version`: integer |
| Record | `id`: integer; `name`: string; `hasTime`: boolean; `fields`: array; `lookups`: array; `indexedTimeLayout`: null or object |
| Field | `serialTypeId`: integer |
| Lookup | `name`: string; `fieldId`: integer |
| Pair layout | `kind`: `"pair-int32"`; `highFieldId`: integer; `lowFieldId`: integer |
| Single layout | `kind`: `"single-int64"`; `fieldId`: integer |

`formatVersion` is `1`; profile is exactly `{"id":"qdcxx-native-binary","version":1}`.
Other well-typed versions yield `UNSUPPORTED_VERSION`; other IDs yield `UNSUPPORTED_PROFILE`.
Identity precedes record interpretation. Numeric members require integer JSON spelling in
`0..2147483647`; `-0` is zero. Fractions, exponent spellings, wrong kinds and out-of-range parsed
values yield `INVALID_SCHEMA`. Malformed numbers or parser numeric overflow yield `MALFORMED_JSON`.

Record IDs equal their zero-based positions, independently of wire sender IDs. Record names are
nonempty and unique; lookup names are nonempty and unique within their record. Names become owned
exact UTF-16 scalar text without normalization, trimming, case folding or alias generation. Escaped
NUL and supplementary scalars are retained. Fields have implicit zero-based record-local logical
IDs. Every field needs at least one lookup; each lookup targets an existing field. Distinct names
may target one field. Empty schemas and empty-field records are valid under these invariants.
Core `LocalSchema::create` remains authoritative and receives the original count/name limits.

Accepted full field IDs are **0/1/2/3/4/8/9/10/24/40/56/72/88/104/120/137/152/233/249** from
[FIELD-TYPES](../encoding/FIELD-TYPES.md). No bit mask widens this set. JSON declares identities,
not semantic field values. Existing codecs retain exact nullable UTF-16, empty/null byte and opaque
payload semantics. No Java slots, synthetic VOID tails, units, conversion or event mapping is inferred.

## Explicit indexed/time layout

| `hasTime` | `indexedTimeLayout` | Contributor full IDs |
| --- | --- | --- |
| `false` | `null` | No contributors |
| `true` | Pair object | Each of **4/8/24/40/56/72/88** |
| `true` | Single object | One of **104/120/152** |

Pair IDs must be distinct and refer to logical fields. `highFieldId` maps to the core layout's
first/high contributor and `lowFieldId` to its second/low contributor, independently of wire order,
names or Java slots. A single contributor supplies the signed 64-bit layout. Partial, duplicate,
dangling or mismatched declarations yield `INVALID_SCHEMA`. Unknown layout kinds and supported
types in unsupported contributor roles yield `UNSUPPORTED_INDEXED`; full field identities outside
the catalog yield `UNSUPPORTED_TYPE`. No layout is inferred from names.

The [pair](examples/native-binary/pair.json), [single](examples/native-binary/single.json) and
[ordinary](examples/native-binary/ordinary.json) files are complete JSON declaration examples. Even an empty or
ordinary successful document uses `BINARY_FIELDS_V1` through this entry.

The pair example is an explicit local projection of `TimeAndSale` selecting only `Time`, `Sequence` and `Price`. Qualification of a complete standard event schema and its adapter is a separate scope.

## Ownership and failure

Input is borrowed unchanged only during the synchronous call and needs no padding. No input, DOM or
parser storage escapes. Success returns a complete owned immutable `LocalSchema` with independent
provenance. Copies share the established identity; independent loads create foreign identities even
for identical input. Failure publishes no partial result and leaves caller-owned schemas/identity
unchanged. Retained contexts and downstream copies follow their existing core contracts.

Stable tokens remain `MALFORMED_JSON`, `INVALID_SCHEMA`, `UNSUPPORTED_PROFILE`, `UNSUPPORTED_VERSION`,
`UNSUPPORTED_TYPE`, `UNSUPPORTED_INDEXED`, `RESOURCE_LIMIT` and `ALLOCATION_FAILURE`. Diagnostics
contain no input/path echo or parser-owned message. Independent defects have no general first-error
order; resource/parsing failures can prevent structural checks.

## Independent finite limits

`BinaryJsonSchemaLimits::loading` retains the [ordinary defaults](NATIVE-JSON.md#finite-loading-limits):
1,048,576 input bytes, depth 16, 1,024 records, 16,384 fields, 32,768 lookups, 1,024 UTF-16 units per
name and 1,048,576 total name units. Three additional limits intersect independently:

| API field | CLI option | Default | Scope |
| --- | --- | --- | --- |
| `parserBytes` | `--max-schema-parser-bytes` | 67,108,864 bytes | Pinned fresh-parser ordinary allocation requests |
| `workUnits` | `--max-schema-work` | 67,108,864 units | Logical byte/visit/comparison credits |
| `schemaModelBytes` | `--max-schema-model-bytes` | 67,108,864 bytes | Logical two-copy declarations and controls |

Zero is literal; equality is admitted. Exceeding a bound or checked arithmetic yields `RESOURCE_LIMIT`.
A document fitting old counts can exceed a new bound. Actual input/counts drive admission even with
huge configured maxima. Failures do not retry with larger budgets.

For actual input bytes `B`, `C=max(B,32)`, `D=min(loading.depth,B)+1` and checked upward `round64`:

```text
parserRequests = (B+64) + 4*(round64(C)+9) + 8*round64(C+3)
    + round64(floor(5*C/3)+64) + 9*D + H
H = sizeof(simdjson::internal::dom_parser_implementation)
    + 6*sizeof(void*) + alignof(max_align_t)
```

Every intermediate, including `5*C` and rounding headroom, is checked before SDK allocation. The
four-byte structural indexes, eight-byte tape slots and nine depth bytes follow the pinned source.
All nine built-in implementations share the variable expressions and five-word derived state; `H`
adds a conservative word/alignment allowance. One fresh parser allocation and one parse include the
first DOM and padded-input allocations without capacity-growth retry. Allocator metadata, caller
input and unrelated process allocations are outside this request scope.

For `R` records, `F` fields, `K` lookups, total record/lookup name units `N`, longest name `U`, and
per-record `Fi/Ki`, the complete preflight uses:

```text
T = R*sizeof(LocalRecord) + F*sizeof(LocalField) + K*sizeof(FieldLookup)
    + sizeof(char16_t)*(N+R+K)
fixed = 2*sizeof(vector<LocalRecord>) + sizeof(LocalSchema)
    + 2*sizeof(LocalRecord) + 2*sizeof(u16string) + 32*sizeof(void*)
schemaModel = fixed + 2*T
work = 32*B + 8*(R+F+K+N+1)
    + (U+1)*(R*(R-1)/2 + sum(Ki*(Ki-1)/2)) + sum(Fi*Ki)
```

The fixed term includes controls, temporaries and logical provenance allowance. Two copies cover
staging and the core's copied declarations; names include terminators. Count/name/model preflight
precedes proportional declaration allocations. `schemaModelBytes` is a logical model, not actual
STL capacities, total allocation requests, resident memory or an exact heap cap. Postallocation
`storageBytes()`/capacity observations cannot authorize admission retroactively. Parser, staging and
result overlap until return. Caller input, retained old schemas and downstream session/collector
copies remain separate owners; they cannot be subtracted to fit a limit. The sum is not exact heap use.

The initial `32*B` charge precedes depth scan/parser/counting, including rejected input. It allows
one depth scan, one input copy, four parser/UTF-8 visits, sixteen grammar/array/identity visits, six
name-count/conversion visits and four control/failed-prefix visits per byte. Eight linear credits
cover declaration validation/copy/cleanup; name pairs cost `U+1`, and coverage charges every `Fi*Ki`
iteration. Triangular arithmetic halves before multiplication. These are logical credits, not bytes,
CPU instructions or elapsed time; no throughput/latency guarantee follows.

## Dependency qualification and use

JSON-enabled CMake checks complete simdjson 4.6.4 amalgamation SHA-256 before compilation:

| File | SHA-256 |
| --- | --- |
| `singleheader/simdjson.h` | `fec0cb4559e25456789efb153b7e5eaa27b50ce2bc2c9d3b531ea2000928380a` |
| `singleheader/simdjson.cpp` | `826660ed60cf467641ab2f93a0e1a2dacf603c444f76d835358c814bc2caba3a` |

Qualification covers built-in SDK implementations and ordinary SDK selection, whose first selection
may read `SIMDJSON_FORCE_IMPLEMENTATION`. The loader performs no application schema/property/source
resolution, I/O or SDK-global mutation. Foreign custom active-implementation replacement and concurrent
foreign SDK-global mutation are outside this resource contract. Toolchain/backend changes need renewed
request/capacity checks; observed sizes/requests are separate from the source envelope.

In the qualified x64 builds, the base size is 56 bytes and enabled backend sizes are 96 bytes;
the fixed allowance is 112 bytes on MSVC 19.42 and 120 on GCC 13.3. A four-byte input observation
requested/peaked at 1,141 ordinary parser payload bytes, below envelopes 1,157/1,165 respectively.
These samples exclude observer headers, aligned/unintercepted and process allocations. They
corroborate those builds; they do not substitute for the nine-backend source bound or prove total heap.

[Build/package instructions](../../docs/CPP-BUILD.md#installed-binary-json-loader) describe the
conditional export and private dependency. JSON OFF does not find, fetch, link, inspect SDK source
or add optional dependency cache entries. Exact upstream
[v4.6.4 source](https://github.com/simdjson/simdjson/tree/1bcf71bd85059ab6574ea1159de9298dcc1212c5),
[LICENSE](../../licenses/simdjson-4.6.4/LICENSE) and
[LICENSE-MIT](../../licenses/simdjson-4.6.4/LICENSE-MIT) retain original legal attribution.

The existing inspector's [configuration-validation mode](../../docs/TAPE-INSPECT.md#binary-json-configuration-validation)
adds no HISTORY tape reading, subscriptions, transport or new facade. TapeReader, formatter,
CatalogSegment and subscription contracts remain unchanged. The
[installed consumer](../../tests/conformance/schema_json_consumer/main.cpp) and
[qualification protocol](../../tests/conformance/schema-json/VALIDATION.md#binary-json-profile-qualification)
exercise existing native HISTORY composition; they establish no Java HISTORY production or full wire equivalence.
