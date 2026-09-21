# QTP boundary inventory

Initial descriptive result of QDCXX-004, Java QD **3.355**, commit **`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`**. This is research, not a complete normative wire specification or C++ API decision.

Evidence: **S** = inspected source; **D** = upstream documentation/comment or test assertion (not a test run); **I** = inference/proposal; **U** = unresolved or unmeasured. All source names resolve through the immutable [source inventory](SOURCES.md). **C** = executed capture, introduced by QDCXX-006 only for the bounded [measured cases](CAPTURES.md). The source inventory below remains source-level evidence unless linked to those captures.

Read alongside [DataScheme and wire schema](SCHEMA-EXCHANGE.md), [encoding dependencies](../encoding/README.md), [capture proposals](../../tests/fixtures/protocol/README.md), and [open questions](../../docs/upstream/OPEN-QUESTIONS.md#qtp-investigation-qdcxx-004).

## Responsibility boundaries

| Layer | Evidence and responsibility | State/boundary |
| --- | --- | --- |
| Transport bytes | **S:** SocketReader reads bytes into chunks and calls application processChunks; SocketWriter retrieves chunks and writes them to the stream. | Socket reads/writes and chunks are not QTP frames. Scheduling, reconnect and chunk ownership are transport concerns. |
| Application connection | **S:** MessageAdapterConnection creates ConnectionQTPParser/Composer per connection, accumulates input, retrieves output and checks heartbeat deadlines. | Connects protocol state to transport notifications and clocks, outside DataScheme semantics. |
| Framing | **S:** BinaryQTPParser.parseImpl reads length, waits for complete body, confines parsing to BufferedInputPart. BinaryQTPComposer patches length at completion. | Input retains incomplete frames; byte counts define frame ends. |
| Messages | **S:** Binary parser dispatches type; AbstractQTPParser buffers record callbacks. | Callbacks need not correspond one-to-one with frames. |
| Protocol session | **S:** Peer record-ID map, accumulated protocol descriptor, sent descriptions, requested-field map and adapter capabilities/auth state. | Lifetime differs from message-local symbol compression. |
| Core records | **S:** BinaryRecordDesc maps to local DataRecord/field slots and emits RecordCursor values. | Local scheme remains local; mapping needs no socket. |

**D:** ApplicationConnection requires at most one thread at a time for each of processChunks and retrieveChunks; readiness notification can race with processChunks returning. **S:** ConnectionQTPParser documents asynchronous reads of requested descriptions by the composer. **I:** Preserve directional protocol state and abstract byte processing in a future design; do not copy Java threading or put sockets/executors in qd-core. Java's qd-core module name is not the proposed C++ boundary. [Connection/transport sources](SOURCES.md#connection-and-transport)

## Frame and message envelope

**S:** Envelope: `compact(length) | compact(type) | payload`; length counts type plus payload, excluding its own prefix. Parser uses readCompactLong for length/type, rejects negative length or length above MAX_MESSAGE_SIZE, and type outside 0..Integer.MAX_VALUE. Zero-length body means heartbeat without a type byte; composer emits an empty heartbeat as one zero byte. `DXP3` is magic inside DESCRIBE_PROTOCOL, not every frame's prefix. No checksum or universal record count appears in this envelope. [Framing sources](SOURCES.md#framing-and-records)

**S/D:** Parser maximum body size defaults to 100,000 bytes (`com.devexperts.qd.qtp.maxMessageSize`). Composer threshold defaults to 8,000 and is a batching threshold, not a hard maximum: one record can exceed it. Binary composer reserves a maximal compact-int length header then writes actual compact-long body length. Header reservation, socket aggregation threshold and parser acceptance limit are distinct. [QTPConstants; composers](SOURCES.md#framing-and-records)

| Type IDs | Meaning in inspected binary path |
| --- | --- |
| 0 | HEARTBEAT; empty or structured payload |
| 1 / 2 | DESCRIBE_PROTOCOL / DESCRIBE_RECORDS: capabilities versus record layouts |
| 3 | Reserved describe; ignored |
| 4 | PART: reassembly of an embedded framed message |
| 5 | RAW_DATA |
| 10 / 11 / 12 | TICKER data / add subscription / remove subscription |
| 15 / 16 / 17 | STREAM data / add subscription / remove subscription |
| 20 / 21 / 22 | HISTORY data / add subscription / remove subscription |
| 49..56 | RMI types, including legacy messages; payload semantics outside scope |
| 61 | Reserved text marker; ignored by this parser |

**S:** MessageConstants defines these values; MessageType identifies record-bearing types and reorder constraints. Other types reach processOtherMessage, rather than being inherently illegal framing. Compression markers in MessageConstants do not establish compression inside this envelope. File/text/blob and dxLink WebSocket formats are separate paths, not specified here. [Message sources](SOURCES.md#messages-and-negotiation)

## Partial input, batching and errors

- **S:** EOF in length resets to the mark and waits; complete length with insufficient body also resets and waits. Record decoding begins only after a whole frame is available. EOF inside an already complete declared body is corruption. No final-EOF method here turns pending partial input into a truncation diagnostic. [BinaryQTPParser.parseImpl](SOURCES.md#framing-and-records)
- **S:** Frame bodies repeat records until exhausted. Data entries start with symbol/event flags and compact record ID, then described fields. Subscriptions contain symbol and ID; HISTORY_ADD alone adds compact-long subscription time. REMOVE_EVENT data uses leading Event-prefixed descriptor fields plus compact-long time instead of an ordinary full record. [BinaryQTPParser; BinaryRecordDesc](SOURCES.md#framing-and-records)
- **S:** AbstractQTPParser merges record messages and permits only reordering allowed by MessageType masks; parse flushes pending records in finally. Heartbeat and DESCRIBE_PROTOCOL flush pending records before processing; DESCRIBE_RECORDS does not. Composer may interrupt a message for a first-use description and resume it; empty composed messages are collapsed. [Abstract parser/composer](SOURCES.md#framing-and-records)
- **S:** Invalid length reports corrupted stream and stops with default resync behavior. Invalid/truncated type reports stream corruption; malformed known payload reports corrupted message with type. After body errors, parser seeks to declared frame end if consumer returns. Base resync hooks do not scan for magic. [BinaryQTPParser](SOURCES.md#framing-and-records)
- **S:** Data/subscription errors restore buffer limit from before the frame; earlier buffered frames can still be delivered. DESCRIBE_RECORDS is non-atomic: earlier installed descriptions survive a later error in the same frame. A protocol delta is parsed before merging, avoiding partial merge on malformed input. [BinaryQTPParser.recoverBuffer/parseDescribeRecords/parseDescribeProtocol](SOURCES.md#framing-and-records)
- **S:** MessageAdapter normally closes on corrupted stream/message and unknown message. doNotCloseOnErrors, skipCorruptedMessages and skipUnknownMessages alter policy (the latter two default false). Parser callbacks do not promise continuation of a connection. Unchecked exceptions/allocation failures are not universally translated to those callbacks. [MessageAdapter](SOURCES.md#messages-and-negotiation)

## Protocol description and negotiation

**S:** ProtocolDescriptor serializes DXP3, properties, send-message list, receive-message list, then optional EndpointId. Each list has compact count and MessageDescriptor entries: compact ID, UTF string name, properties. These are message IDs, not DataRecord IDs. [ProtocolDescriptor; MessageDescriptor](SOURCES.md#messages-and-negotiation)

**S/D:** Updates accumulate: top-level/per-message properties merge by key, messages by ID, non-null EndpointId replaces old value. Omitted properties remain; empty string can replace a value. deltaFrom emits a full initial descriptor, later changed properties/new message IDs; disappeared messages are not removals. ProtocolDescriptorTest and DescriptorDeltaEmissionTest assert these distinctions; they were not run. [Tests](SOURCES.md#upstream-tests)

**S:** MessageAdapterConnection enables describe-protocol before adapter start. MessageAdapter emits capability/auth deltas; AgentAdapter and DistributorAdapter use remote receive/send capabilities, filters, stripe and aggregation properties. DistributorAdapter's no-describe legacy path immediately enables subscription sending. Framing itself does not enforce a universal handshake-before-any-message rule. Full auth, aggregation and legacy timing state machines remain **U**. [Adapter sources](SOURCES.md#messages-and-negotiation)

**S:** Incoming protocol description sets composer options from opt. A parseable QDS-3.<number> below 263 disables wide-decimal support in the default writer-layout branch; absent/unrecognized version permits it. Requested layouts take a separate branch. ProtocolOption declares hs (history snapshot). Writer masks flags by message type/options; PentaCodec reader currently accepts its encoding independently of negotiated options. [Connection sources](SOURCES.md#connection-and-transport), [encoding](../encoding/README.md)

**S versus D:** BinaryQTPParser calls onDescribeProtocol **before** consumer.processDescribeProtocol, expressly for QD-808 initialization. Hook Javadoc says after; executable source order wins in this inventory. Capture first outgoing data after negotiation, not just steady state.

## Heartbeats, partitions and state lifetime

**S:** HeartbeatPayload uses compact content bits: 1=timeMillis (compact long), 2=timeMark, 4=deltaMark, 8=lagMark (compact ints), in that order. Empty heartbeat invokes no processHeartbeat callback. MessageAdapterConnection refreshes disconnect deadline when processing input chunks, not only heartbeat payloads. It schedules heartbeats by time/bytes, can prepend one to already composed chunks, and tracks RTT/lag. Timing is connection policy; heartbeat bytes are protocol. [Connection sources](SOURCES.md#connection-and-transport)

**S/D:** PART starts with compact-long sequence ID. First nonempty part must expose embedded compact body length and type; later parts carry continuation bytes. IDs can coexist; completion makes the embedded message parseable. ID-only part cancels an outstanding sequence or is ignored if absent. Nested PART is rejected. Embedded length is checked against Integer.MAX_VALUE, with a source TODO for an appropriate maximum, not the ordinary 100,000 default. Extra bytes beyond remaining length are not fed into embedded parsing. Splitting the embedded length/type inside the first part is unsupported here. This differs from arbitrary TCP fragmentation of an outer frame. [BinaryQTPParser.parseMessagePart; MessageConstants](SOURCES.md#framing-and-records)

| State | Observed scope and reset |
| --- | --- |
| Incomplete outer bytes | **S:** input across parse calls; resetSession does not clear external input. |
| Symbol/MRU flag compression | **S:** reset per data/subscription message on read, per message on write; no cross-frame repetition dictionary. |
| Received record-ID descriptors | **S:** persist; re-description replaces mapping; parser resetSession clears map and unknown-name log suppression. |
| Peer protocol descriptor / last heartbeat | **S:** accumulated/reused; resetSession clears these. |
| Requested fields by local ID | **S:** persist in ConnectionQTPParser; its resetSession clears schemeMap. |
| Sent descriptions / writer cache | **S:** AbstractQTPComposer resetSession marks descriptions new; BinaryQTPComposer has no override clearing private recordMap. Re-emission does not prove layout renegotiation. |
| PART assemblies | **S:** persist across calls; parser resetSession does not clear partitionedMessages. **U:** reuse/cancellation and resource limits. |
| Adapter/timers/auth/lifecycle | **S:** new MessageAdapterConnection creates fresh parser/composer. **U:** broader wrapper reuse/reconnection profiles. |

Sources: [framing/record classes](SOURCES.md#framing-and-records), [PentaCodec](SOURCES.md#encoding-and-core), [connection classes](SOURCES.md#connection-and-transport). Reset caveats are observations, not recommended C++ behavior.

## Validation and limits

QDCXX-004 verified the locked checkout and inspected source/tests without execution. QDCXX-006 adds source-built Java captures for P01/P03/P04/P05, with [exact scope, reproducibility and verification](../../tests/fixtures/protocol/VALIDATION.md); upstream tests, sockets and benchmarks remain unexecuted. This does not fully specify primitive encodings, malformed resource limits, RMI, file resynchronization, TLS/compression, auth or every adapter transition. [Capture inventory](../../tests/fixtures/protocol/README.md) distinguishes executed subsets from pending families.

## QDCXX-013 contract

[DESCRIBE_RECORDS raw payload contract](DESCRIBE-RECORDS.md) / [Контракт сырого payload DESCRIBE_RECORDS](DESCRIBE-RECORDS.ru.md).
