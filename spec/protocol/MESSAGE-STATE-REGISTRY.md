# QTP message and state registry

[Русская версия](MESSAGE-STATE-REGISTRY.ru.md). English is authoritative.

Use this registry to select an existing decode/encode profile and locate its state and refusal
contract. It consolidates the implemented boundaries; it does not enable a message, option or
collector. Java compatibility uses [upstream.lock](../../upstream.lock), QD 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. The earlier [boundary inventory](README.md)
and [locked source inventory](SOURCES.md) retain their source/capture evidence distinctions.

## Message identities and live directions

IDs belong to the message envelope, independently of sender record IDs. [FrameReader](FRAMING.md)
extracts any structurally valid nonnegative signed32 type and opaque payload; successful extraction
does not establish payload support. An empty envelope has no type and is distinct from type 0.

In this table, **client input** is `LiveSession::create` with `feed`; **server input** is
`createServer` with `feedServer`. Both use the same control handling. Standalone codecs do not
enforce session negotiation. Output methods compose bytes explicitly and perform no I/O.

| ID / identity | Live decode and admission | Live / standalone encode | State and absent-support outcome |
| --- | --- | --- | --- |
| No type / empty heartbeat | Both profiles emit EMPTY_HEARTBEAT. | Tape `marker()` emits this envelope; the live composer exposes no empty-envelope method. | No protocol, binding, authentication or record-time transition. It does not authenticate EOF. |
| 0 HEARTBEAT | Both profiles emit HEARTBEAT; known content bits are listed below. | `composeHeartbeat` always emits type 0, including absent payload; tape `heartbeat` uses its narrower grammar. | Presence and raw values stay metadata; no clock read, automatic scheduling or implicit application timestamp. Unsupported bits fail live input. |
| 1 DESCRIBE_PROTOCOL | Both parse the complete DXP3 descriptor and merge a full/delta candidate. | `composeProtocol`; `authorize` is the separate credential-bearing type-1 path. | Atomic publication updates effective capabilities/properties, authentication and `hs`; omitted keys/messages persist. Unsupported extensions fail before publication. |
| 2 DESCRIBE_RECORDS | Both atomically replace effective peer sender descriptions/bindings. | `SubscriptionWriter::describe` / `LiveSession::describe`. | Incoming and outgoing maps are independent. Live output does not record announced state; caller orders descriptions before DATA. Invalid batches publish no candidate. |
| 3 DESCRIBE_RESERVED | Unsupported in both live profiles. | No live composer method. | Live input fails UNSUPPORTED; only the explicit binary catalog tape profile ignores this frame. |
| 4 PART | Unsupported, even when the peer advertises it. | No PART composer/reassembly API. | Live input fails UNSUPPORTED. Outer transport fragmentation is handled by the framer and does not enable PART. |
| 5 RAW_DATA | Unsupported in both live profiles. Standalone `decodeRawData` reads a complete zero-flags payload. | No live RAW publication; `TapeWriter::data` emits RAW in its recording profile. | Generic tape RAW decoding does not establish collector fanout or live readiness. Actual live RAW fails UNSUPPORTED. |
| 10 TICKER_DATA; 15 STREAM_DATA; 20 HISTORY_DATA | Client: exact peer send capability, protocol received and authentication gate; whole-frame DATA. Server input rejects DATA. | `composeData`; session `publishData` additionally requires the matching peer receive capability and authentication gate. | Family stays in `LiveEvent::messageType`, including empty DATA. Decoder flags/time policy is explicit; family alone creates no collector. Server DATA input fails UNSUPPORTED. |
| 11 TICKER_ADD; 12 TICKER_REMOVE | Server: exact peer send capability and authentication gate, then one whole owning subscription batch. Client input rejects subscriptions. | `compose` / `subscribe`; TICKER tuple is symbol + sender ID. | Session output requires peer receive-add and the requested add/remove identity. Missing capability is NOT_READY; unsupported client input is UNSUPPORTED. |
| 16 STREAM_ADD; 17 STREAM_REMOVE | Same server/client distinction as 11/12. | `compose` / `subscribe`; STREAM tuple is symbol + sender ID. | Same directional gate; explicit selector policy separately controls All. |
| 21 HISTORY_ADD; 22 HISTORY_REMOVE | Same server/client distinction; known targets require explicit indexed metadata. | `compose` / `subscribe`; only ADD carries the signed64 raw bound. Both HISTORY forms require indexed metadata. | Bound is a record coordinate, not inferred milliseconds. REMOVE rejects a supplied bound. No implicit business conversion. |
| 49–56 RMI; 61 TEXT; 75 ZIP; 139 GZIP; any other type | No live payload implementation. | No matching composer method. | Live input fails UNSUPPORTED. Java identities or compression markers do not grant native support; tape refusal is listed below. |

Detailed grammars: [live session](../subscriptions/LIVE-SESSION.md),
[subscriptions and descriptions](../subscriptions/WRITER.md),
[incoming subscription tuples](INCOMING-SUBSCRIPTIONS.md), [outgoing DATA](LIVE-DATA-WRITE.md),
[complete DATA payloads](STREAM-DATA.md). The standalone subscription decoder consumes unknown
*described* local targets without emitting them, but rejects undeclared sender IDs. The live server
uses the peer description direction. Incoming tuples never select the outgoing DATA layouts.

Peer descriptor declarations may retain an unsupported identity without implementing its payload.
Local `composeProtocol` accepts only exact supported ID/name pairs in the chosen directions:
common controls in both, DATA in send and client receive, subscriptions in client send or server
receive. `subscribe` is unavailable on server sessions (UNSUPPORTED). `publishStreamPrefix` /
`composeStreamPrefix` select a nonempty STREAM prefix under their explicit soft/hard byte bounds;
the returned output is still one complete envelope with the same negotiation and ownership rules.

## Recording profiles

Tape selection is explicit and independent of live negotiation. These are acceptance summaries;
the linked profile defines exact descriptor grammar, errors, bounds and completion requirements.

| Profile | Decode / encode identities | State and refusal boundary |
| --- | --- | --- |
| [TapeReader LEGACY](TAPE-READER.md) | Initial 1, descriptions 2, empty markers, DATA 15. | Restricted tape/NONE/empty-options descriptor; append-only descriptions and zero flags. Explicit heartbeat 0 and other actual types are unsupported. |
| [TapeReader RAW_MESSAGE_V1](RAW-TAPE.md) | Initial 1, descriptions 2, empty markers, heartbeat 0, DATA 5. | MESSAGE time, absent/empty/exact `hs`, zero flags. `hs` does not authorize nonzero flags. Other actual families and repeated descriptors are refused. |
| [TapeReader BINARY_CATALOG_V1](BINARY-MESSAGES.md) | Full/delta 1, descriptions 2, empty markers, heartbeat 0, reserved 3, DATA 5/10/15/20. | Four families share the explicit known-flags decoder in wire order. Type 3 emits a bounded ignore event and resets nothing. PART, subscriptions, RMI, TEXT, compression and all other actual IDs are UNSUPPORTED/MESSAGE. |
| [TapeWriter RAW/MESSAGE v1](TAPE-WRITER.md) | `begin` 1; `describe` 2; `heartbeat` 0; `data` 5; `marker` empty; `finish` emits no bytes. | READY → ACTIVE → FINISHED; any failed operation makes FAILED. Descriptions must precede DATA; announcements commit only on whole-operation success. Zero flags; no implicit footer. |

The catalog alone admits its documented headerless self-description path and full/delta descriptors.
Its `FieldTimePolicy::READ` and `TapeExternalTimePolicy::SIDECAR` are independent explicit extensions;
the older constructor signatures keep them disabled. [FIELD/MESSAGE](TAPE-TIME.md) and
[companion time](SIDECAR-TIME.md) do not become live capabilities or infer indexed/business time.

## Options, flags and extensions

`hs` is the supported negotiated protocol option. The following other bits/settings have distinct
owners; accepting their syntax does not negotiate a new feature.

| Item | Decode / effective state | Encode / absent support |
| --- | --- | --- |
| `opt=hs` | Live recognizes an exact comma-separated `hs` token; duplicates are idempotent, unknown tokens are informational, whitespace/substrings do not match. Initially absent, null or empty options disable peer `hs`. `historySnapshotEnabled` is local `LiveLimits::historySnapshot` AND effective peer `hs`. A delta omitting `opt` retains it. | Local nonempty `opt` must be exactly `hs` and enabled locally. Other local option strings are UNSUPPORTED. Tape options follow their narrower profile table. |
| DATA event bits | Explicit BINARY_CATALOG_V1 event policy accepts every subset of `0x5f`: TX_PENDING `0x01`, REMOVE_EVENT `0x02`, SNAPSHOT_BEGIN `0x04`, SNAPSHOT_END `0x08`, SNAPSHOT_SNIP `0x10`, SNAPSHOT_MODE `0x40`. Descriptor options do not mask reader bits. | Live DATA permits these bits only with enabled `hs`; unknown bits or nonzero bits without `hs` are rejected, whereas Java masks unsupported output bits. ZERO_FLAGS and old payload entry points reject every nonzero flag. |
| Unknown/reserved event bits | Projected signed low32 flags are checked after a complete symbol, before sender ID: negative masks, `0x20`, REMOVE_SYMBOL `0x80` and other unknown bits are unsupported. | No silently masked native output. Generic flags do not establish transaction/snapshot completion in a collector. |
| REMOVE_EVENT | Replaces ordinary values with a signed64 removal time; explicit FIELD policy handles the separate physical Event* prefix. Zero removal time remains present. | Writer requires an empty value span; rejects a leading non-VOID outgoing `Event*` field. Nonindexed output uses zero removal time. See the exact [removal contract](LIVE-DATA-WRITE.md). |
| Symbol repeat / MRU prefixes | FF repeats a prior symbol; F8 repeats current MRU flags; F9 introduces flags. State resets at each data/subscription message. A prefix is syntax, not a negotiated feature. | DATA uses canonical Penta/CESU-8/repeats. Subscription EXACT_ONLY keeps FD/FF; explicit canonical policies have the [selector contract](../subscriptions/SELECTORS.md). All is enabled only by the selected STREAM policy. |
| Heartbeat content bits | Live: `1` timeMillis compact long, `2` timeMark, `4` deltaMark, `8` lagMark compact ints, in that order. Absence differs from present zero; unknown bits are UNSUPPORTED, truncation/extra bytes MALFORMED. | Live composes the same finite grammar. RAW/catalog tapes and TapeWriter admit only absent payload, bits 0 without time, or bits 1 with timestamp. Other heartbeat forms are unsupported there. |
| `authentication` / `authorization` | An omitted key retains the effective property. Each admitted type 1 re-evaluates that property: nonempty means REQUIRED, empty means ACKNOWLEDGED; absent/null leaves the prior authentication state. Re-evaluation can replace AUTHORIZATION_EMITTED even when this delta omits the key. Authorization text is excluded from public snapshot views. | Generic descriptor composition forbids both credential keys. Only `authorize` emits credentials, under explicit adapter-supplied VERIFIED_TLS_BASIC eligibility. Plain/no eligibility is CREDENTIALS_FORBIDDEN. |
| Per-message properties / aggregation | Snapshot message views expose local overrides; resolve inheritance from top-level properties. The receiver's peer SEND DATA `aggregationPeriodInfo` is an independent optional range report, not a request echo. Omitted deltas retain old reports; absent/malformed/UNKNOWN or fenced source yields no current report. | Peer RECEIVE DATA `requestedAggregationPeriod` carries current local intent; explicit empty resets to server default. Updates are type-1 property deltas, with no new ID or ACK. See [server aggregation](../transport/SERVER-AGGREGATION.md) for validation, source fencing and the documented numeric compatibility exception. Other retained properties grant no implicit filter/stripe/aggregation implementation. |
| EndpointId / descriptor extensions | Live accepts one structurally valid optional endpoint tail and discards it; it is not routing identity. Unknown trailing bytes are UNSUPPORTED. Catalog tape structurally checks the known tail then refuses it. | Public live descriptor composition has no endpoint output field. No arbitrary extension composer is implied. [Endpoint witnesses](../../tests/fixtures/subscriptions/endpoint-identity-v1/README.md) define exact live cases. |
| `version`, `time`, field layout | Version text does not enable wide-decimal fallback or select the Java pin. FIELD input requires local policy; outgoing live `time` is absent/empty/NONE/none. Local nonempty `type` must be `qtp`. | No Java legacy schemeKnown/readAs/implicit type conversion is enabled by a property. Exact layouts and explicit conversion profiles retain their own [schema contract](SCHEMA-EXCHANGE.md). |

Exact symbols preserve their full UTF-16 identity, including empty text, embedded zero, unpaired
units and regional suffixes; decoding does not strip or normalize them. The selected symbol grammar
still validates malformed encodings and bounds. Selector All and reserved leading `*` follow the
explicit subscription selector policy; a descriptor or symbol spelling cannot enable it.

## State ownership and failure

| State owner | Transition, lifetime and failure |
| --- | --- |
| Framer | Retains incomplete outer bytes across feeds; one owning frame per result. `finish()` alone signals EOF. Failure is terminal; no scanning/resync. `reset()` resets only the standalone framer. |
| Live epoch | Fresh identity at construction, fixed local schema and outgoing layouts. No epoch-reusing reset. Feed failure is terminal; later feeds accept zero. `finish` needs complete framing and a received protocol descriptor, then CLOSED. |
| Effective peer snapshot | Whole descriptor/description candidate succeeds before publication. A failed candidate leaves old snapshot, generation and authentication/`hs` intact. Retained old owners remain valid. Sender replacements never rewrite previously owned records. |
| Live authentication | UNCHALLENGED initially; successful local `authorize` → AUTHORIZATION_EMITTED. Every admitted type 1 derives REQUIRED from a nonempty effective authentication value or ACKNOWLEDGED from an empty value, including one retained from an earlier descriptor; absent/null effective value preserves the prior state. REQUIRED and AUTHORIZATION_EMITTED gate incoming DATA/subscriptions and outgoing DATA/subscriptions. Type 2 does not re-evaluate authentication. |
| Live output operation | Missing directional capability → NOT_READY; active auth gate → AUTHENTICATION_REQUIRED. Typed validation/work/resource/allocation failures return no output and leave a healthy session usable. Spent work is retained. Control output does not acknowledge peer receipt. |
| Pending output owner | Owns immutable whole bytes; `consume` advances a local suffix only. Full consumption releases no backing credit until owner destruction/replacement. Byte availability does not authorize reconnect replay. |
| Standalone payload result | DATA decoder retains its completed-record prefix on failure; subscription decoder rejects the whole batch. Live wraps DATA admission atomically and exposes no failed current-frame prefix. Tape retains its separately specified completed prefix. |
| Receiver / collector | Receives normalized owning records after protocol admission. Local membership, delivery, snapshot completion and source reset belong to [receiver](../subscriptions/RECEIVER.md), [STREAM](../subscriptions/STREAM-COLLECTOR.md), [TICKER](../subscriptions/TICKER-COLLECTOR.md), [HISTORY](../subscriptions/HISTORY-COLLECTOR.md) and [recovery](../transport/RECOVERY.md). A decoded family/flag or local command ticket is not peer ACK or completed delivery. |

Read each selected API/profile for the units, zero-value semantics and refusal behavior of its
bounds. For example, `LiveLimits::dataEventBytes=0` selects the fixed event arena. Requested
backing, logical work, owner counts and caller-retained results have separate accounting;
this registry adds no universal message-size, RSS or latency guarantee.

## Source and verification map

These links identify implementation and existing verification, not a claim that every Java
adapter, platform or combination was exercised. Historical validation reports keep their scope.

| Contract | Source checkpoints | Existing checks / evidence |
| --- | --- | --- |
| Live dispatch, deltas, readiness, auth, heartbeat and epoch | [live_session.cpp](../../src/protocol/live_session.cpp): `dispatch`, `receiveSubscription`, `validateOutgoingProperties`, `subscribe`, `publishDataImpl`, `heartbeat`, `finish`; [public API](../../include/qd/protocol/live_session.hpp) | [live tests](../../tests/conformance/live_session_tests.cpp), [validation scope](../../tests/conformance/subscriptions/LIVE-VALIDATION.md) |
| Six subscription identities and separate output ownership | [subscription_writer.cpp](../../src/protocol/subscription_writer.cpp); [incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp) | [writer tests](../../tests/conformance/subscription_writer_tests.cpp), [incoming tests](../../tests/conformance/incoming_subscription_tests.cpp), [Java writer capture](../../tests/fixtures/subscriptions/writer/README.md) |
| DATA flags, removal and explicit policies | [stream_data.cpp](../../src/protocol/stream_data.cpp); [live_decode.hpp](../../src/protocol/live_decode.hpp); [subscription_writer.cpp](../../src/protocol/subscription_writer.cpp) | [live writer tests](../../tests/conformance/live_data_writer_tests.cpp), [flags validation](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md) |
| Tape family/descriptor dispatch and recording state | [tape_reader.cpp](../../src/protocol/tape_reader.cpp): `dispatch`; [tape_writer.cpp](../../src/protocol/tape_writer.cpp) | [family tests](../../tests/conformance/tape_message_family_tests.cpp), [RAW tests](../../tests/conformance/raw_tape_tests.cpp), [legacy reader tests](../../tests/conformance/tape_reader_tests.cpp), [writer tests](../../tests/conformance/tape_writer_tests.cpp) |
| Post-protocol membership and receiver lifetime | [subscription_receiver.cpp](../../src/client/subscription_receiver.cpp) | [receiver tests](../../tests/conformance/subscription_receiver_tests.cpp); [HISTORY receiving boundary](../subscriptions/HISTORY-RECEIVING.md) |
| Aggregation properties and independent peer report | [aggregation contract and primary sources](../transport/SERVER-AGGREGATION.md) | [aggregation tests](../../tests/conformance/server_aggregation_tests.cpp), [production oracle](../../tools/upstream/server-aggregation-oracle/README.md) |

Java source checkpoints are `MessageConstants`, `MessageType`, `ProtocolOption`,
`ProtocolDescriptor`, `BinaryQTPParser`, `BinaryQTPComposer`, `EventFlag` and `PentaCodec`,
resolved at the locked revision through [SOURCES](SOURCES.md). The native atomic candidate,
completed-prefix and unsupported-output policies above are deliberate documented boundaries;
they do not relabel Java's broader parsing/masking behavior as malformed wire grammar.
