# Public feed current-state waits

[Русская версия](FEED-WAITS.ru.md). English is authoritative.

`Endpoint::awaitState` and `Subscription<Event>::awaitState` wait for a current
lifecycle state without application polling. They take a target state, an absolute
`std::chrono::steady_clock::time_point` deadline (default `max()`) and a
`std::stop_token` (default empty). They neither connect nor close the owner,
advance network work, register state observers, nor consume listener capacity.
Connection, recovery and event delivery remain owned by the existing SDK worker.

## Observation and result

`StateWaitResult<State>` contains `status`, the exact observed `state`, and the
owner's first lifecycle `diagnostic`, copied under the state publication mutex.
The advisory state-observer diagnostic remains separate. Results have this order:

| Condition at the locked observation | `WaitStatus` |
| --- | --- |
| Current state equals the target | `REACHED` |
| A different `CLOSED` or `FAILED` is current | `TERMINAL` |
| The token has a stop request | `CANCELLED` |
| The steady-clock deadline has elapsed | `DEADLINE_EXPIRED` |

This order also applies before the first suspension. An already reached target
wins over cancellation and an expired deadline; a different absorbing terminal
wins over both. Cancellation wins over deadline expiry. A concurrent state change
can occur after the returned snapshot. A target `FAILED` can be reached normally:
the result diagnostic describes the actual lifecycle failure, not an exception
from the wait itself.

These are **current-state** conditions, not remembered-transition subscriptions.
For example, `awaitState(CONNECTING)` called after the endpoint has become
`CONNECTED` does not succeed because it previously passed through `CONNECTING`.
It waits for that state to be current again, or for another finishing condition.
Use [state observers](FEED-STATES.md) when every selected transition matters.
`NOT_CONNECTED` and `CONFIGURED` can already match before `connect()`.

`CLOSED` still proves native/registration and event-capture retirement. Opt-in
state observers can still execute or release captures after that observation.
An `awaitState(CLOSED)` result is therefore **not a quiescence barrier**. Use the
existing creator-thread `closeAndAwaitTermination()` for the actual detached
`CloseResult` and retirement of all native owners and event/state captures. It
retains its existing drain semantics; neither a state-wait deadline nor cancellation
changes the endpoint's drain allowance or imposes a hard process completion bound.

## Threads, lifetime and errors

Only the runtime's creating thread may call these waits, including on handles
that outlive their endpoint facade or runtime. Foreign threads and SDK callbacks
reject with `api::Error` / `WRONG_THREAD`. A creator-thread callback or capture
destructor executing in **any** endpoint retirement context rejects with
`INVALID_STATE`, including a different endpoint and an already matching target.
These checks occur before a ready-result shortcut. Existing distinct-endpoint
`closeAndAwaitTermination()` retirement behavior is unchanged.

An invalid state enum value rejects with `api::Error` / `INVALID_ARGUMENT`.
Cancellation, a different terminal state and deadline expiry are ordinary results;
they do not introduce another transport error or mutate lifecycle diagnostics.
Native C++ mutex/condition failures can propagate `std::system_error`, as for the
existing blocking retirement wait. Application/OS scheduling can delay return.

The detached port stays strongly owned through stop-callback deregistration.
Registration happens before acquiring the port mutex, because a pre-cancelled
token can invoke the callback inline. The callback synchronizes with that same
mutex before notifying. All state publications use that mutex and notify before
calling state observers. The waiting lock is released before the stop callback
is destroyed, because deregistration can wait for a concurrent requesting thread.
This closes predicate-to-suspension lost-wakeup windows without a transition queue,
polling loop, detached worker or application callable under a control lock.

## Console application cancellation

The [standalone example](../../examples/feed_quote/README.md) waits for endpoint
`CLOSED`, terminal failure, its receiving deadline or a shared stop token. A
subscription-failure observer owns a copy of the application's `std::stop_source`
and requests cancellation. That failure remains an error exit; only an actual OS
interruption produces exit 130. The wait does not drive the feed.

The SDK installs no process signal handlers. The example's scoped `SignalStop`
is constructed after `ApiRuntime` bootstrap and before the first lazy worker or
other application thread. On POSIX it saves the creator mask and complete
`SIGINT`/`SIGTERM`/`SIGPIPE` dispositions, blocks interrupt/termination signals,
uses known nonignored dispositions for `sigwait`, and ignores `SIGPIPE` while
output/native work is live. An ordinary reader calls `request_stop`; no async
signal handler does so. Its teardown sends `SIGTERM` only to its own live reader
thread, distinguishes that synthetic wake from real interruption, joins, and
restores the saved dispositions and mask. Pending real signals racing restoration
retain the operating system's prior-policy semantics.

On Windows the example handles only Ctrl+C/Break. A console callback signals an
owned event under a process-lifetime SRW lock; an ordinary reader requests stop.
Handler removal and exclusive clearing of the borrowed handle exclude entered or
late callbacks before native close. Console close/logoff/shutdown keep their prior
OS behavior; graceful drain for forced console termination is not promised.

A main-thread cleanup guard quiesces the SDK while this bridge and output captures
remain alive, including exception unwinding after `connect()`. Partial bridge
construction rolls back each installed mask, disposition, handler and handle.
Destruction wakes the reader even without an interrupt; unrecoverable native
teardown refusal terminates rather than leave a live thread or dangling handler.
No-argument/English/Russian help returns before bootstrap or signal installation.

## Compatibility evidence

The pinned [Java QD source](../../upstream.lock),
[`DXEndpoint.java:783–820`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L783-L820),
distinguishes lifecycle completion (`awaitNotConnected`) from completed processing
and warns against waiting on the endpoint executor. The C++ current-state result,
steady-clock deadline and stop token are an explicit extension of that separation;
they do not claim Java method/type identity or implement Java `awaitProcessed`.

Cancellation lifetime follows the C++ [stop callback registration/deregistration
contract](https://eel.is/c++draft/stoptoken.concepts). Platform bridge behavior uses
[POSIX thread signal-mask inheritance](https://man7.org/linux/man-pages/man3/pthread_sigmask.3.html),
[`sigwait`](https://man7.org/linux/man-pages/man3/sigwait.3.html),
[thread-directed `pthread_kill`](https://man7.org/linux/man-pages/man3/pthread_kill.3.html)
and Microsoft's [console-handler contract](https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler).
