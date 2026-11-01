# Final STREAM_DATA validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

recorded checks on 2026-09-21 UTC after final C++ changes; this is not independent review. No errors
were detected in these checks. All six suites pass in each test profile, including the five existing
suites. Four new cases execute 85,549 assertions. [Coverage](COVERAGE.md) defines limitations.

| Executed profile | Passed evidence |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release | Debug, Release |
| WSL GCC 13.3.0 x86_64 Debug / Release | Debug, Release |
| GCC Debug ASan+UBSan | Tests, build |
| BUILD_TESTING=OFF | Configure, build |
| Uncrustify 0.83.0 → clang-format 20.1.8 | Check: 23 files |
| google-java-format 1.36.1 / JDK21 | Check: 8 files |
| Negative/baseline | Log, [proof](negative-proof.json) |

All build/test commands run through tools/run_utc.py. Matching build logs are retained in logs.
[Build configuration](build-profile.json) records compilers, sanitizer flags and production no-_deps
check. Production configure explicitly set BUILD_TESTING=OFF and FETCHCONTENT_FULLY_DISCONNECTED=ON;
FetchContent is not entered and no dependency is fetched. Initial sandboxed MSVC SDK discovery was
access-denied; the authorized build succeeded. That environment failure is not counted as a passed check.

[Java evidence](../../../fixtures/protocol/stream-data/VALIDATION.md) records two independent clean
captures and negative/baseline Verify. Existing goldens and upstream.lock are unchanged. Fifteen
negative fixture variants were rejected and the unchanged baseline passed. Pinned C++ Format then
Check and Java Format before captures / final Check passed. [Source hashes](source-sha256.json)
identify final production/test/CMake inputs. Audit checks provenance, capture equality,
historical bytes, final source hashes, paired Markdown links and diff whitespace; see UTC log.
EN/RU technical claims and evidence references are synchronized. Ready for
independent review, not an independent review result.

Only ordinary exact type 8 and zero raw flags are supported. Completed-prefix retention, finite
resources and error positions are designed C++ policy, not Java callback parity. No allocation-failure
injection, enormous resources, 32-bit platform, full event/time/conversion/session/transport or
performance validation was executed. Public APIs remain provisional.
