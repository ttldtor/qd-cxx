# Offline producer validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

recorded checks on 2026-10-30 UTC used Windows 11 x64, Zulu JDK 8.80.0.17
`1.8.0_422-b05` and Maven 3.9.16. These results apply to the exact
[producer lock](../../../../../tools/upstream/real-tape-corpus/producer.lock.json),
[Java source](../../../../../tools/upstream/real-tape-corpus/RealTapeCorpus.java) and
[launcher](../../../../../tools/upstream/real-tape-corpus/Invoke-Corpus.ps1) hashes in the
[baseline provenance](../../../../fixtures/protocol/real-tape/producer/baseline/provenance.json)
and [current provenance](../../../../fixtures/protocol/real-tape/producer/current/provenance.json).
Full child command arrays, UTC phase bounds and complete runner text logs remain private; the portable files contain
source/runtime/classpath/artifact hashes without local paths.

## Executed checks

Initial clean Capture generated candidates for both exact producers. Final-source clean Verify
then passed through the documented
[invocation](../../../../../tools/upstream/real-tape-corpus/README.md#prerequisites-and-invocation):
baseline 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and current 3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. A fresh official `git ls-remote` confirmed the latter
HEAD and master; the separate checkout identified release tag `qd-qds-3.356`. Current
`Tools Help Connect` returned exit 0 without connecting. The original locked checkout and
`upstream.lock` remained unchanged.

| Check | Observed result |
| --- | --- |
| Controlled build | `qds-file -am clean compile`, build cache disabled; JDK 8 enforcer passed; upstream tests skipped |
| Production writer/reader | Three positive cases per producer, three exact records each; complete descriptor/value/flag/time/physical-byte assertions passed |
| Byte repeatability | Every Verify generated a second independent seven-file set; exact artifact names and bytes matched |
| Committed expectations | Fresh Verify matched all seven artifacts per producer; 14 portable artifacts inventoried |
| Production negative | Derived truncation delivered two records and one parser error for each producer; observation ended `rejected`, without `complete` |
| Comparator negatives | Missing, extra, empty, changed wire bytes and changed observation bytes all rejected for both producers |
| Java CLI | No-argument/`--help` stdout equality, empty help stderr, invalid arguments exit 2 |
| PowerShell CLI | `Test-Help.ps1`: no-argument/`-Help` stdout equality, empty stderr, no working inputs or files, incomplete/unknown arguments rejected |
| Wrong producer pin | Baseline checkout requested as current was rejected before creating the run directory |
| Inherited child pipes | Maintained `Test-Lifetime.ps1` replayed root exit with a child retaining stdout/stderr; completed within its 3-second budget and both recorded Java PIDs were absent |
| Forced resource stop | Maintained lifetime test forced a 1-second active-root deadline resource stop; parent and child PIDs were absent afterward |
| Windows path handling | Native regression and both final Verify runs used working directories containing spaces and Cyrillic; relative classpath/output arguments bypassed the observed JDK 8 absolute-Unicode-classpath limit |
| Java formatting | Pinned google-java-format 1.36.1 Format then Check passed across 29 repository Java files using separate JDK 21+ |

Original QTP sizes are 121 bytes (`compact-none`), 258 (`quote-none`) and 299 (`quote-message`),
with a 297-byte derived negative, for both selected versions. Observations are canonical UTF-8/LF.
Each final Verify used a 128 MiB probe heap, 15-second process timeout, 512 MiB Maven heap and
600-second build timeout. No positive timeout occurred. These are configured limits, not peak
RSS measurements. Runner log streams have an 8 MiB checked limit with documented polling overshoot.
The native runner owns a Windows job before Java starts; deadlines include root completion,
stream drain and payload finalization. Forced runner termination closes the job, with at most
a five-second confirmation wait. UTC logs merge decoded Java stdout/stderr; they are not raw
separate Java streams. Help checks use separate direct-file child redirects.

Capture alone does not promote expectations. Portable artifacts were copied unchanged, then
regenerated again by fresh Verify using final Java/launcher source hashes. The final-source runs
retain their complete logs and metadata separately from exploratory build attempts.

## Limits and review

No errors were detected in these named positive checks. Negative parser errors and timeout are
intentional observations. The checks exercise production FileWriter/FileReader and full standard
Quote generation, not a diagnostic decoder. Source classes report `QDS-UNKNOWN`; commit identity
and controlled build hashes establish provenance.

No C++ production code or CTest registration changed in this unit; the Java/PowerShell workflow
is the relevant regression check. Windows working modes explicitly require the existing native
UTC runner and reject unsupported platforms before path/source/file work. Help remains portable.
Windows/Linux native integration results belong to their own reports. Linux/macOS execution of
this producer workflow, POSIX descendant cleanup, arbitrary tape decoding, C++ value/order
parity, business conversion parity, containers, sidecars and split sources are not established.
Optional live demo acquisition is not implemented or run. Full logs and private recordings are
not published. Arbitrary Unicode JDK/Maven/source installation paths are not qualified. The parent
shared corpus inventory and historical evidence remain untouched.

These are recorded checks. Independent review is a separate gate and is not implied by this report.

## Native fixture consumption

On 2026-10-30, Windows Release and Linux Release JSON-OFF native inspectors read
all four compact/Quote NONE fixtures from the two producer pins to completion,
with three mapped/processed records each. Their complete version-7 outputs were
byte-identical across platforms. This qualifies these portable native inputs;
the PowerShell producer launcher remains Windows-qualified, and MESSAGE/broader
data-family acceptance remains a separate pending contract.

The final repository-wide Java Format then Check passed for 30 files with the
pinned formatter and JDK 23.0.2; it changed no Java source bytes.
