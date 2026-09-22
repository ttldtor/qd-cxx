# Bounded static incoming binding

## Explicit Quote field extension

QDCXX-022 adds opt-in LocalProfile::QUOTE_FIELDS_V1 alongside the unchanged default
COMPACT_INT_V1. [Exact tagged values](../encoding/QUOTE-VALUES.md) and
[default Quote mapping](DEFAULT-QUOTE.md) define supported full types 2/8/56/120, raw-zero defaults,
ordered repeated targets and exact-match-only binding. Each action owns its full type, including
skips; known targets with a different full type fail UNSUPPORTED_PROFILE. Existing schema/name/work
bounds and atomic plan ownership remain. The type-8 descriptions below remain the legacy profile
contract; the opt-in extension supersedes their exclusion of these three identities only.

[Русская версия](STATIC-BINDING.ru.md). English is authoritative.

QDCXX-014. This provisional API plans one QDCXX-013 raw description against an explicitly supplied,
validated immutable local schema. It does not decode data or install an incoming-ID table.

## Profile and evidence

Only ordinary exact full type 8 (COMPACT_INT) fields are supported. Time indexes, object fields,
VOID, LONG, representation conversions, metadata and field replacers are excluded. Unknown local
records and missing fields can be skipped only after every raw field passes the type-8 check.
Other in-range types produce UNSUPPORTED_PROFILE, without changing the raw parser's broader acceptance.
Malformed manually constructed raw inputs produce INVALID_DESCRIPTION instead.

[Java inputs](../../tests/fixtures/protocol/binding/inputs.tsv) use locked QD 3.355 production
BinaryQTPParser and real DefaultScheme. The additive capture contains descriptions followed by data,
delivered local IDs, ordered values and errors. Historical P03/P04 remain baseline evidence for
directional ID remapping, field order and alias/property lookup; see [CAPTURES](CAPTURES.md).
BinaryQTPParser.parseDescribeRecords and BinaryRecordDesc.field2Desc/readFields, linked in
[SOURCES](SOURCES.md), explain record-name lookup and ordered assignment. Repeated intermediate
writes are source-supported; only their final value is externally observed. Designed C++ plans
are projections, never parser callbacks. Missing local values observed as zero are Java storage
observations, not application defaults.

## Local subset and validation

The local input is a narrow in-memory projection of [JSON-CONTRACT](../schema/JSON-CONTRACT.md),
not another interchange format or importer. Local record IDs equal record positions; field IDs
equal field positions. For this ordinary int-only subset, logical field position also projects
to Java int ordinal. Full numeric type IDs are retained and checked without masking. Record names
are unique, nonempty, well-formed UTF-16. Record-scoped lookup entries explicitly map nonempty,
well-formed UTF-16 keys to field positions. Every key must be unique, even if duplicate entries
would target the same field; every target must exist. Each field must have at least one key.
Additional full/local/property/alias keys must be supplied explicitly: no dot removal, prefixing,
case folding or Unicode normalization occurs. A loader must validate the full source contract
before projecting; runtime does not require XML/JSON/provenance, semantic names or Java storage.
hasTime=true and any non-8 local type are rejected as unsupported. No synthetic/index/object
shape is representable as a supported field. Empty schemas and empty records are valid.

LocalSchema::create borrows input only during the call, validates it completely, then owns a copy.
Only const views are exposed; mutation requires a new validation. Failure publishes no schema.
The schema lives in qdcxx_core without protocol, file or transport dependencies.

## Planning and ownership

bindRecord matches exact UTF-16 record names, then explicit record lookup keys. Returned plan owns
senderId, optional localRecordId and one ordered action per wire field. An engaged targetFieldId
means assignment of that field; disengaged means consume/skip one supported compact int. Repeated
targets remain repeated. Unknown record has a successful plan with no localRecordId and all skips,
including a valid zero-field plan. No description means no plan; the API cannot look up an
undescribed ID. It neither equates unknown record with invalid input nor synthesizes descriptions.
Local fields absent on the wire have no action or default. Names and schema references are not
retained in the plan. Plan lifetime is independent of both inputs. Calls are independent: repeated
sender IDs do not define replacement, reset or session policy. INT_MAX sender IDs need no ID-indexed
allocation. A successful plan is not a delivered event or executable payload decoder.

## Finite budgets and failure policy

All budgets are mandatory finite size_t counts; zero is valid, there is no unlimited sentinel.
SchemaLimits bounds records, total fields, total lookup entries, per-name UTF-16 units and aggregate
record/key name units. BindingLimits bounds fields, per-name units and aggregate record/field name
units. These bound logical elements, not allocator metadata or exact heap capacity. Input memory
already belongs to the caller. Validation and lookup use bounded linear scans; no performance claim.

Schema validation checks record count first, then each record's ID/name uniqueness, supported
hasTime shape, field/lookup budgets, full field types, key validity, target range/uniqueness and field coverage.
Name budgets are charged with subtraction before addition. bindRecord checks field count, raw
record header/name budget, then all raw field structural checks/name budgets/types before lookup
and allocation. First encountered failure is returned. No partial plan/schema escapes.
Errors are INVALID_SCHEMA, INVALID_DESCRIPTION, UNSUPPORTED_PROFILE, RESOURCE_LIMIT or
ALLOCATION_FAILURE, distinct from an ordinary unknown match. std::bad_alloc and std::length_error
are translated into ALLOCATION_FAILURE. No exact Java exception parity is promised. Concurrent
read-only calls require inputs not be modified during the call; no mutable shared state is added.

## Conformance boundaries

[Coverage](../../tests/conformance/protocol/binding/COVERAGE.md) and
[validation](../../tests/conformance/protocol/binding/VALIDATION.md) identify executed checks.
Budgets, invalid schema diagnostics, immutable ownership and extreme C++ IDs are designed API
policy, not Java parity claims. Huge allocations and allocator-failure injection are excluded.
Importers, event decoding, outgoing layouts, conversion matrices, dynamic session state, PART,
transport and full DataScheme behavior remain outside this task.

## Shared type identity

[SerialFieldType](../../include/qd/core/serial_field_type.hpp) names the implemented full type identity
COMPACT_INT=8 once in qdcxx_core. Local validation and the planner compare numeric inputs with
std::to_underlying(SerialFieldType::COMPACT_INT). The enum is intentionally partial; it does not restrict
raw structural acceptance or discard representation bits. Input serialTypeId remains int32_t.
Literal oracle IDs in tests remain independent expected values.

The structural upper bound is shared separately as MAX_WIRE_SERIAL_TYPE_ID=0xff in
[describe_records.hpp](../../include/qd/protocol/describe_records.hpp). Both raw parsing and binding
use it. It is a protocol validation bound, not an enumerator or a promise to support every type.

## Explicit additional field profile

[FIELD-TYPES](../encoding/FIELD-TYPES.md) defines EXTENDED_FIELDS_V1 with exact 2/8/10/40/56/72/88/120,
owned nullable UTF-16, logical field positions independent of Java slots, and cumulative string work/ownership bounds.
Both existing tape modes keep their descriptor, flags, timing, terminal-error and append-only policies.
Earlier type exclusions below describe the older profiles; only this explicit profile extends them.
No Profile event adapter, indexed schema, importer or implicit discovery is added.
