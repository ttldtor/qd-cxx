# Bidirectional live session

[Русская версия](LIVE-SESSION.ru.md). English is authoritative.

`qd/protocol/live_session.hpp` defines a pure, incremental QTP session. The caller
supplies bytes, work credits and an explicit transport security decision. The
session owns protocol state and output bytes; it does not own sockets, clocks,
timers, threads or application callbacks.

See the [subscription contracts](CONTRACTS.md), [composer contract](WRITER.md)
and [runtime decision](../../docs/decisions/ADR-0006-live-runtime.md) for the
adjacent boundaries. A session is not a subscription client or a collector.

## Construction and identity

`LiveSession::create` takes a validated local schema and a matching
`SubscriptionWriter` by ownership transfer. The writer must have no outstanding
outputs. Failure leaves both caller inputs usable; transfer occurs only after
fallible preparation succeeds. Limits and security eligibility are explicit and
immutable for the resulting session.

Each successful construction creates a fresh opaque epoch. Peer binding
generations belong to that epoch; equal numeric generations from different
sessions do not establish identity. There is no reset that reuses an epoch.
Local schema identity stays fixed while peer descriptions and effective protocol
properties can change.

## Input admission

`feed` accepts at most one complete envelope per call. Its `accepted` count is
relative to the supplied span: resubmit the unaccepted suffix. Fragmented input
is retained by copying accepted bytes. Empty input is not end of stream; call
`finish` explicitly. Incomplete input at the end is a terminal error.

An event owns a complete admitted envelope. A malformed data or description
frame publishes neither a record prefix nor candidate peer state. Previously
admitted events and snapshots remain valid after later input failure. Inspect
the first static/numeric diagnostic for the terminal error; peer text and
credentials are not diagnostic values.

Empty envelopes, explicit heartbeat messages, protocol descriptors, record
descriptions and data are distinct event kinds. Heartbeat fields preserve
presence separately from zero. Heartbeat time is protocol metadata and does not
implicitly timestamp application records.

`LiveEvent::messageType` preserves the original message identity, including the
distinction between TICKER, STREAM and HISTORY data when there are no records.
Only an empty envelope has no message type. `historySnapshotEnabled` reports
the intersection of explicit local `hs` support and the effective peer option.

Protocol descriptors use the selected `DXP3` grammar. Incoming deltas merge into
effective peer state; omission does not clear a property or capability. The `hs`
option is an exact comma-separated token; unknown option tokens do not imply
support and do not by themselves invalidate a descriptor. Actual unsupported
messages or descriptor extensions are rejected explicitly. Advertising `PART`
does not implement `PART` reassembly.

Snapshot message properties expose local overrides. Resolve inheritance from
the snapshot's top-level properties when interpreting an effective message
property; the view does not materialize a second inherited map. Authorization
values are not published in snapshot views.

## Output and authentication

Control composition is explicit. `composeProtocol` and `composeHeartbeat` do not
read a clock or schedule output. `describe` and `subscribe` reuse the bounded
composer. Subscription readiness comes from the peer's relevant receive-add
capability, not merely its ability to send data.
The requested message itself must also be declared: a remove operation needs
both the family receive-add capability and its receive-remove declaration.

`authorize` is the sole credential output path. It requires adapter-supplied
`VERIFIED_TLS_BASIC` eligibility; the protocol library cannot establish that TLS
was verified. Credential-free plain sessions cannot emit credentials. Successful
composition records local emission, not peer acknowledgement. Authentication
state distinguishes no challenge, a nonempty requirement, local authorization
emission and an explicitly empty effective peer authentication property.

Every output owns its whole immutable envelope. `pendingBytes` exposes a local
partial-write suffix and `consume` advances that cursor only. An invalid advance
leaves the cursor unchanged. Consuming bytes neither acknowledges delivery nor
releases the original backing reservation. Destroy or replace the output owner
to release it. Secret-bearing output must receive the same protection as caller
credentials.

## Ownership and budgets

All access, copying, moving and destruction of a session and its related owners
must be serialized and nonreentrant. This includes releasing a retained snapshot
or output after destroying the session. Cross-thread release is not provided.

Record text, field spans and descriptor views borrow storage from their owning
event or snapshot. Retain that owner while using a view. Moving an event transfers
the lifetime of its existing views. Snapshot handles retain immutable peer
generations; replacing current peer state does not mutate older handles.

`LiveLimits` separates frame, logical collection, backing-capacity and owner
bounds. Fixed snapshot, event and scratch arenas have no fallback allocator.
Their full capacity counts, including unused space and allocations superseded
inside a monotonic arena. Old owners, candidate state and temporary parsing can
coexist and all count. Fixed epoch reservations remain while epoch leases exist.
Raw control output reserves `outputBytes + 9` bytes for compact-length staging;
the exposed whole envelope still cannot exceed `outputBytes`.
`residentBytes` and `peakBytes` measure requested backing reservations, not heap
allocator overhead or process RSS.

`LiveEvent::residentBytes()` reports only its requested State plus complete event
arena, including spare capacity. `LiveEvent::releaseBytes()` reports the current
whole epoch reservation as a conservative byte-work allowance before releasing
an event: the last event reference to a snapshot can also destroy its nested
containers. This allowance includes shared configuration/snapshot storage and
possibly unrelated live owners. Sample and debit it immediately before release
in the same serialized operation; it is not event-specific staging or a CPU-time
measurement. Both getters take constant work and return zero after move.
`LiveIdentity::sameEpoch()` compares valid opaque epochs while ignoring binding
generation; `matches()` still requires the same generation. Initial generation
zero may be valid; two invalid identities never match either comparison.

`LiveIdentity::releaseBytes()` reports only the fixed opaque epoch-control storage,
or zero for an invalid token. Unlike a snapshot or event owner, the token retains no
schema, frame or snapshot graph. Sample and debit this fixed release allowance
before dropping a source identity in the serialized owner domain.

Caller-created copies outside these owners, including independently retained
core provenance tokens, have their own lifetime. These per-epoch counters do
not measure all allocations made by the application.

`LiveWork` contains caller-turn operation, visit, UTF-16 and byte credits. Calls
consume the supplied credits, including failed work, without refund. The caller
chooses when a new turn begins. A continuing live session has no cumulative tape
file quota; its wire offset must still remain representable.

## Compatibility boundary

The existing tape profiles, parser entry points and their default error behavior
remain separate. A live session does not add collectors, delivery guarantees,
reconnect reconciliation, automatic authentication retries, transport adapters
or a complete network client. Generic raw records remain separate from standard
event projection.

The [validation scope](../../tests/conformance/subscriptions/LIVE-VALIDATION.md)
separates native ownership/admission tests from actual production Java callbacks.
