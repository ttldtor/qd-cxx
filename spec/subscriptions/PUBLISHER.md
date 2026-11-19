# QTP publishing and relay

[Русская версия](PUBLISHER.ru.md) · [Server trust and resource boundaries](../transport/SERVER-TRUST-BOUNDARIES.md). English is authoritative.

`qd/api/publisher.hpp` is installed with `qdcxx::feed`. Create a `RuntimeBootstrap` on the main thread before other application threads, then create a `PublisherEndpoint` from it. Keep the bootstrap alive until the endpoint and any receiving `ApiRuntime` using it are destroyed. The endpoint owns one worker and its listening socket. It returns the selected port after the worker binds. Each receiving Feed endpoint connects on an independent TCP connection in the ordinary TICKER profile.

`getPublisher()` returns a detached producer handle. `publish(quote)` copies one validated composite Quote into the finite FIFO or returns a detached diagnostic. Handles can be used on other application threads. `awaitDemand` waits for presence or absence of the aggregate exact symbol. A symbol currently requested by at least one client is present. A late subscriber receives the latest value retained while another client was interested. The last withdrawal removes the value. A local submission does not guarantee socket or application receipt. Closing stops the worker and releases pending work and sockets. Any remaining producer handle observes the closed admission fence.

Demand identity includes the contract, physical record name and exact symbol.
`awaitDemand(family, recordName, symbol, ...)` observes one key. A nonempty name
that is never admitted to that contract's union is always absent while the endpoint serves it.
The symbol-only overload observes presence in any record of the specified contract. `getDemand(family)`
copies the latest published union for that contract, including the minimum signed
raw HISTORY bound for each key. The snapshot owns its names and symbols and may be
retained after subsequent changes or endpoint shutdown. The number of rows is bounded
by the configured downstream connection count times the maximum keys per client.
This observes applied downstream subscriptions. A relay publishes the same local demand
while the upstream connection is unavailable, and replays current keys after reconnect.

## Connection demand filters

Set `PublisherOptions::demand` to the default policy, with optional `ticker`, `stream`
and `history` classifiers. `peerDemand` may replace that entire policy for up to 32 distinct
numeric remote IP addresses. IPv4-mapped IPv6 and IPv4 spellings of the same address compare equal.
The transport selects one immutable policy at connection acceptance. Scope IDs, duplicate addresses,
DNS names and dynamic filters are configuration errors. A filter slot left empty admits all keys for its contract.
Peers behind the same address share the same policy, so address selection does not establish a peer identity.

`FeedFilter::acceptRecord` evaluates the existing QD filter expression against the physical record name
and original exact UTF-16 symbol. For example, `:Quote&IBM` admits the physical `Quote` record for `IBM`.
Matching does not use event conversion or register a listener. Rejected ADD tuples are omitted before
registering client interest and never contribute to the upstream union. A filter query bound refusal
retires only that peer without applying the batch. REMOVE goes straight to removal of previously
admitted matching keys for that connection. Repeated or denied-key removals do not alter other clients' demand.

### Policy replacement for a connected peer

`getPeers()` lists the active connection identities and latest policy revision of each peer.
`replacePeerDemand(id, policy)` queues a complete replacement for one connection. A copied
dynamic `FeedFilter` may be supplied as an immutable generation. Refresh it explicitly to apply
another generation. Static address rules continue to select the initial policy of new connections.
Each connection retains at most 32 requested keys per family, including keys denied by its filter.
HISTORY retains the last raw bound for each requested key. Requests are recorded only after the
subscription command is applied. A later replacement admits newly permitted requested keys and
withdraws newly denied keys without a second wire subscription from the client.

One replacement is outstanding per endpoint. The returned ticket remains usable after an observation
deadline expires, and completes as APPLIED, REJECTED or PEER_CLOSED. The worker rejects a replacement
after five seconds without eligibility to apply it. APPLIED publishes all affected demand families
and the connection policy revision together. Independent calls to `getDemand(family)` before or after
that completion each observe the latest published state for their own family. They form no multi-call
transaction while the endpoint keeps processing activity. REJECTED preserves the previous policy.

The worker checks physical record permission before encoding DATA. Replacement prunes denied queued
records and drops any denied retained frame before it starts writing. The endpoint closes the selected
peer if revocation would interrupt a partially written DATA frame or an open HISTORY transaction or
snapshot already sent to that peer. The ticket returns PEER_CLOSED and the usual client retirement
withdraws that connection's applied keys. Other peers keep their own policies and subscriptions.
On an upstream reset, an already opened HISTORY transaction or snapshot also requires closing
that downstream connection before a new source generation can send DATA.

For TICKER and STREAM, a held delivery batch keeps the relative order of its permitted rows.
When a byte-bounded STREAM delivery selects homogeneous record ranges, the publisher retains
those ranges and their payload reservations through the last frame of the batch. Encoding
reads selected rows from the retained ranges. Other deliveries use ordinary Mux record views.
If an unstarted DATA frame contains a newly forbidden row, the whole frame is discarded and the
permitted rows are composed again from that batch. An unaffected frame remains valid. A tracker
notification or `getCurrentFilter()` call alone changes neither the peer policy nor upstream demand;
the current immutable filter must be passed to `replacePeerDemand`. Upstream reconciliation uses
the resulting effective union, including the earliest remaining HISTORY bound of other peers.
This boundary applies to the supported exact-key publisher profiles and does not extend the
[filter expression scope](FEED-FILTERS.md).

Each peer sends `DESCRIBE_PROTOCOL` and describes its own records before subscribing. The endpoint resolves the peer's record ID to the local composite `Quote` and sends its own `DESCRIBE_RECORDS` before DATA. Unsupported described record names are ignored while decoding subscription tuples. The profile uses ordinary TCP and QTP with exact symbols and composite Quote. The listener defaults to numeric IPv4 loopback and an ephemeral port. There are at most eight active or retiring client slots, 32 applied keys per family and client, 256 decoded wire tuples per subscription frame, and 1024 queued publication requests. The configured slot and publication counts may be smaller. A bounded queue admission failure, invalid peer input or blocked write for ten seconds retires the corresponding connection. The other clients retain their subscriptions and independent progress. TICKER may conflate pending updates for the same symbol.

Use a nonempty exact symbol of at most 256 UTF-16 units. Bid and ask times use signed 32-bit whole seconds on the wire and must be exact multiples of 1000 milliseconds. `timeMs` lies within the second of the later bid or ask time. The low 22 sequence bits and the signed nano part are sent in the optional `Sequence` and `TimeNanoPart` fields. The endpoint transfers the four wide decimal raw values and both exchange codes. Snapshot-derived business numerics, nonzero `eventTime` and other values outside the wire profile are rejected at admission. The built-in Feed registry supports the eight default Quote fields. Add the two optional fields to its explicit registry to receive the full time and sequence precision of this profile.

See [server demand](SERVER-DEMAND.md), [incoming subscriptions](../protocol/INCOMING-SUBSCRIPTIONS.md), and the Java [DXPublisher documentation](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html) for the observable last-value and demand behavior.

## Separate upstream publisher

Set `PublisherOptions::source` to a numeric TCP address and port to relay the
composite Quote stream from a separate QTP publisher. In this mode the local
producer returns `INVALID_STATE` and its FIFO is absent. The listening port
stays available while the source is offline. The source-facing session advertises
TICKER DATA in its receive direction, ADD/REMOVE in its send direction, and
sends the local Quote description before the first subscription. Upstream exact subscriptions use canonical Penta encoding when available and raw UTF-16 otherwise. The source
independently declares its own DATA record ID and fields. Relay frames admit up
to 32 mapped composite Quotes with the supported exact field representations.

## Mixed profile

Set `PublisherOptions::mixed` to `true` before creating the source or relay. The
endpoint advertises TICKER Quote, STREAM TimeAndSale and HISTORY default-source
Order on the same QTP connection. The three contracts keep independent desired
maps and delivery capacity. Every accepted peer owns its own registrations and
partial output. A negotiated HISTORY snapshot option (`hs`) is required for
the HISTORY direction.

`Publisher::publish` has typed overloads for all three records. STREAM admits
separate ordered occurrences even when their fields are identical. HISTORY
uses signed raw bounds, presents collector-projected snapshots and transactions,
and carries the raw indexed identity for removals. The default Order source has
indices from zero to `INT32_MAX` and no source suffix. END and SNIP use index zero.
Use `awaitDemand(family, ...)`
to observe the exact symbol union for one contract. Local publication follows
the bounded default physical layouts. The source rejects fields and metadata
that cannot be represented by those layouts. Local TimeAndSale accepts zero event flags
and the optional nano field. A remote DATA envelope admits at most 32 mapped rows. Each
peer reserves 32 delivery entries per family. The HISTORY collector shares up to
`16 * maxConnections * 32` indexed points between peers and keys.

The first interested client adds the key upstream and the last client withdrawal
removes it. The relay marks a change locally written only after the complete
frame is accepted by the local TCP transport. The source can observe the wire
command independently. `getSourceStatus()` reports the last negotiated
generation and current connection state. `awaitSourceGeneration()` waits for a
new negotiated generation before a finite deadline. Neither method waits for a
particular subscription to reach the source.

On failure the relay drops pending source input, clears cached and queued
material from that generation and opens a fresh session after the configured
retry delay and collector reset. The new session replays current demand.
Downstream registrations generally survive source recovery. If a downstream
socket already accepted a prefix of a stale DATA frame, the relay closes that
connection to preserve framing. Fully accepted bytes can still reach the
client. The source generation is local diagnostic state and carries no
guarantee of delivery continuity.

## Full record relay profile

Set `PublisherOptions::catalog` to a binary schema and its ordered QTP wire names. Set
`source` and leave `mixed` false. The endpoint validates the complete outgoing description
before reporting it has started, then owns its copy of the schema and field names. Use
`loadBinaryJsonSchema` and `loadBinaryJsonWireNames` with the paired files from the same
[default or dxlink export](../../tools/schema-export/README.md). The C++ publisher consumes
typed values and works without the JSON library.

Every physical record can be requested from TICKER and STREAM. HISTORY additionally requires
an indexed layout and negotiated `hs`. Each direction maps the peer's sender IDs to the
catalog's physical record identities. Received DATA is decoded as records, routed by contract
and subscription, then encoded with the relay's own description. Nullable fields, distinct
source-specific names, and signed HISTORY removal indices are preserved. Each collector retains
its usual snapshot, conflation, transaction and queue semantics.

The complete description must fit in one 64 KiB QTP frame. The current endpoint limits of 32
keys per client per contract and 256 subscription tuples per frame also apply in this profile.
An upstream DATA frame may contain up to 32 mapped record occurrences, even if they share one key. A frame exceeding this allowance fails the upstream generation and current demand is replayed on reconnect. Operate the relay with a source that advertises the three QTP contracts and supports `hs`.

For the local catalog profile, see [publication of physical records](RECORD-PUBLICATION.md).

## Publication ownership and capacity

A successful publish call commits one immutable owning publication. Preparation, validation and copying
happen outside the mailbox mutex; the link and close share one fence. Concurrent producers are ordered
by successful commit. Non-overlapping calls from one producer preserve call order, and the rows of one
call remain contiguous at this handoff. Validation, capacity and allocation failures admit no prefix.
A producer already preparing during close may safely finish its detached work, then receive `CLOSED`.
No retained publication, schema or reservation depends on a native runtime after termination.

`pendingQuotes` bounds unconsumed source rows, including the worker's detached publication. In catalog
and explicit byte modes, `recordLimits.pendingBytes` includes one immutable binding reservation plus
each publication's queue node/shared control, packed descriptors, source metadata, payload, alignment
and block control. `rowBytes` continues to bound one row's logical field descriptors, symbol and variable
payload. A budget too small for the fixed binding is rejected at endpoint creation. A complete request whose bytes or row count
cannot fit an empty input domain reports `RESOURCE_LIMIT` at `publisher-publication-capacity`;
Temporary byte occupancy reports `publisher-pending-records`; row-quota occupancy uses `publisher-pending-quotes` for typed publication and `publisher-pending-records` for catalog publication. The legacy typed profile derives a finite byte
capacity from its existing row count and worst single-publication layout, preserving its row capacity.

The complete input credit remains charged across detachment and all partial worker steps. It is returned
only after the collector consumes the entire publication or the publication is definitively discarded;
socket delivery is independent. Physical storage remains charged through its final owning reference
(and a shared control through final weak release). The binding uses the existing `LocalSchema` capacity
model; allocation bookkeeping, stacks and operating-system buffers are outside this accounting.

The source epoch and detached Mux context each reserve and retain the same immutable binding once in
their independent metadata domains. Payload-only rows require that exact owner; a missing or different
binding is rejected before admission. Per-frame and per-client payload windows therefore do not repeatedly
charge the catalog. The ordinary full-capacity block retain continues to include its complete binding.
The physical domain is finite and conservatively derives from the input, source epoch, collector/cache
and Mux/consumer capacities; these independent reservations do not describe an exact RSS limit.

The worker detaches ready publications without waiting for another row. Its service quantum, delivery
window and QTP frame size remain independent of input capacity and generator call size. When retaining
an entire payload would exceed a downstream window, the collector selection receives a separately owned
packed range. This copy occurs at the retention boundary and keeps the original whole-publication input
credit until all ranges are consumed. A single valid row retains the existing soft-window allowance.

The implementation follows
[ADR-0016: Whole-publication queue](../../docs/decisions/ADR-0016-whole-publication-queue.md).
The matched performance comparison is recorded separately.
