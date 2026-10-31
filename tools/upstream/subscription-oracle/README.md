# Offline subscription production oracle

[Русская версия](README.ru.md). English is authoritative.

Runs locked Java production composers, parsers, collectors and connection adapters with bounded
synthetic inputs. It opens no sockets and does not implement a native subscription client.
See the [capture profile](../../../tests/fixtures/subscriptions/CAPTURES.md) for evidence and limits.

Requires CMake 3.25+, PowerShell 7.4+, explicit JDK 8 and a clean checkout matching
[upstream.lock](../../../upstream.lock). Prepare its classes and trusted receipt with
[schema preparation](../../schema-export/README.md). Preparation can fetch Maven dependencies;
this launcher itself neither fetches nor builds upstream. A receipt checks class bytes against
the supplied trusted preparation, not their origin independently. Nonempty JAVA_TOOL_OPTIONS,
_JAVA_OPTIONS and JDK_JAVA_OPTIONS are rejected. No global configuration is changed.

## Invocation

From the repository root in PowerShell; replace the illustrative installed-tool/checkout paths:

```powershell
cmake -DHELP=ON -P tools/upstream/subscription-oracle/Capture.cmake
$repo = (Get-Location).Path.Replace('\', '/')
$jdk8 = 'C:/tools/jdk8'
$upstream = 'C:/work/QD-locked'
$receipt = "$repo/.work/schema-export/prepare/classpath-receipt.json"
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DRECEIPT=$receipt" `
  "-DRUN=$repo/.work/subscription-check" -DMODE=Verify `
  -P tools/upstream/subscription-oracle/Capture.cmake
```

No arguments and standalone `-DHELP=ON` print identical help without requiring providers or
starting work. Use this CMake setting for script help; CMake owns the global `--help` option.
JAVA_HOME, UPSTREAM, RECEIPT and RUN are required for work. RUN must not exist. MODE defaults
to Capture (validate only); Verify additionally compares against EXPECTED, defaulting to
`tests/fixtures/subscriptions/expected`. No mode publishes or modifies expected files.
Exit 0 means help/success; exit 1 means argument, preparation, compilation, observation or comparison failure.

The launcher verifies provider/source identity before and after, compiles the four local Java
files into RUN/classes, then runs each capture in a fresh JVM with a 256 MiB heap and 60-second
process timeout. Additional help/error-contract JVMs have 15-second timeouts. Locale is en/US,
timezone UTC and encoding UTF-8 for capture/help. These are child process settings.
RUN retains command output, provider receipts and UTC phase bounds. Clock-dependent session
frames are moved unchanged to RUN/session-raw; only RUN/artifacts is deterministic/publicable.
Failed runs preserve partial evidence; use a fresh RUN for retry.

## Direct tools and results

The compiled mains require their prepared classpath. WireOracle uses `--capture NEW_DIRECTORY`,
CollectorOracle `--output NEW_DIRECTORY`, SessionOracle `--out NEW_DIRECTORY`.
All accept no arguments or standalone `--help` without provider work.
Wire exits 3 for usage, 2 for observation/I/O (including help output); Collector exits 3 for
usage/existing output and 1 for observation/I/O; Session exits 3 for usage and 1 for
observation/I/O/existing output. Existing outputs are never overwritten.
The session helper sets a server AuthManager timeout only in its own JVM.

```powershell
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 `
  -Actual .work/subscription-check/artifacts -Expected tests/fixtures/subscriptions/expected
Get-Help ./tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Full
```

Comparison is read-only, checks canonical typed TSV and exact artifact bytes, and accepts
empty binary deltas. Without Expected it validates structure only. Bounds and exit codes are
in its built-in help. Binary files are not normalized. The Java tests/HelpFailureProbe helper
injects a failed stdout stream into a help-only invocation; it is not a production QD model.

Ordinary native builds and CTest do not invoke this optional developer oracle or require Java.

## Comparator regression checks

`pwsh -NoProfile -File tools/upstream/subscription-oracle/tests/Test-Comparison.ps1 -Help`
prints standalone help. Supply `-Artifacts RUN/artifacts -RunDirectory NEW_TEST_DIRECTORY`
to run positive/negative checks; the parent of NEW_TEST_DIRECTORY must exist. It preserves
private mutated copies, results and logs, never changes Artifacts, and limits each comparator
process to 30 seconds. Exit 0 is help/success, 2 test/I/O failure, 3 invalid combinations;
PowerShell parameter-binding errors can exit 1. Reparse creation unavailable on the host is
recorded as an explicit test skip. No arguments also print help without creating outputs.
