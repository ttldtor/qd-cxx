# Configuration version oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies481 deterministic observations from pinned QD3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. The Java harness uses production QD collectors,
`VersionedRecord`, `ConfigurationDelegate` and its public event accessors. It starts no network or
endpoint and performs no SDK build or download. The existing production classpath must already exist.

```powershell
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help ./tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/configuration-version-check
```

No arguments and either help switch require no working inputs and start no work. `Mode` defaults to
`Verify`. Operational invocation requires `Upstream`, oracle `JavaHome` (JDK8), existing UTC `Logger`
and a new `RunDirectory` inside this checkout's `.work`. Redirecting ancestors are rejected. Capture
creates the maintained fixture/provenance candidates and refuses an existing fixture directory;
Verify compares exact observations, source hashes and provenance without replacing fixtures.

One Java process runs at a time, with heap128MiB and timeout60s. Timeout cleanup owns only the launched
process tree. Full UTC command/exit logs, raw stdout/stderr, exact source and class/resource inventories,
two timezone outputs and a receipt are retained in `RunDirectory`. Exit0 means success or help; exit1
means invalid input, unavailable/mismatched reference, timeout, process or observation failure.
Formatter Java is configured separately; it must not change the oracle JDK8.

The scenarios cover standard, absent-Version and wide-Version mappings on TICKER and STREAM, fresh
negative/zero/signed-extreme seeds, lower/equal updates and multiple versions in one batch. Native Feed
tests separately cover late-subscription cache replay in `misc_feed_tests.cpp`.
The absent case uses an explicit production `VersionedRecord` without the field; it witnesses disabled
mapping, without qualifying a separate environment-property source. Actual default events include
symbol, eventTime, version and attachment. The writer checks errors after flush and close before success.
UTC and Pacific/Honolulu results must match exactly. See [fixtures](../../../tests/fixtures/configuration-version/README.md)
and the [native receiving contract](../../../docs/MISC-EVENTS.md).
