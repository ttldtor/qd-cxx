# Field conversion oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies the [schema-bound field conversion contract](../../../spec/events/FIELD-CONVERSIONS.md)
using the existing production classes of the exact revision in `upstream.lock`. It calls generated
QuoteMapping/ProfileMapping and actual MarketFactoryImpl delegates on configured production records.
It does not implement a diagnostic copy of their numeric formulas.

```powershell
Get-Help ./tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/conversion-verify
```

No arguments and `-Help` show English Get-Help; `-HelpRu` shows Russian help. Help ignores operational
paths and starts no process, input validation, directory creation, network or SDK work. Verify is the
default operational mode. All four paths are required for Capture/Verify: Upstream is an existing
locked checkout with built classes; JavaHome is JDK8; Logger is an existing UTC argument-vector logger;
RunDirectory is a fresh directory contained in this checkout's `.work`. Redirecting ancestors and an
existing run directory are refused. The formatter uses a separate JDK and does not build the reference.

The helper starts one bounded subprocess at a time, including `java -version`: 128MiB Java heap and
60-second timeout. Only its own timed-out process tree is stopped. It compiles the harness into the
new run directory, then observes UTC and Pacific/Honolulu. It records full commands, raw stdout/stderr,
UTC start/end/exit, source hashes and production-class inventories before/after. It refuses a dirty
relevant upstream source, a wrong pinned revision, different timezones or a changed class inventory.
No dependency download, network connection, upstream build or upstream file mutation occurs.

Capture replaces only `tests/fixtures/field-conversions/oracle.tsv` and its `provenance.json` with
candidate observations. Verify compares these files and harness/launcher/source/class provenance
without replacing them. Both modes retain all working logs under RunDirectory. Public provenance is
UTF-8 without BOM with LF; raw TSV bytes are preserved by `.gitattributes`.

Exit0 means success or help. Exit1 means invalid/missing inputs, path containment, unavailable/wrong
reference, timeout, subprocess error, output or provenance mismatch. Failure logs remain available;
choose a new run directory for another attempt.

The [capture inventory](../../../tests/fixtures/field-conversions/CAPTURES.md) describes supported,
boundary, missing and negative cases. Pinned BYTE/SHORT factory refusal is recorded directly. String
cases pass actual encoded bytes through production StringField read/write before delegate creation.
Each of the four Quote and ten Profile public business-double getters is captured separately,
including its own missing default, for comparison with the typed native business getters.
Production binary64 delegate results are separate from WideDecimal accessor bits; they do not imply
that composing a LONG and then presenting it as binary64 equals direct LONG-to-binary64 conversion.
This is offline production mapping/delegate evidence, not a live service test.
