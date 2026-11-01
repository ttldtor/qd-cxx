# Final offline inspection validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

recorded checks executed 2026-09-21 UTC; not independent review. No errors were detected in the final
checks listed below. [Coverage](COVERAGE.md) specifies what was exercised and what remains excluded.

| Profile | Evidence |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 8 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 8 suites | Debug, Release |
| WSL Ubuntu GCC 13.3.0 x86_64 Debug / Release, 8 suites | Debug, Release |
| GCC Debug ASan+UBSan, 8 suites | Tests, build |
| BUILD_TESTING=OFF, tool ON / OFF | ON configure, ON build, OFF configure, OFF build |
| Uncrustify 0.83.0 → clang-format 20.1.8, 27 files | Format, Check |
| google-java-format 1.36.1, JDK 21, 9 files | Check |
| 17 rejected mutations and unchanged baseline | Log, [proof](negative-proof.json) |

Every test profile includes all six prior suites plus process-level CLI and injected I/O tests.
Final builds/tests were repeated after the final incomplete-envelope diagnostic, test and C++
comment/format changes. All build/test logs
use tools/run_utc.py and UTC; paired build/configuration logs are in logs.
[Build profiles](build-profile.json) record compiler identities, flags and both production directories
without `_deps`. BUILD_TESTING=OFF bypasses FetchContent and Python discovery. The unused
FETCHCONTENT_FULLY_DISCONNECTED configure warning in fresh production builds reflects that bypass;
the pinned doctest CMake deprecation warning is not a test failure.

Earlier sandbox attempts could not discover Windows SDK or execute the CTest-selected Python:
SDK log, Python log.
Authorized retries succeeded; those failed attempts are not counted as passing checks.

[Java validation](../../../fixtures/protocol/offline-inspection/VALIDATION.md) records two independent
clean composer/parser captures and negative/baseline Verify completed before CLI implementation.
No historical fixture or upstream.lock byte changed. [Source hashes](source-sha256.json) identify
final production/test/build inputs; audit script, audit result and
UTC audit log cover source/capture hashes, historical bytes, EN/RU evidence
links and whitespace. New fixture hashes are in [fixture-sha256.json](fixture-sha256.json).

EN/RU contracts, usage, coverage, results and state are synchronized. The API-GATE is accepted:
processedRecords/wireFields count budget charges including failed attempts, with unchanged wire
semantics/check order/positions. Independent review remains pending;

The tool supports only raw offline initial descriptions and ordinary type-8 zero-flags data with
one immutable context. No allocation-failure injection, enormous resources, 32-bit environment,
importer, transport, dynamic session, timing or full compatibility claim. Physical output failure
may leave partial bytes; a completion marker is insufficient without exit 0.
