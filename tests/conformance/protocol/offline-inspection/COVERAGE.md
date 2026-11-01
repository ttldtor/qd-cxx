# Offline inspection conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Contract](../../../../spec/protocol/OFFLINE-INSPECTION.md),
[CLI](../../../../docs/QTP-INSPECT.md), [results](VALIDATION.md),
[Java evidence](../../../fixtures/protocol/offline-inspection/COVERAGE.md).

The process-level [checker](../../check_offline_inspection.py) compares exact stdout, stderr and
exit status against [72 designed cases](../../../fixtures/protocol/offline-inspection/designed.json).
Every case uses bytewise, split and whole/larger-than-file reads with sizes 1,2,3,7,16,4096 and
file-size+1 (deduplicated). The 116-byte production-composed stream additionally uses every size
1..117 and 4096; all outputs are byte-identical and its seven local records match the independent
Java callback observations. This tests file-read partitioning, not callback batching or timing.

| Surface | Executed coverage |
| --- | --- |
| Integration | Several initial description/data frames; heartbeat before/between/after; message-local symbols; remapped IDs; target/skip/duplicate targets; missing fields; unknown local and zero-field records. |
| Empty policy | Empty file, description-only, heartbeat-only, empty description followed by empty data; no populated coverage inferred. |
| First error | Data before descriptions; repeated/empty descriptions after data; duplicate IDs within/across frames; explicit zero/unknown/PART/ticker; non-8 fields including unknown names; nonzero flags; undescribed IDs; truncated payload/final frame/prefix; completed prefix before error; later frame not processed. |
| Offsets | Noncanonical length and type encodings on success and truncation; exact frame boundaries, distinct consumed/error checkpoints and derived absolute positions. |
| Resources | Exact/zero/excess input, frame, frame count, description/action/name totals, record/field/value/symbol/output budgets; per-message minima; skips/repeated targets charged across messages. |
| Invocation | Missing/duplicate/unknown arguments, malformed/negative/overflowing numbers, incompatible storage limits, help, missing file, directory-as-unreadable-file and real closed stdout pipe. |
| Portable injection | Read failure before input and at exact zero input limit; read failure after completed output; rejected write; early flush failure; completion flush failure; broken stderr; exception-enabled output stream. |
| Independent schema | Integration is called with independently validated zero-field local S instead of the built-in adapter, still consuming the wire S.v. |

[offline_io_tests.cpp](../../offline_io_tests.cpp) provides deterministic streambuf failures,
separate from real executable subprocess tests. Physical partial writes cannot be rolled back;
an unsuccessful final flush has nonzero exit even if completion bytes reached the sink.
No partial decoded record is intentionally emitted. Record-budget tests in
[stream_data_tests.cpp](../../stream_data_tests.cpp) verify charged attempts, skips, duplicate
targets, primitive failures, exact/exhausted budgets and unchanged earlier results/positions.
An additional injected read failure after a new partial length header checks that the new frame's
index/start replace the previous delivered frame in diagnostics. Allocation-error counter preservation is source-inspected at the common return paths; no allocator
failure injection is claimed.

The validator rejects unknown row kinds, wrong arity, invalid numeric/hex syntax, invalid ordering,
empty selection, missing/duplicate completion and semantic mismatches. The
[negative checker](../../check_offline_negative.py) rejects 17 temporary mutations of values,
symbols, IDs, names, offsets, row types/arity, syntax, completion, selection and Java observations;
then the unchanged baseline passes. [Proof](negative-proof.json). Expectations remain unchanged.

All six prior suites remain regression checks in all seven test profiles. No full protocol,
importer/transport/dynamic-session, nonzero flags, huge allocations, exact RSS, 32-bit platform,
performance or universal Java exception/callback parity is covered. These are recorded checks;
independent review is still required.
