# Local STREAM client dispatch

[Русская версия](CLIENT-DISPATCH.ru.md). English is authoritative.

This layer composes one local STREAM consumer with the
[collector](STREAM-COLLECTOR.md), exact desired interests and immutable delivery
batches. It implements the local [command/callback/lifecycle barriers](CONTRACTS.md).
The composing caller owns a `LiveSession` or `TcpRuntime` and passes its complete
admitted `LiveEvent` values in owner order. Protocol decoding, wire output, sockets,
partial input, TLS, shared upstream MUX and reconnect remain outside this layer.

## Execution and independent ownership

The dispatcher is an externally serialized, nonreentrant logical owner. Direct
data/progress/inspection/move/destruction operations belong to that domain; this
does not create an OS thread per client. Only public command handles, ticket leases
and detached immutable batch/identity handles have the documented cross-thread
submission/copy/query/release behavior. Recursive mutating owner operations from a
callback are rejected rather than starting nested progress; owner-local inspection
does not start callbacks.

Public handles retain only the bounded command/control port, not the dispatcher,
source session or callback. Explicit public-handle lifetime is independent of
internal port references. Last public release requests nonblocking abort; it does
not execute owner cleanup on the releasing thread or prove quiescence.

The application supplies a shared Consumer object. A separate library-held guard
protects the executing callable through return/exception and retirement. No callback
or final library-held Consumer release occurs under a port/container lock. Consumer
allocation and its arbitrary captured application graph are an explicit opaque
application-memory scope; library byte accounting includes its fixed retained
reference, not a guessed transitive size. Such application objects must respect
the lifetime/thread preconditions of anything they themselves capture.

## Admission and prepared batches

`StreamDispatcher::admit` accepts complete compatible STREAM data and source control
events. Other data families are rejected. Control events produce no consumer batch
and enter RELEASE_ONLY directly. Ownership transfer and rejection are explicit;
rejected input remains usable. A single owner-only pending event has UNSTAGED or
RELEASE_ONLY state. Work exhaustion cannot
erase a successfully accepted event. Retry staging/release before subsequent ordinary
mutations, callbacks or source admission; zero selection is not staged twice.

The external driver hands a polled event to the dispatcher before later ordinary
command progress in its chosen source order. Holding an event outside the dispatcher
does not make it dispatcher-admitted data. The first accepted source event pins one
epoch even with zero selected records; another epoch is rejected. A new run/reset
adapter must establish a new dispatcher rather than silently mixing epochs.

Multi-record callbacks have an explicit finite record bound. The collector prepares
an eligible prefix using its existing membership filtering into fixed metadata
storage, including admission revision and original binding generation. The client
reserves/copies the complete detached candidate before atomically acknowledging
that prefix. Work/allocation failure before acknowledgement starts no callback and
does not lose the eligible queued prefix. Single and batch offers are mutually
exclusive; intervening operations cannot acknowledge a stale prepared offer.

Borrowed batch records/text/value spans expire at callback return. Retaining an
owning batch is allocation-free and keeps its quota until the final lease disappears.
Detached backing contains copied values, immutable core provenance/membership,
independent thread-safe run identity and scalar binding generation. It holds no
protocol intrusive owner, port, callable or live operation; final release may occur
on another thread after source/dispatcher destruction.

## Commands and callbacks

Submit preflights a bounded complete command copy and result slot. Full/closed
ordinary capacity refuses admission explicitly. Each accepted command eventually
reports owner-applied, rejected or cancelled-closing under continued owner progress.
Command IDs do not wrap. Completed ticket values are immutable and independently
retained; their slots remain reserved until the caller releases them. Querying a
ticket does not wait for the owner or pump callbacks.

Acceptance is distinct from application and from wire composition/local writes/
peer acknowledgement. This dispatcher implements no automatic desired-to-sent
reconciliation engine. The driver composes and drains its existing source owner's
output explicitly; completed command results retain no output or source owner.

Commands submitted during a callback apply only after return. They cannot revoke
records already in that borrowed batch. Effective remove/clear/replace completion
then prevents future callbacks for retired activations. No callback side effect is
retried. An exception records a static typed failure and bounded numeric evidence,
without copying arbitrary exception text or secrets, and begins visible retirement.

## Stop and truthful completion

Reserved allocation-free, idempotent stop controls are independent of ordinary
queue/result capacity; graceful stop may escalate to abort. Stop acceptance closes
ordinary intake, but its owner cutoff and completion remain distinct observations.
Queued unapplied commands complete cancelled-closing in bounded progress steps.

Graceful stop includes the pre-cutoff pending event and still-eligible staged
prefix. Abort discards local pending/staged data visibly; protocol-admitted but
not classified records are reported without inventing eligibility or delivery.
The first observed non-cancellation failure is not replaced by a later stop or
empty queue. Pending caller-owned batches are separate from pending callbacks.

Terminal publication requires local callback/command/staging quiescence and no
future callback that can start. It does not establish socket/output/partial-frame
quiescence, sink flush or whole-run close; those are composing-driver outcomes.
Caller-held detached batches/results may survive terminal without retaining a
callable/live operation. A caller that stops pumping does not obtain fabricated
completion, and arbitrary blocking C++ callback code cannot be preempted.

Execution-host destruction from its active callback is unsupported; releasing the
last public client handle there is supported. Outside active owner operations,
destructor/move-assignment fallback performs bounded uncredited abort cleanup,
never a clean-drain claim.

## Resource scopes and optional build

Finite limits cover fixed commands/results/controls, copied key/text capacity,
batch records/fields/text/bytes, candidate/executing/externally retained batches
and all requested library backing in the client scope. Old/candidate peaks and
final cross-thread lease release remain accounted. Desired, collector and source
epoch bounds are independently enforced scopes to aggregate explicitly, not a
fictional transitive heap/RSS total. No cumulative tape lifetime quota is added.
Owner and producer work are separate; failed charges are not refunded. Cleanup
performed by an ordinary progress step is preflighted rather than hidden in it.

`ORG_TTLDTOR_QDCXX_BUILD_CLIENT` defaults to OFF. The enabled `qdcxx_client` target
uses the standard platform `Threads::Threads` dependency; no runtime/JSON SDK is
required. CLIENT OFF and RUNTIME OFF preserve the pure library discovery path.
An enabled installed client or runtime package resolves Threads explicitly.

The [validation scope](../../tests/conformance/subscriptions/CLIENT-VALIDATION.md)
describes ownership, callback, work and cross-thread scenarios separately from transport qualification.
