# Ordinary wire-derived schema

[Русская версия](WIRE-INVENTORY.ru.md). English is authoritative.

## Explicit boundary

`TapeReader(TapeLimits, TapeMode)` explicitly selects ordinary schema discovery.
It starts with a validated empty `EXTENDED_FIELDS_V1` schema. Only exact full
types 2/8/10/40/56/72/88/120 from [FIELD-TYPES](../encoding/FIELD-TYPES.md) are
accepted. Generated records have `hasTime=false`; this is an ordinary projection,
not inference of Java time indexes, business fields, aliases or event mappings.
Container mode remains independently explicit. Existing supplied-schema readers,
[static binding](../protocol/STATIC-BINDING.md), admission and default profiles keep
their previous behavior. Discovery is never enabled by inspecting input.

## Names, identity and source order

Exact UTF-16 record names identify local records. Local IDs follow first appearance
of distinct names, independently of sparse sender IDs. Exact field names identify
logical fields within that record; first appearance fixes local field order and
full type. New names append without renumbering. Names must satisfy LocalSchema's
nonempty well-formed UTF-16 policy; there is no normalization or manufactured alias.

Distinct sender IDs may name one record with reordered fields, subsets or extra
fields. Their union retains every distinct field. Each context binding preserves
one action per original wire field, in wire order, targeting the stable local
field ID. For example, sender 90 `R(A:8,B:10)`, then sender 7
`R(B:10,C:40,A:8)` produces local record 0 with A/B/C and sender targets 0/1 and
1/2/0. Source descriptions retain original append order and exact field names/types.

Repeated field names within a single description are unsupported, even with equal
types; discovery cannot represent their source meaning unambiguously. Reusing a
record/field name with a different full type is unsupported. Representation bits
are never masked. Duplicate/redefined sender IDs retain the existing malformed
refusal. Existing raw parsing and supplied-schema repeated-target behavior do not
change. No global failure priority is promised for multiple simultaneous defects.

Empty records and empty description batches are valid. An absent source field
gets the existing raw-zero/null default in the current union shape. Earlier
returned records are not retroactively padded after fields are added. A consumer
must use source descriptions/bindings to distinguish absence from a present zero.

## Owned snapshots and failure

Each successful complete DESCRIBE_RECORDS event, including an empty batch, creates
a freshly validated LocalSchema identity and complete StreamDataContext. All
candidate source declarations, local names, plans and defaults are prepared before
publication. The actual schema/context assignment and source-list swap are
nonthrowing; no callback or external code occurs between them. Atomicity is at the
single-threaded feed boundary, not a concurrent-access guarantee.

Any parse, profile, schema, resource or allocation failure keeps the previous
schema, context and accepted source list intact. Charges and the first terminal
diagnostic may advance. A valid prefix of a rejected description batch is not
published. Input memory is borrowed only during feed; all retained names are owned.

`schema()` and `descriptions()` return borrowed immutable views of current owned
storage. Context/plan/source/schema views expire on successful description updates
or reader destruction, move or assignment. Rejected updates preserve their storage and identity.
`schema().boundedCopy(limits)` explicitly retains independent declarations with
the same provenance. Previously returned RecordContext tokens remain valid for
that old identity and never retain the reader; they do not match the next discovery
generation. Numeric local IDs remain stable. Supplied-schema mode retains its
original caller provenance across description updates.

Every decoded StreamDataRecord owns its exact nonnegative `senderId`, independent
of localId and provenance. The appended member defaults to -1 for caller-created
records without source identity; existing aggregate initializers keep their earlier
meaning. It is one fixed-size scalar per emitted record, covered by record-count
bounds, without allocation or additional field-value charges. Old output formats
ignore it. It is not a sender-to-local conversion or a permission to infer an ID.

## Finite admission and construction

All existing frame, source description/action/name, data and final SchemaLimits
remain mandatory finite bounds; zero is valid. Before ANY owning LocalRecord
materialization or count-based reserve, borrowed scans preflight the complete final
union: records, fields, lookups, per-name and aggregate UTF-16 units, key validity,
and representable nonnegative int32 local record positions. There is one generated
lookup per distinct field. LocalSchema::create's later validation does not replace
this preflight. No untrusted count-sized reserve is used.

In discovery mode `constructionUnits` additionally charges these admitted phases
before their owning allocations/copies; supplied-schema accounting is unchanged:

| Phase | Logical charge |
| --- | --- |
| Raw description parse | Entire encoded payload byte count, including failed parses |
| Copy retained source list | Per copied record: 1 + name units + field count + field-name units |
| Temporary local declarations | S = record count + record-name units + field count + lookup count + lookup-name units |
| Validated owned schema copy | S again |
| New context | Source plan count + source action count + local record count + local default-value count |

Newly parsed source names move into the candidate without a second copy charge.
Each raw record/field requires a structural byte and decoded UTF-16 units do not
exceed encoded character bytes; the full payload is therefore a conservative
bound on temporary raw elements/names. Counts use subtraction without overflow.
An unaffordable phase does not execute. Earlier admitted charges are never
refunded, including on later semantic or allocation failure. Failure is terminal.
These are logical work/storage units, not allocator capacity, cumulative vector
relocation work, CPU cycles or measured RSS. Fixed provenance tokens do not retain
historical schema declarations; the reader keeps only its current snapshot.

## Errors and evidence

Unsupported full types/type conflicts use UNSUPPORTED/TYPE at `wire-schema`;
ambiguous duplicate fields use UNSUPPORTED/LOCAL_PROFILE at `wire-schema`.
Invalid keys use MALFORMED with SchemaError detail; duplicate sender IDs use
MALFORMED at `binding` with INVALID_DESCRIPTION. Finite and allocation failures
retain RESOURCE_LIMIT and ALLOCATION_FAILURE at their construction/schema/binding
checkpoint. Existing supplied-schema diagnostics and precedence remain unchanged.

[Reader](../protocol/TAPE-READER.md), [CLI](../../docs/TAPE-INSPECT.md) and
[focused validation](../../tests/conformance/protocol/wire-inventory/VALIDATION.md)
describe use and executed boundaries. Locked production mixed-field STREAM/RAW
fixtures are compared by exact field names and complete owned values, because
wire-first local ordering can differ from an explicitly supplied schema's order.
Synthetic 48-record inventory tests are additional designed evidence, not a claim
of full Java DataScheme, arbitrary serial-type or business-event compatibility.
