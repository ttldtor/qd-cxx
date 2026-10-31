# ADR-0005 — Live subscription ownership and serialization

[Русская версия](ADR-0005-live-subscriptions.ru.md). English is authoritative.

Status: selected design for upcoming implementation. This records a technical direction;
it does not record project-owner acceptance or existing live subscription/runtime support.

## Context and evidence

The existing C++23 libraries provide owning schemas, generic records and prepared event mappings.
The [binary message contract](../../spec/protocol/BINARY-MESSAGES.md) preserves source families and
schema generations, but rejects subscription messages and introduces no callback contract.
Decoding TICKER/HISTORY records does not establish latest-value, snapshot or transaction delivery.

The [schema exchange analysis](../../spec/protocol/SCHEMA-EXCHANGE.md) distinguishes local record
identity from directional sender IDs and documents cached writer-layout limits during reset.
The pinned [Java source index](../../spec/protocol/SOURCES.md#connection-and-transport) identifies
connection/parser/composer ownership; [P03/P05 captures](../../spec/protocol/CAPTURES.md) provide
bounded remapping evidence. P05 suppresses nondeterministic heartbeat/protocol metadata and is
not a timing or handshake experiment.
These sources motivate separate state boundaries; they do not prove the selected C++ lifecycle.

The existing [STREAM decoder](../../spec/protocol/STREAM-DATA.md) can report a completed prefix
when a payload fails. [Offline replay](../../spec/protocol/OFFLINE-REPLAY.md) defines its own
publication and cancellation behavior. Neither contract establishes concurrent live dispatch.
The new normative reference is [subscription contracts](../../spec/subscriptions/CONTRACTS.md),
with [conformance obligations](../../tests/conformance/subscriptions/CONTRACTS.md).

## Decision

### Separate deterministic state from live ownership

| Boundary | Owns | Constraint |
| --- | --- | --- |
| `qdcxx_core` subscription state | Owning local identities, desired-interest maps, atomic mutations and explicit finite budgets | Pure state operations require nonconcurrent, non-reentrant owner access; no global thread context, callbacks, I/O, clocks or runtime dependency. |
| Live protocol/session | Connection epochs, directional sender tables, parser/composer state, negotiation and epoch-local emitted demand | Recreate connection state for every connection epoch; local identity never becomes a sender ID. |
| Client/dispatch | Registrations, membership activations, owning delivery/control queues, callback serialization and lifecycle | One logical serialization domain and a bounded thread-safe command port; callbacks enqueue mutations. |
| Execution host/runtime adapter | Scheduling, socket/timer operations and completion of owned asynchronous work | Host lifetime extends through quiescence; OS threads and networking/TLS backend remain undecided. |

Logical serialization is selected without selecting one OS thread per client or an execution
framework. Independent state instances have independent ownership. The upper client supplies
cross-thread command submission; the pure core does not become thread-safe through ambient state.

### Keep interest, emission and delivery distinct

Desired interest survives a disconnected session and incorporates offline mutations. Emitted demand
records only what the current connection has emitted. Neither command acceptance nor emission
implies per-key peer acknowledgement, delivery, or synchronization with remote state. Reconnect
re-emits the current desired snapshot after the selected readiness/authentication conditions hold.
Command-port acceptance only transfers an owning command copy; its later application has a separate result.

One run uses one immutable local schema context; another context requires another run. Equal IDs
or equal shapes do not permit cross-context aggregation. Exact selectors preserve owned UTF-16;
the reserved `*` prefix is rejected rather than interpreted as an implicit wildcard. HISTORY bounds
retain signed 64-bit raw record time without inferring Unix milliseconds or business time semantics.

An add/remove/replace/clear validates and budgets its whole candidate before publication. Failure
leaves the previous desired set, revision and memberships intact. A committed removal retires that
membership activation and suppresses its queued deliveries at the barrier. Re-adding the same key
creates a fresh membership token; unchanged keys retain their activation. This prevents an old
queued value from acquiring a new interest merely because its key compares equal.
Add is an upsert: an identical entry is a no-op, while a changed HISTORY bound starts a new
activation for that key. Its collector transition still requires the selected compatibility evidence.

### Make queued values and generation barriers explicit

Every queued value owns its payload and schema provenance. A callback's borrowed view expires
on return; a retained library-owned delivery keeps its quota reservation until its final lease
is released. Prepared event
mappings validate the local schema context before activation. Existing generic/custom records
and extra fields remain available within the selected field/schema profile.

Live admission is atomic for the whole current frame: a failed frame publishes none of its data.
Existing decoder completed-prefix reporting and existing tape/replay behavior remain unchanged.
A fresh connection epoch discards old-epoch pending data and reports reset before new-epoch data.
Cached TICKER/HISTORY state becomes stale/incomplete; sender IDs, pending transactions and queued
data cannot silently cross that barrier. This establishes structural generation ownership; precise
collector recovery semantics still require compatibility evidence. No exactly-once, lossless gap
recovery or automatic retry of callback side effects is promised.

### Serialize callbacks and stop without self-wait

The logical owner orders commands, admissions, callbacks and terminal outcomes. A command enqueued
from a callback can mutate state only after that callback returns; recursive dispatch is forbidden.
Blocking completion waits from the owner/callback context are rejected, and stop cannot self-join.
Callable storage and arguments remain protected until an executing callback returns, including
when the callback removes its registration or destroys its handle.

Handle destruction requests nonblocking stop. It does not prove quiescence or authorize destroying
the execution host. The caller must keep that host alive until callback return and completion of
owned work, and keep pumping a caller-driven host after requesting stop. A callback that does not
return cannot be preempted by a deadline. Graceful drain,
abort, failure precedence and the single terminal result follow the normative lifecycle tables;
completion is observed only after quiescence, when no owned operation or callback can still run.
No callback-side-effect retry is introduced.

### Bound a continuing feed

Construction options expose explicit finite bounds for keys, command/delivery/control queues,
cache/staging owners, bytes/items and per-operation work. Candidate publication accounts for old,
candidate and temporary ownership at peak. Reserved control capacity keeps reset/failure observable
when data capacity is exhausted. Per-client data/control credits are reserved before activation;
retained library allocations keep that reservation even after retirement. Caller-created independent
deep copies and process RSS are reported separately from library-owned resident budgets.

Per-operation work resets at its documented boundary; resident charges are released when their
owners are released. A continuing feed does not inherit a tape's cumulative file/lifetime quota.
Any optional lifetime quota requires a separately explicit policy. The conformance workload fixes
client/rate/record/schema/duration/limit parameters; throughput and RSS remain measurements until
a justified target is selected.

## Alternatives and rationale

| Alternative | Reason for the selected boundary |
| --- | --- |
| Put sessions, callbacks and transport in core | This would couple deterministic identity/budget tests to runtime lifecycle and violate the inward dependency direction. |
| Treat sent demand as acknowledged interest | Current remapping evidence does not establish a universal per-key acknowledgement; separate states preserve observable uncertainty. |
| Reuse parser/composer caches across reconnect | Directional IDs and cached layouts can outlive the assumptions that created them; fresh epochs make invalidation explicit. |
| Publish a failed frame's completed prefix live | It would expose partial live admission; retaining the decoder result separately preserves old offline contracts. |
| Dispatch callback mutations inline | Inline mutation permits recursive delivery and invalidation of executing callable storage; queued commands establish a return barrier. |
| Block in handle destruction | Destruction inside a callback could self-wait; nonblocking stop plus an explicit host lifetime permits safe completion. |
| Copy cumulative tape budgets into live input | A finite-file work quota would terminate a healthy sustained feed despite bounded resident state. |

## Compatibility and remaining work

C++23, existing public APIs/profiles, fixtures and offline behavior remain unchanged. This ADR
selects no runtime, networking/TLS dependency or acquisition mechanism. Core/codecs/events remain
independent of sockets, clocks, JSON, ambient configuration and credentials.

Lifecycle and resource policies are design outputs; generation ownership is selected here only
at the structural level. The pending compatibility probes must establish the six subscription
messages, readiness/authentication, heartbeat/options, collector semantics, wildcard support and
peer-specific reconnect behavior before dependent implementation claims support. Runtime/TCP/TLS
selection remains a separate decision. Required unsupported peer behavior is a visible blocker,
not an implicit fallback. No business-event catalog, persistence, retry framework, server API or
protocol extension is added by this decision.
