# Java tape reader validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Author checks on 2026-09-22 UTC; no independent review is claimed.
Initial clean captures a/b preceded the TAPE-READER contract, ADR-0002 and production C++ edits.
Final clean captures below reran the same Java source with the completed launcher/provenance workflow.
All expected inputs and observations match those initial captures byte-for-byte.

- [Final capture A metadata](evidence/a/probe-metadata.json), [clean baseline/build environment](evidence/a/metadata.json).
- [Final capture B metadata](evidence/b/probe-metadata.json), [clean baseline/build environment](evidence/b/metadata.json).
- [Exact comparison](evidence/compare.txt): all 102 artifacts / 51 cases match without normalization.
- [Ten mutation rejections and baseline](evidence/negatives.txt): values, type, local ID, observed input-byte
  position, unknown row, missing/duplicate completion, empty observation, missing file, empty coverage.
- [Full clean negative Verify](evidence/negative/probe-metadata.json) rejected original.tsv's changed value;
  [baseline Verify](evidence/verify/probe-result.txt) then passed with original expectations unchanged.
- Every invocation also passed the unchanged QDCXX-019 22-artifact Verify, preserving old inspector errors
  and raw default/constrained/text/truncated files. Build/Java logs and actual command arrays are adjacent.

[Reproduction commands](../../../../tools/upstream/tape-reader-oracle/README.md) use PowerShell 7.6.6,
Zulu JDK 8 and Maven 3.9.16, locked clean QD 3.355. Formatter uses Microsoft JDK 21 separately:
google-java-format 1.36.1 Format then Check passed for all 12 repository Java files. No old Java source
changed. Upstream tests were skipped; these are production harness observations, not upstream test-suite results.

Source, launcher, harness class, original/case artifact hashes and timestamps are in actual metadata;
underlying baseline metadata contains environment/service observations, classpath content and dependency
JAR digests. Tool-hash audit is retained with the C++ [final validation](../../../../tests/conformance/protocol/tape/VALIDATION.md).
The negative copy and all run directories were new. No timeout was accepted; each production probe
process finished within its 30-second bound. Primitive callback positions were not fabricated.

No errors were detected in these checks. General tape, compressed input, time sidecars, recovery,
RAW_DATA/default TapeConnector, Quote, transport, huge allocations and allocator injection were not tested.
The C++ terminal/atomic/strict policies are deliberate task-authorized differences, not Java observations.
Historical evidence/upstream.lock remains unchanged.
