# Feed API threads and object lifetime

[Русская версия](API-THREADS.ru.md) · [Feed API](FEED-API.md) · [Runtime embedding](../spec/transport/RUNTIME-EMBEDDING.md)

This reference covers the current `Endpoint`, `ApiRuntime`, `Feed`, `Subscription`,
request handles and event executor API. Overloads in each row share its thread rule.
The declarations in [feed.hpp](../include/qd/api/feed.hpp) and
[callback_execution.hpp](../include/qd/api/callback_execution.hpp) define individual
argument, capacity and failure conditions.

## Execution contexts

The **creator** is the application main thread that constructs the runtime. The
**SDK worker** owns native progress and collector state. An **application caller**
may submit producer operations from another thread while the object remains alive.
An **event executor** is an optional application scheduling policy for event listeners.

Producer operations serialize changes to SDK state internally. Applications must
keep each object, input buffer and callback dependency alive for the required call
or callback interval. Copying separate `shared_ptr` handles supports independent
owners; resetting or destroying the same handle must not race a call using it.
Likewise, moving a request or task must not race operations on that object.

A synchronous operation can take a mutex, allocate, copy input or release application
captures. "No completion wait" below means that it does not join native work or wait
for a delivery result. It does not promise lock-free execution or a maximum call time.

## Runtime and endpoint operations

| Operation | Caller and work | Lifetime and completion |
| --- | --- | --- |
| `Endpoint::create` | Creator only; the first call precedes application threads. Validates options and obtains the library-owned runtime. | One independent endpoint per call; the common worker starts lazily. Later foreign-thread calls fail with `WRONG_THREAD`. |
| `ApiRuntime` constructors | Creator only. Owned mode acquires bootstrap before application threads. Borrowed mode requires an already prepared bootstrap. | The bootstrap lease survives runtime close and is released at runtime destruction. The host retains a borrowed bootstrap until then. |
| `ApiRuntime::createEndpoint`, both overloads | Application caller; validates and copies configuration, reserves an endpoint under the runtime control lock. No connection or listener invocation. | Calls may coexist while the runtime object stays alive. Admission races with shutdown through that lock; closed admission fails with `CLOSED`. The caller synchronizes runtime destruction. |
| `ApiRuntime::closeAndAwaitTermination` | Creator only; closes all endpoints and joins its worker. | Waits for native work and selected callbacks/captures. Reentrant shutdown or capture-retirement entry fails with `INVALID_STATE`. |
| `ApiRuntime` destructor | Creator only; performs joined shutdown. | Wrong-thread destruction terminates. Owned bootstrap cleanup follows application-thread retirement. |
| `Endpoint::getFeed` | Application caller; returns the shared facade. | Retaining the facade keeps detached control state, without extending the runtime's native lifetime. |
| `connect`, `connectTape`, `connectOnDemand` | Application caller; validates/copies arguments and admits asynchronous source work. | I/O runs on the SDK worker. Inputs are borrowed during admission; accepted source data is owned. Fixed-source and retry policies still apply. |
| `disconnect`, `disconnectAndClear`, `reconnect` | Application caller; commits intent and delivery fences. No completion wait. | A selected callback may finish. Disconnection and state observations alone do not establish callback retirement or completed cache clearing. |
| `setTapePaused`, `restartTape`, `stopTape` | Application caller; submits tape control. No completion wait. | Restart fences the old source; repeated pending restarts join it. Stop/close take precedence. Actual terminal result comes from endpoint await. |
| `setOnDemandSpeed`, `pauseOnDemand`, `stopOnDemandAndClear`, `stopOnDemandAndResume` | Application caller; submits replay control. No completion wait. | The worker applies the transition. Owned playback observations and diagnostics describe its progress. |
| `setRequestedAggregationPeriod` | Application caller; commits a local revision. No completion wait. | Accepted local intent survives offline/closed handles; it is independent of the peer's reported range. |
| `createFilter` | Application caller; synchronous classifier preparation without an endpoint control lock. | Ordinary categories start no I/O. An explicitly supplied context resolver may perform its finite bootstrap wait. The returned filter owns its classifier state. |
| `close`, endpoint destructor | Application caller; requests closure without joining. | Ordinary admission exhaustion cannot prevent close. Destruction does not establish callback quiescence. |
| `closeAndAwaitTermination` | Creator only; requests close and waits for actual retirement. | Includes native, selected callback and capture retirement. Repeated calls return the retained local result. Foreign/worker calls fail before waiting; reentrant retirement fails. |
| `awaitState` | Creator only, including after runtime retirement. | Waits for current state, another terminal state, cancellation or its steady-clock deadline. It does not close the endpoint or wait for observer quiescence. |

Endpoint observations `getState`, `getDiagnostic`, `getStatistics`,
`getEffectiveConfiguration`, `getRequestedAggregationPeriod`, `getAggregationPeriodInfo`,
`getOnDemandPlayback`, `getOnDemandPlaybackDiagnostic`, `getStateChangeDiagnostic`,
`getEventExecutor` are application-caller operations. They copy accepted state or return
an owning policy handle without progressing I/O or waiting for callbacks. An owning
snapshot remains valid after the endpoint changes. Allocation and profile refusals
remain those of the particular method.

Endpoint listener registration/removal and `setEventExecutor` follow the callback
rules below. Configuration acceptance does not schedule a listener inline.

## Feed and subscription operations

| Operation | Caller, blocking and reentry | Lifetime and shutdown |
| --- | --- | --- |
| `Feed::createSubscription`, `createTimeSeriesSubscription` | Application caller, including a callback; synchronous validation and atomic registration. | The returned subscription owns a detached control port. A closed endpoint refuses admission. |
| `Feed::getLastEvent`, `getLastEventIfSubscribed` | An external caller can wait up to the local one-second request deadline. A call on the owning SDK worker reads its serialized state directly. | Returns an owning event/default/absence according to the method. No network progress runs on the caller; busy/timeout admission can fail. |
| `Feed::getIndexedEventsIfSubscribed`, `getTimeSeriesIfSubscribed` | An external caller can wait for a worker snapshot under the local one-second request deadline. A call from its owning SDK worker returns an incomplete observation instead of waiting. | Requires existing interest and a current source. Returns an owning result with explicit completeness; it does not create temporary interest. |
| `Feed::requestLastEvent`, `requestLastEvents`, `requestIndexedEvents`, `requestTimeSeries` | Application caller; synchronous admission, asynchronous completion. | Coordinates are copied, requests own cancellation responsibility, and the admitted deadline bounds the operation. These calls do not connect an endpoint implicitly. |
| Subscription `addSymbols`, `removeSymbols`, `setSymbols`, `addSelectors`, `removeSelector`, `setSelectors`, `clearSymbols`, `setFromTime` | Application caller; validates and commits the desired change. Accepted-symbol observers may run synchronously on the calling thread after commit. Reentrant changes use the finite deferred driver. | Borrowed inputs expire at return. Failure before commit preserves the old desired revision. Selected data callbacks can finish. |
| Subscription `setAggregationPeriod`, `setEventExecutor` | Application caller; commits policy for later selection. No delivery-completion wait. | An existing interval or selected callback retains its original policy. Policy replacement does not invalidate an executing packet. |
| Subscription observations | `getSnapshot`, `getFromTime`, `getStatistics`, `getAggregationPeriod`, `getAggregationPeriodInfo`, `getHistorySnapshot`, `getEventTypes`, `containsEventType`, `getState`, `getDiagnostic`, `getChangeDiagnostic`, `getStateChangeDiagnostic`, `getCallbackDispatchDiagnostic`, `getEventExecutor`: application caller, no native progress or callback wait. | Results own their values or policy handles. A subscription HISTORY observation is distinct from the Feed queries above: it copies the published observation and qualifies it against current fences. |
| Subscription `close`, destructor | Application caller, including a callback; requests close without joining. | Does not wait for the active event packet. The runtime completes retirement; ordinary capacity exhaustion cannot prevent close. |
| Subscription `awaitState` | Creator only; the same state-wait rules as the endpoint. | Remains creator-affine on a detached terminal handle. `CLOSED` alone is not an observer-retirement barrier. |

The one-second local read deadlines above describe current implementation policy.
They are not network freshness guarantees or hard limits on OS scheduling. Use the
asynchronous request API when the application needs an explicit operation deadline.

## Callback channels

| Registration/removal | Execution and reentry | Retention and failure |
| --- | --- | --- |
| Subscription `addEventListener` / `removeEventListener` | SDK worker by default; selected `CallbackExecutor` otherwise. Serial and non-overlapping per subscription, including nested task execution. Producer operations may be called from listeners. | The span is borrowed until the invocation returns. Copy an event to retain it. A selected listener may finish after removal. Listener failure fails that subscription with `CALLBACK_FAILURE`. |
| Endpoint or subscription `addStateChangeListener` / `removeStateChangeListener` | SDK worker; creator-thread retirement if the worker never started. No initial callback. | Selected observers may finish after removal. Exceptions are advisory in `getStateChangeDiagnostic`; endpoint await includes their retirement. |
| Subscription `addChangeListener` / `removeChangeListener` | An uncontended producer dispatches after commit before returning, even offline. Concurrent/reentrant mutations reserve deferred phases; the existing callback owner completes them. | Borrowed changes expire at callback return. Removal reserves a final CLOSED phase and can refuse before mutation. Exceptions remain advisory in `getChangeDiagnostic`. |
| Endpoint `addOnDemandPlaybackListener` / `removeOnDemandPlaybackListener` | Serial advisory delivery on the SDK worker, outside control locks. | Selected calls can finish after removal; failures are retained in `getOnDemandPlaybackDiagnostic`. |

Application calls and selected capture/policy destruction occur outside SDK control
locks. Captures can be destroyed on the thread releasing their last retained owner;
the application must make that destruction valid there. Removing a registration is
not a join. Retain objects borrowed by endpoint/native subscription listeners through successful endpoint await. Independent subscriptions have the additional ownership rules below.

Different subscriptions can already run on separate application executor workers.
A slow default listener blocks the shared SDK worker; a custom policy lets sibling
subscriptions progress, subject to host queue capacity and source-retirement fences.
The SDK does not stop host workers or preempt application callbacks.

`CallbackExecutor::post` must return promptly and accept only a successfully
transferred task. It may execute inline. Rejecting or throwing after execution has
started is an executor contract failure. `CallbackTask::run` supports concurrent or
repeated claims: one wins, older/cancelled claims are inert. Nested execution can
return `DEFERRED`; the existing driver completes it after outer retirement. A task's
move, assignment or destruction must not race its member calls. Dropping accepted
unrun work is observable. Full outcomes and reservations are in the
[callback execution contract](../spec/subscriptions/CALLBACK-EXECUTION.md).

## Independent subscriptions across feeds

[IndependentSubscription](../include/qd/api/independent_subscription.hpp) owns a desired
selector set and handlers independently of an endpoint. Construction and
`withAttachmentListener` run on the application caller and start no connection.
`attach`, `detach`, `detachAndClear`, `setSelectors`, `getSelectors`,
`getAttachmentDiagnostic` and `close` synchronize their state internally. The
application keeps the object and any supplied Feed alive during the call and
synchronizes object destruction against concurrent access.

Each attachment has its own native subscription. Different feeds can invoke the
same independent listener concurrently, including from different runtimes. The
application synchronizes shared listener state. This differs from the serial rule
for a single native Subscription and is part of the current API contract.
`setSelectors` commits the local desired set, then reports per-attachment refusals;
it does not provide an atomic transaction across feeds.

`detach` and `close` fence new selection without waiting for selected callbacks.
The destructor calls `close`. `detachAndClear` invokes its nonthrowing clear handler
inline when nothing is selected, otherwise once from the last selected callback's
exit, outside the independent object's lock. Reentrant detach is supported. An
accepted clear survives later close or reattachment and names the old attachment ID.
For borrowed handler dependencies, first stop and join application calls into the
independent object, including inline clear handlers. Close the independent subscription
to release its own retained handlers, then await the corresponding endpoints on their
creator threads for selected data and deferred clear calls to retire. Only then release
those dependencies. Endpoint await alone neither joins a producer's inline clear nor
releases handlers retained by a still-open independent object.

## Requests and detached data

`LatestEventRequest` and `HistorySnapshotRequest` are move-only cancellation owners.
`isDone` and `cancel` can run on application callers against a stable live handle;
deadline observation may complete an expired request. Destruction or replacement
cancels a pending request and releases its state. Move/destruction requires caller
synchronization against other calls on that same object.

`LatestEventRequest::await` and `HistorySnapshotRequest::get` wait until completion or
the admitted deadline. Pending waits from the owning SDK worker fail with
`WRONG_THREAD`; a result already completed can be read there without waiting. These
operations are not restricted to the runtime creator. Application executor callbacks
should avoid waiting for work whose progress depends on their own retirement.
Repeated successful reads return owning values. A moved-from handle has no request;
its wait/read fails with `INVALID_STATE`. `LatestEventRequest::getSymbol` returns a
borrowed view valid while that handle retains the state.

Owning events, immutable snapshots and statistics survive runtime shutdown. Borrowed
callback spans do not. Detached endpoint/subscription handles retain terminal state
and diagnostics, with original failures preserved; they cannot restart native work.
Late cancelled executor tasks retain detached pool metadata, not live sockets or
callback payload. Each retained object's memory remains owned until its final release.

## Completion and the accepted evolution

For endpoint/native subscription callbacks, use `closeAndAwaitTermination` on the creator before releasing their dependencies; independent subscriptions additionally follow the shutdown order above.
`CloseResult::drained` reports successful local admitted-prefix drain. Subscription
failure and application output failure remain separate checks. Drain timeouts cannot
preempt user code or guarantee an upper bound on native OS retirement.

The accepted next architecture keeps each logical subscription serial across event
and future record delivery, permits independent subscription work in parallel, and
uses owning immutable data for transfer between threads. The planned public
`RecordFeed`, common configurable SDK executor and unified result/coroutine API are
not yet available. This reference describes today's callable methods; their future
extensions must preserve ordering, source fences, exact retirement and owned results.