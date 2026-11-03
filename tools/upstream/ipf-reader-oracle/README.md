# Plain local IPF reader oracle

[Русская версия](README.ru.md). English is authoritative.

`pwsh -File tools/upstream/ipf-reader-oracle/Invoke-Oracle.ps1 -Help` shows built-in English help;
`-HelpRu` shows Russian. Help needs no working inputs, creates no directory and starts no Java.

```powershell
pwsh -File tools/upstream/ipf-reader-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/ipf-reader-verification
```

The launcher uses existing pinned QD 3.355 production classes and JDK 8, without an SDK build,
download, network request, compression test or new C++ dependency. `Mode` defaults to Verify;
`Upstream`, `JavaHome`, `Logger`, `RunDirectory` are required for work. The run directory must be
inside this checkout's `.work`. Use a fresh owned run directory for a new capture: the launcher
replaces named outputs but deletes no historical/orphan files. Java uses 128 MiB heap and a 60-second
process timeout, one process at a time; only its owned timed-out process tree is stopped.

Capture writes candidate raw inputs/outcomes and provenance under `tests/fixtures/ipf-reader`.
Verify regenerates and compares the exact filename/hash inventory, formatted source hashes,
reference revision/source hashes and unchanged production-class inventory. It does not replace
reviewed fixtures. The reference uses its default warn-and-return missing-completion behavior.
All full UTC logs, raw streams, source/class inventories and receipt remain in RunDirectory.
Exit0 means success/help; exit1 means invalid inputs, timeout, process failure or a mismatch.
After Java/launcher edits, use the [pinned formatter](../../format/README.md) and Capture/Verify;
do not repair old hashes manually. See [case format/coverage](../../../tests/fixtures/ipf-reader/README.md)
and the [reader contract](../../../spec/ipf/READER.md).
