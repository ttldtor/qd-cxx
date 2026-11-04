# Event text oracle

[Русская версия](README.ru.md). English is authoritative.

The launcher invokes actual pinned QD Quote/Profile/Trade/TradeETH/Summary, TimeFormat, EventUtil, enums, sources and WideDecimal methods. Java 25 is the numeric reference; Java 8 observations remain separately classified according to [ADR-0011](../../../docs/decisions/ADR-0011-java-event-text-reference.md). No SDK/class rebuild or network access occurs.

~~~powershell
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk25 -LegacyJavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-text-check
~~~

Mode defaults to Verify. Operational paths are mandatory; RunDirectory must be fresh and inside this checkout's .work. Capture writes only the event-text fixture/proof/provenance candidates. Verify compares without changing them. Both retain full commands, UTC times/exits, raw streams and source/class inventories in RunDirectory. Java runs sequentially with heap128MiB and timeout60 seconds; only an owned timed-out process tree is stopped. Exit0 means success/help, exit1 invalid input/reference, timeout/process, provenance or observation failure.

No arguments and -Help produce identical English help; -HelpRu translates it; Get-Help provides native English reference. These modes need no operational input and create no workload output. JDK25 is also suitable for the separately pinned Java formatter.

The fixture records profile, kind, input and result. Numeric/character/time/enum/flag results are exact ASCII; other results are comma-separated lowercase four-digit UTF-16 units, preserving malformed units and NUL. Numeric version proofs require identical bit round trips and the defined decimal-selection classification. Three cache exceptions require exact cached/fresh Java results; every other row is strict.

Profiles comprise six UTC/fixed contexts (including both signed32 millisecond extrema and a subsecond offset), and eight exact named zones listed in the [fixture record](../../../tests/fixtures/event-text/README.md). Each named process sets the Java default before TimeFormat initialization; no shared formatter is rebound. Named C++ comparisons use the separately qualified immutable IANA 2026f capability.