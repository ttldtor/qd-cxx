# Peer execution and independent scaling

[Русская версия](PEER-EXECUTION.ru.md). English is authoritative.

This is the selected design for peer isolation and service scheduling. It changes no
runtime API. The current implementation and the acceptance of a later implementation
are separate below. Existing QTP wire layouts and ordinary single-contract operation
remain unchanged; this design does not negotiate new channels or merge record identities.

## Current execution boundary

The [TCP listener](TCP-LISTENER.md) owns its accepted sockets on one serialized thread.
Each read/write attempts one nonblocking native operation; it supplies no callbacks,
readiness registration or worker. Moving a listener on its owner thread does not grant
cross-thread use or transfer one accepted socket to another owner.

The [publisher](../subscriptions/PUBLISHER.md) owns one worker. In
[publisher.cpp](../../src/api/publisher.cpp), `PublisherServer::progress` accepts a peer,
applies policy work, services peers, advances the family queues and admits source work.
`progressPeers` traverses the peer slots. Each `Peer` owns an independent `LiveSession`,
input window, requested and admitted demand, family registrations, policy revision,
retained delivery and partial output. Its `nextFamily` rotates DATA families.

[Delivery](../../src/api/publisher_delivery.hpp) retains an `OwnedMuxBatch` inside its
callback and returns `PAUSE`. The worker advances its cursor only after the complete
frame is locally written, or suppresses revoked rows. It releases the completed batch
and requests resume. The borrowed callback view and temporary composition views do not
escape their calls. Per-peer failure and retirement already exist; common worker,
listener, family queues and source recovery remain shared boundaries.

These facts establish state separation, not parallel execution or a measured scaling gain.
The current publisher limit remains at most eight active or retiring peers. The
standalone listener's larger capacity does not enlarge the publisher profile. Numeric
remote-address filters select demand policy; they do not authenticate a client.

## Selected ownership model

A **peer execution domain** is the sole serial mutator of a peer's protocol, demand,
delivery cursors and output. A domain is an execution boundary, not necessarily a thread.
Start with the existing single worker and a rotating peer scheduler. Do not introduce a
worker or queue at every processing layer. Queue handoff is required only when execution
ownership changes or isolation needs it.

| State | Mutation owner | Transfer or lifetime rule |
| --- | --- | --- |
| Listening socket and unpublished accepts | Listener owner | Transfer only through the qualified ownership boundary below |
| Accepted socket, peer session, mappings, filter, cursors, pending frame | Assigned peer domain | One owner for read/parse/compose/write/close; no concurrent reset or I/O |
| Source decoding, collectors and aggregate demand | Source/collector domain | Ordered immutable portions and applied demand commands cross boundaries |
| Shared record backing and schema declarations | Immutable owning storage | Every receiving domain holds its own lease and quota charge |
| Public command/result snapshots | Their documented synchronized boundary | Detached values; no pointer into a worker's mutable state |

The initial single-domain mode performs both peer and listener work on the existing
worker. If measurements justify multiple peer domains, use fixed peer affinity for the
whole connection lifetime. Assign new peers to a domain with available reserved byte
capacity, choosing the least assigned work among eligible domains and rotating ties.
Work estimates use observed encoding/service cost and reservations; an IP address,
symbol hash or instantaneous socket write count is not a load estimate. Do not migrate
live sessions or steal mutable peer work. Rebalance new connections; a later live
migration would need a separate quiescent transfer contract.

The current listener cannot implement this ownership transfer. QDCXX-370 must first
qualify a one-time accepted-connection transfer or a separate owning connection port.
The resulting owner performs the entire peer read/parse/compose/write/close path. The
listener retains acceptance and endpoint coordination; it cannot continue using a
transferred descriptor. This is a required future internal boundary, not an available
public API or permission for foreign-thread calls through today's listener.

Transfer follows this sequence:

```text
listener owner       owning transfer token          destination peer domain
reserve destination capacity and bootstrap lifetime
accept unpublished socket -> token owns descriptor, identity and reservation
                         -> hand off token -> consume token exactly once
                                            prepare peer/session, then publish ready
receive completion       <- owning outcome <- activation or close diagnostic
```

Preparation reserves the destination slot, byte/work capacity and result/close capacity
before accepting work for it. An unpublished accepted socket is quiescent: no read,
write, callback or retained I/O borrow is in flight. The transfer token owns the native
descriptor, fresh connection identity/generation, bootstrap lease and destination
reservation; sender state becomes non-owning when the token is handed off. The identity
must remain unique across listener and destination slot reuse. Only the token's current
owner may close or activate it. Successful activation consumes the token and publishes
the peer only after session preparation succeeds. Failure before handoff closes at the
listener; failure afterwards closes at the destination and returns a detached outcome.
Retain descriptor ownership and its reservation for retry only when the qualified
native outcome leaves a live, owned descriptor. Otherwise relinquish the descriptor
and report a detached error; never retry a stale numeric handle. The current
[close implementation](../../src/transport/tcp_listener.cpp) invalidates the Linux
handle even on close failure and invalidates a Windows handle on `WSAENOTSOCK`.

Cancellation/endpoint shutdown fences admission, marks pending tokens for retirement
and waits for their owning outcomes. It never races a second native close against token
consumption. A late activation after the fence retires immediately at its destination;
its outcome cannot publish a new active peer. The process bootstrap outlives every
listener, token, destination connection and final close retry. An acknowledgement is a
lifecycle observation, not a second descriptor owner. No existing active session is
migrated under this transfer protocol.

A separate encoding-preparation queue feeding one I/O owner is not the initial choice:
it adds handoff while retaining shared I/O/session serialization. Reconsider it only if
attribution identifies separable preparation cost and a matched experiment justifies
its queue, lifetime and latency cost.

Use one domain as the initial configuration and control baseline until multi-domain
qualification demonstrates a useful
result. Domain count is explicit endpoint configuration applied at restart, validated
and funded before activation; it is not inferred from hardware concurrency. No new
thread library, scheduler dependency or numeric thread default is selected here.

A logical stripe partitions work, a logical channel identifies an independently
negotiated wire context, and a source identifies origin/continuity. They are distinct.
A peer execution domain creates none of them. Single-contract and ordinary mixed QTP
sessions keep their existing identities and ordering. Parallel peers may have different
sender IDs, mappings, negotiation and filters, so a shared record portion does not imply
that their encoded frames are interchangeable.

## Fair service and admission

Use a rotating runnable-peer list, preserving the successor across turns. Each ready
peer receives one bounded turn before the same peer is revisited. Lifecycle/control,
input parsing/subscription work and output each receive bounded service opportunities
within that turn. Alternate ready read/write directions, reserving control work; a
continuously readable peer cannot spend the entire quantum before output is considered.
An output opportunity writes an existing suffix or prepares DATA. Existing family
rotation remains inside the peer. `WOULD_BLOCK` yields only that direction immediately,
without spin: the other eligible direction and control can still use the remaining
peer quantum. Thus an idle read does not suppress ready DATA, and blocked output does
not suppress incoming revocation/close. Readiness/cadence supplies the next opportunity.
Accept bursts and source routing also have bounded turns, so neither can monopolize
the execution owner.
Bound service by bytes and elapsed work time; existing finite operation credits remain
safety bounds. Stop at a safe protocol checkpoint and retain the continuation. Record
counts may supplement scheduling or express semantic result limits, but do not replace
byte admission. A ready partial portion is eligible immediately; do not wait for a full
batch. Quanta, cadence and defaults must be selected against measured small/large-record,
fanout and control workloads. This design adds no arbitrary numeric caps.

Reserve separate control capacity for acceptance/negotiation, demand changes, policy
replacement, close and completion reporting. It permits progress when DATA capacity is
occupied; it does not preempt a partially written frame or promise a wall-clock deadline
under an unbounded native operation. A control burst also yields at its configured work
quantum. Application callbacks must not execute on the network owner; the internal
retain-and-pause delivery callback is bounded and is not an application callback.

At a cross-domain boundary, use SPSC only when there is exactly one producer and one
consumer; multiple producers require the corresponding bounded composition. A queue
per peer or a lock-free implementation is not an acceptance requirement. The common
resource owner atomically reserves input, collector, consumer-delivery, retained-data,
output and handoff capacity. Shared physical storage is charged once, while every
consumer's retention quota is charged separately. Containers, schema declarations,
metadata and temporary generation overlap are included. Each consumer quota is released when that consumer relinquishes its lease. Physical
capacity is returned only by its last owner; shutdown does not erase user retention charges. Byte budgets
do not claim an exact process RSS ceiling.

A peer that exhausts its own capacity cannot spend another peer's reservation. Apply
the selected contract policy: STREAM reports a gap/termination by default, HISTORY
reports incompleteness and TICKER conflates only where its contract permits. Shared
input pause or explicitly allowed loss requires application-selected, observable policy.
Distinguish a permanently oversized object from temporarily occupied capacity. Common
source failure, endpoint shutdown and physical resource exhaustion remain shared
failure boundaries; isolation does not manufacture independent upstream sources.

## Generations, retirement and reconnect

Carry independent identities across every asynchronous handoff: connection identity,
source generation, immutable schema/mapping context, applied demand revision and filter
policy revision. They answer different questions and must not be collapsed into one
counter. Commands and completions retain the original identities even after retirement.
The receiver rejects stale work and releases its owners without applying it to a reused
slot. A completion for an old write must still account its accepted prefix against that
old frame before final retirement; it can never advance a replacement peer's cursor.

The connection state sequence is:

```text
ACCEPTED -> NEGOTIATING -> ACTIVE -> RETIRING -> RETIRED
                               failure/close -> RETIRING
replacement accept -> fresh identity and NEGOTIATING
```

Retirement fences new peer work, cancels unstarted work, withdraws only that peer's
applied demand and stops further writes. Drain outstanding I/O acknowledgements and
internal callbacks before releasing their backing. Retain the slot and descriptor
ownership while a native close requires retry; do not declare retirement complete or
reuse the slot early. Endpoint close fences acceptance first, retires every owned peer
and waits for owned work to quiesce. A callback already executing finishes against its
original lease; cancellation never destroys its arguments underneath it.

Reconnect creates a fresh peer session, empty directional wire mappings, negotiation
and outgoing descriptions. It must not reset other peers' sessions, demand, filters,
partial outputs or scheduler position. Retained old data keeps its original immutable
schema and provenance; it is not reinterpreted by the new mapping. Old unacknowledged
DATA is not automatically replayed, and local socket acceptance remains distinct from
peer receipt. Existing source reconnect can affect all consumers of that source and
continues to follow the [publisher recovery contract](../subscriptions/PUBLISHER.md).

Filter replacement remains a transaction across the peer and source/collector owners.
Before committing, prepare the candidate filter, requested-to-admitted demand, revocation
masks and aggregate-demand delta; reserve all handoff, result and retirement capacity.
The peer owner fences further affected DATA and checks the existing selected-peer
close condition: revocation of a partial frame or open HISTORY continuity retires that
peer before source commit. Candidate pruning remains staged until the commit barrier.

The source/collector owner validates the connection, source generation and expected
demand revision, serially commits the affected family deltas together and returns an
owning acknowledgement. The peer owner revalidates identity, then installs the already
prepared filter/revision, applies the staged pruning/masks and discards an affected
unstarted frame. It publishes APPLIED without a new fallible allocation before resuming
DATA; permitted rows are composed afterwards under the committed policy. APPLIED requires both owners' commits; independent demand observations
retain their documented snapshot limits. Work queued during the fence retains its
context/revisions and passes the final peer-owner policy check before encoding/writing.

A rejected candidate before source commit preserves the previous policy and demand
and releases the DATA fence. The selected-peer close path still retires the peer and
withdraws its demand. Cancellation,
retirement or stale acknowledgement after that commit must withdraw the old connection
identity's demand and retire that peer, using the reserved completion/retirement path;
it cannot roll back over a replacement session or report the unchanged-policy outcome.
Other peers' demand remains intact. Cross-domain preparation never bypasses this barrier.
A schema/layout change
requires a fresh qualified session or channel domain; it cannot mutate a live mapping.
The old/new/temporary peak must be reserved before switching.

## Scaling trigger and implementation acceptance

Existing loopback Quote/STREAM measurements with exact 1/2/4/8-client delivery show
source publication rates of approximately 1.197/0.643/0.335/0.170 million records/s.
Separate resource observations use roughly two producer-process CPU cores; their thread
roles and per-phase cost are not established by that number. This is motivation for
attribution and peer parallelism experiments, not evidence that encoding is the sole
bottleneck. Increasing pending-record depth showed modest, variable rate changes with
higher p99. No structural speedup or physical-network capacity is established here.

First implement rotating service and observable per-domain work on the current owner.
Qualify additional domains when matched measurements show an owner-bound service stage,
ready peers waiting behind it, and unused independent CPU capacity. Compare one and
several fixed domains with identical input, wire policy, budgets and exact delivery;
retain the additional boundary only if useful throughput improves without unacceptable
per-peer latency, fairness or memory cost. Set workload-specific acceptance values
before the run. If listener admission or source/collector work remains the bottleneck, that
boundary needs its own measured follow-up; adding peer domains is not a remedy by fiat.

QDCXX-370 addresses confirmed server-operation defects and limits. QDCXX-358 compares
the owner loop and an Asio adapter for correctness, complexity and cost; replacing a
dependency requires a separate decision. A later implementation of this peer-domain
design must independently cover:

- Existing ordinary single-contract and mixed sessions: exact values, order, descriptions,
  negotiated capabilities and completion/errors against existing conformance evidence.
- Saturated fast peers together with slow, blocked, malformed and reconnecting peers:
  eligible peers receive bounded-turn service; demand and output of unaffected peers survive.
  A silent subscribed peer still receives DATA, continuously ready input leaves output
  opportunities, and blocked output still permits inbound revocation/control.
- Reconnect/slot reuse during partial input, partial output, policy replacement and queued
  completions: stale identities cannot mutate new sessions; final prefix accounting is exact.
- Filter and mapping generations, failed candidate preparation and HISTORY continuity:
  no forbidden DATA after the owner barrier, no mixed frame generations and explicit close/reset.
- Small/large records, oversized objects, occupied quotas, concurrent reservations, retained
  batches after close and failed native retirement: bounded ownership and no double credit.
- One-time accepted-socket transfer, activation/cancel/close races, bootstrap lifetime,
  failed close retry and stale destination identity; cross-thread race/lifetime checks,
  shutdown with outstanding work, callback quiescence,
  fair control service and producer/consumer cardinality of every handoff.
- Matched 1/2/4/8-peer measurements: source rate, each receiver's exact delivery and p99,
  service wait, per-phase CPU, copying/encoding/queue cost and live/retained memory, with
  environment and executable provenance. Physical-network qualification remains separate.

This design selects the boundaries and initial strategy. It does not establish runtime
implementation, a release, user acceptance, linear scaling or a supported number of
OPRA lines. Configuration owners must publish type, units, defaults, validation,
requested/effective values and restart/reconnect/immediate application for each setting;
unsupported settings must not be reported as effective.
