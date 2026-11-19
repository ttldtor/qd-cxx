# Server execution strategy

[Русская версия](SERVER-EXECUTION.ru.md). English is authoritative.

This decision compares the current server owner loop with an adapter using the already
pinned standalone Asio. It selects an initial strategy and the evidence required for a
later switch. It adds no runtime API, dependency, thread or implementation. The
[peer execution design](PEER-EXECUTION.md) continues to govern independent connection
ownership; the [trust boundary](SERVER-TRUST-BOUNDARIES.md) remains unchanged.

## Decision and current evidence

Retain the current low-level socket owner loop as the production baseline. Keep the
protocol, buffers, demand and connection lifecycle inside their serial peer execution
domains. Evaluate a readiness/completion adapter on the existing pinned Asio as a later
implementation when waiting, scanning or wakeup cost is demonstrated to matter. A
backend change does not change peer affinity, byte budgets or callback contracts.
A single domain is the initial configuration, not a permanent scaling restriction.

The current server and the existing receiving adapter have different scopes:

| Existing component | Source-backed behavior | Limit of reuse |
| --- | --- | --- |
| [TcpListener](TCP-LISTENER.md) | Serialized owner, nonblocking accepted sockets, one native attempt per read/write call, no pending handler or readiness registration | No current accepted-socket transfer or foreign-thread I/O API |
| [PublisherServer](../../src/api/publisher.cpp) | One worker; accept, policy, peer service, family queue progress and source admission; peer-slot traversal; mailbox condition-variable wait uses a 2 ms cadence | Current at-most-eight-peer profile; the loop does not provide multiple execution domains |
| [RuntimeCore](../../src/transport/runtime_core.cpp) | Owner-affine caller-pumped Asio context; Windows connected-data completions, Linux readiness followed by one native I/O attempt; finite handler credits | Receiving connection adapter, not an accepted-connection server adapter or shared domain scheduler |
| [RuntimeCore storage](../../src/transport/runtime_core.hpp) | Sole socket owner, immutable pending output, counted pending handlers and callback depth, reserved context/handler arenas | Per-connection qualification does not qualify a shared context, new accept path or larger resource graph |

[Runtime configuration](../../cmake/Runtime.cmake) verifies standalone Asio 1.38.2 and
its complete [source receipt](../../cmake/runtime-profile/asio-1.38.2.sources.json).
The qualified runtime platforms remain Windows x64/MSVC and Linux x86-64/GCC. Existing
Asio, c-ares and TLS selections are separate component requirements; this decision
replaces none of them. In particular, it does not introduce Boost or a new scheduler.

The earlier local Quote/STREAM observation of about 1.2 million source records/s for
one receiver and roughly two producer-process CPU cores motivates attribution. It is
not a cost measurement of the proposed adapter, does not identify encoding as the sole
bottleneck and does not qualify a physical network or a number of OPRA lines.

## Correctness shared by both alternatives

One execution owner performs a connection's reads, parsing, protocol mutation, encoding,
partial-write accounting and close. Independent connections can occupy separate domains;
several connections can share a domain. A descriptor and its close authority have exactly
one owner. A new adapter must first qualify the one-time transfer of an unpublished,
quiescent accepted connection, or a separate owning connection port, under the peer
design. It cannot move a live `TcpListener` connection to another thread through today's
API. Successful adoption consumes the transfer owner once; failed adoption leaves one
well-defined owner for close and its detached outcome. Bootstrap lifetime, unique
connection identity, destination reservations and late activation after shutdown remain
part of that transfer contract.

Use one serial dispatch owner per domain. Dispatching several Asio handlers concurrently
does not permit concurrent `LiveSession` calls or socket mutation. A later multi-threaded
context would require an explicit serial execution boundary and its own qualification;
one strand or queue per protocol layer is not prescribed. Reuse the existing single-
owner protocol path instead of adding an encoding worker and queue by default.
Application callbacks remain outside the network owner and internal locks.

The two scheduling mechanisms must preserve the same service rules:

- Rotate ready connections and keep the successor between turns. Bound each turn by
  measured byte/time quanta and finite operation credits; ready partial portions proceed
  immediately. Existing record-count profile limits remain until qualified replacement.
- Give input, output and reserved control work bounded opportunities. `WOULD_BLOCK`
  yields only that direction; continuous input cannot starve output and blocked output
  cannot prevent subscription revocation or close. A readiness event grants an attempt,
  not permission to drain a connection without a quantum.
- Keep source/collector mutations with their owner. Cross-domain commands carry owning
  immutable portions and independent connection, source, schema/mapping, demand and filter
  identities. The peer design's staged filter/demand barrier applies to either backend.
- Admit input only with destination capacity. Keep the unread suffix before another read;
  park further reads while capacity is unavailable and explicitly resume when released.
  Enable write service only for pending output. Reserve control/close/result capacity
  separately, so DATA saturation cannot consume its only completion route.

### Commands, wakeups and partial I/O

Producer/source commands must own their data and reserve command/result/handoff capacity
before admission. Commit the command under the admission fence, then notify its owner
through a qualified thread-safe wake path. Coalesce notifications for already-pending
work rather than posting a task per record. Before sleeping, the owner rechecks pending
commands, ready continuations and the next deadline together with its wait protocol;
a command arriving between the check and sleep must not be lost. A committed command
must be processed or receive its retained terminal result. Wake failures cannot silently
leave accepted work stranded. The current publisher's mailbox predicate/notification
pattern is evidence for that obligation, not an existing Asio server wake implementation.

An Asio handler records readiness or exact completion evidence against its original
connection/operation identity; bounded owner progress decides whether to rearm it.
The existing receiving handlers do not rearm themselves. Do not turn handlers into
unbounded protocol loops or application callbacks. At most one read and one write
operation per connection are outstanding in the initial adapter design; their distinct
state and reservations survive cancellation until the owned completion path drains.

A frame's backing and exact suffix remain immutable while an operation can access them.
Advance only the reported accepted prefix, even when success races cancellation; keep
uncertain acceptance visible on failure. Do not combine old and replacement frames or
replay unacknowledged DATA automatically. An unstarted revoked frame can be discarded;
a revoked partial frame follows the selected-peer close rule. A stale completion still
settles its old operation's accounting and resources, but cannot mutate a reused slot,
new session, mapping or filter generation. Cancellation alone neither releases buffers
nor acknowledges peer receipt.

### Shutdown and actual retirement

Shutdown fences acceptance and ordinary commands first. Retire pending transfer objects
at their sole owner, then request connection close/cancellation on each assigned domain.
Continue dispatching owned completions and internal callbacks until their counts and
executing depth are zero; account final write prefixes before releasing output. Stopping
dispatch or observing a requested close is insufficient evidence of retirement. Native
close ownership and retry depend on the platform result: retry only a descriptor that
remains live and owned; never retry a released numeric descriptor.

```text
admission fenced -> transfer owners retired -> connection close/cancel
                 -> owned completions and callbacks drained -> domain work quiescent
                 -> worker joined by eligible caller -> bootstrap may retire
```

Preserve the [public thread/close rules](../../docs/API-THREADS.md): an endpoint's `close`
request or `CLOSED` observation is distinct from actual native/callback/capture retirement;
the appropriate await operation establishes its documented barrier. Shared `ApiRuntime`
shutdown affects its owned endpoints, while closing one endpoint does not terminate
unrelated endpoints. No worker or callback waits for its own domain to join. The current
publisher's joined `closeAndAwaitTermination` is a separate API, not a substitute for
Feed endpoint rules. Keep [bootstrap ownership](RUNTIME-EMBEDDING.md) through tokens,
connections, handlers, worker join and the documented runtime destruction boundary.
Neither cancellation nor destructor fallback gains a hard native latency guarantee.

## Complexity and cost comparison

Both alternatives retain the same protocol/collector work. Asio and lock-free queues do
not inherently reduce encoding, copying or filtering cost. The following are cost
categories to measure, not measured rankings:

| Concern | Current owner loop | Asio readiness/completion adapter |
| --- | --- | --- |
| Runnable discovery | Visits configured peer slots and probes nonblocking sockets; idle probes/cadence cost grows with the visited set | Registers operations/waits and dispatches ready results; registration, queue, wakeup and cancellation costs remain |
| Busy connections | Direct native operations with explicit continuation and byte offsets | Handler dispatch and operation state added to the same bounded protocol work; benefit is workload dependent |
| Idle/bursty connections | Cadence bounds next probe opportunity subject to scheduling; wake notifications already serve producer commands | Can avoid repeated socket probes when truly waiting, but requires a qualified combined I/O/command/deadline wake strategy |
| Storage | Fixed listener table, peer/session windows, queues and retained frames | Adds operation/handler/service storage and generation-retaining lifetimes; shared physical payloads still retain per-consumer quota charges |
| Correctness surface | Explicit fair traversal, deadlines and native error handling; no in-flight listener callbacks | Adds cancellation/completion races, executor affinity, retained buffers and drain; can reuse qualified receiving patterns only within proven scope |
| Maintenance | Existing small native adapter has platform branches | Reuses the pinned SDK but needs new server integration and source/resource qualification; library size alone is not a complexity measure |

A worker or queue at every layer adds handoff, synchronization, reservations and shutdown
states without changing ownership needs. Keep adjacent stages on one owner until an
actual boundary or measured isolated workload requires transfer. Use SPSC only for an
actual single-producer/single-consumer boundary; no blanket lock-free requirement applies.

## Transition gate and verification plan

Keep the current loop until a separate implementation proves the following. This page
performs no runtime experiment and claims no adapter cost or speedup result.

1. Preserve the peer design's sole descriptor/protocol owner and accepted-transfer,
   filter-generation and reconnect barriers. Qualify accepted-socket adoption, failed
   adoption, shutdown during handoff, stale results and platform close outcomes.
2. Fund the complete adapter peak before activation: context/service/handler arenas,
   input/output, queued commands and results, shared/consumer retention and overlap during
   cancellation. Do not transplant the receiving adapter's per-connection allowance into
   a shared-context or server configuration without qualification.
3. Prove exact values/order/descriptions and independent peer progress for ordinary
   single-contract and mixed sessions. Cover silent subscribers, continuously readable
   input, blocked output with incoming control, slow/malformed/reconnecting clients,
   partial I/O, control saturation, lost-wakeup races and joined shutdown. Run applicable
   Windows Release and Linux sanitizer checks when implementation changes behavior.
4. Compare the loop and adapter on the same source revision, platform/toolchain, records,
   frame policy, byte budgets, domain count, service quanta and exact-delivery checks.
   Use 1/2/4/8 clients within the current profile; a larger profile requires its own
   admission qualification. Include idle, bursty and saturated traffic plus unequal peers.
5. Separate clean rate/latency runs from attribution instrumentation. Record source rate,
   each receiver's exact count/order and p99, per-direction/control service wait, no-progress
   probes, wakeups, handler/queue operations, copying/encoding CPU and live/retained memory.
   Keep process and thread observation windows, setup, drain and peer receipt distinct;
   never infer phase cost from total CPU or average clients' p99 together.
6. State workload-specific thresholds before measuring. Switch only if demonstrated
   waiting/scan/wakeup improvement or a required integration capability justifies the
   extra lifecycle/resource complexity without unacceptable throughput, per-client
   latency, fairness or memory regression. If encoding or source/collector work dominates,
   address that measured stage separately. Publish requested/effective configuration,
   defaults, units, validation and restart/reconnect timing; do not silently auto-select
   a backend or domain count from hardware.

Changing the pinned dependency or its qualified platform/backend profile requires a
separate decision. Choosing an Asio adapter on the existing pin is not such a replacement,
but still requires the implementation and checks above. Readiness does not add server
TLS, client authentication, new wire channels, a release or owner acceptance.
