# Caller-driven TCP runtime

[Русская версия](TCP-RUNTIME.ru.md). English is authoritative.

For opt-in liveness checks, finite reconnect and desired-subscription replay,
see the separate [connection recovery contract](RECOVERY.md).

`qd/transport/tcp_runtime.hpp` is the optional credential-free TCP adapter for
the [pure live session](../subscriptions/LIVE-SESSION.md). It owns one run and
one protocol epoch. It does not implement TLS, business callbacks, collectors,
reconnection or a subscription-client facade.

## Ownership and progress

Create `RuntimeBootstrap` on the application main thread before other application
threads. Coordinate its SDK reference with every other c-ares user. Keep it alive
until all runtimes and application threads retire, then destroy it on that thread.
A runtime never performs global c-ares cleanup. Violating the borrowed-bootstrap
lifetime/thread precondition terminates rather than cleaning SDK state below an
active borrower.

Before SDK ownership, bootstrap samples native entropy once: Windows reports
failed `RtlGenRandom` as a typed bootstrap failure without SDK cleanup.
Linux `arc4random_buf` has no recoverable status; normal return does not promise
future availability. Native failure/latency remains an explicit platform prerequisite,
as detailed in [ADR-0006](../../docs/decisions/ADR-0006-live-runtime.md).

`TcpRuntime::create` validates and copies borrowed endpoint/server settings, then
adopts the schema and composer only after fallible preparation. Failure preserves
the caller's schema and composer. Numeric IPv4/IPv6 endpoints bypass DNS. Names
are explicit ASCII/A-label input; there is no URI, service-name, IDNA, environment,
hosts-file or system-resolver lookup. Keep the original hostname distinct from
resolved addresses for later TLS identity checks.

`socketSendBufferBytes` requests per-socket `SO_SNDBUF`; zero keeps the native
default. Positive values must fit the native integer option. The OS can adjust
the request, including rounding or doubling it. This setting does not make kernel
socket buffers part of the SDK requested-storage accounting.

`start` activates the run. `poll` performs bounded progress and returns at most
one whole owning `LiveEvent`. Unaccepted session input remains in the owned read
suffix before another socket read. A returned event and its snapshot retain their
immutable protocol storage across later failure or runtime destruction.

All runtime operations, moves, queries, destruction and related protocol-owner
releases are serialized and nonreentrant on the creating thread. Actions returning
`expected` reject a wrong thread; queries/moves/destruction have an owner-thread
precondition. This is not a cross-thread command port.

There is no Asio timer service or timer helper. The caller drives monotonic
deadlines and DNS ticks by polling; `nextWake` is a scheduling hint. The host provides
an explicit progress cadence for pending I/O: the hint is not a socket-ready
notification or permission to ignore readiness until a distant deadline. Linux uses
readiness followed by one native receive/send attempt. EINTR/EAGAIN does not
start an unlimited retry loop in the same turn. Windows uses direct IOCP for
connected data and bounded native nonblocking connect/readiness checks, avoiding
the SDK's select-reactor fallback thread.

## Output and termination

Protocol, heartbeat, description and subscription methods compose into one owned
output slot. An occupied slot rejects another output before composition. No API
accepts arbitrary caller-provided wire bytes or forwards authorization through
this plain adapter. Local composition and socket acceptance are separate from
peer acknowledgement.

`requestClose` is an idempotent abort request, not completion. It stops admission
and new output, destroys the DNS channel outside callbacks, and cancels/closes
owned socket work. Continue polling until `CLOSED` and terminal evidence are
observable. Completion means all owned handlers have returned; no later internal
callback can access retired state. A raced successful write result is accounted
once. Terminal evidence separates primary failure, a distinct secondary failure, reported
write bytes, known discarded input suffix and the local pending output suffix.
Successful Linux native-send counts identify local acceptance. A cancelled or
errored Windows overlapped write can leave acceptance unknown; the terminal flag
preserves that uncertainty. Pending output can overlap an unreported accepted
prefix and must not be described as definitely unsent or automatically replayed.

Peer EOF finishes the protocol input explicitly; incomplete framing is a protocol
failure. Earlier admitted owners remain valid. Destruction without prior close
uses an external host-teardown fallback, including completion drain. That fallback
is not an observable bounded-retirement API or a hard elapsed-time guarantee.

## DNS profile

Use the [qualified source receipts](../../cmake/runtime-profile/README.md), default
c-ares allocators and explicit 1–4 numeric IPv4 DNS servers. Resolve A then AAAA,
with one wire query at a time and no next query inside a C callback. Disable cache,
search, aliases, sorting, EDNS and failed-server probes. UDP-to-TCP fallback remains
inside the same bounded query profile. Always destroy the channel for retirement;
`ares_cancel` alone can leave queries active after an allocation failure.

Only the requested owner name or its bounded, loop-checked CNAME chain in that
response can supply connect addresses. Ignore unrelated records. Relevant escaped
or non-A-label presentation names are unsupported. A CNAME-only reply does not
trigger hidden follow-up queries. Collect both families into the finite deduplicated
endpoint array before serial connection attempts; overflow rejects the result,
without silently truncating addresses.

## Resource and work boundaries

Fixed adapter/input/context/handler capacities and the protocol epoch reservation
count for their whole lifetime. DNS uses a separately source-derived conservative
retained/candidate/temporary SDK allowance, including container capacity and moving
reallocation. These are requested-storage reservations, not allocator overhead,
kernel buffers, process RSS or measured latency. Caller-created independent copies
remain outside the per-run ownership tree.

`TcpWork` and `LiveWork` are caller-turn credits, never automatically reset or
refunded. Native socket buffer requests are charged before submission, even for
partial/cancelled operations. DNS callback attempts share quotas across descriptors;
exhaustion returns WOULD_BLOCK before another native attempt. Full UDP datagrams
are preserved. A complete SDK progress entry requires its conservative parser,
copy and lifecycle work allowance before entry; a deadline cannot preempt parsing.
Control retirement has its own finite allowance and does not depend on ordinary
data credits. Native OS/entropy service availability is an explicit platform
precondition, not a hard real-time promise.

## Build boundary

`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME` defaults OFF. Enabled builds require the explicit
qualified Asio and c-ares roots; SDK source hashes are checked before compilation.
Public headers use standard/project types. The installed static target retains
the native link closure and dependency notices. Runtime OFF neither finds/fetches
SDKs nor changes JSON, recording, replay or pure-library dependencies.

See the [native verification matrix](../../tests/conformance/transport/TCP-VALIDATION.md)
for execution profiles, retained-owner and pending-operation evidence boundaries.

<a id="tls-source-profile-tcp-runtime-en"></a>

## Separate verified TLS interface

The implemented [TLS runtime](TLS-RUNTIME.md) composes the common native/DNS owner with a separate
bounded memory-BIO engine and [resource/work contract](TLS-RESOURCES.md). This plain `TcpRuntime`
remains credential-free and does not acquire TLS behavior from a stored endpoint setting. Shared-core
extraction must preserve the established plain semantics and pass its full regression gates.
TLS header/resource checks do not substitute for those gates or completed TLS backend qualification;
see [current evidence](../../tests/conformance/transport/TLS-VALIDATION.md).

### Current TLS evidence boundary

The TLS runtime is implemented. The focused current-source suite passed 39 native tests: Windows MSVC 19.42 x64 Release /MD, 11,486 assertions; Linux GCC 13.3 x64 Debug ASan+UBSan, 11,625 assertions, with LSan disabled. Full regression passed on each OS: 58/58 tests with JSON enabled and 57/57 with JSON disabled. Generated-reference verification is recorded separately. The refreshed installed package passed four TLS 1.2/1.3 cases across both OSes. These observations do not establish release, complete subscriptions or security certification. The TLS qualification page linked above distinguishes executed cases from source-only reasoning.
