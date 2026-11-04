# Production glossary oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` invokes the locked production Java `CFI` and `AdditionalUnderlyings`
classes from `dxfeed-api`, plus existing `dxlib`/`qd-core` class roots. It never rebuilds QD,
starts networking or changes JDK/global settings. The existing checkout must match
[upstream.lock](../../../upstream.lock) with unchanged relevant source files.

```powershell
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 `
  -Mode Verify -Upstream C:/qd -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/glossary-check
```

No arguments, `-Help`, `-HelpRu` and PowerShell `Get-Help` need no working inputs and
start no Java/source work. Verify is the default; Capture replaces this oracle's
`tests/fixtures/glossary/oracle.tsv` and `provenance.json`. Other writes are owned logs,
compiled harness and receipts under the supplied checkout-local `.work` directory.
Exit 0 means success/help; exit 1 means missing/invalid inputs, reference mismatch,
process failure, timeout or differing observations/provenance. Invalid options fail.

The actual `java -version` must identify JDK8; compiler/runtime executables are hashed.
Each Java process has a 128 MiB heap and 60-second timeout; read-only Git checks have
30-second timeouts. Only owned timed-out process trees are killed. Formatter JDK21 is
separate. Production class inventories before/after must agree. Provenance records
pinned source hashes, executable/harness/launcher hashes, complete class-inventory hash,
fixture hash and row count. Full command streams and exit receipts remain local.

The deterministic harness covers all 676 category/group prefixes, every reachable
attribute and its complete ordered values/descriptions, valid/short/unknown/malformed
CFI and packed integers; AU malformed separators, null, duplicates, signed zero,
legacy exponent overflow, UTF-16 boundaries and static lookup. Map rendering has
8,192 seeded random binary64 cases, neighbors of all decimal powers, signed first
128 subnormals and normal/subnormal transitions. It compares UTC/en-US against
Pacific/Honolulu/fr-FR outputs byte for byte. These are production observations,
not a replacement parser or a claim of exhaustive binary64 testing. Additional map observations
cover precision-rounding failures and adjacent binary64 values, plus signed decimal-midpoint
neighborhoods across 1e15 through 1e18.

Native `glossary` CTest reads the captured rows through public APIs and tests owning
lifetimes, constant initialization, finite limits and allocation-failure guarantees.
`glossary_oracle_help` checks noargs/EN/RU help plus invalid invocations when PowerShell
is available; the same check can run separately:

```powershell
cmake -DORG_TTLDTOR_QDCXX_PWSH=pwsh -P tests/conformance/glossary_oracle_help.cmake
```

See [fixtures](../../../tests/fixtures/glossary/README.md) and the
[contract](../../../spec/glossary/VALUES.md). No live service or SDK evidence is claimed.
