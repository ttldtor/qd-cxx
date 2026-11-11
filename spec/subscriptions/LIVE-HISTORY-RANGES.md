# Live HISTORY ranges

[Русская версия](LIVE-HISTORY-RANGES.ru.md).

The SDK owns range application, physical demand, retained HISTORY views and reset acknowledgements.
Applications do not assemble collectors, provide per-turn work credits or poll for delivery.
This extends [HISTORY receiving](HISTORY-RECEIVING.md) and preserves the
[time-series identity contract](TIME-SERIES-IDENTITY.md).

## Eligibility and accepted state

`Feed::createTimeSeriesSubscription<Event>()` and its immutable type-list overloads return the
existing `Subscription<Event>` facade in time-series mode. The endpoint must explicitly use
`DeliveryContract::HISTORY`, and every selected business type needs an actual verified native
time-series mapper. Recognition by a registry, a caller-declared capability, a timestamp field
or an indexed record layout grants no such permission. The current nine business types are
unsupported; indexed-only Order also does not imply TimeSeriesEvent. A later business mapper
uses this same owned receiver and range path rather than a second storage or callback API.

The mode is immutable. Its initial `getFromTime()` is signed64 MAX. `setFromTime(ms)` captures
an inclusive Unix-millisecond lower bound; all signed64 values are preserved. MAX disables
ordinary historical events while retaining the logical symbol and its physical sentinel demand.
Ordinary-mode range operations reject `UNSUPPORTED_PROFILE`; a mutation after close retains
the existing `CLOSED` admission fence. The retained mode property remains readable after close.

One accepted cut contains the complete effective symbol set, fromTime, immutable type set and
checked logical revision. `SubscriptionSnapshot::fromTime` is absent in ordinary mode. In
time-series mode its symbols are effective TIME_SERIES identities carrying that same range.
Plain input symbols receive the current range under the subscription mutex. Nested/indexed
decorators are not flattened into an invented physical symbol; unsupported live shapes reject
before any state changes. Equality and removal exclude the range attribute from identity.

Changing the range on an empty set still updates its property/revision. Equal values are no-op.
Candidate symbols and complete observer snapshots are allocated before publication. Allocation,
work, observer-capacity and revision-exhaustion refusal preserves the entire old accepted cut.
An observer's post-change snapshot contains the candidate range, never the previous property.
Observer removal-before-addition phases and outside-lock callback lifetime remain unchanged.

## Application and delivery fences

The worker captures symbols and logical revision together. Only a verified mapper's explicit
time policy converts Unix milliseconds into physical record coordinates; an indexed-only route
keeps its raw index-domain bound. Equal physical keys coalesce to the minimum requested bound,
so removing or narrowing one logical view does not withdraw another view's demand.

An immutable detached MUX batch records the logical command identity of its successful mapping
commit when facade accounting is attached. A merely accepted producer revision, submitted command
or APPLIED command ticket is not that mapping receipt. HISTORY callback selection requires this
applied identity to match accepted state. TICKER/STREAM selection semantics are unchanged.

For deferred callbacks, an accepted HISTORY change causes the worker to cancel obsolete
not-started work before waiting for its retirement. A host may retain the inert old task indefinitely;
its late run cannot re-enter the old delivery. Actual callback BEGIN is a claim under the port mutex
against the immutable applied revision. The mutex is released before user code. An invocation that
already claimed BEGIN may finish, even if its first user instruction follows a concurrent mutation.
Cancellation and completion use the existing accounting and final-capture retirement path.

If that revision fence discards actual selected HISTORY delivery, the SDK restarts the initial
snapshot of the affected view over its existing retained cache. This includes every currently
selected key in that view: an unchanged key already delivered earlier may receive another local
snapshot. Business delivery is therefore not exactly once. Removed keys are excluded, sibling views
and the source epoch remain unchanged, and the replay itself emits no physical REMOVE. The new
logical mapping is qualified only after replay preparation commits; initial, queued and callback
retirement evidence must still complete before the new range can report delivered completeness.

Old collector memberships and queued projections are withdrawn by authoritative desired mutation.
Fresh memberships own their own initial cursors; no parallel payload cache is introduced. Range
expansion below retained coverage remains partial until a fresh source snapshot establishes coverage.
An old snapshot tail without a new BEGIN cannot establish that missing coverage. Passive HISTORY
observations retain AS-OF semantics: accepted/mapping/source/clear/retirement mismatch suppresses
current completeness, without I/O or a promise of future freshness.

## Wire, budgets and recovery

An existing physical key's bound-only change is HISTORY ADD with the new bound, without a preceding
REMOVE. Actual key removal emits REMOVE. MAX remains an actual ADD bound. Output refusal leaves the
emitted bank unchanged; newer accepted ranges may coalesce before successful admission. The network
has no multi-frame atomicity or peer-acknowledgement guarantee.

The existing finite outer-turn work object funds snapshot copying, routing, duplicate-key comparisons,
collector reconciliation, publication and cleanup. Checked conservative precharge includes complete
old/candidate backing and worst-case bounded comparisons. Helpers do not replenish spent credits.
After disconnect/reset the latest accepted range and set are replayed into a fresh source epoch;
retained old batches and observation copies remain historical and cannot become current in it.

The [attached Java oracle](../../tools/upstream/live-history-range-oracle/README.md) observes the
pinned production DXFeedTimeSeriesSubscription, underlying QDHistory demand and public callbacks.
It distinguishes these Java observations from native receiving and network-recovery evidence.
