# Public record subscriptions {#record-feed-contract}

[Русская версия](RECORD-FEED.ru.md). English is authoritative.

Status: selected public contract for further implementation. `RecordFeed`,
`RecordSubscription`, `RecordSelector`, `RecordBatchView` and `OwnedRecordBatch`,
and the operations named below, are planned API vocabulary. They are not declared
in the current public headers. The existing [typed Feed](../../docs/FEED-API.md)
and [lower-level receiver](RECEIVER.md) retain their supported profiles.

## Endpoint and schema boundary {#record-feed-boundary}

An `Endpoint` exposes both `getFeed()` and `getRecordFeed()`. Both facades use the
same connection, directional protocol state, recovery lifecycle and source reset.
Obtaining either facade opens no connection. One ordinary QTP session serves
TICKER, STREAM and HISTORY; a subscription explicitly selects one family.
The existing single-contract typed endpoint remains a compatible configuration.
This contract does not change the current typed endpoint's implemented restrictions.

The Endpoint binds an explicitly supplied, validated immutable `LocalSchema` and
keeps its complete declarations alive. A built-in catalog is a default, not a
requirement. A native or externally loaded custom schema follows the same boundary.
No `EventType`, registered event mapping, JSON loader or business conversion is
required to subscribe to a record. All logical fields of the admitted schema,
including fields unknown to an event mapping, remain available. Unsupported exact
types, index layouts or wire profiles cause an explicit preparation refusal;
silently reducing a schema to its mapped event fields is forbidden.

Schema binding prepares record/field access and directional wire-to-local plans
once per binding generation. The receiving, collector, Mux and publication path
passes immutable owning record blocks or ranges. A generic subscriber does not
pay for a typed projection it did not request. Mutable collector state remains in
its processing partition. This specifies ownership and observable behavior; block
layout, pool strategy and performance remain implementation and measurement work.

[ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.md),
[record ownership](../schema/RECORD-OWNERSHIP.md) and
[ADR-0015](../../docs/decisions/ADR-0015-shared-record-blocks.md) define the existing
schema and storage decisions reused here. The complete schema owner required by
this API is stronger than retaining a current `RecordContext` or `OwnedMuxBatch`.

## Planned operation surface {#record-feed-operations}

The following surface selects the public API form. It is a contract reference,
not a compilable example or an installed-header declaration.

| Owner and operation | Input and result | Admission and lifetime |
| --- | --- | --- |
| `Endpoint::getRecordFeed()` | Shared `RecordFeed` facade for this Endpoint | No I/O; a detached facade may survive Endpoint destruction and then refuses new work as closed. It does not keep native work alive. |
| `RecordFeed::getSchema()` | Immutable owning handle to the complete bound `LocalSchema` | Preserves declarations and schema identity independently of the facade. |
| `RecordFeed::createSubscription(contract)` | Explicit TICKER, STREAM or HISTORY; shared `RecordSubscription` | Creates an initially empty desired selection. Validates the family and reserves control resources atomically; no implicit connect or callback. |
| `RecordSubscription::addSelectors`, `removeSelectors`, `setSelectors`, `clearSelectors` | Exact `RecordSelector` keys; add/union, remove, replace or clear | Inputs are borrowed during the call. Accepted asynchronous work owns its inputs before return. Invalid input or admission failure preserves the entire prior selection. |
| `RecordSubscription::getSnapshot()` | Owning desired-selection snapshot and revision | Reports accepted local intent; applied selection and wire emission are separate observations. |
| `RecordSubscription::addRecordListener`, `removeRecordListener` | Callback taking a borrowed `RecordBatchView`; listener identity | Listener selection and retirement follow the existing callback rules below. No callback runs inline during registration. |
| Subscription state, diagnostic, statistics and state listeners | Existing Feed observation categories | Own detached observations; distinguish data delivery, reset, gap, incompleteness and terminal failure. |
| `RecordSubscription::close()` | Nonblocking retirement request | Fences ordinary admission. An executing callback may finish; close does not invalidate an independently owned batch. |
| `RecordBatchView::records()`, `getSchema()` | Borrowed ordered records and immutable schema declarations | Valid only for the callback, including nested views. Copying a descriptor grants no additional lifetime. |
| `RecordBatchView::retain(first, count)` | Contiguous selected range; owning batch or explicit refusal | Retains original immutable blocks and schema; atomically admits the capacity it keeps alive. No payload copy. |
| `RecordBatchView::copy(first, count)` | Contiguous selected range; independently owned batch or explicit refusal | Copies selected payloads and metadata, preserving schema provenance. Source data is unchanged. |
| `OwnedRecordBatch::records()`, `getSchema()`, `retainedBytes()` | Immutable records, owning schema handle and charged retained capacity in bytes | Move-only quota ownership. Moves empty the source; destruction releases its lease. Views borrow this owner. |

A `RecordSelector` contains a `RecordContext` from the bound schema and an exact
physical UTF-16 symbol. A HISTORY selector additionally carries its inclusive
signed raw source-index lower bound where required by the declared index layout.
The family belongs to the subscription and is part of every physical interest key.
A range is not implicitly Unix milliseconds: business time conversion requires an
explicit schema-specific mapping. Updating a HISTORY bound replaces that membership.
The physical key is schema/record, exact symbol and family; a HISTORY bound is its
attribute, not another key. Add upserts a changed bound; remove uses the physical key
without comparing the bound. Identical duplicates collapse; conflicting duplicates
in one command are refused. No-op changes preserve the revision. No string heuristic
changes the record, source, index or family. Wildcards require an explicitly enabled
supported selector profile; ordinary text is never silently interpreted as a wildcard.

Foreign or invalid contexts are refused even if numeric IDs, names and layouts
match. Provenance-preserving schema copies are compatible. A future schema
replacement cannot retag old contexts; new desired selections must bind explicitly
to the replacement. Replacement activation policy belongs to its owning API.
This contract requires old data to remain inspectable through its original owner.

Selection mutations follow [subscription command semantics](CONTRACTS.md):
submission, owner application and wire emission are distinct. Accepted commands
receive an observable applied, rejected or cancelled-closing result and revision.
The remove/replace/clear application barrier prevents new callbacks for retired
memberships; an already executing batch may finish. Re-add creates a fresh activation.
A callback-originated mutation is queued, never recursively dispatched to force a barrier.

## Record and field metadata {#record-feed-metadata}

Every record view exposes the following information without constructing an event.
An owning batch preserves the same information. Metadata absent at the source is
explicitly absent; zero, empty and unknown must not collapse into one value.

| Category | Required information and interpretation |
| --- | --- |
| Local record | `RecordContext`, local record ID, exact record name, schema identity and access to its complete immutable declaration. A local ID is meaningful only within that schema origin. |
| Schema fields | Logical field order and record-local field IDs; complete serial type IDs; all declared record-scoped lookup keys and their targets; `hasTime` and the explicit index layout where present. Do not invent one canonical field name when several lookup keys exist. |
| Field values | One exact tagged value per logical field for an ordinary data row, in schema order. Preserve raw scalar bits, null versus engaged empty, exact UTF-16 units and opaque bytes. Synthetic Java VOID slots are described by the schema projection, not fabricated as payload fields. |
| Physical symbol | Exact decoded UTF-16 symbol and available raw cipher metadata. Keep embedded NUL and unpaired units where the accepted source profile permits them. Do not rewrite it to the business event's regional or decorated symbol. |
| Delivery | TICKER/STREAM/HISTORY family, selected membership activation, desired/admission revisions and delivered flags. A batch is homogeneous in subscription/family but can contain several records, symbols and backing blocks. |
| Source | Source-origin identity, ingress connection epoch and immutable binding generation; available original producer and relay-route provenance remain separately qualified. These are not interchangeable with an event source, exchange code, sender record ID or schema identity. |
| Raw source evidence | Optional sender ID, original flags, cipher, raw removal time, FIELD event-time words/ordinal and tape MESSAGE cursor time when actually supplied. Retain original source evidence separately from flags or values generated by collector delivery. |
| Indexed delivery | Optional full signed raw index, removal indication and applicable snapshot/transaction flags; HISTORY source/trigger metadata and range completeness/reset observation. Raw removals may omit ordinary field payload; preserve that absence rather than inventing source values. Collector-generated HISTORY removals and boundaries preserve their defined schema defaults and projected index fields, identify their delivery origin, and expose actual trigger metadata separately without inventing raw source evidence. |
| Delivery origin | Whether the row is live, initial/cached, a collector default/version projection or another explicitly supported origin. A replay/default does not become a new raw source observation. |

The schema handle describes the admitted local schema. Directional sender IDs and
wire-field order belong to the exact source binding generation. They do not replace
local IDs or field order. The API must identify which source metadata is available;
it must not infer an original producer or relay history that the source did not
supply. Process-local source labels have no authentication or global uniqueness
promise. A retained source identity must not be reused while old owners remain live.

[FIELD-TYPES](../encoding/FIELD-TYPES.md),
[indexed schemas](../schema/INDEXED-SCHEMAS.md) and the
[normalized model](../schema/NORMALIZED-MODEL.md) define exact values and identity.
Current source/delivery separation can be inspected in
[`LiveRecordView`](../../include/qd/protocol/live_session.hpp) and
[`MuxRecordView`](../../include/qd/mux/queue.hpp); those types do not already implement
this complete public metadata surface.

## Borrowing, detachment and threads {#record-feed-ownership}

The dispatcher owns the callable, selected data, complete schema and required
metadata through callback return or exception. All nested spans, strings and field
views expire then. A copied record view, field view or `RecordContext` alone cannot
keep payload or declarations alive. Close, self-removal or release of the last
public subscription/Endpoint handle during a callback does not invalidate its batch.
No application invocation or capture destruction runs under a state/container lock.

`retain` and `copy` publish an owner only after full validation, checked size
calculation and atomic reservation. Invalid ranges, permanent oversize, temporary
capacity pressure and allocation failure produce distinct refusal reasons. Failure
returns no partial owner, preserves the source borrow and leaves a healthy
subscription healthy. Retrying is the caller's choice; application side effects
and completed callbacks are never repeated automatically. An empty selected range
returns an empty owner without retaining unrelated backing.

Retention can keep a complete block for a single selected record. Its charged
capacity includes every backing allocation and associated schema/source/control
object kept alive. Independent copying creates new selected-record backing and
shares an immutable provenance-preserving schema owner; it does not copy an Endpoint,
executor, session, decoder plan or callable. Both forms survive reconnect, schema
replacement, subscription/Endpoint/runtime close and destruction.

Separate owning handles support concurrent immutable reads; final release is valid
on any thread without a live worker. Access, move or destruction of the same handle
requires synchronization. Keep an owner across an asynchronous boundary and obtain
views from it there. A borrowed callback cannot cross suspension. Moving a shallow
view is not detachment; existing `LiveEvent` release affinity remains unchanged.

Subscriptions serialize their own callbacks and model notifications. Independent
subscriptions may execute concurrently only under the selected execution policy;
there is no implicit cross-subscription callback order. The default uses the
Endpoint's existing execution owner. The contract adds no worker thread. Optional
external execution must transfer an owning packet and preserve selection fences,
serial delivery and capture retirement as specified by
[callback execution](CALLBACK-EXECUTION.md). A user callback must return for that
serialization domain to progress; the SDK cannot preempt arbitrary C++ user code.
Producer calls may take locks, allocate and copy; no lock-free or bounded-time call
is promised. Creator-only waits and shutdown retain the [API thread rules](../../docs/API-THREADS.md).

## Families, ordering and shared Feed {#record-feed-families}

| Family | Required generic-record behavior |
| --- | --- |
| STREAM | Preserve every admitted selected record, duplicates and per-source order while capacity remains available. No latest-value replacement. |
| TICKER | Latest state per physical key; initial/cached delivery and conflation follow the existing collector contract. Expose default/version projections and their original source evidence. |
| HISTORY | Preserve signed raw indexes, transactions, snapshot/removal flags and declared range bounds. Report range completeness explicitly; admission or a transaction end alone does not prove complete history. Unsupported record/index combinations are refused during preparation. |

These use the existing [STREAM](STREAM-COLLECTOR.md), [TICKER](TICKER-COLLECTOR.md)
and [HISTORY](HISTORY-COLLECTOR.md) semantics. Callback portions are delivery work
units, independent of QTP frames, publication calls or transactions. A transaction
may cross portions; transaction flags and completeness remain authoritative. Ready
data is forwarded without waiting to fill a portion. There is no global exchange
order across independent sources, no exactly-once guarantee across reconnect and
no correspondence between Java and C++ callback boundaries.

Feed and RecordFeed union compatible physical interests by schema/binding, record,
exact physical symbol and family before wire emission. A typed consumer's event
projection stays separate from the generic consumer's exact fields. Removing one
consumer leaves another consumer's interest active. Same record/symbol under
different families remains different demand; one family's conflation cannot alter
another family's records. Conflicting bindings/profiles fail explicitly before
activation instead of opening a hidden second connection or changing another
consumer's schema. Explicit channel/isolation configuration remains separate work.

Whole live DATA frames validate before publication: malformed or resource-failing
suffixes expose no prefix from that frame. Already delivered earlier frames remain
observable. Offline decoding's completed-prefix rule does not apply to live input.
On reset, old selections finish or are cancelled, old pending data is visibly
accounted as discarded/gapped, and the reset notice precedes replacement-epoch
records. Retained old owners remain valid old values, never current collector state.

Endpoint disconnect, clear, reconnect and final shutdown retain the separate logical,
native and callback-retirement observations in the [lifecycle contract](FEED-LIFECYCLE.md).
Closing a RecordSubscription affects that consumer; closing the Endpoint fences both
facades. Owning batches remain valid and charged after the termination barrier.

## Byte policy and failures {#record-feed-resources}

Use separate configurable byte domains for input, collector state, consumer queues,
user-retained data and encoded output, following [RESOURCE-LIMITS](RESOURCE-LIMITS.md).
A common owner admits reservations atomically. Charge physical shared capacity once
and each consumer quota independently for capacity that consumer can retain. Include
payload, spare/abandoned capacity, containers, descriptors, indexes, schema, source
metadata, alignment and controls; count old/candidate/temporary coexistence at peak.
Return credit after corresponding storage is destroyed, including outstanding shared
controls where applicable. Endpoint closure or a statistics reset returns no credit
for live owners. Allocator/OS overhead, stacks and RSS remain separate observations.

`retainedBytes()` reports the capacity charged to that owner, not just visible
payload bytes and not a sum to use as physical RSS when owners share allocations.
Retention admission exposes its required and available capacity and whether refusal
is permanent for the configuration or temporary. Copy admission includes temporary
peaks; failed allocation rolls back reservations. Work already performed is not refunded.

This contract introduces no numerical defaults, fixed callback-record ceiling or
new dependency. Implementation must document effective budget types, units, owners,
defaults, ranges, validation and application timing, and derive representation storage
from bytes with checked arithmetic. Current qualified count limits remain effective
in their existing profiles until replacement storage/accounting passes migration
checks; they are not permanent RecordFeed or QTP limits. Byte/time work quanta may
split processing while preserving all accepted remaining work.

Synchronous preparation/command errors use the public `api::Error` and detached
`Diagnostic` conventions: invalid/foreign input, unsupported profile, closed owner,
resource/work exhaustion and allocation failure are distinguishable. Retain/copy
return an explicit result without throwing for expected admission refusal; planned
result reasons distinguish invalid range/context, permanent oversize, temporary
occupancy and allocation failure. Exact public result declarations remain part of
the implementing API change. No internal candidate error enum is declared public here.

A throwing data listener terminates its subscription with observable
`CALLBACK_FAILURE`; other consumers continue when shared state is valid. No automatic
reinvocation occurs. Executor refusal, dropped/cancelled work and transport/protocol
failure retain separate diagnostics. By default queue overflow terminates STREAM
with an explicit gap, reports HISTORY incompleteness, and permits TICKER conflation
only within its contract. Sharing input does not silently impose shared pausing or
loss; those require explicit observable application policy. Retain/copy refusal is
not queue overflow. Diagnostics and terminal/reset control have finite reserved
storage and contain no credentials or unbounded peer text.

## Source traceability and implementation checks {#record-feed-verification}

Java QD 3.355 at the commit in [upstream.lock](../../upstream.lock) is the primary
compatibility baseline. The following source locations establish individual facts;
they do not claim runtime compatibility or an executed oracle comparison.

| Source | Fact used and boundary |
| --- | --- |
| [QDContract.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDContract.java) | Distinct TICKER, STREAM and HISTORY contracts. |
| [DataRecord.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/DataRecord.java) and [DataField.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/DataField.java) | Record/scheme identity, fields, names, exact serial types and time layout. C++ logical IDs do not require Java array layout. |
| [RecordCursor.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordCursor.java), [RecordSource.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordSource.java) and [RecordMode.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordMode.java) | Cursor views, source position, raw fields and mode-qualified metadata. C++ owning blocks, quota leases and cross-thread final release are explicit native ownership choices. |
| [Subscription contract](CONTRACTS.md), [schema ownership](../schema/RECORD-OWNERSHIP.md), [ADR-0015](../../docs/decisions/ADR-0015-shared-record-blocks.md), [publication ownership](RECORD-PUBLICATION.md) and [API threads](../../docs/API-THREADS.md) | Existing source-backed local contracts and accepted owning/byte direction. Existing tests of an internal block do not prove the public facade. |

Future implementation must provide separate evidence for:

1. An installed consumer using complete custom/default/dxlink schemas, extra fields,
   exact full types, null/empty strings, opaque data and physical symbols, without
   event registration; explicit unsupported-type and foreign-schema refusals.
2. Feed plus RecordFeed on one Endpoint and one actual session, all three families,
   shared demand/remove behavior, typed projection independence and conflicting bindings.
3. STREAM duplicates/order, TICKER initial/live/conflation/default provenance and
   HISTORY signed indexes, removals, cross-portion transactions, range completeness
   and source-trigger metadata, each against its own locked Java production path.
4. Callback self-close/removal, exception, last-handle release, concurrent independent
   subscriptions and executor cancel/reject/drop; no callable/payload destruction
   under locks and no new callback beyond the applied retirement/reset barrier.
5. Retain and selected copy after callback, reconnect, schema replacement and full
   Endpoint/runtime destruction; complete schema/metadata, any-thread final release,
   null/empty distinctions, moved/empty owners and absence of retained operational owners.
6. Exact/one-short byte admission, permanent versus temporary refusal, concurrent
   reservations, checked overflow, allocation failure at every owning stage and
   rollback without a partial owner or failed healthy subscription. Include large
   retained blocks, small selected copies, old generations and final weak controls.
7. Malformed live-frame suffix, slow-consumer policies, reset/gap ordering, repeated
   close and genuine native/callback retirement, including pending accepted commands.
8. Identical-workload copy/allocation/capacity and end-to-end latency/throughput
   measurements across receive, processing and publication. Sharing or source review
   alone establishes no speedup. Preserve mandatory platform/sanitizer and EN/RU
   documentation checks for the implemented change.
