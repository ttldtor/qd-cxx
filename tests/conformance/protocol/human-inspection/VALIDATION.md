# QDCXX-018 validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 UTC (local calendar date 2026-09-22). No errors were detected in the final
checks below. These are recorded checks; independent QDCXX-018 review is pending.
[Coverage](COVERAGE.md) defines the exercised profile and exclusions.

| Profile/check | UTC evidence |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 10 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 10 suites | Debug, Release |
| WSL Ubuntu GCC 13.3.0 x86_64 Debug / Release, 10 suites | Debug, Release |
| GCC Debug ASan+UBSan, 10 suites | Test, build |
| BUILD_TESTING=OFF, tool ON / OFF, no _deps | ON configure, ON build, OFF configure, OFF build |
| Uncrustify 0.83.0 → clang-format 20.1.8, 30 files | Format, Check, read-only mismatch/baseline/idempotence |
| Original 17 rejected mutations and baseline | Log, [new proof](legacy-negative-proof.json) |
| 84 exact human cases, 9 rejected mutations and baseline, byte limits | Log |
| Designed artifact reproduction, current source/profile capture | Log, [builder](design_fixtures.py), [metadata preparation](prepare_evidence.py) |
| Historical bytes, EN/RU evidence links, diff audit | Log, result, audit script |

Every test profile includes the eight unchanged prior suites plus two human suites. Final builds/tests
ran after the last C++ header/format edits. Corresponding configure/build logs are in logs.
[Build profiles](build-profile.json) record actual compiler IDs, flags, sanitizers and dependency absence.
Production-only builds bypass test FetchContent and Python discovery. The final production builds
were incremental after successful initial builds; no-work output is not claimed as a clean rebuild.
The pinned doctest CMake deprecation warning is not a failed test.

The first sandboxed Windows build failed during SDK discovery (failed attempt);
the authorized retry succeeded. The failed attempt is not a passed check.
No Java sources changed or fresh Java captures were run. Reused [Java production composer/parser evidence](../../../fixtures/protocol/offline-inspection/VALIDATION.md)
and historical recorded results/hashes remain untouched. QDCXX-017 was closed against its committed
revision and the prior review scope specified by QDCXX-018: MSVC Release/GCC ASan+UBSan, eight suites,
17 mutations/baseline and hash/diff checks. Other QDCXX-017 profiles/captures remain initial results.

## Measured encoded stdout budgets

Counts include header, frames, escaped records and summary. Exact total succeeds; total minus one
fails before the summary. Tests additionally exercise exact row-prefix budgets and one byte below
each next complete row, requiring the exact preceding prefix and resource exit 6 at `write`.
All 25 successful cases without extra arguments receive these checks; full measurements are in the
log. Selected values:

| Designed input | Required bytes |
| --- | --- |
| empty (empty symbol, one S record) | 308 |
| ascii | 311 |
| cyrillic | 321 |
| supplementary | 313 |
| lone-high / lone-low | 314 / 314 |
| lookalikes | 329 |
| injection | 358 |
| c0 / del-c1 | 489 / 507 |
| line-bidi | 429 |
| legacy-duplicate-target-skip | 512 |

## Reproduction and review

From the repository root; use a configured profile with the tool and tests enabled:

```powershell
python tools/run_utc.py --log .work/review-018-build.log -- cmake --build .work/msvc --config Release
python tools/run_utc.py --log .work/review-018-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
python tests/conformance/check_human_inspection.py .work/msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/human-inspection
python tests/conformance/check_offline_negative.py .work/msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/offline-inspection --proof .work/review-018-negative.json
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/protocol/human-inspection/audit.py
```

The first line of every UTC log records the exact executed command and cwd; GCC commands run under
WSL at `/work/qd-cxx`. [Source hashes](source-sha256.json) describe current build/test/tool inputs;
[fixture hashes](fixture-sha256.json) cover all 16 new designed artifacts. Old source hash snapshots
remain historical, not current-source claims; the new audit intentionally does not compare current
formatter sources against QDCXX-017 hashes. It does verify prior fixture/capture hashes, untouched
libraries/oracle tooling and old conformance files against the QDCXX-017 commit.

To reproduce designed fixtures, run the builder with `--output` pointing to a new directory; it rejects
existing destinations. The metadata-preparation log records a byte-for-byte reproduction of all 16
artifacts in a fresh temporary directory. Verification reads expectations, never regenerates them.
The nine negative human checks mutate in-memory copies and use the same exact comparison as baseline.
EN/RU explanations were reviewed for matching claims; the audit checks reciprocal/relative links and
identical non-document evidence targets, not translation semantics.

No allocation-failure injection, huge-resource/RSS bound, 32-bit run or new Java measurement is claimed.
Physical write/flush failures can leave partial bytes or a summary despite nonzero exit. This is only
bounded raw-QTP initial-description/type-8/zero-flags inspection: no tape/compression/time-file reader,
protocol descriptors, importer, transport, dynamic sessions, broader fields or general dxFeed support.
