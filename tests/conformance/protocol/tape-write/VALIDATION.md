# Outgoing production witness validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

QDCXX-031 local verification on 2026-10-11 used unchanged QD 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK 8 `1.8.0_422`, CMake and PowerShell 7.4+.
This report covers the new Java witness tooling/corpus; no C++ writer was implemented or tested.

| Executed check | Actual result |
| --- | --- |
| Fresh checkout origin/HEAD/tag/clean checks; clean `qds-file -am` reactor build | PASS; build reported 1:08, fourteen actual classpath/resource trees retained. |
| Pinned google-java-format 1.36.1 on formatter JDK 23.0.2, Format then Check | PASS; 25 repository Java sources checked. |
| Two new Capture executions, 20:54:20–20:54:24 UTC | PASS; eleven production cases, eight records per case, zero parser errors. |
| Strict cross-capture comparison | PASS; all 43 artifact bytes equal, including current provenance/receipt and ordered observations. |
| Fresh production Verify, 20:54:57–20:55:00 UTC | PASS; all 43 artifacts match retained expectations. Verify did not rewrite expectations. |
| Byte/type/order/completion/provenance/file-set mutations | PASS; all six rejected, followed by a successful unchanged-baseline comparison; the order case swaps valid rows and requires the order diagnostic. |
| Hidden extra file/directory mutations | PASS; both rejected by exact-set guards, including hidden items. |
| Invalid full type in both comparison peers | PASS; rejected by typed grammar, independently of peer equality. |
| Unchanged real Quote/Profile default witnesses | PASS; six old `.qtp` files byte-identical, all old observation lines identical. |
| Repeated production Verify, comparator, Java Check and mutation suite | PASS; nine mutants and unchanged baseline; only current tool provenance changed after verification fixes, all 42 other artifacts unchanged. |
| Existing Windows MSVC Release CTest baseline | 44/45 passed inside sandbox; formatter alias-control creation was denied. Its isolated repeat outside sandbox passed 1/1. No new C++ writer is covered by this baseline. |

The source/provider/input hashes and bytecode receipt are public in the
[retained expectations](../../../fixtures/protocol/tape-write/expected/provenance.tsv).
[Capture details](../../../fixtures/protocol/tape-write/CAPTURES.md) and
[coverage/source facts](../../../fixtures/protocol/tape-write/COVERAGE.md) explain each witness.
The [launcher](../../../../tools/upstream/tape-write-oracle/README.md) preserves complete raw logs and
run-specific metadata in each explicitly supplied run directory.

The old text corpus's CRLF versus new LF is handled only by full-line comparison to historical
observations. New artifacts retain exact bytes. Diagnostic scans and constructed omission inputs
are not counted as production parser acceptance. No fault-injected disk failure, private tape,
transport, general durability/publication, or future C++ writing/rewriting support is established.
