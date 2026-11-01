# Tape reader validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Native execution, 2026-09-22 UTC. Independent review is pending. [Coverage](COVERAGE.md), [Java evidence](../../../fixtures/protocol/tape-reader/VALIDATION.md),
[contract](../../../../spec/protocol/TAPE-READER.md), [architecture](../../../../docs/decisions/ADR-0002-bounded-tape-reader.md).

## Executed matrix

All **13 CTest suites** passed in each profile: all ten old suites plus protocol_binary_tape,
tape_inspection_io and tape_inspection_cli. Seven reader cases / 18,061 assertions passed in the
additional direct MSVC Debug run; two IO cases and CMake process assertions are separate.

| Profile | Build / test evidence |
| --- | --- |
| MSVC 19.42.34444 Debug | build, tests |
| MSVC Release | build, tests |
| ClangCL 19.1.5 Debug | build, tests |
| ClangCL Release | build, tests |
| WSL GCC 13.3.0 Debug | build, tests |
| WSL GCC Release | build, tests |
| GCC Debug ASan+UBSan | build, tests |

Configuration logs are adjacent; each UTC log starts with the exact command, cwd and TZ and ends with
exit status. Windows uses Visual Studio 17 2022, x64, SDK 10.0.26100.0; GCC uses Ubuntu-24.04 WSL x86_64.
Sanitizer flags are `-fsanitize=address,undefined -fno-omit-frame-pointer`. No sanitizer findings were
reported in these executed tests. No other platform, architecture or performance claim is made.

BUILD_TESTING=OFF passed all four QTP/TAPE inspector combinations: OFF/OFF,
OFF/ON, ON/OFF,
ON/ON. Configure logs and caches were checked: no _deps directory and no
Python3 discovery entries. No new third-party runtime dependency or new Python script/dependency.
The unchanged run_utc.py and old Python process suites remain in use under the staged migration policy.
New tests use C++/doctest, CMake process checks and Java/PowerShell evidence validation.

## Results and reproducibility

The original constrained.qtp delivers exactly seven records with Java's local identities, symbols and
values. Original default.qtp and text.txt reject explicitly with exit 4; original truncated.qtp fails
with exit 5 and completed prefix. No tape bytes are stripped/reordered. Old inspector TSV/human/profile/
exit behavior passes its old suites across the whole matrix. The unchanged Java tape baseline Verify
also reproduces all 22 historical artifacts in each new clean oracle run.

Use [build/API commands](../../../../docs/CPP-BUILD.md) and [CLI](../../../../docs/TAPE-INSPECT.md).
Representative commands (see logs for exact paths and full compiler options):

```powershell
cmake -S . -B .work/tape-msvc -G "Visual Studio 17 2022" -A x64 -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON
cmake --build .work/tape-msvc --config Debug
ctest --test-dir .work/tape-msvc -C Debug --output-on-failure
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

Pinned Uncrustify 0.83.0 then clang-format 20.1.8 Format/Check pass for 37 files. Initial compressed
handwriting required a second complete pipeline pass for convergence; final Check passed. Java Format
preceded captures and final Check passed for 12 files with google-java-format 1.36.1/JDK21; oracle used
JDK8. Initial Windows builds needed SDK access outside the sandbox; two old Python suites initially
selected an inaccessible Python 3.14 executable. Final configuration explicitly uses installed Python
3.11 and all tests passed. These environment retries are not successful test runs.

Historical audit compares 1,167 non-Markdown historical fixture/lock files with a
Git archive of HEAD: 1,083 raw-identical, 84 existing checkout LF/CRLF representation differences only,
zero other differences. None was normalized or rewritten. Historical tracked fixtures and upstream.lock
have no Git diff; original tape bytes also pass the raw oracle comparator. The test-generated tracked
Python cache was restored from its exact HEAD blob; no generated cache is part of this change.
Final audit records source/tool hashes, whitespace/link checks and the staged/unstaged audit.

No errors were detected in these checks. Public API remains provisional. No allocation-failure injection,
huge-resource, throughput, authenticated-footer, default TapeConnector/RAW_DATA, Quote, compression,
time-sidecar, importer, network or general-session compatibility is claimed. Next real-event milestone
remains unchanged default Quote with exact types 56/2/120. Independent review remains pending for this historical record.
