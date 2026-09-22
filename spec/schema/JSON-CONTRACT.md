# Resolved local schema JSON contract — version 1

[Русская версия](JSON-CONTRACT.ru.md). English is authoritative.

QDCXX-008, 2026-09-21. This is the designed interchange contract implementing the accepted [ADR-0001](../../docs/decisions/ADR-0001-normalized-model-boundaries.md), including its approval clarifications. It is not Java-produced JSON. [Coverage](examples/COVERAGE.md) maps all 17 existing subcases; [validation](examples/VALIDATION.md) distinguishes document checks from implementation conformance.

## Boundary and version policy

One JSON document represents either a successful resolved local schema or a failed resolution. Three ownership boundaries do not require three formats. Core reads successful `records` directly, without opening provenance, XML or construction history. No importer, C++ storage/API, JSON library, canonical bytes/hash or whole-session persistence is defined here.

`formatVersion` is integer `1`; `profile` is exactly `{"id":"qd-3.355-xml-captured","version":1}`. Format versions describe representation; profile versions describe behavior. Reject unsupported format/profile versions and IDs before interpreting records; never silently migrate. The profile is bounded to the 17 subcases, pinned QD 3.355 commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, direct XML loading, sysprops/dxprops disabled and the referenced provider/environment evidence. It does not claim arbitrary XML support. Native construction requires a later profile; copied-main-alias behavior applies only to this XML-import profile.

Reject unknown members at every defined object level. Only the optional root `extensions` object permits arbitrary namespaced keys and data-only JSON values: readers may ignore them and must never use them to change semantics. Extensions cannot introduce required behavior. Duplicate object keys, non-JSON numbers, malformed Unicode and unpaired surrogates are invalid. UTF-8 is used; member order and whitespace have no meaning. Arrays retain order; never case-fold, trim, collapse dots or Unicode-normalize names.

All fields below are required unless explicitly optional. `null` is allowed only for slot `fieldId`, index `fieldId`, and diagnostic `serialTypeId`, with the meanings below. Missing required members are errors, not defaults. Empty arrays mean a known empty collection; `records:[]` is successful empty XML, not failure or unknown schema. Empty names/references are invalid. There are no numeric data values, timestamps, floats or 64-bit payload values in this version. IDs/ordinals are JSON integers in 0..2147483647 (booleans are not integers); versions are positive integers in that range. Full serial type IDs are in that range but this profile admits only the table below. Larger future values need a separately specified representation.

## Root and provenance

| Member | Meaning |
| --- | --- |
| `formatVersion`, `profile` | Versions and profile as above; profile has required `id` and `version` only. |
| `kind` | `local-schema` or `resolution-failure`; discriminates the required body. |
| `provenance` | Object with `inputs`, `profileEvidence`, `expectationRef`. |
| `records` | Required only for `local-schema`; ordered emitted records. Forbidden on failure. |
| `diagnostics` | Required only for `resolution-failure`; nonempty array of diagnostics in recorded attempt order. Forbidden on success. |
| `extensions` | Optional opaque object as above. |

`provenance.inputs` is an ordered array of `{ref, sha256}` objects, one per root input occurrence, including repeats. `ref` is an opaque nonempty resource reference; in these examples it is a relative file path resolved from the JSON file. `sha256` is 64 lowercase hexadecimal characters identifying the original input bytes, not a canonical schema hash. Empty inputs means no URLs were supplied. `profileEvidence` is an ordered array of exactly two references: capture metadata first (upstream, commands, loader/provider/classpath fingerprints), environment inventory second (provider order and properties). `expectationRef` identifies the unchanged TSV used to design this projection. These evidence references are mandatory traceability metadata, not instructions to fetch or execute anything; unavailable evidence does not prevent core consumption. Evidence auditing may separately report inaccessible resources.

Original inputs and their order retain disabled entities, templates, docs, aliases including explicit-main declarations, enums, tags, bitfields and custom type declarations without embedding an XML AST or operation engine. References do not promise execution of these features. For S08-main-alias the retained source/TSV explicitly marks Chosen.Name main for both records while resolved Copy uses First.Name. Consumers must use the resolved effective names, not recompute them from provenance. More detailed field origins/operation traces are deferred; original inputs and expectation retain the captured information. Profile evidence includes observed null default mappings/empty mapping lists for these artificial names; no executable Java mapping classes are serialized.

## Records, fields and Java projection

Each record is `{id, name, hasTime, fields, indexes, javaSlots, lookups}`. `id` equals its position in `records`, is local to this artifact and has no sender-ID meaning. Record `name` is an exact nonempty string, unique in the artifact; record lookup is exact matching by name. `hasTime` is a boolean. Arrays `fields` and `indexes`, object `javaSlots` and array `lookups` are always present.

Each field is `{id, semanticName, effectiveName, fullName, localName, propertyName, serialTypeId}`. `id` equals its position in the record's `fields`; references are record-local. Fields are active logical values in resolved model declaration order, before Java category partitioning/index promotion; disabled fields and synthetic VOID positions are excluded. `semanticName` is the original resolved model field name, unique within a record. `effectiveName` is the model's selected main name; `localName`, `fullName` and `propertyName` are final runtime names. For this bounded profile, localName equals effectiveName and fullName equals record.name + `.` + localName. Property names must be stored explicitly: Value can coexist with the ChosenName lookup. These equalities are profile constraints, not assumptions about future providers. Full names of all slots must be unique across the artifact.

`serialTypeId` is the complete numeric SerialFieldType identity, including representation bits. Names in this table explain the identity and are not serialized enum substitutes. Other IDs are unsupported by this local profile, without any implication for wire validity.

| ID | Identity | Local projection |
| --- | --- | --- |
| 0 | VOID | Synthetic int position only; never a logical value. |
| 8 | COMPACT_INT | One int head. |
| 9 | BYTE_ARRAY | One obj head. |
| 10 | UTF_CHAR_ARRAY | One obj head. |
| 104 | LONG | One int head followed immediately by one VOID tail. |
| 120 | WIDE_DECIMAL | One int head followed immediately by one VOID tail. |
| 1 / 3 | BYTE / SHORT | Recognized model types; failed local construction only. |

`javaSlots` has exactly `int` and `obj`, arrays in category-relative ordinal order. Each slot is `{ordinal, role, fieldId, fullName, localName, propertyName, serialTypeId}`. `ordinal` equals its array position. `role` is `head`, `tail` or `index-placeholder`. A head owns one logical field via `fieldId` and exactly repeats its three runtime names and full type ID. Each logical field has exactly one head in the correct category. A tail is int/VOID, refers to the same field as its immediately preceding LONG/WIDE_DECIMAL head and has the captured `$VoidTail` names (head full/local name plus `$VoidTail`, property equal to tail local name). Every such head has exactly one tail; other heads have none. An index-placeholder is int/VOID with `fieldId:null` (no logical value), and names record.name + `.$VoidTimeField`, `$VoidTimeField`, `$VoidTimeField`. VOID cannot appear elsewhere. These are comparison positions, not required C++ arrays or storage.

`indexes` is empty exactly when hasTime=false; otherwise it contains exactly two `{position, fieldId}` objects for positions 0 and 1. Each position refers to that int ordinal. Non-null fieldId must own its COMPACT_INT head at that position; null means that position has an index-placeholder. At least one position must be a real index. A placeholder has exactly one index owner and cannot occur outside positions 0/1. Ordinary undeclared Second/First fields remain logical fields. Missing versus disabled index declarations are different source states retained in provenance, even if their position projects to the same VOID. Object, long, duplicate and other unmeasured index shapes are unsupported/deferred.

## Scoped lookups and validation

Each lookup is `{scope, name, category, ordinal}`. `scope` is `record`, `scheme-int` or `scheme-obj`; `category` is `int` or `obj`; the target ordinal resolves in the containing record's javaSlots category. Scheme scopes describe artifact-wide lookup APIs, with category matching the scope. They are stored with their target record for compactness. Keys are unique within each record scope and across all records for each scheme scope. Missing keys return no field; there are no null targets. Lookups can target synthetic slots; they must not be reduced to logical-field aliases. Array order preserves evidence probe order but does not change lookup results.

Every slot has exact full-name and local-name record lookups and a full-name scheme lookup in its category. Additional record keys are explicit; do not manufacture prefixed aliases or collapsed names. The examples include the captured positive lookup keys; the validator also checks all captured negative probes. Completeness beyond those names and this profile is not claimed. Reject dangling references, duplicate/conflicting keys, wrong scope/category, mismatched head names/types, wrong slot ownership/order and invalid indexes. Check emitted order against generation order with disabled records removed, logical field order against active model order, and Java order against the unchanged slot rows; do not use captured model HashMap traversal for record order. Structural validation of these documents does not implement XML resolution.

## Failed construction diagnostics

Each diagnostic is `{code, stage, recognition, sourceRefs, subject, serialTypeId, evidenceRef, oracleStage}`. `sourceRefs` is an ordered subset of the input references (empty only for no inputs); `subject` is a nonempty descriptive locator, not a local ID of a nonexistent schema. `evidenceRef` is the TSV reference retaining exact Java exception class/message/origin; `oracleStage` is the captured `model.load` or `runtime.load` attempt. `recognition` is `succeeded` (model types resolved) or `not-reached` (input precondition failed). `serialTypeId:null` means the diagnostic is not about a serial type; it does not mean an unknown numeric type. No partial successful schema is published.

| Captured case | code / stage / recognition / type |
| --- | --- |
| S01-no-urls, both loader attempts | `no-source-inputs` / `input` / `not-reached` / null. Two entries retain distinct oracleStage and messages in evidence. |
| S05-disabled-both | `all-index-fields-disabled` / `local-construction` / `succeeded` / null. Distinct SchemeException evidence is preserved. |
| S15-byte / S15-short | `unsupported-local-type` / `local-construction` / `succeeded` / 1 or 3. Approved ADR deviation replaces accidental NullPointerException while preserving rejection. |

These code/stage combinations are the complete failure examples of version 1, not a universal exception taxonomy. Other failures need separate evidence and a reviewed contract extension; never recategorize them silently. BYTE/SHORT recognition does not authorize local construction and their rejection does not ban wire codes. No Java class/message equality is required for the designed diagnostic; original observations remain unchanged.

## Exclusions and checks

Peer/direction bindings, sender IDs, original requests and selected outgoing layouts are excluded from local schema identity. In particular absent, originally empty and nonempty requests selecting zero fields remain distinct protocol states ([P05](../protocol/CAPTURES.md)); none is represented by local `fields:[]`. Local slots are not a wire layout. LONG/time-index wire omission is source evidence, not a newly captured projection here.

Imports, generators, alias collisions/removal, parser/default edge cases, known-event mappings, dxFeed properties/providers, rebuild isolation, conversions and dynamic/reset/PART/session behavior remain unsupported/deferred as bounded in [NORMALIZED-MODEL](NORMALIZED-MODEL.md) and [open questions](../../docs/upstream/OPEN-QUESTIONS.md). Preserve source metadata without approximating these behaviors. Native C++ construction semantics remain open.

No formal JSON Schema or new dependency is supplied. The standard-library [checker](../../tests/conformance/check_json_contract.py) checks this example corpus against TSV and exercises selected invalid mutations; it is neither a general importer nor a production validator. The rules above define the contract; exact executed checks and limitations are in [VALIDATION](examples/VALIDATION.md).

## Separate native field policy

The native C++ [FIELD-TYPES](../encoding/FIELD-TYPES.md) profile accepts exact 2/8/10/40/56/72/88/120
for ordinary hasTime=false records supplied by the caller. Logical positions remain independent of
Java int/object slots and VOID tails. This does not widen this XML-captured JSON profile, implement
a JSON/XML loader, or infer local semantics from incoming descriptions.
