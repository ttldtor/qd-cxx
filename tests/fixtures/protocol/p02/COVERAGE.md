# Executed P02 framing evidence

[Русская версия](COVERAGE.ru.md). English is authoritative.

QDCXX-012 adds P02 separately from historical P01/P03/P04/P05. The locked Java QD 3.355 source
is built with JDK 8; [FramingOracle.java](../../../../tools/upstream/protocol-oracle/FramingOracle.java)
extends the existing source-built driver. [Exact observations](../expected/P02.tsv) include 17
subcases. [Validation](VALIDATION.md) records reproduction and immutable Verify.

| Subcase | Stimulus and observation |
| --- | --- |
| empty | `00`: zero body, position advances by one, no heartbeat callback. |
| explicit-zero | `0100`: explicit type 0, empty payload, advances by two, no callback. |
| tiny-heartbeat | `020000`: type 0/content bits 0, one heartbeat callback. |
| concatenated | The preceding three frames together; boundaries distinct, one callback. |
| negative-length | `7f` followed by tiny heartbeat; stream error, diagnostics consume available tail. |
| limit-minus / limit-at / limit-plus | Body 7/8/9 with configured maxMessageSize=8; type 3 and zero opaque bytes, then tiny heartbeat. First two accepted; third stream error before payload dispatch. |
| partial-length / partial-body | `80` / `0300`: positions reset to 0, respectively 1/2 pending bytes, no errors. |
| truncated-type | `0180` followed by tiny heartbeat: stream error, cannot borrow next-frame bytes; returning consumer allows following callback. |
| negative-type / large-type | Type -1 / 2147483648 then tiny heartbeat: stream error and following callback. |
| overlong | Two-byte length=2 and type=0 followed by tiny heartbeat; accepted, following callback. |
| unknown | Type 62 then tiny heartbeat: ordered unknown notification, then heartbeat callback. |
| bad-payload | Type 0 with content bits 1 but no value, then tiny heartbeat: message corruption(type=0), then callback. Structurally valid frames. |
| good-before-error | Tiny heartbeat, truncated-type frame, tiny heartbeat: callback, stream error, callback. |

Every input is replayed whole, at every nonempty two-chunk split, and bytewise with empty calls
before/between/after bytes. Exact bytes and schedules are recorded in p02.input/p02.replay rows;
p02.feed records chunk and cumulative supplied counts. p02.state records totalPosition, available,
callback count and error count. The retained suffix is exactly input[totalPosition:supplied].
The explicit recording consumer returns from all error notifications; it does not close a connection.

Protected observational hooks report before-length, after-type and after-successful-payload positions.
They do not alter input or synthesize callbacks. After-body is not necessarily the frame end: reserved
type 3 ignores payload; the outer parser subsequently seeks to the declared end. Before-length and
final positions establish those boundaries. Empty-frame hook events are not heartbeat callbacks.

Header-error reporting calls appendBytes, which reads extra available bytes. In this small profile
whole input is consumed after invalid length; fragmented runs can parse subsequently supplied bytes.
This measured diagnostic side effect explains schedule-dependent Java error recovery. Type/body
errors are confined to the body and the outer parser seeks its end. C++ terminal-first-error
[projections](../../../conformance/protocol/frames.tsv) intentionally differ from Java recovery.

One minimal adapter comparison starts a real default MessageAdapter subclass and passes type 62
through BinaryQTPParser: alive/open becomes not-alive/closed after the unknown notification.
No socket or connection is used for this comparison. Full policy flag combinations remain deferred.
The P02 semantic file contains no record rows; it is not the frame expectation source.

No final-EOF call, huge input/allocation, payload implementation, PART assembly, transport, timing,
other buffer subclasses or complete error-policy matrix is claimed. Existing P01/P03/P04/P05 inputs,
captures and expectations are unchanged; old statements about unexecuted P02 are historical.
