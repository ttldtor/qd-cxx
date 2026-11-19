# Endpoint disconnection, reconnect and clear

[Русская версия](FEED-LIFECYCLE.ru.md). English is authoritative.

This contract extends the [public feed API](../../docs/FEED-API.md). Connection
intent, physical native retirement, source reset and callback quiescence are
separate observations. `NOT_CONNECTED` reports logical disconnection; it is neither
a native-retirement acknowledgement nor a cache-clear completion barrier.

## Accepted operations

`disconnect()` immediately commits `NOT_CONNECTED` and fences new input from the
old native attempt. It preserves desired interests, subscription/listener identities
and already projected SDK cache. Repetition while disconnected does nothing.
`connect()` with the first accepted fixed address resumes using a fresh source
epoch. Another address remains rejected. `reconnect()` requests a fresh attempt
only while logically online; it does nothing before first connect or after explicit
disconnect. `disconnectAndClear()` additionally records a sticky clear request,
including when already disconnected. An unchanged state emits no duplicate
notification. All three methods do nothing in absorbing `CLOSED` or `FAILED`.
`connect()` on those terminal owners still rejects under its existing contract.

These producer calls are thread safe, including callback reentry. They do not run
native work or callbacks inline. Physical intents may coalesce; accepted distinct
state observations and an accepted clear are retained. An already selected whole
event-listener snapshot can finish after a control call from one of its callbacks.
Ordinary event-listener close/exception policy still applies to that fanout.
Controls preserve every normalized event-type selection, including `RegisteredEvent`
subscriptions. Source/clear eligibility is checked before delivery-shape dispatch.

## Finite notification admission

`RuntimeOptions::maximumPendingStateChangesPerEndpoint` is positive and defaults
to 8. Endpoint creation allocates the fixed ring before publication. Pending,
executing and retiring selected snapshots share that capacity. A transition reserves
its slot and a non-reused control revision before changing state, intent, clear or
the delivery fence. Exhaustion throws `api::Error` with `RESOURCE_LIMIT` and leaves
all those observations unchanged. An operation with no distinct transition needs
no notification slot, but still requires a control revision when it changes intent.

One slot remains charged through release of that selection's snapshot, including
application capture destruction caused by final release. Destruction happens outside
control locks. State-listener box capacities separately remain charged through each
box's final destruction. Worker dispatch consumes a finite capacity-sized allowance
before further endpoint progress; reentrant refill cannot extend that drain forever.
Close/failure use the reserved retirement path even with a full ordinary ring.
Earlier accepted observations precede later native or terminal observations.
Registration, removal, exception isolation and terminal observer retirement follow
[the state-observer contract](FEED-STATES.md). State waiters wake on publication,
before observer invocation; [current-state waits](FEED-WAITS.md) are unchanged.

## Retained cache and source identity

A plain pause retires the native attempt and drops undecided decoded/source offers without
resetting the stable adopted queues or their projected cache. Only after genuine
native `SUSPENDED` quiescence may cached projection replay to new/changed offline
subscriptions. It retains the original queue/source identity. The exact accepted
pause revision authorizes that delivery; clear or resume invalidates authorization
immediately. An old held native offer is never relabelled with a newer revision.
The producer port request immediately fences new publication. The subsequent
serialized lower-owner action establishes the retained-cache boundary and requests
funded disposal of an uncommitted source offer before further staging. Already
begun nonpreemptible projection may finish; committed cache is not retroactively
undone. This is not a cache snapshot at the external wall-clock request instant.
Work exhaustion retains the pending cutoff and refuses new source offers until
disposal completes. No source reset or epoch change implements plain pause.

Retained progress uses immutable local delivery origin. Previously queued source,
live or initial notifications, including a TICKER initial queued before pause, do
not become cached replay. Only a cached INITIAL projection newly constructed in
retained-cache progress may start an offline callback. Old unselected FIFO records
are released with actual work, discard and gap accounting. A later final drain
keeps this pause fence; ordinary close without prior pause keeps its existing
prefix policy. Selected callbacks and caller-owned leases remain unchanged.
Clear immediately fences every new stale event selection. Worker progress then
performs the real queue/source reset and required acknowledgements. This API has
no separate public physical-clear completion wait. Future last-value/snapshot read
APIs must honor the accepted clear revision before reading retained state; they may
not expose stale cache during the interval before physical reset acknowledgement.
The current public facade exposes event delivery, not a last-value cache query.
A separate clear after a completed reset requires a new actual source reset,
queue epoch and fresh notices/ACK; an earlier global ACK is not reused. Requests
during an unfinished reset may still coalesce.

Resume/reconnect performs a real old-source reset before replacement attempt
creation. Thus plain pause preserves cache during the disconnected interval; it
does not promise preservation across a fresh source epoch. Desired interests and
registration identities survive reset. Replay convergence/emitted state is reset
even if desired membership did not change, so a fresh peer receives actual ADDs.
Actual factory attempts, work, action identities and finite quotas are never
replenished by lifecycle calls. Caller-owned Event copies and immutable retained
batches remain valid and charged to their original leases through pause, clear,
reconnect and final close. Ordinary close is final and cannot be undone.

Old poll results cannot publish `CONNECTED`/`CONNECTING` or select new old-source
data after a newer accepted producer fence, including rapid disconnect/connect.
Native state publication and event selection validate the exact control revision
under the endpoint mutex. Selection takes endpoint then subscription mutex; no
application invocation or capture destruction occurs under either lock.

## Closing an endpoint and interpreting the result

Choose the completion operation according to the resource being released:

| Observation or operation | Guarantee |
| --- | --- |
| `close()` returns | Further ordinary admission is fenced and local drain is requested. Retirement continues asynchronously. |
| `getState()` or `awaitState()` observes `CLOSED` | Native registrations and event callbacks/captures have retired. Terminal state observers and observer capture release can still be in progress. |
| `closeAndAwaitTermination()` returns a `CloseResult` | This endpoint's native work and SDK-held callback/observer captures have retired, including selected state and OnDemand observers. The result describes the actual local close outcome. |
| `getStatistics().final` is true | The endpoint's final statistics snapshot has been published after capture retirement. Its totals and observation time are frozen. |
| `ApiRuntime::closeAndAwaitTermination()` returns | All endpoints belonging to that runtime have retired and its SDK worker has been joined. |

Call the blocking close on the creating thread before destroying resources used by
callbacks. The SDK worker and other foreign threads reject `WRONG_THREAD`. A
creator-thread subscription-change observer, or reentry into retirement of this
endpoint, rejects `INVALID_STATE`; see the [thread contract](../../docs/API-THREADS.md).
Repeated endpoint waits return the
same detached result. An inactive endpoint can finish on its creator without
starting a worker. A retained lifecycle failure remains `FAILED` after retirement.

Closing one endpoint leaves its runtime available to other endpoints. In the
ordinary `Endpoint::create()` path the shared default runtime has process lifetime.
An explicit `ApiRuntime` provides application-controlled worker shutdown. Its
bootstrap ownership lasts until runtime destruction. Caller-owned event copies,
snapshots and retained immutable data keep their own lifetimes; endpoint retirement
does not invalidate them or promise that process memory returns to zero.

### Meaning of the close fields

| Field | Meaning |
| --- | --- |
| `drained` | The admitted local prefix reached `DRAINED`, without an expired drain allowance or unresolved gap. |
| `deadlineExpired` | The local drain allowance expired and retirement escalated to abort. |
| `hadGap` | Local recovery or queue evidence retained a data gap, discarded input or unknown upstream completeness/eligibility. |
| `diagnostic` | The first retained endpoint failure. It can describe an earlier failure after recovery and is independent of subscription and advisory observer diagnostics. |

For an application that requires a clean local drain, check `drained`. Its
definition already includes the deadline and gap conditions. Preserve the other
fields for diagnostics or an explicit application policy that tolerates a gap.
Subscription errors and application output failures require their own handling.
An application may also require data to have arrived; the close result itself
does not establish that an event was received.

A delivery benchmark additionally checks its expected record count, values,
duplicates and order at every receiver. Local drain alone cannot establish peer
acknowledgement, receipt of unread socket data or complete upstream history. A
reconnect example may deliberately accept a reported gap while requiring a live
connection, healthy subscription and usable output; that policy does not establish
lossless delivery. See the existing [reconnect](../../examples/feed_reconnect/main.cpp)
and [Candle](../../examples/feed_candle/main.cpp) examples for their explicit checks.

The drain allowance bounds the attempt to drain local data. It cannot preempt
application callbacks or impose a hard bound on OS resource retirement. A deadline
or cancellation passed to `awaitState()` affects that observation wait only.
The blocking close still waits for actual retirement. Advisory observer failures
remain available through their separate diagnostic APIs after it returns.

## Java compatibility and lower control

The pinned [Java implementation](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXEndpointImpl.java#L378-L396)
stops/waits before ticker/history clear and then disconnects. C++ preserves
immediate accepted logical disconnection and offline cached replay, but physical
clear is asynchronous as specified above. Caller-owned event values remain owned
values. The existing C++ nonblocking `connect()`/observer dispatch difference
remains explicit in [FEED-STATES](FEED-STATES.md#compatibility-evidence).

The caller-driven plain `TcpRecovery` exposes `requestDisconnect(clear)` and
`requestReconnect()`, with `RecoveryPhase::SUSPENDED` and
`RecoveryStatus::sourceResetPending`. See [bounded recovery](../transport/RECOVERY.md).
The public feed still owns its worker; applications do not poll this lower owner.
TLS policy, supported event families and process/bootstrap ownership are unchanged.