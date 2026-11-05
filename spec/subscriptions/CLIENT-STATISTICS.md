# Layered client statistics {#client-statistics}

[Русская версия](CLIENT-STATISTICS.ru.md). English is authoritative.

The endpoint IO/recovery subset is available through `Endpoint::getStatistics()`;
see the [feed guide](../../docs/FEED-API.md#endpoint-statistics) for its publication boundary.
The subscription catalog is available through `Subscription<Event>::getStatistics()`;
see the [delivery accounting boundary](../../docs/FEED-API.md#subscription-statistics).
The value-only helpers in [statistics.hpp](../../include/qd/api/statistics.hpp) provide reader-owned baselines, interval rates and callback means.

## Evidence and decisions

Java QD 3.355 is pinned by [upstream.lock](../../upstream.lock) to
0ddc0a09779fb61d74cc0b37c6449ff47dcb715f:

- [QDStats.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/stats/QDStats.java)
  defines separate byte, subscription-record, data-record, lag and RTT accumulators,
  plus record added/changed/filtered/removed and derived size values.
- [IOCounters.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-monitoring/src/main/java/com/devexperts/qd/monitoring/IOCounters.java)
  reports interval byte/record rates per second. Read/write data lag is the
  corresponding lag delta divided by the data-record delta. RTT is the sum of
  read/write RTT deltas divided by all read/write data/subscription-record deltas.
  Published lag/RTT units are microseconds; zero denominators produce zero in
  that Java report. These formulas do not prove a C++ measurement exists.
- [DXFeedSubscription.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedSubscription.java)
  documents getTotalDroppedEvents() as cumulative buffer-overflow loss since
  subscription creation, not filtering, conflation or inferred upstream loss.

The catalog and observation rules below are C++ engineering decisions, separate
from those upstream facts. They do not copy Java monitoring topology or wrapping
arithmetic and do not change wire semantics.

Existing [StreamCounts](../../include/qd/collectors/stream.hpp) is serialized
collector evidence: staged records, explicit pop acknowledgements, withdrawals,
discards, ignored input and saturation. Source reset preserves its totals.
A collector pop does not prove a successful application callback.
Existing [QueueResources](../../include/qd/mux/queue.hpp) reports logical charges,
including retained generations; independently sampled fields can show adjacent
instants and quiescent values are exact. Shared backing and eventBytes overlap
forbid summing every byte field or interpreting it as RSS. This contract does not
strengthen those APIs or change their resource bounds.

## Common rules for every catalog row

Every observation carries a stable owner identity, monotonic observation time,
statistics generation and source epoch. Opaque identities must not alias old
owners. A source epoch identifies one established transport/session source,
not a desired-state revision or statistics baseline. An unestablished attempt has
its own attempt identity and explicitly no active source.

Unless a row says otherwise, totals start at zero on successful owner creation,
accumulate until owner retirement, survive reconnect and selector changes, and
count the named items. Gauges describe the observation instant. Rejected owner
creation publishes nothing. Rejected operations do not charge admitted work;
bytes already transferred remain charged. Endpoint lifetime totals aggregate
every connection epoch; an optional per-epoch view contains only that epoch.
Queued old-source work retains its origin through retirement.

Availability is explicit per field: available, unsupported, no sample, saturated.
Unsupported means no verified producer for the selected backend/profile; no sample
means a supported producer has not completed a measurement. Neither is numeric
zero. Available idle counts are zero. Diagnostics/lifecycle remain separate;
missing statistics neither create nor hide failures. The table's names are
semantic labels, not promised C++ field declarations.

All new cumulative counts and duration sums saturate at UINT64_MAX with a sticky
per-field marker. Maximum duration uses the same bound. Derived values using a
saturated operand are unavailable. Gauges remain within validated instance limits.
No wrapping or extra allocation is allowed. Identity exhaustion prevents a new
identity with explicit lifecycle diagnostic rather than aliasing old samples.
Existing lower-level size_t counters retain their existing contracts.

A snapshot is a detached, bounded owning value retaining no socket, callback,
schema or runtime. It remains readable after shutdown. Publication provides a
coherent owner-local cut and timestamp/generation; separate endpoint/subscription
reads are not a global atomic cut. Reads perform no network progress, DNS,
callback dispatch, callback wait or reset. Final totals freeze after actual
callback/capture quiescence, not merely on a close request.

## Endpoint catalog

Each transport/protocol occurrence is counted once before subscription fan-out.
Each row inherits the common start/lifetime/overflow/availability rules.

| Measurement | Layer; unit | Charge/source and availability |
| --- | --- | --- |
| readBytes, writtenBytes | Transport; octets | Positive bytes actually returned by successful read/write progress, including partial I/O, not requested buffer length. Plain TCP counts QTP stream octets. TLS must distinguish plaintext QTP from encrypted transport octets; uninstrumented layers are unsupported. |
| readMessages, writtenMessages | QTP; complete frames | Read: structurally accepted complete frames, including control/unknown types with supported framing, before payload processing. Write: complete frames whose last byte crossed the declared QTP boundary. Partial/malformed/abandoned frames do not count as complete. |
| readDataRecords, writtenDataRecords | QTP; record occurrences | Complete validated records committed by decoding, or records in fully transferred outgoing messages. Before projection/filtering/fan-out; atomically rejected payloads contribute zero records. Unsupported outgoing data profiles are explicit. |
| readSubscriptionRecords, writtenSubscriptionRecords | QTP; ADD/REMOVE record occurrences | Complete validated incoming records or records in fully transferred outgoing messages; actual replay after reconnect counts again. Desired selectors not yet emitted do not count. Unsupported incoming profiles are explicit. |
| connectionAttempts | Connection lifecycle; attempts | Once when an attempt starts under the finite policy, including DNS/connect failures. Repeated no-op connect does not count. |
| establishedConnections, closedConnections | Connection lifecycle; connections | Actual establishment and retirement respectively; a failed unestablished attempt is not a closed connection. Each established source retires once, including close/failure/recovery. |
| activeConnections | Connection lifecycle; connections, gauge | Established, not retired connections; zero initially and after retirement. Does not imply peer application readiness. |
| dataLagRead, dataLagWrite | QD wire timing; microseconds, weighted average | Verified protocol timing accumulators and matching record weights in one epoch/interval only. Currently unsupported at the facade; event timestamps, wall-clock subtraction and callback duration are not substitutes. |
| roundTripTime | QD wire timing; microseconds, record-weighted average | Verified QD RTT accumulators and documented weights only; not TCP ping or connect duration. Currently unsupported at the facade. |

No generic received-event total aliases bytes, records and subscription events.
Optional per-record breakdowns require validated schema identity; numeric record
IDs from incompatible schema epochs must not merge. No unbounded per-symbol map
is required.

## Subscription catalog

The owner is one subscription. Units are event occurrences unless stated otherwise.
Two overlapping subscriptions count two independent occurrences. Listener fan-out
does not multiply subscription event counts. All rows inherit the common rules.

| Measurement | Layer; unit | Charge/source and availability |
| --- | --- | --- |
| desiredSelectors | Desired state; distinct selectors, gauge | Successfully accepted current set, including pending reconciliation. Duplicate/no-op/rejected mutations do not increase it. Clear/close withdraws according to existing lifecycle. |
| activeSelectors | Local source mapping; distinct selectors, gauge | Worker-applied mapping, qualified by source epoch and desired revision; not peer acknowledgement. Zero before mapping/after retirement; reconnect may temporarily differ from desired. |
| admittedEvents | Delivery admission; occurrences | Accepted into this subscription's bounded delivery state, including events later conflated/discarded/failed. Rejected incoming events are not admitted. |
| deliveredEvents | Subscription dispatch; occurrences | Admitted events whose dispatch to the selected listener set completed normally. No listeners means no completed dispatch; existing pending policy applies. Does not prove user persistence or external acknowledgement. |
| droppedEvents | Delivery overflow; occurrences | Known buffer-overflow loss only: rejected incoming events plus evicted admitted victims. Lifetime meaning matches Java getTotalDroppedEvents(); excludes conflation, clear/close and unknown upstream gaps. |
| rejectedOverflowEvents, evictedOverflowEvents | Delivery overflow breakdown; occurrences | Respectively incoming events never admitted, and previously admitted queued victims. Same owner/lifetime and coherent cut as droppedEvents. No double counting. |
| conflatedEvents | TICKER delivery; superseded occurrences | Previously admitted queued values replaced by a newer accepted value. Excludes STREAM loss and byte/record coalescing. Available zero for non-conflating profiles. |
| discardedEvents | Local cancellation; occurrences | Admitted events retired by remove/clear/close/abort/epoch policy without completed dispatch, excluding overflow, conflation and callback failure. Cleanup never claims delivery. |
| callbackFailedEvents | Dispatch failure; occurrences | Events in an admitted batch whose selected-listener dispatch failed due to a throw. Count once even if earlier listeners saw them. Remaining queued events retired by failure are discarded. |
| pendingEvents, inFlightEvents | Delivery state; occurrences, gauges | Queued retained events and selected, not yet finalized dispatch events respectively. Conflated latest values count once; zero initially/after retirement. Selected callbacks remain in flight until finished. |
| queueCapacityEvents | Delivery configuration; slots, gauge | Effective finite per-instance queue capacity/reservation; not free space/RSS. Reads do not allocate or grow capacity. |
| callbackCalls, callbackFailures | User invocation; calls | Each actual listener invocation and each throwing invocation respectively; multiple listeners may mean multiple calls per batch. Existing failure policy stops dispatch. |
| callbackDurationTotal, callbackDurationMaximum | Local listener timing; nanoseconds, sum/maximum | Steady-clock durations around completed user invocations, including throws/blocking, excluding queue wait/wire lag. Total starts at zero, maximum has no sample before first completion; timing is no sample until a call finishes. |
| callbackDurationSamples | Local timing; completed calls | Denominator for local mean duration, including normal/throwing calls; same generation/source as durations, zero before first completion. |

Here listener/callback means an event-delivery listener only. Lifecycle/state
observers are excluded from callback counts, durations and failed-event accounting;
their advisory exceptions and separate observer diagnostics do not fail event delivery
or replace the primary diagnostic. No observer-counter set is specified here.

Event-delivery callback failure preserves first diagnostic, failure and no-retry behavior.
Statistics authorize neither callback preemption nor earlier capture destruction.
Unknown upstream gaps stay qualitative unless a protocol producer gives an exact
count. Resource/work enforcement counters and budgets are never reset or refunded
by statistics reads, baselines or observation-generation changes.

## Reset and derived values

There is no destructive public reset of owner-lifetime totals in this contract.
Consumer reset records a baseline and generation without mutating delivery.
Lower-level resets do not reset facade totals. New owners start new generations;
reconnect changes source epoch, not statistics generation. A future destructive
reset requires an explicit contract revision specifying atomic generation change
and crossing work. Reset of observation must never refund actual work/resources.

Rates require two available unsaturated observations of the same owner,
generation, metric definition and scope, with positive monotonic elapsed time.
Lifetime rates may cross reconnect only when labelled as such; epoch rates require
the same epoch. Missing fields, incompatible generations, source boundaries for
epoch rates, saturation and nonpositive time yield unavailable, not wrapped,
negative or invented zero. Use checked deltas/scaling without intermediate
overflow; units are items/second or bytes/second with documented precision.
Means require positive matching sample deltas; combine numerators/weights, never
average interval averages. Readers and their baselines are independent.

The public `StatisticsBaseline` owns a detached cut. Reset replaces that reader's
copy only; readers synchronize shared mutation of the same baseline themselves.
Lifetime rates explicitly use `LifetimeStatisticRate` and can cross reconnect.
Epoch rates require the same positive, non-saturated established source ordinal;
endpoint qualification also requires available established-connection counters.
The selected member identifies the same cumulative metric at both cuts; the
callback duration sum and maximum cannot be selected for these count rates.
A sticky unknown write acceptance makes written-byte rates unavailable without
affecting other fields. Rates use floating-point seconds and return rounded `double`
values; nonpositive or unrepresentable intervals and incompatible, decreasing,
missing or saturated inputs yield no value. Callback means divide the difference
of duration totals by the positive difference of completed-sample counts; an
initial supported no-sample total with zero samples contributes zero. Helpers do
not wait for callbacks, drive network progress, or retain runtime owners.
## Accounting and required implementation cases

These are requirements for later tests, not executed runtime evidence:

1. Partial I/O: a ten-byte frame written as 3 + 2 + 5 yields ten bytes and one
   message; abort after five yields five bytes and zero complete messages/records.
   Valid framing followed by payload rejection counts the frame, zero atomically
   rejected records. A malformed next frame preserves accepted prefix counts.
2. Reconnect: failed DNS then two established/retired sources gives attempts 3,
   established 2, closed 2, active 0. At a coherent cut:
   establishedConnections = closedConnections + activeConnections.
   Totals never decrease; replay ADD counts again, desired selectors do not.
   Old pending work keeps its epoch.
3. Overlap: one decoded record projected to one event matching two subscriptions
   increments readDataRecords once and each admittedEvents once. Two listeners
   affect callbackCalls, not admittedEvents. Endpoint records need not equal the
   sum of subscription events.
4. Before saturation, within one owner lifetime:
   admittedEvents = deliveredEvents + conflatedEvents + discardedEvents +
   callbackFailedEvents + evictedOverflowEvents + pendingEvents + inFlightEvents.
   droppedEvents = rejectedOverflowEvents + evictedOverflowEvents.
   Rejected input never enters the admission identity; final pending/inFlight are
   zero. For interval baselines include gauge deltas.
5. Two admitted same-key TICKER updates replacing the first give admitted 2,
   conflated 1, pending 1. Deliver latest: delivered 1, pending 0, dropped 0.
   Rejected STREAM input increments rejected overflow/drop only; eviction moves
   an admitted victim to evicted overflow. Conflation is not Java-compatible loss.
6. A two-event batch whose second selected listener throws counts both events as
   callbackFailed once even if the first listener returned. Count actual calls,
   failures and completed timing samples. Remove cannot erase selected calls.
   Close during a blocked invocation retains inFlight; abort discards queued
   events. Reads never wait for that call or prematurely claim quiescence.
7. Baselines change neither totals nor another reader's baseline. Equal times
   give unavailable rates. At UINT64_MAX, increments saturate only the affected
   field and disable its derived rate; exact numeric identities are no longer
   provable after saturation. Detached old snapshots survive shutdown and are
   incomparable to a new owner. Reset cannot replenish an exhausted work budget.
8. Uninstrumented wire timing is unsupported even with traffic. A supported timing
   source without matching records has no sample. A 2 ms callback may contribute
   2000000 ns locally; it proves neither 2000 us wire lag nor RTT.

Statistics storage stays inside existing finite instance-local budgets, without
ambient process settings, unbounded labels or retained history. Later verification
must cover concurrent readers, reconnect/failure, consumer reset and terminal
observations. This specification is not race-safety or runtime proof.
