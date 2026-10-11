# QDCXX-027 author verification

[Русская версия](VALIDATION.ru.md). English is authoritative.

Implementation scope: QDCXX-027 Python-free development tooling.
Starting HEAD was `dcccd0242252cadb674f4970cc723fbf113bed53`, with a clean tree.
This is author verification, including additional
self-review; independent review and acceptance remain pending.

This report retains the original implementation runs. The current source hashes, strict trace audit,
Windows-1252 checks and affected-suite reruns for review findings #1/#2 are in
review-fix verification. Earlier logs and hash snapshots remain unchanged.
Unicode forwarding is now checked through a native-argv test helper; the production Windows CLI
still depends on its active code page for path representability. The real inspector's awkward-path
case retains spaces and semicolons using ASCII.

## Baseline and parity

Baseline metadata, protected hashes,
33 original CTest names and the
case-by-case map were recorded before replacement.
The original MSVC Release build and
full CTest passed 33/33.
The unchanged Python tools ran with `PYTHONDONTWRITEBYTECODE=1` and explicit `-B` where directly invoked.
Offline negatives rejected all 17 labels and restored the full baseline.
The explicit human baseline covered 84 human cases, 72 legacy TSV cases,
all original output-row boundaries and 9 embedded mutations. Reported subprocess counters are not
used as a coverage oracle: the original scripts omit some invocations from those counters.

Old/new comparison and its named table
use the replacement's exact temporary mutations. All 17 offline rejection stages agree; all 9 human
mutant byte strings equal the original expressions, whose rejection was executed in the baseline.
The replacement mutation record also checks the exit-only adapter
failure. Each final full CTest reruns all mutation labels and the complete restored offline/human suites.
[Coverage](COVERAGE.md) maps grammar, byte comparisons, chunks, Java observations, CLI errors and I/O branches.

## Implementation decisions

- CMake reads the unchanged JSON manifests with finite adapter limits and preserves exact argument
  boundaries, including empty strings, semicolons and Unicode. Test-only C++ validates raw files and
  throws on failures in Release too. Expected JSON strings use a hex-to-binary bridge because CMake
  text output on Windows would introduce CRLF. Observed stdout/stderr are never normalized.
- The existing `offline_inspection_cli` and `human_inspection_cli` names remain. Three explicit additions
  are `inspection_checker_contract`, `inspection_cli_negatives` and `inspection_process_contract`.
  Existing argument and injected-I/O suites remain. Real preclosed stdout tests cover no arguments,
  help, TSV and human on both OS families; missing executables and deadlines are harness failures.
- `Run-Utc.ps1` requires PowerShell 7.4+, preserves per-stream line order, UTF-8 replacement decoding,
  universal newlines and final fragments, and sets TZ only for the child. It does not promise ordering
  between independent streams or raw-byte capture. A worker enters a Windows job or Unix session
  before starting the command; console/log failures terminate owned descendants even after the root
  exits. Deliberately detached Unix sessions are outside that ownership boundary.
- Windows setup uses the unchanged lock, validates cold and cached archive hashes and all entry paths
  before extraction, rejects links/reparse paths and publishes a verified staging directory with
  rollback. Re-extracting verified warm archives repairs damaged installed executables. No pip or
  additional package is required. These Windows EXEs do not establish Linux-native formatter acquisition.
- Python denial is process-tree image tracing: Windows creation debug stops and Linux ptrace exec
  stops inspect the resolved image before execution. PATH aliases, the actual absolute interpreter,
  and a descendant whose parent swallows failure each must fail with exit 97 and `DENIED` evidence.
  This is a workflow check, not a sandbox for deliberately renamed or embedded Python runtimes.

## Executed developer-tool checks

Windows uses PowerShell 7.6.6; WSL Ubuntu uses a task-local portable PowerShell 7.4.7.
The latter resides under ignored `.work/`; no global installation was changed.
Legacy logger characterization,
argument characterization and
legacy parity preceded the replacement tests.
Windows hardening and
WSL hardening cover running-root and orphan-root cleanup.
Final Python-denied logger runs are Windows and
Linux: arguments, child-only TZ, invalid UTF-8/BOM/newlines,
5000 lines plus final fragments on each stream, exit 37, missing executable, unwritable log and
closed console; Linux also exercises `/dev/full`. No errors were detected in these checks.

Setup parity confirms identical installed EXE paths and SHA-256
against unchanged `setup_cpp.py`. The final guarded setup
passes all 28 named checks, including real cold download, warm repair, hostile synthetic archives,
interrupted/wrong-hash downloads, rollback/preservation, wrong versions, a Check defect and
Format → Check → Format idempotence. The pinned pipeline remains Uncrustify 0.83.0 then clang-format 20.1.8.
Guarded full Check passed all 70 C++ files.
After adding only entry-point Doxygen, final formatting repeated Format
for all three new C++ files and Check for all 70 files under the same Python image guard.

## Final Python-unavailable build/test matrix

All seven full profiles ran 36/36 CTest successfully, preserving all 33 original names and adding
three checker/process tests. No errors were detected in these checks.

| Compiler/profile | Full CTest evidence |
| --- | --- |
| MSVC 19.42 Debug | 36/36 |
| MSVC 19.42 Release | 36/36 |
| ClangCL 19.1.5 Debug | 36/36 |
| ClangCL 19.1.5 Release | 36/36 |
| GCC 13.3 Debug | 36/36 |
| GCC 13.3 Release | 36/36 |
| GCC 13.3 ASan+UBSan | 36/36 |

All eight option combinations were freshly configured and built on each platform. Every applicable test ran successfully. Named matrix audit, Windows completion and Linux completion retain the results.

| BUILD_TESTING | QTP | TAPE | MSVC Release | GCC Release |
| --- | --- | --- | --- | --- |
| OFF | OFF | OFF | built | built |
| OFF | OFF | ON | built | built |
| OFF | ON | OFF | built | built |
| OFF | ON | ON | built | built |
| ON | OFF | OFF | 17/17 | 17/17 |
| ON | OFF | ON | 28/28 | 28/28 |
| ON | ON | OFF | 25/25 | 25/25 |
| ON | ON | ON | 36/36 | 36/36 |

Resolved tool/environment records: Windows,
Linux/WSL.

Each case configures a fresh cache with `CMAKE_DISABLE_FIND_PACKAGE_Python`, `Python2` and `Python3`
set to TRUE. The verifier checks compiler identity, original applicable test names, zero disabled tests,
absence of interpreter cache entries and absence of doctest discovery/fetch in production builds.
Pinned doctest sources are supplied from the existing cache only to testing builds. Every applicable
CTest runs without `-R`, `-E`, label filters or reduced fixture selection. Test logs list individual
case names/chunk schedules and human output budgets; `.tests.json` retains exact CTest commands;
`.exec.tsv` retains every traced process image. Only deliberate denial controls contain `DENIED`.

Reproduction commands used (fresh run tags are required on repetition):

```powershell
pwsh -NoProfile -File tests/conformance/python-migration/Verify-NoPython.ps1 -Guard .work/python-migration-baseline-a/Release/qdcxx_no_python_guard.exe -DoctestSource .work/msvc/_deps/doctest-src -PythonControl 'C:/Program Files/Python311/python.exe' -RunTag final-b
```

Inside WSL, from `/work/qd-cxx`:

```sh
.work/python-migration-pwsh/pwsh -NoProfile -File tests/conformance/python-migration/Verify-NoPython.ps1 -Guard .work/python-migration-linux-guard -DoctestSource .work/linux-debug/_deps/doctest-src -PythonControl /usr/bin/python3 -RunTag final-b
```

The Windows guard was bootstrapped with the existing MSVC Release CMake tree; the Linux guard with
`g++ -std=c++23 tests/conformance/no_python_guard.cpp -o .work/python-migration-linux-guard`.
Guard binary hashes and resolved tools are retained in each final `environment.json`.
GCC ASan+UBSan uses non-PIE binaries and `detect_leaks=0`: LeakSanitizer cannot inspect ptraced
processes. AddressSanitizer and UndefinedBehaviorSanitizer remain enabled; this is not a leak-check claim.

## Recorded failures and boundaries

The first baseline configure attempts failed on a split unquoted doctest path / SDK sandbox access,
then the cached bad path: first,
retry. A fresh correctly quoted
configure succeeded with SDK access.
The first manual Linux logger guard invocation omitted the guard's
command argument; it is failed harness evidence, not a test pass.
The first Linux matrix configure failed because
multiple `Get-Command` results were converted into one executable string. Selecting the first native
application fixed this; the final fresh run is `linux-final-b`. Failed evidence is retained unchanged.
An early restricted cold download and a concurrent helper rebuild also failed during development;
actual cold setup and rebuilt helpers were rerun successfully. Those trials are not acceptance results.

After all four full Windows profiles passed, the first production-only configure in `windows-final-a`
finished CMake but the guard waited for an owned compiler `vctip.exe` descendant. That verification
process was stopped; its job removed its own remaining processes. The
unfinished configure log is retained.
The Windows guard now terminates remaining traced descendants after the root exits and continues
draining debug exit events. A 30-second orphan regression
completed in about two seconds with no surviving child. A fresh full Windows run is `windows-final-b`;
it uses CTest parallelism 4 with the same complete suites. Linux's recorded run used serial CTest;
the guard's Linux execution policy is unchanged. The guard edit received pinned Format and Check.
A fresh Linux guard rebuild from the final source reproduced
the exact SHA-256 of the binary used by the completed Linux matrix.

No production/API/wire/schema change, new Python source/dependency, Java source edit, new Java capture
or private-tape analysis was performed. Existing Java-derived fixtures were consumed by CTest;
Java Verify workflows were not rerun. Remaining primitive/protocol Python negatives, JSON validation,
old evidence audits and generators remain required in the [migration ledger](../../../docs/PYTHON-MIGRATION.md).
The manual Python command retained in CPP-BUILD is explicitly unmigrated.

## Preservation, self-review and handoff

Handoff audit confirms all 4004 protected original files and the
owner's index are byte-identical. HEAD remains `dcccd0242252cadb674f4970cc723fbf113bed53`;
index SHA-256 remains `33322613B5F77CBC535C6E869BA707574F389B843BAD602D894ADDE800A69460`.
Original source hashes identify the pre-review CMake/C++/PowerShell files.
Matrix audit checks all 23 configure/build combinations, applicable test
counts, all 156 named scenario schedules/exits and 72 named legacy TSV cases in every QTP-enabled run,
absence of sanitizer diagnostics and absence of Python images except the deliberate denial controls.
The audit checks changed and untracked authored whitespace, reciprocal/relative document links,
raw-file Git filters and raw-file whitespace attributes. The pre-review audit required whitespace
exceptions for 88 concrete raw paths according to Git's actual diagnostics; authored whitespace rejection remains enabled.
Executed preservation audit:

The historical preservation audit wrote Git objects only to an ignored isolated object directory and
roundtrips every new raw evidence file through Git clean/smudge with `core.autocrlf=true`.
It does not write the owner's index or object database. Raw paths use scoped `-text` attributes;
only concrete raw files with actual whitespace receive whitespace exceptions. An injected authored
Markdown trailing-space defect must still be rejected. Historical raw evidence is never normalized.

The resulting diff, including untracked code and documents, was reviewed for scope and correctness.
Reuse/quality/efficiency review suggested seven optional simplifications; none was
needed for correctness. Shared parsing would add allocation or coupling; tiny duplicate conversions
and unused exports did not justify extra abstraction; regex/poll tweaks had no measured bottleneck;
a warm-install manifest would introduce a new trust/cache protocol. These suggestions were deferred.
A separate harness audit led to explicit compiler pinning, a swallowed-descendant denial control and
orphan-root logger cleanup, all included in the executed final checks. This is author self-review.
