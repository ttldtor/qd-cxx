# Summary production oracle

[Русская версия](README.ru.md).

SummaryOracle.java calls the actual locked QD3.355 generated SummaryMapping, SummaryDelegate and
Summary getters. Complete composite/regional records come from production DXFeedScheme; physical
long slots and VOID tails are retained. No converter or delegate implementation is copied into
the harness. OpenInterest observes the actual getAsInt path separately from long/double/wide accessors.

Use Get-Help ./Invoke-Oracle.ps1 -Full, invoke without arguments, or pass -Help/-HelpRu. Help needs
no operational paths, creates no workload files and starts no Java, SDK, Git or network work.

```powershell
pwsh -File tools/upstream/summary-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/summary-verify
```

Mode defaults to Verify: raw fixture/provenance are compared and retained. Capture replaces only
tests/fixtures/summary/oracle.tsv and provenance.json. Four operational paths are required: exact
locked checkout with already built production classes, JDK8, UTC logger, and a new run directory
contained within this checkout's .work. Redirecting ancestors and reused directories are refused.
No upstream source/classes, SDK, configuration or public endpoint is changed.

Subprocesses are sequential and bounded to60seconds; Java uses at most128MiB heap. Only the
launcher's own timed-out process tree is stopped. Git HEAD/diff and actual JDK8 version are logged,
as are javac and each observation run. Git executable discovery selects one application path.
The actual version line is extracted explicitly, without using logger COMMAND text as version data.
Logs retain full commands, exits, UTC intervals, stdout/stderr and class/source inventories.

Fixed production inputs enable Summary regional exchanges Q/z (fixture inputs, not implementation
constants), default wide=true, explicit wide=false tiny fallback, and explicit price/volume int
profiles. Each runs in UTC and Pacific/Honolulu; corresponding output bytes must match.
Source hashes, harness/launcher/JDK hashes, real Java version, production-class inventory digest
and raw fixture digest appear in public provenance without local paths.

Exit0 means successful help/Capture/Verify. Exit1 means invalid inputs, unavailable/changed reference,
process error/timeout, observation mismatch or I/O failure. Capture writes candidate fixture/provenance
and new run artefacts; Verify writes only its new run artefacts. Preserve failed runs and choose a
fresh directory for retries. No public feed connection or SDK rebuild occurs.

Format the owned harness with the pinned Java formatter before Capture and run Verify after final
source freeze. Raw TSV uses no Git text conversion; provenance is UTF8withoutBOM/LF.
See [Summary](../../../spec/events/SUMMARY.md) and [capture layout](../../../tests/fixtures/summary/CAPTURES.md).

## Corrected OpenInterest long reference

[ADR-0010](../../../docs/decisions/ADR-0010-summary-open-interest-width.md) corrects the demonstrated
pinned delegate loss for finite representable OI. The original helper, raw `oracle.tsv` and its
provenance remain unchanged. Use the separate maintained helper:

```powershell
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -Help
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -HelpRu
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/oi-reference-check
```

Get-Help/no arguments/EN/RU help need no operational inputs and launch no Java. The same four
operational paths and Verify(default)/Capture modes, 128MiB heap, 60-second subprocess timeout,
new contained .work directory, offline existing-class prerequisites and exit0/1 rules above apply.
Capture writes only `open-interest-long-v1.tsv`, `open-interest-boundary-v1.tsv` and their separate
provenance; it never rewrites the original capture. Verify writes only fresh run artifacts.

The new harness reconstructs complete captured physical fields through production field factories,
checks the actual delegate OI against every original successful row, and independently calls the
actual mapping-long accessor. Its reference records both values, the corrected expected value and
an explicit IDENTICAL/CORRECTED_INT_ACCESSOR_LOSS classification. Every native sample needs exactly
one matching reference; absence, duplicates, wrong raw value or misclassification fail. No OI cell
is ignored. Additional production boundary records cover all six admitted numeric types,2^31/2^32,
signed64 edges, NaN/infinity and regional absence. Corresponding UTC/Honolulu outputs must match;
provenance binds the immutable raw capture and both new outputs. Format the new harness separately
with the same pinned formatter before Capture and run Verify on final source.
