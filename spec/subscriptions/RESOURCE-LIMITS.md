# Resource budgets and record-count limits

[Русская версия](RESOURCE-LIMITS.ru.md). English is authoritative.

This contract classifies the existing receiving, callback, collector and local
publisher limits. The current implementation and the accepted replacement policy
are described separately. The replacement does not take effect by publishing this
document. Existing bounds continue to protect their storage until the replacement
has passed the migration checks below.

Use the [receiver contract](RECEIVER.md), [Mux queue contract](../mux/QUEUES.md)
and [publication contract](RECORD-PUBLICATION.md) for the currently available API.
The generic receiver and the high-level Feed receiving profile are distinct layers;
a high-level profile ceiling does not become a QTP protocol limit.

## Roles of a limit

| Role | Meaning | Accepted treatment |
| --- | --- | --- |
| Semantic result limit | An explicit request limits the result it asks to obtain. `HistorySnapshotOptions::maximumResults` defaults to 128 returned values; larger results fail explicitly. | Preserve its documented result and completion semantics. Account for the result's storage independently. |
| Work quantum | A progress step processes a finite portion before serving another participant. | Keep finite byte/time work budgets; a record count may assist scheduling. Continue the remaining accepted work without requiring a full portion. |
| Implementation storage constraint | An array, index, packet reservation or preallocated queue has a qualified finite capacity. | Preserve safety until independently charged replacement storage is available. Derive representation capacities from byte budgets with checked arithmetic where possible. |
| Accidental admission ceiling | A count rejects otherwise valid input even though the relevant byte budgets have room, without a semantic reason. | Replace general count-based admission with the independent byte budgets below. Increasing the count alone does not implement this policy. |

A single setting can currently serve several roles. Its scheduling role does not
justify using it as a general admission limit. These counts describe local C++
policies; they establish no corresponding Java QD or wire restriction.

## Current receiving and callback limits

| Limit and current value | Resource purpose and present effect | Configuration or replacement |
| --- | --- | --- |
| `EndpointOptions::batchRecords`, 1 through 4, default 4 | Selects complete physical records for one callback and sizes queue batches and the network STREAM routing quantum. Projection work and custom packet reserves use the compile-time ceiling of 4 independently of the selected value; selecting 1 does not reduce the packet reserve. This work quantum operates within the qualified storage ceiling. | Configured at endpoint creation. Replace the hard profile ceiling only after byte-accounted batch descriptors, projected values and packet preparation have independent capacity and work guarantees. |
| `EndpointOptions::pendingRecords`, 1 through 128, default 128; at least `batchRecords` | Reserves detached record slots per subscription and contributes to aggregate queue capacity. Independent record/batch byte limits also apply. This count can exhaust before those byte limits and is an admission ceiling backed by current finite storage. | Configured at endpoint creation within the qualified range. Replace the general row quota with per-consumer byte admission and budget-derived descriptor/index capacity; preserve whole-batch retention and old-generation accounting. |
| `CallbackSlotPool::PHYSICAL_RECORDS = 4`, `MAXIMUM_EVENTS = PHYSICAL_RECORDS * core::MAX_DELIVERY_PROJECTIONS` | Bounds worst-case typed projection fan-out. The pool prepays a conservative 1 MiB payload allowance plus packet/registered-event storage. A static assertion covers the supported worst-case text and opaque payload. Projection work is also derived from the physical-record ceiling. | Internal qualified storage calculation, with no independent public record-count setting. Replace the calculation together with packet reservation and projection-work sizing. Raising endpoint validation alone is unsafe. |
| Custom callback identities and simultaneous packet reservations | The pool's slot vector is sized once. `RuntimeOptions::maximumCustomCallbackSubscriptions` and `maximumCallbackBytes`, default 16 and 32 MiB, bound reservations together with fixed identity storage. A stale executor task retains an identity/generation, rather than an old packet. Typed packet events use an owning vector. | Runtime creation supplies these limits. Preserve finite identity storage and generation checks; independently reserve each packet's actual retained capacity before preparation/dispatch. Slot identities are not a substitute for payload accounting. |
| `TAPE_FRAME_RECORDS = 64` | The local tape profile funds complete DATA decoding, fields and source record storage for 64 records. This is a decode/admission constraint, not only a callback quantum. | Internal profile constant. Migrate the local profile to a byte-qualified input envelope and checked decoded expansion before removing the count bound. Preserve malformed/incomplete-input handling and complete event ownership. |
| `STREAM_ROUTE_RECORDS = 16` | Bounds source routing per queue progress step in profiles using this value. Network STREAM instead uses `batchRecords` to pace source prefixes to callback selection. | Internal work quantum. Keep progress resumable and fair; do not reject a larger admitted source event because it spans several routing steps. Byte/time work budgets replace its general sizing role. |
| Network DATA row capacities derived by `sizeNetworkData` | `maximumReceiveMessageBytes` is 8,192 through 65,536 type-plus-payload bytes. Complete rows are conservatively bounded by `(messageBytes - 1) / 2`; one extra processing attempt covers an incomplete suffix. Decoded storage has a separately charged expansion allowance. | Configure the incoming message byte envelope at endpoint creation. Keep the derived index/work capacities consistent with this envelope and local field expansion. The derived count is a representation bound, not an extra independent row quota. |

The network byte-envelope calculation does not remove the local tape limit or the
callback/pending limits. It also does not establish that typed delivery avoids
owning projections: the current custom executor path prepares owning event values
before posting the callback.

## Current publisher and collector limits

| Limit and current value | Resource purpose and present effect | Configuration or replacement |
| --- | --- | --- |
| `PublisherOptions::pendingQuotes`, default 64, supported 1 through 1,024 | Despite its name, bounds all unconsumed local source rows across enabled contracts, including a detached whole publication. Separate row and committed-row counters preserve admission and statistics. The row quota can refuse input while INPUT bytes remain available. | Configured at publisher creation. Preserve the legacy profile's documented finite row capacity during migration. In the byte-budget profile, replace the general row quota with complete publication byte reservations and budget-derived descriptor/index capacity. Keep row statistics observable without using their count as the budget. |
| `streamBatchRecords`, default 1, supported 1 through 8 | In the legacy count-based profile, bounds STREAM calls and source/output batches. This couples input admission, temporary storage and output work. | Retain the legacy setting's behavior. With `streamBytes`, it already does not limit publication or output; remaining row quotas still apply. Do not transfer the legacy ceiling into the replacement byte profile. |
| `sourceWindowRows(options)` | In byte mode takes the minimum of delivery-window row capacity, `pendingQuotes` and an INPUT-byte-derived bound. Sizes temporary source/collector work. Its dependence on `pendingQuotes` couples service size to mailbox depth. | Internal derived capacity. Separate the source service quantum from input capacity and generator call size. Advance a larger publication by owned ranges under byte/time work budgets. Keep complete input credit while any source range remains unconsumed. |
| `deliveryWindowRows(options)` and `MAX_STREAM_WINDOW_ROWS` | Bound temporary view/index storage from the wire-byte envelope and detached delivery bytes. A valid wire row needs at least a symbol marker and compact record ID. These are conservative representation bounds. | Configure `streamBytes.targetPayloadBytes`, `maxFrameBytes` and `deliveryWindowBytes` independently. Preserve checked sizing and the existing single-row soft-window allowance; the complete frame still must fit the hard frame budget. |
| Legacy `CLIENT_PENDING_ITEMS = 32`, batch-owner slots 8 and fixed `CLIENT_LIMITS` | Preallocate per-peer detached record, batch and command storage with separate byte allowances. Catalog rows adjust field/arena sizing. In byte STREAM mode `publisherClientLimits` already derives data-item capacity from two delivery windows and reserves corresponding record/batch bytes. | Current publisher profile values are internal. Replace any remaining general item/owner admission ceiling with per-peer byte reservations and derived representation capacity, preserving isolation and finite control storage. Keep an explicit resource reason for any retained control-slot limit. |
| Collector/queue `sourceRecords`, `routeRecords`, `batchRecords`, `cleanupRecords` and caller-supplied work credits | Bound source representation, routing, callback selection and cleanup work. `dataItems`, `batchOwners`, frame/point slots and byte limits also bound retained state. A scheduling count and an outstanding-storage count have different release points. | Lower-level callers explicitly supply these limits today. Preserve their current contract while introducing the replacement policy at the owning profile. Derive storage capacity from retained bytes; keep resumable work quanta and explicit work-exhaustion results. |
| HISTORY point/update counts and TICKER key/cache counts | Bound distinct retained state and per-view updates. `HistoryReceivingOptions` defaults to 4,096 points, 256 frames and 128 pending updates per subscription, alongside independent byte budgets. The publisher permits 32 keys per client per family. Its HISTORY collector shares `16 * maxConnections * 32` points across keys and peers; 16 is a pool-sizing multiplier, not a per-key guarantee. These storage policies differ from a request's semantic result size. | Feed HISTORY settings are configurable at endpoint creation; these publisher profile counts are internal. Preserve indexed transaction/snapshot and TICKER conflation rules. Replace general storage row ceilings together with collector-state and consumer byte accounting; do not turn exhaustion into silent truncation or a successful incomplete result. |

`PublisherRecordLimits::rowBytes` bounds one logical catalog row. `pendingBytes`
accounts for the INPUT binding and complete publication storage in local catalog
mode and explicit `streamBytes` mode. A legacy typed publisher without byte mode
derives its INPUT capacity from the once-held binding plus `pendingQuotes` times
the worst single-publication layout, preserving its row capacity. These capacities
do not mean wire bytes.
The [publication ownership contract](PUBLISHER.md#publication-ownership-and-capacity)
already keeps whole-publication credit through detachment and partial consumption.
That guarantee must survive the removal of a row quota.

## Accepted byte-budget policy

Input, collector state, each consumer queue, user-retained data and output frames
receive separate configurable byte budgets. A common resource owner atomically
grants reservations so that children cannot promise the same capacity twice.
Charge retained capacity for payload, containers, descriptors, indexes, strings,
schemas and metadata. Charge shared physical storage once in its physical domain
and independently charge the consumer quotas that can keep it alive. A quota does
not create another physical payload allocation or make retention free.

Each setting needs a type, byte unit, owner, default, supported range, validation
rule and application mode. Validate structural settings before activation. A
requested value becomes effective only when the consuming component accepts it;
unsupported values must not appear as effective settings. Reject shrinking below
occupied capacity and live reservations. Defaults and implementation pool sizes
require a documented resource calculation and measurements; this contract assigns
no new numerical caps or public API names.

Handoff size is independent of input capacity, QTP frame size and generator batch
size. Forward ready data without waiting to fill a portion. A byte/time work budget
can stop the current progress step; it must preserve the remaining accepted data
and allow other participants to progress. Record counts may remain semantic result
limits or auxiliary scheduling quanta. They must not impose a general admission
ceiling or replace memory accounting.

One execution owner mutates each state partition. Transfer ready portions through
a queue when ownership changes or isolation requires it. The whole-publication
publisher mailbox remains a multiple-producer, single-consumer boundary. Select
SPSC only where the actual producer/consumer topology proves that relationship.
Neither adding queues nor choosing a lock-free structure establishes improvement.

## Ownership, pressure and errors

Reserve the complete admitted object before publishing any of it. A failed
reservation, validation or allocation exposes no partial publication or desired
state. Concurrent producers must not reserve the same remaining capacity. Preserve
the existing admission/close fence and collector-specific ordering. A worker-owned
cursor continues to carry the complete INPUT credit until every row is consumed
or the publication is definitively discarded.

Callbacks borrow their batch only for the invocation. Retained generic backing
and owning typed projections have their own lifetimes. Queued, executing and
retiring callbacks keep their selected data and callable alive through completion,
including cancellation, executor rejection and final capture release. A retained
owner stays charged after endpoint closure; release capacity only after the
corresponding backing is destroyed. Stale executor tasks must not select a reused
slot or keep an obsolete payload alive.

The replacement must distinguish these outcomes:

- An object larger than an empty applicable budget is permanently oversized under
  that configuration. Waiting for another object to release cannot fix it.
- Temporarily occupied capacity may permit retry or waiting after release. Keep
  the admission attempt atomic and expose the capacity reason.
- Exhausted work credits stop or refuse the specified work; they do not mean a
  memory overflow, successful delivery or a completed command.
- By default, when a consumer queue exhausts its budget and cannot retain the next
  record, STREAM terminates with an explicit gap and HISTORY reports incompleteness.
  A slow consumer whose data still fits does not trigger that failure. TICKER
  conflates only as its contract permits. Shared input pause or explicitly permitted
  loss is an observable application choice.
- Retained delivery, resubscription and reset retain separate ownership and
  completion rules. Resetting a counter must not forgive live old-generation
  storage or acknowledge an undelivered callback.

Pausing multicast reads does not backpressure its sender. Automatic overflow
spooling is outside this policy. Byte budgets bound the documented retained
capacity model; OS buffers, stacks, library and allocator overhead require separate
measurement, so these budgets are not exact process RSS caps.

## Migration and acceptance

Replace the storage guarantee before removing the corresponding count check. For
each affected profile, the implementation must:

1. Identify every owner from input through collector, queue, callback/retention
   and output, including detached and retired generations. Calculate peak
   coexistence and checked representation sizes before allocating.
2. Add independent byte configuration and atomic reservations at those ownership
   boundaries. Keep reserved, occupied and retained capacity and refusal reasons
   distinguishable. Preserve the legacy count-based profile while the byte profile
   is qualified; document its effective settings explicitly.
3. Change general row admission to the byte policy and separately fund source,
   routing, projection, frame composition and cleanup work. Keep progress possible
   for a partially consumed publication and a partially transmitted frame.
4. Verify many short records beyond each former count ceiling while bytes remain
   available; large records; one object above an empty budget; temporary occupancy;
   variable generator batches and partially filled frames. Prove all-or-none
   refusal and exact field values, duplicates and ordering.
5. Exercise slow and healthy consumers together; retention after close; concurrent
   reservations; allocation failure; callback cancellation/rejection/retirement;
   resubscription and reset with retained old generations. Verify gap,
   incompleteness, conflation and work exhaustion independently.
6. Compare Quote/STREAM with generator batch 8, variable row sizes and 1/2/4/8
   clients. Measure full delivery, per-client p99, copying, encoding, contention
   and retained capacity. Use the existing [benchmark contract](../../tools/nettest/benchmark/QUOTE-STREAM.md)
   and [metric definitions](../../tools/nettest/benchmark/METRICS.md). A deeper
   queue can reduce retries or delay refusal without improving completed-delivery
   throughput; removing a count limit carries no promised speedup.

Run applicable project regression builds/tests and independent contract review
before claiming an affected profile has migrated. Protocol bytes, exact values,
single-contract and legacy behavior remain covered by their existing contracts.

## Source map

| Implementation boundary | Authoritative source |
| --- | --- |
| Feed options and validation | [`feed.hpp`](../../include/qd/api/feed.hpp), [`endpoint_configuration.cpp`](../../src/api/endpoint_configuration.cpp) |
| Tape/network decode, route and projection sizing | [`receiving_profile.cpp`](../../src/api/receiving_profile.cpp): `sizeNetworkData`, `TAPE_FRAME_RECORDS`, `STREAM_ROUTE_RECORDS`, `CALLBACK_RECORDS` |
| Custom packet storage, reservation and retirement | [`callback_execution.hpp`](../../src/api/callback_execution.hpp), [`callback_execution.cpp`](../../src/api/callback_execution.cpp), [`feed.cpp`](../../src/api/feed.cpp): `callbackPool`, typed `Invocation` |
| Publication row admission, byte domains and windows | [`publisher.hpp`](../../include/qd/api/publisher.hpp), [`publisher.cpp`](../../src/api/publisher.cpp): `reservePublicationRows`, `publishPrepared`, `sourceWindowRows`, `deliveryWindowRows`, `publisherClientLimits` |
| Generic queue storage and work | [`queue.hpp`](../../include/qd/mux/queue.hpp), [queue reservations](../mux/QUEUES.md#explicit-reservations-and-work) |
| Current collector semantics | [STREAM](STREAM-COLLECTOR.md), [TICKER](TICKER-COLLECTOR.md), [HISTORY](HISTORY-COLLECTOR.md) |
