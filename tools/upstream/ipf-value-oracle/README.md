# Instrument-profile value observations

[Русская версия](README.ru.md). English is authoritative.

Run `pwsh -File tools/upstream/ipf-value-oracle/Invoke-Oracle.ps1 -Help` for built-in English help, or `-HelpRu` for Russian, without
inputs, directory creation or Java/network work. This Windows PowerShell 7 launcher uses an existing
JDK 8, the exact pinned Java QD checkout and its production classes; the formatting JDK is separate.
It does not build or download an SDK. The oracle has no production C++ dependency.

```powershell
pwsh -File tools/upstream/ipf-value-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/ipf-verification
```

`Mode` defaults to Verify. `Upstream`, `JavaHome`, `Logger`, `RunDirectory` are required for work.
The output directory must be inside this checkout's `.work`; existing owned logs can be replaced.
The Java process uses 128MiB heap and a 60-second timeout. One process runs at a time. Both UTC and
Pacific/Honolulu observations must match. Production class inventories before/after must match.
Full UTC command logs, raw streams, class inventories and a receipt remain in the output directory.
Exit 0 means success/help; exit 1 means invalid input, reference/provenance/observation mismatch,
timeout or process failure. The launcher stops only its own timed-out process tree.

Capture replaces only `tests/fixtures/ipf/value-oracle.tsv` and `provenance.json` with candidates;
review them together. Verify regenerates observations and checks fixture, source/harness/launcher
hashes and class inventory without replacing reviewed fixtures. After Java/launcher edits, run the
[pinned formatter](../../format/README.md) and Capture/Verify; do not repair hashes manually.
[Fixture format and coverage](../../../tests/fixtures/ipf/README.md) and the
[value contract](../../../spec/ipf/PROFILE.md) define the evidence scope.
