# Collector invariants

[Русская версия](COLLECTOR-INVARIANTS.ru.md). English is authoritative.

This contract defines observable invariants for TICKER, STREAM and HISTORY independently of
storage layout. It applies at the whole-event admission and local delivery boundaries. The
family contracts describe the current C++ APIs: [TICKER](TICKER-COLLECTOR.md),
[STREAM](STREAM-COLLECTOR.md) and [HISTORY](HISTORY-COLLECTOR.md). Their explicit capacities,
work credits and recovery operations remain in force. Accepted future ownership is identified
separately in [record ownership](../schema/RECORD-OWNERSHIP.md).

The compatibility reference is Java QD 3.355 at [upstream.lock](../../upstream.lock).
The [capture profile](../../tests/fixtures/subscriptions/CAPTURES.md) identifies executed
production observations and representable inputs. A shared implementation mechanism or a passing
decoder does not establish each collector scenario. Acceptance compares values, order,
completeness, failures and retained resources; Java matrices and hash traversal are implementation
choices.

## Common admission, selection and ownership

A key contains exact immutable record-context provenance and the selected symbol identity.
Equal numeric record IDs in independent schemas do not identify the same record. The selected
[selector profile](SELECTORS.md) defines the symbol domain. Default Exact uses exact UTF-16;
canonical matching and STREAM All require their explicit profiles. HISTORY additionally uses the
complete signed-64 raw index. Its index and inclusive lower bound carry no implicit calendar unit
or business source identifier.

A collector receives a complete event admitted by [LiveSession](LIVE-SESSION.md). Invalid frame
prefixes never enter collector state. Planning precedes publication: refusal publishes no selected
prefix and preserves the caller's input owner. Successful admission transfers event ownership only
when that family retains it; zero selection or an equal-only TICKER input need not consume it.
Whole-event refusal may still publish the documented terminal or stale diagnostic.

The collector owns mutable state; callers serialize its operations, movement and destruction with
related protocol-owner use and release. `front` prepares a borrowed offer; `pop` acknowledges local
consumption. Neither executes an application callback nor acknowledges transport or peer state.
Borrowed records must be used before their documented invalidation operation. Copying a record
view or token does not retain payload storage. Failed work preflight and failed/no-op desired
mutations preserve previous offers; charged work is not refunded.

## TICKER: latest state and independent progress

- **State and storage.** One effective latest value exists per key. With `storeEverything=false`,
  an interested view is required for retention; removing the last interest evicts the value.
  With true retention, admitted unrequested values may supply later initial delivery. A stalled
  view keeps its progress metadata and sees the shared latest value without retaining superseded
  payload versions independently. Absence remains distinguishable from present zero/null fields.
- **Filtering and equality.** Selection precedes delivery. The supported default-field profile
  compares complete materialized scalar representations and nullable text/byte/opaque contents.
  Flags alone do not dirty a key; raw REMOVE is not a cache-erasure instruction. Equality and
  explicit version bindings follow the selected record semantics. Current
  [`TickerVersionBinding`](../../include/qd/collectors/ticker.hpp) selects a schema-bound signed
  physical-int version gate; lower versions are ignored, and a fresh negative version materializes
  the documented zero-version defaults. This profile does not establish arbitrary Java object or
  whole-catalogue record equality.
- **Conflation and order.** Multiple effective same-key changes before retrieval may yield only
  the latest value. A change and reversal within one event can still dirty the key. Updating a
  pending key preserves its position; initial and live queues follow the configured retrieval
  quantum. Each view advances independently. Removing and re-adding interest creates a new
  activation and pending position; availability on re-add depends on actual retention.
- **Failure and recovery.** Shared cache/resource exhaustion rejects the whole unconsumed input,
  marks shared state stale and invalidates current offers for every view. Retained stale evidence
  cannot certify a current value. A local view/core desired-state rejection preserves other views.
  Reset clears cache and pending state, retains desired interests and releases the old epoch.
  Exact acknowledgment of the latest reserved reset notice gates fresh admission/delivery; an old
  acknowledgment cannot unlock a newer notice. This owner barrier does not acknowledge each view.

## STREAM: every selected record in source order

- **State and storage.** Pending selected records retain their complete source-event backing and
  original binding provenance. No latest-value cache or pre-add replay is created. Snapshot-only
  access cannot manufacture current values; initial delivery requires actual source data.
- **Filtering and order.** Every admitted matching occurrence remains eligible in source order,
  including identical consecutive values and interleaved keys. Partial single/batch retrieval
  preserves the remaining order. Effective withdrawal retires an activation; remove/re-add cannot
  revive its queued records. Unchanged memberships remain eligible. Batch acknowledgment consumes
  the exact prepared prefix atomically and distinguishes withdrawn records from delivered records.
- **Failure and recovery.** Whole-event capacity overflow reports a distinct terminal
  `RESOURCE_LIMIT`: the incoming owner is untouched, existing owners stay retained and delivery
  stops. Work exhaustion preserves queue and offer for another caller-delimited turn. Removal,
  clear and explicit discard remain available after terminal overflow. `resetSource` can replace
  the epoch of a healthy collector while preserving desired activations and reporting actual
  discards plus unknown upstream loss. It cannot revive terminal overflow; recovery then requires
  a new collector. The source owner must exclude further old-epoch input before switching epochs.

## HISTORY: indexed state, boundaries and consistency

- **State, selection and order.** Points are keyed by record, symbol and full signed raw index;
  ordinary untimed records are rejected. Each view selects its inclusive requested lower bound.
  Shared storage and retention follow interested bounds or explicit true retention. Initial
  history is descending; local update order and its limited same-index pending conflation follow
  the selected family profile. Retrieval and per-partition snapshot quanta are explicit options.
  Independent views retain their own cursors, update queues and transaction progress.
- **Snapshots.** BEGIN starts or restarts the selected snapshot scope. END and SNIP terminate it
  with their distinct coverage/truncation meanings; MODE participates in the selected snapshot
  semantics. Replacement can sweep old points and produce projected removals. Source coverage,
  source transaction consistency and each view's projected consistency remain separate facts.
  An empty queue, zero flags or END above the requested bound does not prove complete coverage.
- **Transactions.** `TX_PENDING` and the selected implicit transaction rules govern consistency.
  Raw progress remains retrievable while a snapshot/transaction is unfinished. A business model
  may wait for consistency separately. Disconnect, failure or reset during a transaction cannot
  turn its delivered prefix into a complete snapshot.
- **Removals.** REMOVE targets the exact indexed point, including the complete removal index
  when a public event projection narrows it. Missing ordinary fields do not become new live
  values. Synthetic removals/boundaries carry schema defaults and projected control flags;
  optional raw source and trigger metadata identify only actual input records. Uncached/repeated
  removal, same-index replacement and reappearance must preserve these distinctions. Event time
  and event sequence do not replace the indexed key. The current ordinary-MAX rejection and
  MAX-REMOVE virtual boundary remain the explicit native policy in the family contract.
- **Completeness.** A complete result requires a covered inclusive requested range and consistent
  source/view state for the exact epoch, membership/revision and observation. A covered upper
  range may complete while a lower range remains incomplete. Range expansion reassesses coverage.
  Reset, source gaps, generation changes and shared staleness revoke fresh completeness, including
  staleness within the same epoch. A detached AS-OF status stays unchanged after owner progress;
  it certifies its recorded observation, without updating itself.
- **Failure and recovery.** Shared old-plus-incoming exhaustion rejects the input and marks shared
  evidence stale. If shared admission succeeds, per-view queue failure retires only the failed
  view's retention/bounds and remains queryable until close; healthy views keep their state.
  Work rejection preserves input, state and offers. Reset discards old points/progress, retains
  interests and requires exact current-notice acknowledgment before fresh admission/delivery.
  False-retention empty metadata is reclaimed explicitly by `forgetUnobserved` or the configured
  inactive-reclamation option when eligible; this never proves continuity across forgotten data.

## Demand, delivery and retained lifetimes

Independent interests are combined by [MuxState](../mux/INTERESTS.md): STREAM/TICKER use the
union of keys; HISTORY uses the signed minimum requested lower bound per key. One consumer leaving
preserves every other consumer's demand. Removing local delivery interest does not establish that a
wire unsubscribe was sent or applied. Local family collectors do not own that reconciliation.

[MuxQueue](../mux/QUEUES.md) composes actual collectors with independent consumer queues.
Its callback borrow lasts through return/exception; `retain()` acquires immutable owning backing.
Retained batches preserve their represented records after withdrawal, reset, source/queue closure
and destruction, while occupying the documented reservations until actual release. That lifetime
permits reading old data; fresh delivery authorization and HISTORY completeness require separate
current evidence. A throwing callback is counted and is never automatically replayed. Collector
`pop`, queue acknowledgment, callback completion and peer acknowledgment are separate boundaries.
The [ownership contract](../schema/RECORD-OWNERSHIP.md) specifies current thread/release rules and
the accepted future RecordFeed ownership target. This page adds no new retention API or publisher
implementation; [publication](RECORD-PUBLICATION.md) must preserve the selected family semantics,
with explicit indexed controls and applicable capabilities for HISTORY.

## Resources and acceptance evidence

Current limits explicitly account fixed metadata, retained whole events, old-plus-candidate peaks,
per-view storage and separate desired/schema/session scopes. Retaining one selected record may
retain an event containing unrequested records. Finite per-turn work enables retry after work
rejection; it is distinct from capacity overflow and from a cumulative lifetime record quota.
Requested bytes do not measure process RSS or promise bounded destruction latency. Resource-policy
changes must preserve the family-specific failures and recovery above and expose configurable
policy choices; this contract introduces no additional numerical count ceiling or storage layout.

Each acceptance case must identify its source/profile, input, observable output and refusal scope.
The existing native cases and validation reports provide bounded evidence for their documented
runtime profiles and Java comparison scopes.

| Obligations | Cases and evidence boundaries |
| --- | --- |
| REQ-TICKER-001, 002, 005; QD-CORE-017 | [TICKER cases](../../tests/conformance/ticker_collector_tests.cpp): latest/equality/late-view, explicit versions, independent stalled view, initial/live quantum, reversal and overflow/reset; [qualification](../../tests/conformance/subscriptions/TICKER-VALIDATION.md). Arbitrary record equality remains unqualified. |
| REQ-STREAM-001–004; QD-CORE-019 | [STREAM cases](../../tests/conformance/stream_collector_tests.cpp): identical/interleaved records, withdrawal/re-add, complete-event overflow, work retry, partial batches and source reset; [qualification](../../tests/conformance/subscriptions/STREAM-VALIDATION.md). No cached snapshot is inferred. |
| REQ-HISTORY-001, 002.1–002.4, 003–005; QD-CORE-021, 022 | [HISTORY cases](../../tests/conformance/history_collector_tests.cpp): complete/truncated/restarted snapshot traces, pending progress, signed removal identity, two requested bounds and synthetic provenance; [qualification](../../tests/conformance/subscriptions/HISTORY-VALIDATION.md). Each marker, removal and completion scenario needs its own comparison. |
| REQ-HISTORY-007.1–007.3 | The HISTORY cases separately cover bound changes, interrupted reset, shared stale transition in the same epoch and detached observations. Historical grouped assessments do not establish every individual source-gap or generation scenario. |
| QD-CORE-011; REQ-PUB-004 | [MUX qualification](../../tests/conformance/subscriptions/MUX-VALIDATION.md), [queue contract](../mux/QUEUES.md) and [publication contract](RECORD-PUBLICATION.md) retain their separate scopes. Demand aggregation or local delivery alone does not qualify end-to-end publisher output. |
