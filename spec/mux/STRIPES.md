# Striped local MUX composition

[Русская версия](STRIPES.ru.md). English is authoritative.

Include [`qd/mux/striped.hpp`](../../include/qd/mux/striped.hpp) and link the optional
`qdcxx_mux_queue` target. `mux::StripedCollector` owns one actual existing [MuxQueue](QUEUES.md)
per physical [symbol stripe](../core/SYMBOL-STRIPES.md). Each queue owns its native STREAM,
TICKER or HISTORY collector and independent consumer registrations. Every exact input row is
assigned to its physical child; rows keep their original relative order within that child.
Cross-stripe callback order is unspecified. A stripe is independent of a channel, event source,
connection epoch and executor domain. The composition introduces no wire format, transport,
worker, clock, external dependency or promise of linear scaling.

## Construction and independent admission

`createStream`, `createTicker` and `createHistory` select one immutable family for the entire
composition. Supply a valid owned `SymbolStriper`, a schema, exactly one complete native options
value per stripe, explicit `StripedLimits` and remaining `StripedWork`. Every child is created
before a root is published. A failed factory exposes no partial root. Child QueueOptions,
collector limits, retention options, demand limits and `ClientLimits` remain independent; no
common minimum quota or implicit quota multiplication is applied.

The root reserves its own object/shared controls, immutable schema/partition retention,
child/result arrays and live logical-port storage. `residentBytes` reports those requested or
conservatively modelled root bytes; physical child ledgers are separate. The detached pure byte
counter control is reported by the existing accounting model and excluded from data capacity.
`operationBytes` bounds one copied command, registration or source-plus-current-slice operation,
including owned descriptors, symbol capacities and retained client metadata. Three explicit slice
bounds cover operational metadata, copied legacy values and retained pure payload. Zero is zero.
Allocator bookkeeping, runtime exception machinery, opaque consumer captures and caller-owned
inspection copies remain outside these byte scopes. Returned inspection copies consume explicit
work and are bounded by `operationBytes`; retaining them does not retain the operational root.

`StripedWork` carries root operation/visit/text/byte credits and the existing child, submission and
event work objects. The implementation never replenishes these objects internally. Attempts and
failed preparation consume the work already performed. A refusal before `beginStage` acceptance
leaves the supplied source event usable. There is no extra source queue and no fixed stripe,
client or row ceiling beyond the explicit existing grants and representable allocation sizes.

## One resumable operation

Use `beginOpen`, `beginAdd`, `beginRemove`, `beginReplace`, `beginClear`, `beginStage` or
`beginReset`, then repeatedly call `advance` with explicit remaining turn credits. At most one
operation is current. A second begin returns `BUSY`, including while an opened client remains
unclaimed. `takeClient` transfers a complete new logical client once, only after all required
registrations opened. Operation IDs are root-local, non-reused identities.

Each child action retains its existing atomic contract. The root stores a truthful per-stripe
result: required, submitted, completed, actual ticket status, input admission, reset identity,
source/admitted/staged row counts and original typed error. `COMPLETE` requires every required
child to finish its requested local barrier. A command's `APPLIED` result does not imply source
synchronization or delivery. Staged row counts describe successful native input calls, including
rows ignored by the collector's actual eligibility rules. They are distinct from delivered copies.

There is no all-stripe rollback. A later refusal preserves completed children and the exact current
child continuation. Retryable work/producer pressure resumes that child; permanent refusal leaves
`FAILED` evidence until explicit cancellation or stop. Completed child actions are never resubmitted.
Progress and borrowed result spans expire at the next begin/advance or owner mutation.

`requestCancel(operationId)` records a cutoff. It submits no later child action. A command already
accepted by a child must reach its real ticket result; cancellation can therefore report that child
as `APPLIED` and completed. An accepted unfinished source uses the existing pending-source discard
path. An accepted reset retains its actual source commit/global ACK progress. `CANCELLED` means
that continuation and owned-input disposal completed, while previously completed effects remain.
`unsubmittedSourceRecords` separately counts root-owned rows excluded before child admission;
it does not invent lost per-client copies. Source cancellation is separate from shutting down children.

## Demand, stripe hints and real filters

Each logical client owns existing independent child ports. Mux fan-out preserves separate consumer
demand, queues, callback pause/stop state and retained leases. Exact commands route to the one
selected enabled child. Explicit STREAM `ALL` reaches every enabled child. Exact/ALL validity and
family support remain governed by each child's existing selector profile.

The optional `SymbolStripeFilter` in `beginOpen` is a **stripe hint**. A proven static overlap can
avoid creating irrelevant child registrations; unknown overlap considers every child. The hint
is independent of a hard record predicate. For example, `byhash4` plus hint `hash0of8` selects one
child; that child's STREAM ALL subscription still receives both `SYM2` and `SYM5`. Java production
`withStripe(hash0of8)` behaves this way. Adding its independent `withFilter(hash0of8)` retains only
`SYM2`. Replacing the hard predicate with the candidate-child hint would change behavior.

`StripedCollector` does not install a `FeedFilter`. Keep an actual hard filter at its existing upper
record boundary, such as publisher physical record admission. A complete policy can intersect its
expression with the generated physical stripe leaf while continuing to evaluate the actual policy.
The pure core and collector targets do not depend on the event API. The production oracle and
`stripe_filter_boundary` regression exercise these distinct responsibilities.

## Ownership, callbacks and retained data

Serialize owner calls, moves and destruction. `StripeClient` copies own pure fixed provenance,
accounting and existing producer ports. They retain no operational root, queue, protocol event,
identity snapshot or consumer callback. Distinct token copies and immutable delivery leases can
cross threads under the existing port rules. The last public logical token records abort on all
its child ports even if a current operation still retains port metadata.

The root alone owns the original accepted source event and at most one current operational slice.
Shared receiving rows retain pure immutable payload aliases. Legacy rows copy selected symbols,
nullable text/opaque values and scalar representations into an independent counted eager arena.
Client delivery uses the existing pure alias/copy boundary and retains no parent event, live identity
or live snapshot. Physical flags, times, removals and original metadata retain their existing meaning.

Callbacks execute through the actual child MuxQueue outside its locks. Recursive begin, advance and native inspection calls
return `REENTRANT`. Reserved controls and passive progress/resource views start no child operation. The root holds one serialized operational lease across a callback; replacing or
destroying the public root there fences later work and retires the old children after that callback
returns. This does not change the direct single-MuxQueue owner's documented lifetime requirement.
Callback throws are counted and never retried. No destructor or detached release starts a new callback.

## Native inspection, snapshots and reset

`observations` returns each child's actual configured capacity, current resources, queue state,
authoritative desired revision, desired size, reset state and optional terminal outcome. Children
are observed sequentially by index; their versions describe each observation, not one atomic global
instant. `aggregateEntries(stripe)` borrows that child's authoritative desired union.

`summary` sums physical desired entries and root-observed admissions/stage calls. STREAM ALL counts
once per selected physical child. Final `QueueCounts` become available only when every child has
actually terminated. Before that point they remain absent; zero is not substituted for unavailable
loss evidence. Known root cancellation, child `hadGap`, unknown eligibility and saturating counters
remain explicit. Upstream continuity remains unknown independently of local clean delivery.

Snapshot/inspection APIs preserve the native family contracts:

- HISTORY `rangeSnapshot(client)` returns owning per-stripe exact keys, current desired/mapping
  revisions, raw source coverage, delivery state and gaps. `retainedHistoryRange` dispatches a bounded
  exact interval to its physical child and returns existing borrowed retained rows. A covered source
  interval is separate from complete delivered coverage.
  The combined owning snapshot uses two funded child-query passes and one exact final reservation.
  The first pass counts keys and backing, then releases each temporary child vector; the second
  supplies the returned observations. Child work pays both queries and their temporary disposal,
  while root work pays the scans and final output. No owner progress or callback occurs between
  passes. A concurrent reserved stop can change status flags but cannot change applied keys without
  owner progress. These remain sequential child observations, with no common atomic instant.
- TICKER `visitTickerLatest` dispatches an exact key and invokes the existing synchronous nonthrowing
  borrowed-value visitor. Retaining or reentering through that borrowed value is unsupported.
- STREAM has no retained snapshot data; the Java snapshot provider is empty. HISTORY/TICKER-specific
  calls on another family return `UNSUPPORTED_FAMILY` explicitly.

`beginReset` requests the real existing source reset in every child, retaining desired demand.
Completion requires every requested child source commit and its applicable collector-global ACK.
Client reset notices and exact per-stripe `acknowledgeReset(stripe,sequence)` remain separate:
callbacks stay gated until the actual notice is acknowledged. Stale ACKs cannot acknowledge a new
interval. Old retained immutable batches survive reset and root closure. An external transport
reconnect can drive this local reset barrier; it does not establish socket reconnection by itself.

`requestStop` first records stop in every child, then drives current cancellation and fair child
cleanup. `closed` requires every actual child terminal. Graceful HISTORY closure reports `ABORTED`
and `hadGap` for an incomplete requested range even after all known rows were delivered; complete
SNAPSHOT_END coverage can drain cleanly. Child failures and known discarded counts stay visible.
One registration's stripe-local quota failure does not impose its quota on healthy consumers or stripes.

## Compatibility evidence

The pinned Java baseline is [upstream.lock](../../upstream.lock). Source boundaries include
[StripedCollector](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedCollector.java),
[StripedAgent](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedAgent.java),
[StripedDistributor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedDistributor.java)
and [matrix Agent](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/Agent.java).
The [production oracle](../../tools/upstream/stripe-oracle/README.md) records actual three-family
composition, partial retrieval, replacement, sizes, drops, closure and stripe-hint/filter distinctions.
Native tests exercise actual Mux delivery, partial progress, reset/ACK, immutable retention, independent
quota failure and closure. These local contracts add no automatic transport or parallel execution API.