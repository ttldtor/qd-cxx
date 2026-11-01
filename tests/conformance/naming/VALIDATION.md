# Naming migration validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Owner-requested follow-up to committed `e1e19e5`, executed 2026-09-22 UTC. This changes C++ namespace
and CMake naming only. [Current names and migration](../../../docs/NAMING.md) document the persistent
rules. Public `qd/...` include paths, `qd-qtp-inspect` filename, CTest case names and exact stdout/stderr
remain unchanged.  These are recorded checks, not independent review.

| Check | Evidence |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 10 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 10 suites | Debug, Release |
| WSL GCC 13.3.0 x86_64 Debug / Release, 10 suites | Debug, Release |
| GCC Debug ASan+UBSan, 10 suites | Test, build |
| BUILD_TESTING=OFF, tool ON / OFF, no _deps | ON configure, ON build, OFF configure, OFF build |
| Pinned Uncrustify 0.83.0 → clang-format 20.1.8, 30 files | Format, Check |
| 17 legacy mutations and baseline | Log, [proof](legacy-negative-proof.json) |
| 84 exact human cases, 72 default/explicit TSV cases, 9 human mutations and baseline | Log |
| Historical hashes, namespace-only C++ diff, names, EN/RU links and whitespace | Audit, result, log |

No errors were detected in these final checks. New build directories isolate this migration from
old QD_* caches. [Profiles](build-profile.json) retain actual compiler IDs, flags, project name,
new option/cache names and dependency absence. Corresponding configure/build commands and output
are retained in logs; each log starts with the exact command/cwd, using tools/run_utc.py.
Cached doctest sources were reused; its production-independent test-only archive remains pinned.

An initial Windows driver incorrectly split a drive-letter argument and configured doctest itself
(attempt, interrupted build).
That is not a successful project check. The driver was corrected, separate project directories used,
and the cache header restored to exact pinned archive bytes after the incidental doctest build changed
only its line endings (restoration). The final Windows matrix ran afterward.
The normal doctest CMake deprecation warning does not represent a test failure.

[Source hashes](source-sha256.json) identify 36 current build/test/audit inputs.
Historical hashes preserve 1396 pre-migration files, including fixtures,
upstream.lock, Java oracle tooling and earlier conformance evidence. Old reports/source hashes retain
old names because they describe old revisions; they have not been relabeled as current checks.
The audit compares all 30 C++ files to `e1e19e5` after reversing the namespace replacement and removing
formatting whitespace. It also checks all 13 project targets use qdcxx_, all eight project option/cache
names use ORG_TTLDTOR_QDCXX_, and the unchanged executable OUTPUT_NAME. This source check complements,
not replaces, the ten behavioral suites. EN/RU explanations were reviewed; automated checks cover
reciprocal/relative links and matching evidence targets, not translation semantics.

To rerun from the repository root after configuring a build with the new option:

```powershell
cmake --build .work/naming-final-msvc --config Release
python tools/run_utc.py --log .work/naming-review-test.log -- ctest --test-dir .work/naming-final-msvc -C Release --output-on-failure
python tests/conformance/check_human_inspection.py .work/naming-final-msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/human-inspection
python tests/conformance/check_offline_negative.py .work/naming-final-msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/offline-inspection --proof .work/naming-review-negative.json
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/naming/audit.py
```

Namespace and target renames require consumers to update and rebuild; no old ABI/namespace aliases
are promised. Header paths and CLI formats require no migration. No new Java captures, 32-bit or
other-platform runs, allocation-failure injection, timing, transport, tape support or broader wire
compatibility are claimed. The output-profile limitations of QDCXX-018 continue to apply.
