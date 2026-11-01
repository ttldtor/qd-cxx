# Java offline evidence validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Checks executed 2026-09-21 UTC, before CLI implementation; not independent review.
No errors were detected in the final checks below. [Coverage](COVERAGE.md) limits the claims.

Two sequential clean native reactor builds of locked QD 3.355 under JDK 8, with upstream tests
and build cache disabled, produced identical raw .qtp bytes and normalized observations:
[capture-a metadata](captures/capture-a/metadata.json), [capture-b metadata](captures/capture-b/metadata.json).
Each retains build logs, compiler/JVM environment, commands, source/input hashes, harness classes,
classpath content and dependency hashes. Only TSV CRLF→LF is normalized; binary bytes are untouched.
The composer/parser workflow delivered seven records from eight inputs, including unknown-record skip.

Verify rejected a temporary 22,44,0→22,45,0 expected-value mutation:
[mismatch result](captures/mismatch/result.txt), [metadata](captures/mismatch/metadata.json).
Unchanged baseline then passed: [result](captures/baseline/result.txt),
[metadata](captures/baseline/metadata.json), [proof](captures/proof.json),
[UTC workflow log](captures/offline-evidence.log), [first clean capture log](captures/offline-capture-clean-a.log).
Actual observations in all four runs match the immutable expectations; the negative run compares
against a separate deliberately mutated directory. Neither baseline nor temporary expectations were rewritten.

The initial harness attempt passed null to visitHeartbeat and failed before producing observations;
[diagnostic](captures/initial-failed/stderr.txt), [log](captures/initial-failed/run.log).
It is not counted as a successful capture. The final source uses an empty HeartbeatPayload (no bytes
emitted), was formatted before both clean captures, and passed final format Check.
[Java Format log](captures/offline-java-format.log). No upstream source was patched.

Reproduce with PowerShell 7.4+, JDK 8, Maven and the clean pinned checkout:

```powershell
python tools/run_utc.py --log <new-log> -- pwsh -NoProfile -File tools/upstream/offline-inspection-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
python tools/run_utc.py --log <new-log> -- pwsh -NoProfile -File tools/upstream/offline-inspection-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

For negative Verify copy expected files to a temporary directory, mutate the TSV value, and pass
`-ExpectedDirectory <copy>`; require VERIFY FAILED, then rerun the original baseline. Capture never
promotes expectations automatically. Origin/commit/tag/version and clean source are checked before
and after builds. Run directories must be new and separate from expected files.

Historical hashes preserve all pre-existing tracked fixture files
and upstream.lock. Artifact hashes cover retained new capture files.
The conformance audit verifies current
harness/helper/input hashes against actual metadata, capture equality, history and final source hashes.
No upstream full test run, Java callback parity for C++ failures, huge-memory or timing claim.
