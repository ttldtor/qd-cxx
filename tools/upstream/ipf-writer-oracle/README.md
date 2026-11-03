# Local IPF writer oracle

[Русская версия](README.ru.md). English is authoritative.

`pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Help` and `-HelpRu` show
English/Russian built-in help without inputs, directories, Java, network or SDK work.

```powershell
pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe `
  -RunDirectory .work/ipf-writer-verification
```

The launcher uses existing pinned QD3.355 production classes and JDK8. Verify is the default;
Capture writes only writer fixture/provenance candidates. Upstream, JavaHome, Logger and
RunDirectory are required for work. The run directory must be inside this checkout's .work;
use a fresh owned directory per capture because the launcher replaces names but deletes no
historical/orphan files. Each owned process has128MiB heap/60second timeout; only its timed-out
process tree is stopped. Processes run one at a time, with full logs/source/class/tool hashes.
Exit0 means success/help; exit1 means invalid/missing inputs, mismatched provenance/observations,
unavailable reference, timeout or process failure. No download/build/network occurs.

Optional `-NativeDirectory <directory>` also reads the exact25 accepted native .ipf outputs
with the actual pinned Java reader, verifies byte identity to Java writer output and canonical
reader outcomes, and writes native-read.tsv inside RunDirectory. Native tests emit these files
only when their test-only `ORG_TTLDTOR_QDCXX_IPF_WRITER_ORACLE_OUTPUT` environment variable names
an owned output directory; this is not a library property. No extra/orphan files are accepted.

[Fixtures](../../../tests/fixtures/ipf-writer/README.md) and the
[writer contract](../../../spec/ipf/WRITER.md) define the fixed32-case coverage and explicit
native fidelity restrictions. Java's actual lossy/rejected output remains captured as evidence.

