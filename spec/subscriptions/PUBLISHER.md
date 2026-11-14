# Composite Quote publisher

[Русская версия](PUBLISHER.ru.md). English is authoritative.

`qd/api/publisher.hpp` is installed with `qdcxx::feed`. Create a `RuntimeBootstrap` on the main thread before other application threads, then create a `PublisherEndpoint` from it. Keep the bootstrap alive until the endpoint and any receiving `ApiRuntime` using it are destroyed. The endpoint owns one worker and its listening socket. It returns the selected port after the worker binds. Each receiving Feed endpoint connects on an independent TCP connection in the ordinary TICKER profile.

`getPublisher()` returns a detached producer handle. `publish(quote)` copies one validated composite Quote into the finite FIFO or returns a detached diagnostic. Handles can be used on other application threads. `awaitDemand` waits for presence or absence of the aggregate exact symbol. A symbol currently requested by at least one client is present. A late subscriber receives the latest value retained while another client was interested. The last withdrawal removes the value. A local submission does not guarantee socket or application receipt. Closing stops the worker and releases pending work and sockets. Any remaining producer handle observes the closed admission fence.

Each peer sends `DESCRIBE_PROTOCOL` and describes its own records before subscribing. The endpoint resolves the peer's record ID to the local composite `Quote` and sends its own `DESCRIBE_RECORDS` before DATA. Unsupported described record names are ignored while decoding subscription tuples. The profile uses ordinary TCP and QTP with exact symbols and composite Quote. The listener defaults to numeric IPv4 loopback and an ephemeral port. There are at most eight active or retiring client slots, 32 decoded entries per subscription command, and 1024 queued publication requests. The configured slot and publication counts may be smaller. A bounded queue admission failure, invalid peer input or blocked write for ten seconds retires the corresponding connection. The other clients retain their subscriptions and independent progress. TICKER may conflate pending updates for the same symbol.

Use a nonempty exact symbol of at most 256 UTF-16 units. Bid and ask times use signed 32-bit whole seconds on the wire and must be exact multiples of 1000 milliseconds. `timeMs` lies within the second of the later bid or ask time. The low 22 sequence bits and the signed nano part are sent in the optional `Sequence` and `TimeNanoPart` fields. The endpoint transfers the four wide decimal raw values and both exchange codes. Snapshot-derived business numerics, nonzero `eventTime` and other values outside the wire profile are rejected at admission. The built-in Feed registry supports the eight default Quote fields. Add the two optional fields to its explicit registry to receive the full time and sequence precision of this profile.

See [server demand](SERVER-DEMAND.md), [incoming subscriptions](../protocol/INCOMING-SUBSCRIPTIONS.md), and the Java [DXPublisher documentation](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html) for the observable last-value and demand behavior.

## Separate upstream publisher

Set `PublisherOptions::source` to a numeric TCP address and port to relay the
composite Quote stream from a separate QTP publisher. In this mode the local
producer returns `INVALID_STATE` and its FIFO is absent. The listening port
stays available while the source is offline. The source-facing session advertises
TICKER DATA in its receive direction, ADD/REMOVE in its send direction, and
sends the local Quote description before the first subscription. The source
independently declares its own DATA record ID and fields. Relay frames admit up
to 32 mapped composite Quotes with the supported exact field representations.

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
