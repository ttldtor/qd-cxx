# QDCXX-014 validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

recorded checks executed 2026-09-21 UTC. No independent rerun is claimed. Production Java
[observations and clean Verify evidence](../../../fixtures/protocol/binding/VALIDATION.md) preceded
implementation. Three new doctest cases pass 1357 assertions; the four CTest targets include existing
compact/UTFString, framing and descriptions regressions. No errors were detected in these checks.

| Profile | Executed result and evidence |
| --- | --- |
| MSVC 19.42, Debug / Release | Four CTest targets pass: Debug, Release. |
| ClangCL 19.1.5, Debug / Release | Four targets pass: Debug, Release. |
| WSL GCC 13.3.0, Debug / Release | Four targets pass: Debug, Release. |
| GCC ASan+UBSan, Debug | Four targets pass: sanitizers. |
| GCC BUILD_TESTING=OFF | Clean configure and build; no _deps directory created. |
| Negative fixtures | [11 rejected mutations/missing inputs and baseline](negative-proof.json), log. |
| Pinned formatting | Uncrustify 0.83.0 then clang-format 20.1.8; google-java-format 1.36.1 on JDK 21. |
| Evidence, links and diff | Audit, UTC audit log, diff check. |

[Configuration snapshot](configurations.json) records actual cached compiler/generator/flags and
BUILD_TESTING settings. Build logs are adjacent to test logs. Existing build directories were reused;
the production-only directory was new. The first sandbox MSVC attempt was denied Windows SDK access;
retry with access succeeded. The first GCC test run exposed retained CR in Windows fixture lines;
the reader was fixed, formatted, then the complete compiler matrix rerun. The initial GCC failure is
retained in its log; the SDK denial is recorded in the task conversation,
and the successful MSVC retry has its own log.
No production semantics or expectations were changed to accommodate those failures.

The audit verifies every historical fixture and upstream.lock hash, four current Java provenance
chains, equal clean captures, 12 described-case projections against actual delivered values and 13-case
coverage including undescribed error. Relative Markdown links and EN/RU evidence references were
checked. Source review checked Doxygen, finite subtraction-based budgets, immutable local ownership,
independent ordered plans, no sender-ID-indexed allocation, no input references retained and no partial
successful result. qd_core has no protocol/transport/loading dependency; doctest is test-only.

Reproduce builds with [CPP-BUILD](../../../../docs/CPP-BUILD.md), including all four CTest targets.
Run `python tests/conformance/check_binding_negative.py <binding_tests> --report <new-report>` and
`python tests/conformance/protocol/binding/audit.py`. Use tools/run_utc.py for new build/test logs.
Fixture override is QD_BINDING_FIXTURES; never mutate historical evidence.

Unexecuted/excluded: Windows sanitizers, allocator-failure injection, huge allocations, 32-bit/macOS,
full JSON/XML import/validation, conversions, event decoding, built-in metadata/replacers/provider
profiles, dynamic re-description/reset/PART and transport. Intermediate repeated writes are source
inference; missing fields do not promise application defaults. API is provisional, no performance or
full Java parity claim.

## Shared type identity follow-up, 2026-09-21 UTC

Removed the two local COMPACT_INT=8 constants in favor of the documented
[SerialFieldType enum](../../../../include/qd/core/serial_field_type.hpp). Numeric inputs and independent
literal oracle expectations are unchanged. This is a C++ refactoring with no wire/profile change.
All four CTest targets were rerun on MSVC Debug and
GCC Debug, plus the pinned formatter
and evidence/link/diff checks. No errors were detected in these checks. ClangCL, Release, sanitizers,
production-only builds and Java captures were not rerun for this follow-up; their earlier results above
retain their original scope. EN/RU contract and results were updated;

## Shared wire-type bound follow-up, 2026-09-21 UTC

Consolidated raw parser MAX_SERIAL_TYPE_ID and planner MAX_RAW_TYPE into documented protocol constant
MAX_WIRE_SERIAL_TYPE_ID in describe_records.hpp. Structural acceptance and type support are unchanged.
Repeated checks included the MSVC Debug build,
all four CTest targets, pinned format check
and evidence/link/diff checks. No errors were detected in these checks. Other compiler/configuration,
sanitizer and Java checks were not rerun for this constant-only follow-up. EN/RU updated.
