# Public feed state observers

[Русская версия](FEED-STATES.ru.md). English is authoritative.

This contract extends the [public feed API](../../docs/FEED-API.md) with advisory
endpoint and typed-subscription state observers. Existing event delivery, wire
semantics, bootstrap ownership and lazy-worker lifetime remain unchanged.

## Registration and selection

`Endpoint` and `Subscription<Event>` expose `addStateChangeListener`,
`removeStateChangeListener` and `getStateChangeDiagnostic`. A listener takes its
owner's old and new state enums by value. A nonempty callable is required.
Registration returns a non-reused per-owner state-listener identity, independent
of event-listener identities. Registration makes no initial call. A separate
`getState()` query observes current state; registration plus query is not an atomic
subscribe-and-snapshot operation. Every later distinct published transition selects
the complete accepted immutable observer snapshot under the same port mutex as the
state update. Callbacks execute in registration order without control locks.

Removal is idempotent, including after close; it excludes the observer from later
selections. A selected callback can still run after removal or close. Reentrant
removal/addition affects subsequent selections. Callbacks may inspect state and
close or operate on other endpoints. Close fences new ordinary admission before
failure or terminal notification, including natural native termination. `CLOSED`
and `FAILED` are absorbing; retirement retries emit no duplicate/regressive states.

## Execution and bounds

The SDK worker dispatches observations for a runtime after its first accepted
connection starts that worker. For a never-started runtime, creator-thread
endpoint/runtime retirement dispatches inactive state observations without creating
a thread. Distinct inactive endpoints can retire recursively. Such creator-domain
retirement can overlap another endpoint's worker if a callback starts that worker.
Only the claimed execution domain dispatches a given endpoint's observations.
Worker/foreign-thread endpoint awaits reject `WRONG_THREAD`; creator-retirement
self-await and runtime-await reentry reject `INVALID_STATE` rather than wait for
the callback containing them.

The first `connect()` preserves its existing immediate `CONNECTING` observation.
Its selected snapshot occupies one deferred notification slot and is dispatched
before later worker observations on every exit path, including early close, native
activation failure and worker infrastructure failure. Worker-start refusal occurs
before state publication, produces no transition and runs no callback on a producer.
There is no unbounded transition queue or detached notification thread.

`RuntimeOptions::maximumStateListenersPerEndpoint` and
`maximumStateListenersPerSubscription` are positive finite independent capacities,
defaulting to 4. They include accepted, selected and removed-but-retiring boxes.
A slot is released only after final application capture destruction. New fields
follow the previous `dnsPort` field, preserving positional aggregate prefixes.
Empty callback rejects `INVALID_ARGUMENT`; closed admission rejects `CLOSED`;
capacity or identity exhaustion rejects `RESOURCE_LIMIT`; allocation failures at
owned storage preparation reject `ALLOCATION_FAILURE`. Close has a reserved path.
Application callable allocations, retained object graphs and blocking time remain
outside these finite SDK object-count reservations.

## Errors and quiescence

An observer exception retains the first static/numeric `CALLBACK_FAILURE` in
`getStateChangeDiagnostic()`. Remaining callbacks in the complete selected snapshot
still run. This advisory diagnostic never changes owner state, `getDiagnostic()`,
`CloseResult` or the normal transport/subscription error policy. Terminal observer
exceptions cannot produce `CLOSED` to `FAILED` or recursive `FAILED` notifications.

Native registrations and every pre-existing event callback/capture retire before
`CLOSED` is published. The new opt-in state observer selected for that terminal
notification necessarily remains alive while receiving it. Its callback/capture
retirement is the sole additional category that can remain after `CLOSED`.
Endpoint await and runtime shutdown return only after both event and state ledgers
are zero; endpoint/subscription registry slots remain charged until then. The event
capture-drain phase does not wait for still-registered state observers that must
receive the later terminal observation. All application callable destruction occurs
outside control locks, with creator retirement reentry guarded. Handles may outlive
the runtime as detached terminal ports, retaining both detached diagnostics.

## Compatibility evidence

The pinned [Java QD reference](../../upstream.lock) defines endpoint change listeners
in `dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java` (add/remove documentation)
and dispatches actual distinct old/new observations in
`dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXEndpointImpl.java`
(`StateHolder.run` and `fireStateChangeEvent`). Registration only appends a listener;
it does not send an initial event. The pinned Java [`connect` contract, lines 725–729](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L725-L729)
waits for the first `CONNECTING` notification to finish. C++ intentionally retains
its existing [nonblocking `connect()` contract](../../docs/FEED-API.md#accepted-changes-and-errors):
it does not wait for state observers. Executor dispatch is preserved by the explicit
C++ owner domains above. Java `DXFeedSubscription` instead exposes symbol-change
and closed observation; the existing C++ subscription lifecycle enum and this
bounded advisory API are an explicit idiomatic extension, not Java type identity.
