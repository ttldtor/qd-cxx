# Utility executable rename validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Owner-requested follow-up to `435c6d2`, executed 2026-09-22 UTC. The only user-facing CMake utility
is now `qdcxx-qtp-inspect` (`.exe` on Windows); its target remains `qdcxx_qtp_inspect`. Help and current
EN/RU commands use the new name. Internal test executable names and public include paths are unchanged.
The rule for future utility filenames is documented in AGENTS.md and [NAMING](../../../docs/NAMING.md).
Version 1 TSV/human format identifiers remain `qd-qtp-inspect`, with unchanged fixtures and wire behavior.

Author checks, not independent review:

- MSVC 19.42.34444 x64 Release: build, 10 suites.
- WSL GCC 13.3.0 x86_64 Release: build, 10 suites.
- Uncrustify 0.83.0 → clang-format 20.1.8: Format, Check.
- New binary/help, CTest executable selection, historical-byte/diff and EN/RU link checks:
  audit, log, result/source hashes.

No errors were detected in these checks. The unchanged suites include 72 legacy CLI cases, 84 human
cases, default/explicit TSV comparisons, output limits, I/O failures and nine human mutations/baseline.
Commands and UTC timestamps are retained in the logs via tools/run_utc.py. Builds reused configured
Release directories and regenerated CMake; this is not a new clean-build or full compiler/sanitizer
matrix claim. Debug, ClangCL, sanitizers, production-only profiles and Java captures were not rerun
for this filename/help-only change. Earlier reports and source-hash snapshots remain historical.

No old-name alias is built; a stale old executable may remain in a reused build directory. Update
commands/shortcuts to the new name. No wire/TSV/human grammar, protocol scope or dependency changed.
No commit was created.
