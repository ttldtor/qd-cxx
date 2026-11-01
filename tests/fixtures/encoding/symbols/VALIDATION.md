# Java symbol validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Checks executed on 2026-09-21 UTC; not an independent review report.
QD 3.355 / 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f, native JDK 8 reactor, skipped upstream tests,
disabled Maven build cache, unchanged external source. Shared-checkout builds ran sequentially.

- [Capture A](captures/capture-a/metadata.json) and [capture B](captures/capture-b/metadata.json)
  are independent clean builds with equal normalized symbols.tsv; A preceded the C++ implementation.
- [Negative Verify](captures/mismatch/result.txt) rejected changed ABC decoded units; temporary
  expectations were not rewritten. [Baseline Verify](captures/baseline/result.txt) passed unchanged.
- [Proof](captures/proof.json) and [UTC run log](captures/evidence.log) retain the execution order.
- Historical SHA-256 manifest records pre-existing fixture/lock
  bytes checked against HEAD through Git clean-filter hashes; none changed.

Each run retains raw/normalized output, Java/Maven versions, build and javac stdout/stderr,
environment, commands, tool/input hashes, compiled harness/classpath content hashes and dependency
jar hashes. Binaries and build directories are not copied into evidence. Initial exploratory capture
.work/symbol-a preceded the final pair and was not promoted; README additions changed provenance only.
Existing helper sources and previous oracle metadata remain unchanged.

Reproduction commands and normalization are in the [runner README](../../../../tools/upstream/symbol-oracle/README.md).
[Coverage](COVERAGE.md) separates direct API observations, post-error state and production callbacks.
C++ expectations deliberately apply terminal errors rather than Java primitive continuation.
No errors were detected in these bounded checks; full protocol or upstream test parity is not claimed.
