# Accepted subscription observations

[Русская версия](OBSERVABLE-SUBSCRIPTIONS.ru.md). English is authoritative.

## Owning introspection

`Subscription::getSnapshot()` returns an independent `SubscriptionSnapshot`: one accepted desired `revision`, exact typed logical `symbols`, and the immutable normalized `eventTypes` set. The copy is atomic under the detached subscription mutex. It stays valid after mutations, close, endpoint retirement and runtime destruction. `getEventTypes()` copies the immutable type set; `containsEventType()` returns false for unknown enumeration values. These operations perform no native or network progress.

The accepted revision is distinct from worker application, MUX command tickets, physical demand, wire emission and peer acknowledgement. Existing [command and application barriers](CONTRACTS.md) still apply. Reordering the same effective logical set, duplicate additions, absent removals and empty clears do not change the revision. Invalid input, allocation, finite capacity/work refusal and identity exhaustion preserve the old set/revision.

Introspection retains logical decorators even when physical receiving demand coalesces. PLAIN and INDEXED identities remain distinct; source names are not identity. [Decorated-selector identity](DECORATED-SELECTORS.md) and [ADR-0012](../../docs/decisions/ADR-0012-decorated-subscription-identity.md) remain authoritative. Unsupported typed history activation is not enabled by introspection.

Close fences mutation but preserves the final accepted logical snapshot for detached observation. This is local retained value state, not continuing source demand.

## Observer phases

`addChangeListener()` installs one nonempty callable and returns a non-reused identity independent of event/state listeners. An initial ADDED phase is selected for existing nonempty symbols; an empty subscription produces no initial phase. Native listener IDs identify registrations; equal callable values are not automatically deduplicated.

A successful effective mutation prepares the complete post-change snapshot and actual delta before publishing the new desired set/revision. Replacement reserves both REMOVED and ADDED phases before either can be visible. REMOVED precedes ADDED at the same revision; either empty phase is omitted. Each phase exposes a borrowed `SubscriptionChange`: its immutable full snapshot and phase symbols remain valid through return/exception. Copy the snapshot or selector values to keep independent storage.

One selected fanout retains its immutable listener selection through completion. Removal or close during that fanout cannot destroy its callable/arguments. A reentrant registration enters ordinary fanout only after its own initial state phase. The next phase selects the current initialized registry: self-removal during REMOVED suppresses that registration's later ADDED. A queued initial phase is cancelled when its observer is removed before selection. CLOSED is selected exactly once when a registration is removed or its subscription retires.

Unselected ordinary phases can be suppressed by terminal closure; the reserved CLOSED snapshot carries the final accepted revision. No ordinary callback begins after that observer's CLOSED phase. An already selected fanout may finish before terminal dispatch.

## Execution and finite reentry

An uncontended producer dispatches its accepted notifications synchronously after commit and outside mutation/container locks. This works before connect and after disconnect, without a worker, socket, caller polling or an application event loop.

All change delivery in one runtime shares a detached finite dispatch domain. A callback or concurrent producer joins its existing turn without waiting or invoking recursively. The current owner drains the admitted selections after the callback returns. Per-subscription commit/phase order is deterministic; no cross-subscription global revision order is asserted.

Changes accepted from an existing data callback retain their high-level accepted revision immediately, but change delivery is deferred until the complete selected data-listener fanout returns. Physical command application remains at the existing owner barrier. A borrowed data batch cannot be revoked retroactively.

Cross-runtime mutations retain the existing receiving API support. Admission reserves a bounded after-return target continuation and complete target payload before commit, debiting the causal root, target and active continuation driver capacities without waiting. Exact weak source-callback barriers, with separate data/change epochs, prevent a concurrent foreign dispatcher from selecting the new phase before that callback and its selected captures return. The executing source keeps its detached domain alive; weak expiry therefore cannot mean an executing source. A causal root lease survives a foreign-owner handoff and prevents budget refill while its pending/executing phases remain. Continuations are drained iteratively outside locks, including at existing data callback exit, without a new worker or caller polling. Blocking creator waits/shutdown reject callback/retirement reentry rather than waiting for their own captures.

`maximumChangeListenersPerSubscription` includes active, selected, removed and capture-retiring callables. `maximumPendingSubscriptionChanges` includes pending and executing phases until final argument/selection release; two-sided replacement needs two available slots. `maximumSubscriptionChangesPerTurn` bounds shared phase/registration admissions throughout the whole outer runtime turn. Reentrant/concurrent producers cannot refill it. Exhaustion refuses admission before revision/registry publication.

Each phase/registration is bounded by existing symbol count/units and the immutable supported type count. Complete candidate, delta and snapshot storage is allocated before commit. Terminal storage is reserved per charged observer separately from ordinary capacity. Last callable release occurs outside all control locks and releases its charge only after application captures have been destroyed. Arbitrary blocking or looping application code remains unpreemptible.

## Close and failure

`Subscription::close()` and its destructor preserve the existing nonblocking request contract: publish the close fence and reserve terminal selection without invoking arbitrary application code inline. Actual existing worker retirement or creator `closeAndAwaitTermination()` delivers CLOSED, including an offline runtime that never started a worker. The caller does not pump or poll. Another already active local callback owner may deliver the reserved terminal phase after its current callback returns.

Endpoint await includes selected change callbacks and final capture destruction. A state observation alone is not that quiescence barrier. Repeated remove/close does not repeat CLOSED. Ordinary notification fullness cannot reject close or lose terminal selection.

Observer exceptions are advisory: selected fanout continues, side effects are never retried, and `getChangeDiagnostic()` retains the first static `CALLBACK_FAILURE`. They do not fail event delivery, alter desired demand or replace the lifecycle diagnostic. Event-listener failure policy is unchanged.

## Pinned Java evidence and explicit native differences

[Production capture](../../tests/fixtures/subscription-observer/README.md) uses Java QD 3.355 at [upstream.lock](../../upstream.lock): `DXFeedSubscription`, `ObservableSubscription` and `ObservableSubscriptionChangeListener`. Actual membership and removal-before-addition are compared directly in native tests. Raw Java traces, including monitor ownership, recursive removal and propagated exceptions, are preserved unchanged.

Java uses synchronous monitor-protected observer callbacks and can recursively invoke CLOSED during listener removal. Native callbacks run outside container locks, defer reentry/contention, use finite admission and contain advisory exceptions. Native close remains an asynchronous retirement request. These are explicit receiving-profile timing/error differences; Java monitor behavior is not classified as a defect. Symmetric decorated identity is governed separately by ADR-0012.

The observer API does not add a publisher subscription, dynamic event family, `Subscription.setFilter`, source scheduler or network progress service. The separate [Feed filter classifier](FEED-FILTERS.md) owns query/category and dynamic-update semantics; ordinary remove/clear and shared-demand barriers remain unchanged.
