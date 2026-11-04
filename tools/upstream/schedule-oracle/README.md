# Schedule production oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies actual pinned Java `Schedule` factories, venue enumeration
and constructed local rule fields. Reflection reads the models produced by the real constructor;
the harness does not reimplement the parser or call future C++ Day/Session queries.

Use PowerShell 7.4 or newer. No arguments, `-Help`, `-HelpRu` and `Get-Help -Full` require no operational
paths, create no run directory and start no Java work. Operational modes require existing production
classes from the exact [upstream revision](../../../upstream.lock), the reference JDK8/TZDB recorded in
the [identity manifest](../../../resources/schedule/manifest.json), and an existing native
`qdcxx-run-utc` logger. The formatter JDK is independent.

```powershell
pwsh -NoProfile -File tools/upstream/schedule-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/schedule-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/schedule-check-new
```

`Verify` is the default; it compares the tracked observations/provenance without replacing them.
`Capture` writes reviewed candidates to `tests/fixtures/schedule/oracle.tsv` and `provenance.json`
only after both timezone/locale runs agree and source/class inventories remain unchanged.
`RunDirectory` must be a new directory within this checkout's `.work`; previous logs are refused,
not overwritten. Source revision, exact source/runtime/tool/input/output hashes and class inventories
are retained. No reference build, reference write, SDK acquisition or network request occurs.

One owned JVM runs at a time with `-Xmx128m` and a 120-second timeout. A timeout stops only the
launcher's own process tree. The Java harness caps inputs at 4,096 rows/16 MiB and output at 16 MiB.
Cases are UTF-8 TSV with six cells: case ID, mode (`D` default/`N` named/`V` venues/`U` typed update),
hexadecimal UTF-16 venue, hexadecimal UTF-16 trading hours, typed version and ordered typed entries.
Updates are encoded through the real Java defaults loader using its own `TimeFormat.GMT`; this
does not claim that the C++ typed-input boundary implements that raw-byte loader.

Results are actual success-model rows, semantic exception classes or venue lists. Tool/reflection
failures terminate capture and are not converted to semantic fixtures. UTC/English and
Pacific-Honolulu/French observations must match. The corpus includes all 631 reference named IDs,
100 named venue definitions, complete grammar families, malformed inputs and typed-default changes.
The fixtures prove these observations, not every possible grammar string or future date query.

Exit 0 means success/help. Exit 1 means missing/mismatched inputs or provenance, an existing run
directory, timeout, failed process or differing observations. Help performs no workload side effects;
operational modes write only the selected run directory and, in Capture mode, the two candidate files.

The internal `ScheduleOracle` entry point also provides no-argument/`--help`/`--help-ru` help, uses
the same finite case schema and writes its explicit output. Run it through the launcher for runtime,
provenance and process limits. Primary behavior is
[Schedule.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/schedule/Schedule.java)
and [TimeUtil.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java).
