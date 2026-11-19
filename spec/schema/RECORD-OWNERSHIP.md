# Record ownership and schema provenance

[Русская версия](RECORD-OWNERSHIP.ru.md). English is authoritative.

This contract specifies the existing C++ schema, field and record representations. It also
separately records the accepted target for future RecordFeed delivery. It adds no runtime API,
record-count ceiling, schema conversion or change to existing resource policies.

## Owners and borrows in the current API

| Representation | Owns | Borrows |
| --- | --- | --- |
| [`LocalSchema`](../../include/qd/core/local_schema.hpp) | Validated declarations and a shared fixed provenance token | Nothing from `create` input after success |
| `RecordContext` / `SchemaIdentity` | Fixed immutable provenance only | Neither declarations nor payloads are retained |
| [`FieldValue`](../../include/qd/core/serial_field_type.hpp) | Scalar bits and optional exact UTF-16/byte payloads | Nothing |
| `FieldValueView` | Scalar bits and nullable view descriptors | Optional UTF-16/byte payloads |
| [`StreamDataRecord`](../../include/qd/protocol/stream_data.hpp) | Symbol, field vectors, provenance and represented metadata | Nothing from decode input/context |
| [`LiveEvent`](../../include/qd/protocol/live_session.hpp) | Admitted frame storage and its immutable generation lease | Its `LiveRecordView` spans/text borrow that storage |
| [`BorrowedMuxBatch`](../../include/qd/mux/queue.hpp) | No lifetime beyond the callback is granted | Detached records for this callback |
| `OwnedMuxBatch` | Shared immutable detached batch backing and reservation ledger | Returned record spans borrow that backing |

`RecordBatch` is a conceptual term, not an existing core type. Actual delivery uses
`BorrowedMuxBatch`, `OwnedMuxBatch` and the callback-only
[`SubscriptionBatch`](../../include/qd/client/subscription_receiver.hpp). A protocol `LiveEvent`
is a frame owner; it is not interchangeable with a detached application batch.

## Schema lifetime and identity

`LocalSchema::create` validates and copies all declarations. Input storage may be changed or
destroyed after success. Every independent successful creation gets distinct provenance, including
an empty schema. Equal names, layouts, profiles and local IDs do not establish a shared origin.

Copy construction, copy assignment and `boundedCopy` copy declaration storage and preserve origin.
They do not extend borrows into the source object. Failed copy assignment leaves the destination
unchanged; `boundedCopy` reports its existing resource/allocation error without changing the source.
Ordinary copying may throw allocation exceptions. Move operations transfer ownership and provenance;
the source has invalid provenance. Self-assignment preserves state.

`records()` and all nested references/views borrow the particular schema's declarations. They expire
on destruction, assignment or moving from that owner; reacquire them from the destination after a
move. A provenance-preserving copy does not keep an old declaration address alive. Keeping the
complete schema object alive, or copying its declarations, is necessary when names/layouts are needed.

`RecordContext` selects one record within a schema origin. `SchemaIdentity` identifies the origin
even for an empty catalog. Copying tokens retains that fixed identity; moving transfers it and
invalidates the source token. They remain valid after all declaration owners are destroyed, but
cannot recover declarations or keep any record payload alive. Invalid tokens never match, including
themselves. A different record ID in the same origin also does not match a `RecordContext`.

`LocalSchema::recordId(context)` explicitly returns `nullopt` for a foreign/invalid context or a
moved-from schema. Provenance-preserving copies accept the context. Context-aware adapters must
use exact origin and record identity: existing prepared event mappings report `WRONG_CONTEXT` for
an unrelated origin even when its record shape is equal. A raw local integer ID or a field view has
no provenance; callers must not substitute it for this check or retag foreign data to bypass it.
The specific consuming API determines its error type; no universal new error is introduced.

Const schema/token reads may run concurrently while each object and its storage remain alive and
unchanged. Copying from a source requires the same stability. Assignment, moving and destruction of
the same object require external synchronization with its users. Distinct owning token copies may
be transferred and finally released on different threads without retaining a network worker.

## Fields, records and detachment

`FieldValueView` copies/moves scalar bits and nullable payload addresses. Neither operation extends
payload lifetime or empties the source view. `validFieldValueView` checks tag/range/payload shape;
it cannot authenticate a pointer's lifetime or schema. Reallocation, replacement, erasure or
destruction of source storage invalidates affected views; concurrent source mutation is forbidden.
An immutable source with a separately maintained owner may be read across threads. A copied
callback view remains subject to the original callback lifetime.

Independent field copying means copying `type`, `raw` and every engaged payload into `FieldValue`.
Preserve disengaged null separately from engaged empty, exact UTF-16 units and opaque bytes as
specified by [FIELD-TYPES](../encoding/FIELD-TYPES.md). Complete record copying also needs symbol,
context and the metadata required by its consuming contract. A `FieldValue` alone has no schema.
Current core has no generic `detach()` operation. Constructing a fresh schema from copied
declarations creates a foreign origin; use a `LocalSchema` copy to retain provenance.

`StreamDataRecord` copying independently copies symbol/field storage and preserves its context;
moving transfers its owning members. Existing views into it must not be assumed valid after a
move or mutation; reacquire them from the destination. It carries provenance, not a complete schema
catalog. Keep a separate schema owner for later schema inspection.

`LiveRecordView`, `MuxRecordView` and nested field views are shallow descriptors. Copying a record
view shares its owning provenance; moving transfers it and invalidates the source context. Both
operations copy payload descriptors and scalar metadata without retaining symbol/field storage.
`LiveEvent` moves transfer its backing, so previously borrowed record spans follow the destination
until that backing is destroyed/replaced. The event and its epoch accounting retain their existing
serialized, nonreentrant access/release contract. Moving one does not permit cross-thread final release.

A borrowed MUX batch is valid throughout its callback, including while a close is requested from
that callback, and expires on return or exception unless its data has an owning lease. Copying the
borrow wrapper grants no extra lifetime. `retain()` returns `OwnedMuxBatch` without allocation under
the existing [queue admission/reservation contract](../mux/QUEUES.md). Copies of that owner share
backing; moves transfer the lease and empty the source. Any remaining owning lease keeps returned
spans valid. Replacing or destroying the last lease invalidates them. Keep an owner across an async
boundary and obtain views from it there; callback-only services cannot cross suspension.

Separate `OwnedMuxBatch` handles permit concurrent immutable reads and final release on any thread;
access and assignment to the same handle need synchronization. A retained batch survives queue/source
destruction and retains no receiver plan, callable or operational owner. `SubscriptionBatch::retain`
preserves generic records only; the projection service still expires with the callback. Existing
detached batches preserve their represented fields/metadata and provenance, not a full local schema.
Current queue memory counters and finite retention reservations remain authoritative; `retain()`
does not implement the future retention-admission API described below.

## Accepted RecordFeed target; not implemented by this contract

Future RecordFeed uses the same Endpoint and connection as typed Feed. Decoder, collectors, MUX
and publisher hand off immutable owning record blocks or ranges bound to schema and source
generation. Bindings are prepared at schema binding. Mutable collector state stays in its processing
partition. This target does not rename current views or relax `LiveEvent` release affinity.

A delivery callback borrows its whole portion until callback completion. Explicit retention keeps
original blocks; independent copying owns selected records. Both owning forms must preserve symbol,
every field, schema and required metadata through reconnect, schema replacement, Endpoint close and
destruction. Final release may happen on any thread without a live Endpoint/executor. Retaining one
record may keep a whole block; the API must expose actual retained capacity. Copying can reduce it.

Retention/copy admission reserves the required capacity atomically. Failure preserves the original
borrow and a healthy subscription; it publishes no partial owner. Temporary capacity exhaustion
differs from a permanently oversized request. No application side effect is automatically repeated.
Shared physical capacity is accounted separately from each consumer quota, including schema,
generation, metadata and temporary copy peaks. Charges survive Endpoint closure until final release.
Existing component counters are not proof of this common resource owner or an exact process RSS cap.

Borrowed model revision views likewise stay in their serial notification domain; longer retention or
cross-thread model reads require an immutable owning snapshot with revision, source generation and
completeness. Retaining a record batch does not create such a model snapshot or prove history complete.

## Internal schema-bound storage candidate

The private `SharedRecordBlock` candidate extends the existing `SharedRecordPayload::Batch` through
an actual derived shared-control owner. A binding takes a complete already owned `LocalSchema` once,
preserving provenance, and exact caller-supplied source-origin/generation labels. These pure numeric
labels carry no authentication or global uniqueness guarantee. Blocks reuse the immutable binding;
no transport, executor or callback owner is retained. Exact nullable fields, raw values and physical
source metadata reside in one packed block. All record contexts and field shapes are validated first.

Copy admission reserves the full requested block backing/control capacity atomically before allocation.
Binding creation likewise reserves its full control/schema-capacity amount before moving the input
schema. Failure publishes no partial owner and preserves the input schema/records and existing borrows.
`CAPACITY_EXCEEDED` identifies permanent oversize; `WOULD_BLOCK` identifies temporary outstanding
reservations. Allocation failure rolls back reservations. These private errors add no public API.

A range retain allocates and copies no payload, reserves the complete retained block/binding capacity
in an independent consumer byte domain, and returns a move-only quota lease. Independent contiguous
range copying owns a new, potentially smaller packed array and shares the binding. Neither operation
imposes a record-count ceiling. Separate immutable owners support reads and final release on any thread;
mutating the same handle still needs synchronization. Actual shared-control requests remain charged
through weak-control lifetime, and backing is freed before its reservation is returned.

Block copy/request counters separate copied payload, field/row/source metadata, padding, backing and
shared control. Binding and byte-domain controls are reported separately. Schema capacity uses the
existing `LocalSchema::storageBytes()` model; schema acquisition/copy allocations, allocator overhead
and RSS are separate observations. The [design decision](../../docs/decisions/ADR-0015-shared-record-blocks.md)
records the complete boundary accounting and its limits.

The candidate is exercised through the existing local event → STREAM collector → MUX shared-payload
path. Its complete binding/source metadata survive inside the retained owner; existing public MUX
views still expose their current represented metadata and provenance. This is a storage candidate,
not the public RecordFeed, whole production pipeline migration or common cross-component ledger.
Existing public retention/reservation and `LiveEvent` release-affinity contracts remain in force.
## Verification and traceability

The source-backed current contract is exercised by
[`binding_tests.cpp`](../../tests/conformance/binding_tests.cpp) (input ownership, copies, moves,
assignment, identity after declaration destruction),
[`event_mapping_tests.cpp`](../../tests/conformance/event_mapping_tests.cpp) (foreign-context
rejection and decoded-record independence), and
[`mux_queue_tests.cpp`](../../tests/conformance/mux_queue_tests.cpp) (retention after source/queue
destruction and cross-thread final release). These are test locations, not a claim of executed checks.
The accepted RecordFeed/model target needs its own implementation and conformance evidence.
