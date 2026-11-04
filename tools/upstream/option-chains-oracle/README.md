# Production option-chain oracle

[Русская версия](README.ru.md). English is authoritative.

`OptionChainsOracle.java` reads reviewed UTF-16 hex fields and calls the pinned production
`OptionChainsBuilder.build`, `OptionChain.getSeries`, `OptionSeries.getCalls/getPuts/getStrikes`
and `getNStrikesAround`. It contains no copied grouping/comparison/selection algorithm.
Only the unordered outer chain map is sorted to make capture deterministic.

```powershell
pwsh -NoProfile -File tools/upstream/option-chains-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/option-chains-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/options-verify
```

Verify is the default and compares exact fixture/provenance. Capture replaces only
`tests/fixtures/option-chains/oracle.tsv` and `provenance.json` with candidates.
Both require existing pinned production classes (`dxfeed-api`, `dxlib`, `qd-core`), JDK8,
UTC logger and an owned RunDirectory inside this checkout's `.work`. No network,
dependency acquisition or SDK build is performed. Each Java/javac/version process runs
alone with heap128MiB and timeout60s; timeout terminates only that owned process tree.

Output includes complete command/stream/exit logs, actual Java version, source/binary
hashes, immutable production-class inventories and receipt.json. Existing owned run
files may be replaced. Public provenance contains relative source paths and hashes;
private environment paths stay in run logs. Exit0 means success/help; exit1 means
invalid inputs/reference, timeout, process failure or mismatch. No arguments and -Help
have identical English output; -HelpRu is Russian. Help requires no inputs and creates
no directories or Java processes. `Get-Help -Full` documents every parameter.

[Fixture format and coverage](../../../tests/fixtures/option-chains/README.md).
