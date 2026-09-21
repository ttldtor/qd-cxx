# C++ framing conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Contract](../../../spec/protocol/FRAMING.md), [test implementation](../frame_tests.cpp),
[validation](VALIDATION.md). All 18 selected inputs are mandatory; no selected input is excluded.

## Independent expectations and schedules

[frames.tsv](frames.tsv) is a reviewed **derived frame-only projection**, not Java callback output.
Input bytes are loaded directly from unchanged [P01](../../fixtures/protocol/expected/P01.tsv) and
new [P02](../../fixtures/protocol/expected/P02.tsv). P01's recorded frame boundaries and type values
supply three opaque payload slices. P02 boundaries/type widths are reviewed against exact input,
Java hooks/positions, error classifications and the [source-linked contract](../../../spec/protocol/FRAMING.md).
No production decoder generated these expectations. Columns record body limit, ordered frame
kind/type/payload/start/end, first C++ framing error, accepted count, pending hex and error offset.
`-` denotes no frames, no type or empty bytes as appropriate.

P02 empty and explicit-zero remain distinct. Unknown 62 and malformed heartbeat payload are emitted
opaquely; only framing errors stop extraction. Java can recover after type/body errors; C++ projections
stop at first framing error. Invalid-length Java diagnostic consumption is not copied into accepted.

Every input runs whole, at every two-part split including empty ends, bytewise with interleaved
empty feeds, and **every three-part partition** with inserted empty feeds. Each call checks accepted
counts, no premature emission, exact retained input suffix, fixed storage size, ordered output and
terminal behavior. Previously returned frames are checked again after later feeds and reset.
finish is checked for clean/truncated/previous-error states, idempotence and rejected subsequent input.

## Resource, boundary and ownership checks

- 10,000 three-byte frames in one 30,000-byte input with an 11-byte reader allocation. Each feed
  accepts/delivers exactly one frame; the unaccepted suffix remains caller-owned. No output queue.
- Saved payload bytes survive further feeds/reset and separate reader destruction.
- Incompatible pending/body budgets and size_t maximum overflow are rejected before allocation.
- Zero body budget permits zero-length heartbeat and rejects a nonempty body.
- A nine-byte overlong length plus nine-byte overlong type exactly fills an 18-byte reader; every
  strict prefix is retained unchanged and does not emit early. Complete type is zero.
- Type INT_MAX succeeds; signed length INT64_MAX fails the small body limit and INT64_MIN fails
  signed-length validation, each after nine prefix bytes without a huge allocation.
- PART type 4 with an invalid-looking payload is emitted opaquely; no reassembly/decoding occurs.

Three doctest cases execute 246,166 assertions. Existing four primitive cases add 43,375 assertions,
covering all 832 compact and 314 UTFString Java observations without exclusions. The negative runner
rejects a changed expected payload byte, invalid input hex, a missing selected input, an empty P02
selection and empty projections; it confirms both original and temporary fixture immutability.

## Limits

Single-threaded API; no concurrency stress. No huge allocations, allocator-failure injection,
uint64 stream exhaustion, 32-bit host, macOS, alternate Java buffers, payload decoding, adapter
policy matrix, frame writing, PART assembly or transport. Exceptions on constructor allocation and
terminal payload-allocation failure are documented contracts, not injected experimental results.
No performance benchmark or complete QTP compatibility claim is made.
