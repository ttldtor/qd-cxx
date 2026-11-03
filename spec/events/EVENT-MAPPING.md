# Prepared local-to-event mapping

[Русская версия](EVENT-MAPPING.ru.md). English is authoritative.

This contract defines the bounded mapping boundary in [ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.md).
The selected Java production observations establish the field rules; the C++ API defines their bounded projection.
Java source resolution and event observations belong to [CONFIGURED-EVENTS](../schema/CONFIGURED-EVENTS.md)
and [PROFILE](PROFILE.md). The ownership, preparation limits and structured errors below are C++ API policy.

## Identity and ownership

Each successful `LocalSchema::create` creates a fresh opaque identity, even when declarations equal
those of another schema. Ordinary copies and bounded copies preserve that identity. Moves transfer it.
A record context owns a reference to this fixed-size identity and the selected local record ID.
Invalid/default contexts never match, including each other. Valid contexts match only when both
identity and record ID match. Counts, type sequences and numeric IDs alone do not establish compatibility.
No canonical hash, process-global registry, global counter or per-event schema comparison is used.

The identity owns no schema names, records, source files or protocol plans. Retaining a decoded record
or prepared mapping keeps only the identity alive; it cannot dangle after schema destruction.
This prevents accidental context mixing, not deliberate relabelling by a caller controlling its own data.
Manually supplied values must carry the context of the schema that defines their local field order.

The decoder copies the context from its validated local schema into each known-record plan and each
completed record. The context is appended to the existing public record aggregate, so old initializers
remain valid but produce an untagged record rejected by the new prepared API. Legacy `mapQuote` remains
the explicitly untagged eight-field compatibility API.

`TapeReader` uses a bounded identity-preserving schema copy. Append-only wire-description updates
rebuild wire plans against that same local identity; they do not invalidate event mappings or old records.
Creating a new local schema creates a new context and requires a new event mapping. Wire sender IDs,
description order and tape timing are not part of local-schema identity.

## Preparation and field resolution

Preparation receives one validated immutable local schema, an explicit local record ID, borrowed
field requests, and finite limits. Each request lists explicit accepted lookup keys, exact full type
identities and whether the field is required. Requests contain data only; there is no executable DSL.
Adapters provide the measured field rules. Source acquisition, property resolution and wire adaptation
are outside this helper. No process properties, environment, XML or JSON are consulted.

Search only the selected record's explicit lookup keys, using exact UTF-16 equality. Do not remove
dots, prepend record names, normalize Unicode or invent aliases. Multiple matching keys may refer to
the same field. Matches to different fields are ambiguous and fail atomically. Missing required fields
fail; missing optional fields retain an absent selection for the adapter's evidenced default rule.
A present field must have one of the request's exact full identities. Shared serialization bits never
authorize a representation. Extra fields remain in the generic record, in their original order.

The immutable result owns the context, the complete local type shape and the selected field positions.
It borrows no requests, names, schema containers or input values. The result is published only after
all requests pass. Event-specific defaults, times, decimal views and flag meanings remain in the adapters.

The concrete shared API is `EventMapping::prepare(schema, localId, requests, limits)` with
`FieldRequest { keys, types, required }`. `selections()` exposes an immutable ordered view of
optional field positions; an empty optional denotes local absence. `RecordValueView { context, values }`
borrows complete tagged local values. `validate(view)` checks that view without copying it.
`LocalSchema::recordContext(localId)` returns an invalid context for an out-of-range ID;
`boundedCopy(limits)` preserves identity after checking all schema logical counts and names.

## Limits, validation and errors

All limits are finite logical counts; zero is a real bound. Preparation bounds selected-record fields,
source lookup entries, requested selections, requested keys, UTF-16 key units and comparison work.
Check counts and text before proportional allocation. Charge comparison work before comparing keys,
using subtraction to avoid overflow. Type alternatives also consume work. These are logical storage
and work bounds, not exact allocator-capacity or heap-byte promises.

`MappingLimits` contains `fields`, `lookups`, `selections`, `keys`, `typeAlternatives`, `nameUnits`,
`totalNameUnits` and `comparisonWork`. Fields and source lookups belong only to the selected record;
keys and type alternatives are summed over all requests. Name units include every source lookup name
and request key, counting repeated keys, but exclude record names because preparation does not search
them. Each key/lookup comparison costs `max(key units, lookup units)` plus one, charged separately to
avoid overflow. Every alternative compared with a present field costs one work unit, even after an
earlier alternative matched. Missing optional fields do not compare types. Empty requests on an empty
record need zero preparation budget. Empty key/type lists, empty or ill-formed UTF-16 keys, and type
identities outside the implemented extended subset plus TIME_MILLIS (152) are invalid inputs; repeated same-target keys and
repeated supported types are allowed and still consume their full budgets.

The [regional Quote adapter](EVENT-TIME.md) additionally bounds and charges its selected record name
before calling this generic helper: one name-unit allowance and three conservative name scans.
The generic helper itself still performs no event-name or regional inference.

Error order is invalid record ID; selected field/lookup/request counts; aggregate request key/type
counts; source and request text bounds; request syntax and supported alternative validation; then
requests in order, comparing keys in request order against lookup order. Each comparison's resource
check precedes the comparison, distinct-target ambiguity fails immediately, missing-required follows
lookup completion, and full-type comparison follows a unique match. Allocation may fail after the
count/text preflight. `MappingError` distinguishes `INVALID_INPUT`, `MISSING_REQUIRED`,
`AMBIGUOUS_FIELD`, `INCOMPATIBLE_TYPE`, `WRONG_CONTEXT`, `INVALID_VALUES`, `RESOURCE_LIMIT` and
`ALLOCATION_FAILURE`. No error publishes a partial mapping.

Projection accepts a tagged value view. Check context first, then complete field count, exact tags and
canonical `FieldValue` shapes before accessing selected positions or copying event strings. In particular,
type 10 requires raw zero, scalars cannot contain a string payload, and scalar ranges follow
[FIELD-TYPES](../encoding/FIELD-TYPES.md). A wrong context cannot authenticate matching counts/types.

Projection bounds owned symbol units, individual string units and their aggregate before copying.
Null strings own zero units and remain distinct from empty strings. No partial event escapes on any
failure. Allocation and length exceptions become a structured allocation error. Projection does not
retain events or accumulate hidden state; a caller retaining multiple results owns their aggregate
budget. The inspector additionally uses finite file, record, decoding-work and encoded-output budgets.

Errors distinguish invalid preparation input, missing required field, ambiguous lookup, incompatible
full type, wrong record context/value shape, resource limit and allocation failure. Validation follows
input order after count preflight; context failure precedes value and ownership checks. This is not a
promise of Java exception-class parity. Failed preparation or projection leaves all inputs unchanged.

## Borrowed queue values

`RecordValueView` continues to borrow complete owning `core::FieldValue` elements. The separate
`BorrowedRecordValueView` borrows complete `core::FieldValueView` elements, whose nullable UTF-16
and byte payloads are spans/views. `EventMapping::validateView` and prepared Quote/Profile `mapView`
check the same provenance-first, full-shape canonical rules as the owning-input API. Validation
does not allocate, copy payloads or retain input values. Successful business projections still own
their symbol and projected strings under the same explicit output limits.

The existing `validate`/`map` names and signatures are unchanged, including calls with empty
aggregate arguments. The [subscription receiver](../subscriptions/RECEIVER.md) uses the borrowed
path directly for immutable queue records and keeps generic extra fields accessible independently.

## Integration and compatibility

Core owns schema identity; the shared event helper depends on core only. Protocol carries context
without depending on event adapters. Quote and Profile use the same preparation/validation machinery.
Generic custom records need no event adapter. A recognizable event name alone does not prove shape
compatibility, and event projection never removes generic extra fields.

Wire profiles, exact binding, raw-zero missing-wire defaults, completed prefixes, charge counters,
terminal failures and timing policies remain governed by [STREAM-DATA](../protocol/STREAM-DATA.md),
[TAPE-READER](../protocol/TAPE-READER.md) and [RAW-TAPE](../protocol/RAW-TAPE.md).
No loader, indexed record, additional serial identity, general wire conversion, transport or mutable
schema replacement is introduced. Independent review remains separate from verification.
