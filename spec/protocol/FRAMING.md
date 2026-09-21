# Bounded incremental QTP framing

[Русская версия](FRAMING.ru.md). English is authoritative.

QDCXX-012 separates frame extraction, payload dispatch and connection close policy. The provisional
`qd_protocol` component depends inward on `qd_encoding`; encoding has no protocol dependency.
No transport, payload decoding, frame writer, PART assembly or session machinery is included.

## Evidence and wire contract

Locked Java QD 3.355 / `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` is the oracle.
[P02 coverage and provenance](../../tests/fixtures/protocol/p02/COVERAGE.md) records executed
`BinaryQTPParser` observations. [Source inventory](SOURCES.md) identifies parseImpl,
parseMessageType, parseHeartbeat, diagnostic appendBytes and MessageAdapter error handlers.

A frame is `compact-long(length) | body`. Nonnegative length counts only body bytes. Zero body
is EMPTY_HEARTBEAT, without a type; nonzero body begins with compact-long(type) in
0..2147483647, followed by opaque payload. Overlong compact forms remain accepted. Both fields
reuse readCompactLong. The type span ends at the declared body boundary. Incomplete outer
prefix/body waits, retaining every byte; incomplete type in a complete body is TRUNCATED_TYPE.
Negative length is INVALID_LENGTH; negative/too-large type is INVALID_TYPE.

P02 `empty` and `explicit-zero` advance Java input without a heartbeat callback. Hook observations
are instrumentation, not fabricated callbacks. `tiny-heartbeat` delivers one callback. Unknown
in-range 62 is a structurally valid frame: the returning Java consumer reports unknown then
processes the next heartbeat. Malformed heartbeat payload reports message corruption in Java,
but remains an opaque valid C++ frame. The default MessageAdapter closes on the measured unknown
message; that policy does not belong to extraction. Body error recovery when a Java consumer
returns is not the C++ lifecycle policy.

Java header-error diagnostics read additional available bytes (appendBytes), so Java consumed
position and following-frame behavior can depend on feed boundaries after invalid length.
C++ accepted-byte accounting deliberately stops at the invalid prefix, or at the full body for a
type error. Java callback lists/positions are not C++ frame expectations. Frame projections retain
input provenance and explicitly apply this first-error policy.

## Provisional API, ownership and backpressure

[frame_reader.hpp](../../include/qd/protocol/frame_reader.hpp) defines FrameReader, FrameLimits,
Frame, FeedResult, FrameStatus and FrameError. Use is single-threaded and non-reentrant.
`feed(span<const byte>)` borrows input only during the call and returns at most one owning frame.
`accepted` counts bytes copied from this call, including incomplete bytes and error bytes. It never
includes the unaccepted suffix. Resubmit that suffix until exhausted or failed; an empty feed is
not EOF. One-frame delivery is backpressure: no internal output queue exists, and processing a
large feed never allocates in proportion to that feed. `totalAccepted()` counts bytes since reset.
`pending()` exposes exact retained bytes, including length prefix; its view expires on the next
mutating operation or destruction. Input must not alias that internal view.

A Frame distinguishes EMPTY_HEARTBEAT from MESSAGE, stores optional type, owning payload and
start/end stream offsets. Frame ownership survives subsequent feed, reset and reader destruction.
No callback is used. A FRAME_READY result has a frame; other results have no frame.
NEED_INPUT includes a clean boundary. `errorOffset()` is the start of the failed frame; on
OFFSET_OVERFLOW it is the current incomplete frame's start. Interpret it only for FAILED.

## Bounds and lifecycle policies

Both finite budgets are mandatory: maximumBodyBytes and maximumPendingBytes. Reject configurations
whose body exceeds signed 64-bit range, whose body+9 overflows size_t, or whose pending budget is
less than body+9. Zero body budget is valid and permits empty heartbeats. Nine is the maximum
compact-long prefix size. Validate signed lengths before conversion or body allocation. A length
above the body budget fails BODY_LIMIT immediately after its prefix.

The reader allocates exactly bodyBudget+9 byte elements once at construction, independent of the
larger pending budget; allocator bookkeeping is additional. Pending bytes never exceed that
storage. An emitted payload separately owns at most bodyBudget-1 bytes (zero when bodyBudget=0).
The implementation requests one payload allocation at delivery; allocator/container overhead is
not a byte-exact heap guarantee. Caller-retained frames are caller-owned memory and are not
bounded in aggregate by the reader. No speculative body allocation follows an untrusted length.
The cumulative uint64 offset is checked before accepting bytes; overflow is terminal.

The first framing/resource failure is terminal, retains offending pending bytes, preserves earlier
returned frames, and accepts zero on later feeds. There is no scanning or automatic resynchronization.
Constructor allocation failure throws; delivery bad_alloc/length_error becomes terminal
ALLOCATION_FAILURE, with exact accepted count and retained bytes. Allocation failure is not a wire
classification. No user callback can throw from feed.

`finish()` explicitly closes input: a clean boundary returns FINISHED, pending bytes fail
TRUNCATED_INPUT, prior failure remains FAILED. Subsequent feeds accept zero; repeated finish is
idempotent. This is C++ lifecycle policy: base Java parse has no final-EOF method. `reset()` clears
only this framer's bytes, counters and lifecycle, keeps its configured allocation, and has no
Java resetSession semantics. Returned frames remain valid.

## Conformance scope

[Protocol validation](../../tests/conformance/protocol/VALIDATION.md) records P01 and all 17 P02
inputs, whole/every two-chunk split/bytewise/empty and selected multi-split schedules, independent
frame projections, exact pending bytes, resource/backpressure/lifetime checks and negative fixture
verification. Payload syntax, adapter policy combinations, huge allocations, allocation-failure
injection, offset exhaustion and other architectures are outside executed coverage.
