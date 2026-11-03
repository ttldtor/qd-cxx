# Event time and regional Quote oracle

[Русская версия](README.ru.md). English is authoritative.

`EventTimeOracle.java` invokes the pinned production `DXFeedScheme`, `MarketFactoryImpl`,
Quote/Profile mappings, delegates, delegate-set selection and value getters. It does not copy
their calculations. The [contract](../../../spec/events/EVENT-TIME.md) defines the native subset.
The [capture inventory](../../../tests/fixtures/event-times/CAPTURES.md) describes the original rows and companion.

Prerequisites: existing built production classes from the exact `upstream.lock` QD3.355 revision,
JDK8, PowerShell7.4+ and an existing `qdcxx-run-utc.exe`. No Maven, network, download, installation,
SDK build or endpoint starts. Java formatting separately uses pinned google-java-format with JDK21.

```powershell
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-time-verify-1
```

No arguments and `-Help` show English full help; `-HelpRu` shows Russian help.
`Get-Help ./tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Full` is also available.
Help ignores nonexistent operational paths and creates no directories or Java processes.
All four paths are required for work. `Mode` defaults to `Verify`: compare tracked fixture and
provenance without replacing them. `Capture` replaces only `tests/fixtures/event-times/oracle.tsv`
and the companion `representability.tsv` and `provenance.json` with review candidates.

`RunDirectory` must be new and inside this checkout's `.work`; previous attempts are never
overwritten. The launcher writes compiled harness classes, raw stdout/stderr, full UTC command/exit
logs, production class inventories and a receipt there. Each compiler/JVM has a128MiB Java heap
and a60-second timeout; only its own timed-out process tree is stopped. Production classes are
inventoried before and after. Provenance records revision, source/tool hashes, class inventory hash,
fixture hash, row count and timezone agreement, without private absolute paths.

Fresh JVMs observe `default`, `sequence`, `nano`, `both`, `millis`, `millis-default`, `minimal`, `reordered`, `missing`.
Quote, Quote&Q, Quote&z and Profile come from actual generated production records. Minimal/missing/
reordered clone those fields and preserve standard witnesses separately. Five raw cursor samples
per accepted record cover negative and signed32/signed64 boundary times, optional raw Sequence/nano,
independent timestamp-sequence metadata and null/empty/nonempty Profile strings. Required-field
factory failures are retained. Additional actual delegate and delegate-set calls cover existing
regional suffixes, trailing attributes, exact regional route selection and composite fallback.

The corpus contains2152 LF-terminated TSV rows. Raw observations retain exact bytes; JSON provenance
is UTF-8 without BOM with LF endings. UTC and Pacific/Honolulu results must be byte-identical.
Native owning/borrowed conformance runs as `ctest -R '^event_time_java$' --output-on-failure`.
Ordinary C++ builds use checked-in fixtures and need no Java. Installed-Feed loopback checks
separately establish real `Quote&Q`/base-symbol delivery; this oracle is not public-service live proof.

The separate four-row `representability.tsv` calls a production Quote&Q delegate in both directions.
It retains two noninvertible spellings and two representable attributed spellings, without changing
the original2152-row time corpus. Native selected regional routes explicitly refuse noninvertible
selectors before mutation publication. Long attributed native routing also checks that conversion
uses one allocation rather than a growing-prefix transient. These are native bounds and subset policy.

Exit0 means help or complete capture/verification. Exit1 means invalid/missing arguments, wrong
reference/JDK, unavailable inputs, outside/reused run directory, process failure/timeout, changed
production classes, timezone disagreement, fixture/provenance mismatch or output failure.
Completed logs remain after a failure; incomplete runs have no PASS receipt.
