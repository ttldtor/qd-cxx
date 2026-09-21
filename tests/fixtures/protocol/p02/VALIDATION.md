# P02 executed validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

The initial evidence gate executed 2026-09-21 UTC before dependent C++ implementation.
Final source-provenance refresh ran afterward, with identical observations. Windows 11 amd64, PowerShell 7.6.6,
Zulu 1.8.0_422-b05 oracle, Maven 3.9.16; formatter separately uses Microsoft JDK 25.0.4.7 and pinned
google-java-format 1.36.1. The upstream lock remains unchanged.

## Results and provenance

Two independent sequential clean source builds produced identical raw stdout for all ten cases,
including 17 P02 subcases / 117 schedules. All 18 historical expectation files matched before
promotion; only P02.tsv and P02.semantic.tsv were added. Capture does not write expectations.

[Capture A](captures/capture-a/metadata.json) and [capture B](captures/capture-b/metadata.json)
retain actual commands, upstream identity/cleanliness, source hashes, compiled harness hashes,
classpath content hashes, dependency inventory, environment/provider output and build logs.
Raw stdout/stderr is retained under each capture's raw directory. P02 corruption intentionally
produces timestamped diagnostic stderr; raw stdout is deterministic. Only CRLF-to-LF normalization
is applied to exact expectations. No diagnostic stderr is presented as a semantic golden.

The existing [Verify self-test](../../../../tools/upstream/protocol-oracle/Test-Verify.ps1) deliberately
changes only a temporary P04-order semantic value 22 to 999. The affected full protocol workflow
rejects precisely that file, then passes all 20 files / ten cases against the real baseline.
[Negative metadata](captures/mismatch/metadata.json), [baseline metadata](captures/baseline/metadata.json)
and [proof](captures/verify-proof.json) retain exact results and both sets' unchanged hashes.
This also verifies unchanged old cases with the modified dispatcher/launcher. P02 expected-byte
mutation detection is additionally exercised by the [C++ negative checks](../../../conformance/protocol/negative-proof.json).

Audit checks all four runs against current harness/launcher/cases/inspector hashes,
all ten exact outputs against the independent captures, 117 complete schedules, default-adapter
closure and 305 historical evidence hashes, including upstream.lock,
all existing schema/protocol/encoding expectations and captures. Audit result
passes. Historical hashes were collected before edits, not rewritten to match new outputs.

New UTC command logs: [capture A](captures/p02-final-a.log), [capture B](captures/p02-final-b.log),
[Verify](captures/p02-verify.log). Maven now explicitly receives `-Duser.timezone=UTC`; child Java
already used UTC. A preliminary unpromoted p02-a trial exposed an incorrectly sized test stimulus;
its limit inputs were corrected before both final captures. No historical fixture was corrected.

## Canonical line endings and preserved initial gate

The original pre-implementation [capture A](captures/pre-implementation/capture-a/metadata.json),
[capture B](captures/pre-implementation/capture-b/metadata.json),
[Verify proof](captures/pre-implementation/verify-proof.json) and their raw logs remain separate.
A final audit found that Git could normalize Windows harness/input line endings after capture.
The task pins canonical LF for the protocol harness/cases and byte-preserving attributes for new
raw evidence. Both captures and negative/baseline Verify were rerun after normalization; current
metadata above records actual refreshed hashes, never edited historical hashes. All observations
and P02 expectations remain identical. This refresh changes provenance bytes, not Java behavior.

## Reproduction

Use the [existing launcher setup](../../../../tools/upstream/protocol-oracle/README.md). Run Capture
with new RunDirectory values p02-lf-a and p02-lf-b sequentially on the same checkout. Review
new P02 output before promotion; never copy regenerated historical files over their expectations.
Run Test-Verify with a new TestDirectory p02-lf-verify. Metadata contains complete native command
arrays; the UTC wrapper commands are preserved in the logs above. Audit retained evidence with:

```powershell
python tools/run_utc.py --log .work/framing-evidence-audit.log -- python tests/fixtures/protocol/p02/audit.py
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-25.0.4.7-hotspot'
```

Format preceded both final captures; Check passes all four Java files. No schema/encoding harness
source changed, so their independent build workflows were not rerun. Their oracle bytes are included
in the immutability audit and compact/UTFString are covered by C++ regressions.

## Interpretation and limits

[Coverage](COVERAGE.md) distinguishes observational hooks from actual callbacks, declared frame
boundaries from after-body hooks, returning-consumer recovery from default adapter closure, and
header diagnostic consumption from frame extraction. The observed header-position discrepancy was
resolved against BinaryQTPParser.appendBytes before implementing the C++ policy. No contradictory
observation was normalized away. Final EOF and terminal C++ failure are explicit lifecycle policies,
not Java-equivalent parser methods. Full consumer flag combinations and all excluded payload/session/
transport behavior remain unmeasured. No upstream tests were run by these source builds.
