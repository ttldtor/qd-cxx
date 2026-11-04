# Price-increment oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies actual production Java8 `PriceIncrements` outcomes from the revision in `upstream.lock`. It uses the existing `dxfeed-api`, `dxlib` and `qd-core` production classes; it does not compile upstream, download dependencies, connect to a service or build an SDK.

```powershell
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/price-increment-verify
```

No arguments, `-Help` and `-HelpRu` show help without working inputs, creating directories or starting Java. `Get-Help` describes parameters. `-Mode` defaults to `Verify`; `Capture` replaces only this oracle's `oracle.tsv` and `provenance.json`. Supply all four explicit input paths for either working mode. `RunDirectory` must be inside this checkout's `.work`; owned logs there may be replaced. Global Java settings are unchanged.

The launcher verifies the pinned revision and clean relevant production sources, records the production class inventory before and after execution, compiles the harness with JDK8, then captures UTC/English and Pacific/Honolulu/French outcomes. Output bytes must agree. Every Java command, including version discovery, has heap128MiB and timeout60s. Git checks have timeout30s. A timeout stops only the owned process tree. Exit0 means success/help; Exit1 means invalid inputs, wrong reference, process failure/timeout or an observation/provenance mismatch.

The fixture covers factory grammar and binary64 values, exact text/hash/range projection, precision and range breakpoints, all rounding modes, signed direction extrema, step validation and seeded numerical rounding neighborhoods. Expected outputs call production APIs directly; no expected binary values are calculated by the harness. The [fixture description](../../../tests/fixtures/price_increments/README.md) defines row formats. Full process logs, source/class hashes and `receipt.json` are written to the selected private run directory. `Verify` does not rewrite the fixture.
