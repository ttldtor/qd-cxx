# Description parser conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Contract](../../../../spec/protocol/DESCRIBE-RECORDS.md), [tests](../../description_tests.cpp),
[Java evidence](../../../fixtures/protocol/p08/expected/descriptions.tsv), [validation](VALIDATION.md).

All 81 observations are mandatory with no exclusions. The direct comparison uses Java's explicitly
diagnostic raw projection: full type IDs, exact UTF-16 names/order, completed prefix, category, primitive
cause, consumed position and stage/error offset. Java constructed-map columns remain separate and are
validated as fixture grammar, not mistaken for raw descriptors. No C++ parser generates expectations.
EOF maps to TRUNCATED_PAYLOAD, UTFDataFormatException to INVALID_ENCODING, IOException in the selected
length case to INVALID_LENGTH; header errors have no primitive cause. This is scoped projection, not
Java exception text or local-map parity.

Each Java-created frame is followed by an independent heartbeat sentinel. FrameReader and payload parser
run whole, at every two-part split including empty ends, and bytewise. Tests compare results before and
after later feeds/reset, ensure malformed payloads do not consume the next frame, and explicitly continue
without prescribing session recovery. Only tests compose these layers.

Small tests cover every budget at zero, below, equal and above its successful boundary, exact resource
positions/stages/causes, aggregate-name exhaustion within later records, prior descriptions on resource
errors, empty payload/zero fields, pair-atomic UTF units, UTF logical body overrun precedence, malformed
header precedence, INT_MAX sparse sender ID, INT_MAX count rejected before allocation and size_t arithmetic
limits without allocating enormous buffers. Temporary input destruction checks owned output. All 0..255
raw types are retained in an additional structural property check, not a serialization support claim.

Three doctest cases execute 50,841 assertions. Existing compact/UTFString and framing regressions remain
mandatory in CTest. [Negative runner](../../check_description_negative.py) rejects changed expected type,
invalid hex, unknown row/status, missing case, empty coverage/file and trailing row; both temporary and
original expectations remain unchanged. Fixture parsing requires exact nonempty coverage and known grammar.

No allocation-failure injection, huge allocations, 32-bit/macOS run, concurrency/performance claim,
local schema binding, event decoding, transport, frame writing or full protocol compatibility.
