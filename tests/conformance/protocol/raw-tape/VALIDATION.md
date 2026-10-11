# RAW/MESSAGE implementation author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Starting HEAD: `c1654f2dbb5163917b848d1b84b17dfb9d0999fc`.
Checks below were executed on 2026-09-22 UTC. No errors were detected in the successful checks
listed here. They are author checks, distinct from independent review.
Implementation is ready for independent review; independent acceptance is not claimed.

## Evidence before implementation

The locked Java [validation](../../../fixtures/protocol/raw-tape/VALIDATION.md) records two clean
captures, 94 scenarios / 284 exact artifacts, ten rejected mutations, a full negative Verify and a
subsequent unchanged baseline Verify. Six cases use the actual FileWriter or
DXEndpoint/TapeConnector/FileWriterHandler producer path; the other inputs are explicitly labelled
diagnostic-wire. The [contract](../../../../spec/protocol/RAW-TAPE.md) was established before C++.
A subsequent check rechecked all 284 artifacts from
clean capture B against repository expectations. This is not another clean upstream build.

## Compiler and production matrix

Both optional inspectors were enabled for every testing configuration. Each full CTest run included
all 20 existing/new suites, including the three RAW suites, with no failures:

| Configuration | Build / CTest evidence | Result |
| --- | --- | --- |
| MSVC 19.42.34444, x64 Debug | build, test | PASS, 20/20 |
| MSVC 19.42.34444, x64 Release | build, test | PASS, 20/20 |
| ClangCL 19.1.5, x64 Debug | build, test | PASS, 20/20 |
| ClangCL 19.1.5, x64 Release | build, test | PASS, 20/20 |
| GCC 13.3.0, Ubuntu 24.04 WSL, Debug | build, test | PASS, 20/20 |
| GCC 13.3.0, Ubuntu 24.04 WSL, Release | build, test | PASS, 20/20 |
| GCC 13.3.0, Debug ASan+UBSan | build, test | PASS, 20/20 |

The sanitizer configuration uses `-fsanitize=address,undefined -fno-omit-frame-pointer`.
Windows uses Visual Studio 17 2022, x64, with `-T ClangCL` for Clang; Linux uses Ninja.
Testing reuses cached pinned doctest source through `FETCHCONTENT_SOURCE_DIR_DOCTEST`; legacy Python
checks run unchanged with `PYTHONDONTWRITEBYTECODE=1`. No new Python or runtime dependency was added.
The doctest CMake compatibility deprecation warning is retained in the logs.

`BUILD_TESTING=OFF` MSVC Release configuration/build passed for QTP/TAPE tool selections
OFF/OFF, OFF/ON,
ON/OFF, ON/ON.
Every directory was checked for absence of `_deps` and Python3 cache discovery.
Exact command arrays and UTC start/end/exit records are in the adjacent configure/build/test logs.
The executed Windows launcher and
Linux launcher preserve environment and local cache paths for audit;
reproduction elsewhere must supply its own equivalent paths.

## CLI, negative checks and review fix

359 before/after CLI runs compared exact stdout bytes, stderr bytes
and exit codes against pre-change executables built from the starting checkout. Tape inputs cover
the old quote/tape fixtures across both local profiles, TSV/human and diagnostic versions; QTP
help/no-argument/invalid-argument behavior and tape invalid-profile behavior are also included.
Tape help is intentionally excluded from before/after byte equivalence because it documents the new
option. Current no-argument/standalone-help equivalence is tested by CTest. The executed comparison
script also checks omitted versus explicit legacy on tape runs.
Existing QTP CTest suites cover its successful legacy inputs.

The initial 13 rejected CLI mutations and baseline passed.
Independent review then demonstrated that the test comparator accepted moving one record between
two equal-timestamp frames. The production reader was unchanged. The comparator now enforces the
generator-defined singleton-record cardinality of the 29 selected CLI fixtures independently of
actual output, and both CMake and PowerShell mutate that frame association. The final
14-mutation run rejected all mutations and accepted both unchanged
Quote and timing baselines. CMake now includes eight rejecting output mutations.

Only the affected comparator was rebuilt after that test-only fix. The affected CLI suite passed
again in all seven configurations: MSVC Debug,
MSVC Release, ClangCL Debug,
ClangCL Release, GCC Debug,
GCC Release, ASan+UBSan.
The earlier 20-suite runs are not relabelled as post-fix runs.

An earlier development CLI check exposed Windows newline translation in CMake `file(WRITE)`.
Actual process output is now captured with `OUTPUT_FILE`; mutations explicitly use UNIX newlines.
CLI LF grammar remains strict; only captured Java artifact CRLF is interpreted as producer line endings.

## Inventory, preservation and handoff

The generated inventory self-test passed, including every
frame prefix, unknown envelopes/types, late descriptions, malformed/truncated framing/descriptions and exhausted
budgets. [REAL-TAPE-GAPS](../../../../docs/REAL-TAPE-GAPS.md) records the separate private observation:
1,280 envelopes, 48 descriptions and 349 fields were structurally enumerated. Additional full types
10/40/72/88 and Profile remain unsupported. Bounded locked-Java replay stopped at its 1,000,000-record
delivery budget; its counter was 1,000,001 including the first rejected excess attempt. It neither
completed nor established a compatibility failure. Private tape bytes,
symbols, event values, hashes and raw replay captures remain local under ignored `.work` only.

Pinned Java Format then Check covered 15 sources; final Java Check passed.
Pinned Uncrustify 0.83.0 then clang-format 20.1.8 Format and
Check covered 51 C++ files; the review fix was formatted and the
final full Check passed. The preservation check
matched all 1,594 pre-task fixture/lock files to the original hashes.
Final documentation/link/whitespace and changed-file audits are retained in handoff audit.

Reuse/quality simplification reviews found no worthwhile change. An efficiency suggestion to move
invalid-mode validation ahead of bounded schema construction was considered but not adopted: this
invalid-configuration path does not justify another helper and changed exception precedence.
No scope deviation or new semantic guess was
introduced. Broader types, flags, compression, sessions, adapters, replay scheduling and transport
remain excluded. Full private-tape support is not claimed.
