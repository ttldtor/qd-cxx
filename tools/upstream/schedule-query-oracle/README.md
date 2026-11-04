# Actual Schedule query oracle

[Русская версия](README.ru.md). English is authoritative.

`ScheduleQueryOracle.java` calls pinned production Schedule/Day/Session, actual Java TimeZone raw
components, all631 Java identities and complete captured SystemV transitions. It does not implement
a substitute calendar algorithm. `Invoke-Oracle.ps1` defaults to Verify; Capture records new actual
fixture/provenance bytes only after UTC/en and Honolulu/fr results agree. Upstream sources/classes
are read-only, class inventories are compared before/after, and the launcher verifies exact JDK8
executable/TZDB hashes from the existing schedule manifest. The formatter JDK is independent.

```powershell
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/schedule-query-verify-new
```

Use an existing checkout of the full revision in `upstream.lock` with its built dxfeed-api/dxlib/qd-core
classes and the exact production JDK8 from `resources/schedule/manifest.json`. All operational paths
are explicit. RunDirectory must be new and inside this checkout's `.work`; no existing logs are
replaced. Each owned process has a128MiB Java heap and120-second timeout; timeout cleanup stops only
that owned process tree. No download, network, upstream build or SDK acquisition occurs. Exit0 is
success/help; exit1 is input, provenance, process or comparison failure. No arguments, `-Help`,
`-HelpRu` and `Get-Help -Full` need no working inputs and start no Java workload.

The six input cells are ID, operation, reserved `-`, UTF-16 definition hex, numeric argument and
operation-specific auxiliary value. D/Y/T/S cover actual day-id/YMD/time/session queries; R preserves
actual raw/total/public raw observations; H preserves old/new fields and Java live navigation after
a typed-default equivalent update. C++ compares the constructor raw component from R separately,
and explicitly tests its stronger immutable navigation policy from H. The
[Java fixture/provenance](../../../tests/fixtures/schedule-query/provenance.json) remains TZDB2024a;
[36 source-derived2026f data expectations](../../../tests/fixtures/schedule-query/tzdb2026f-provenance.json)
are separate and per-case. Invalid or degenerate Java operations that would not terminate are not
run as oracle captures; C++ finite-work refusal has its own bounded test.

## Filter observations

`ScheduleFilterOracle` and its `FilterObservation` helper capture the separate
[filter corpus](../../../tests/fixtures/schedule-filter/cases.tsv), without changing the earlier
query oracle or its provenance. Compile both files against the same pinned production classes and
JDK8, then run with a new output path. The semicolon classpath below is for PowerShell on Windows;
use `:` on Unix. `javac` and `java` refer to that JDK's executables.

```powershell
$qdRoot = 'C:/path/to/pinned-qd'
$classes = 'C:/path/to/existing-empty-classes-directory'
$output = 'C:/path/to/new-observations.tsv'
$cp = "$qdRoot/dxfeed-api/target/classes;$qdRoot/dxlib/target/classes;$qdRoot/qd-core/target/classes"
javac -encoding UTF-8 -cp $cp -d $classes tools/upstream/schedule-query-oracle/FilterObservation.java tools/upstream/schedule-query-oracle/ScheduleFilterOracle.java
java -cp $classes qdcxx.oracle.ScheduleFilterOracle --help
java '-Duser.timezone=UTC' '-Duser.language=en' '-Dcom.dxfeed.schedule.download=' -cp "$classes;$cp" qdcxx.oracle.ScheduleFilterOracle tests/fixtures/schedule-filter/cases.tsv $output
(Get-FileHash $output).Hash -eq (Get-FileHash tests/fixtures/schedule-filter/oracle.tsv).Hash
```

No arguments, `--help` and `--help-ru` need only the compiled runner and start no schedule work.
The runner accepts at most 4096 rows and 16 MiB of input, requires a new output, and writes UTF-8 LF.
It exits zero on success and nonzero on malformed input, I/O or production errors; a failure after
output creation can leave a partial output. It does not change input or upstream classes. For a
fresh capture, repeat in UTC/en and Pacific/Honolulu/fr and compare exact bytes. The fixture uses
UTC definitions, so these results do not establish compatibility across differing TZDB releases.
