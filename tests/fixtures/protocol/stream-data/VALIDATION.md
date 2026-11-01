# Java STREAM_DATA validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Checks executed on 2026-09-21 UTC, not independent review. No errors were detected in these
checks. [Coverage](COVERAGE.md) bounds all claims; upstream tests were skipped.

Two sequential clean native reactor builds with JDK 8, build cache disabled and pinned QD 3.355
produced equal normalized observations: [capture-a metadata](captures/capture-a/metadata.json),
[capture-b metadata](captures/capture-b/metadata.json). Only CRLF-to-LF normalization is applied.
Raw stdout/stderr, build logs, environment inventory, exact commands, source/input hashes, classpath
content and dependency hashes are retained beside metadata. The first capture completed before C++
implementation. Shared-upstream builds were never concurrent; upstream source was not patched.

A temporary expected value mutation 22,44,0 → 22,45,0 failed exact Verify:
[mismatch result](captures/mismatch/result.txt), [metadata](captures/mismatch/metadata.json).
Then unchanged baseline Verify passed: [result](captures/baseline/result.txt),
[metadata](captures/baseline/metadata.json), [proof](captures/proof.json), [UTC log](captures/stream-verify.log).
Temporary and real expected bytes stayed unchanged. Historical SHA-256
records 775 prior fixture/upstream.lock paths, all compared unchanged after work.

Reproduce with PowerShell 7.4+, existing JDK 8, Maven and the locked clean checkout:

```powershell
pwsh -NoProfile -File tools/upstream/stream-data-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
pwsh -NoProfile -File tools/upstream/stream-data-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

Capture never promotes goldens; review normalized/stream-data.tsv before promotion. For negative
Verify pass a temporary copied/mutated directory via -ExpectedDirectory, require VERIFY FAILED,
then rerun baseline. Runner requires a new output directory and checks origin/commit/tag/version and
source cleanliness before/after builds. Historical fixtures/lock are never rewritten.

No diagnostic parser replaces production delivery. Helpers ProtocolOracle/FramingOracle/SchemaOracle
are unchanged and hashed. There is no instrumentation of parser state. Designed C++ prefix retention,
resource/error positions and unsupported flags are documented separately in the
[contract](../../../../spec/protocol/STREAM-DATA.md) and
[conformance results](../../../conformance/protocol/stream-data/VALIDATION.md). No huge allocations,
upstream full test suite, transport, full flag/event/time or universal callback parity claim.

Artifact SHA-256 also fixes the retained environment output,
JDK/Maven version files, raw diagnostics, logs and metadata for all four runs.
