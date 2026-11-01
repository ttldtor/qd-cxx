# Additional field types: validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

The2026-10-30 read-only BINARY_FIELDS_V1 payload extension has a separate
[object validation](OBJECT-VALIDATION.md), including both-pin production evidence,
focused C++ ownership/resource checks and explicit remaining acceptance gates.
The historical QDCXX-024 compiler matrix and receipts below are not relabelled as
verification of the new field identities.

QDCXX-024 started at `a9299317c1d4e7f3202ed8136916fd3ee94b9f3f`.
All dates below and retained logs are UTC, 2026-09-22.
The initial locked-Java observations and [FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.md)
preceded production C++. [Coverage](COVERAGE.md) maps R1–R8 to checks and states their limits.
This is validation and self-review, not independent acceptance. Independent review is pending.

## Executed compiler and production matrix

Both optional inspectors are ON for all seven test configurations; all 25 CTest suites run,
including 20 unchanged suites and five new field-type suites. MSVC 19.42.34444 and ClangCL 19.1.5
use Visual Studio 17 2022, x64, Windows SDK 10.0.26100.0. GCC 13.3.0 runs in Ubuntu-24.04 WSL.
CMake is 4.4.2; pinned doctest 2.4.12 comes from existing verified caches.

| Configuration | Build / CTest | Test log |
| --- | --- | --- |
| MSVC Debug | PASS / 25 of 25 | log |
| MSVC Release | PASS / 25 of 25 | log |
| ClangCL Debug | PASS / 25 of 25 | log |
| ClangCL Release | PASS / 25 of 25 | log |
| GCC Debug | PASS / 25 of 25 | log |
| GCC Release | PASS / 25 of 25 | log |
| GCC Debug ASan+UBSan | PASS / 25 of 25 | log |

Sanitizers use `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie` and linker `-no-pie`;
PIE sanitizer executables and other operating systems/architectures were not tested. The final change
only clarified tape help's version-4 exception; all six CLI suites were rerun in the six ordinary
compiler configurations. The sanitizer build includes that final help. Adjacent `*-build.log`,
`*-configure.log` and `*-final-cli-test.log` retain exact commands and outcomes.

All four GCC Release `BUILD_TESTING=OFF` combinations of QTP/tape inspectors passed configure/build; each has no `_deps` directory or Python/doctest cache entries. Their
`prod-<QTP>-<TAPE>-configure.log` and build logs record actual commands. No new production dependency
or Python script was added. Existing Python launchers/process regression tests remain unchanged.

## Oracle, style and negative checks

[Java validation](../../../fixtures/encoding/field-types/VALIDATION.md) records two clean
upstream captures with all 82 artifacts byte-identical, 17 rejected mutations, a full negative Verify
and subsequent unchanged baseline Verify. Java Format then Check covered 16 sources with the pinned
formatter on OpenJDK 23.0.2; oracle execution used Zulu JDK 8. Current hashes of all four oracle tools and 82 artifacts were rechecked against final Verify metadata:
provenance audit. No oracle source changed afterward.

C++ Format then Check passed for 57 files with Uncrustify 0.83.0 followed by clang-format 20.1.8:
Format, Check. The first formatting
check required one additional pipeline pass for a test-helper blank line; the final check is clean.
Naming, Doxygen, ownership, budget arithmetic and scope were also reviewed manually.

The five new suites cover six value cases, ten tape cases, seven I/O cases, twelve primitive-reader
mutations and twelve complete-output mutations. Each negative run is followed by its immutable
baseline. The CLI comparator independently derives expected grouping from original envelopes and
Java STREAM callbacks, including records in different equal-time frames. Earlier comparator
development failures concerned descriptor expectations and callback batching; those expectations
were corrected against original evidence before the successful matrix, without changing fixtures.

Pre-implementation red checks demonstrated missing char-array API, rejection of the new schema and
CLI profile. The initial old 20-suite smoke run used an existing MSVC Release build; it is not treated
as a fresh baseline rebuild. The full final matrix is the regression evidence. Build setup required
Windows sandbox elevation for SDK discovery; an initial malformed path argument and a later WSL
inline-shell argument issue were corrected in the launch commands, not in production behavior.

All 1982 pre-task fixture/lock files retain their SHA-256 values:
before hashes, audit.
Historical captures, upstream.lock and expectations were not rewritten. The private tape was not
read. New evidence is generated offline; no live endpoint was needed.

## Final scope

source manifest identifies the reviewed implementation/test sources;
completion audit records links, whitespace and an empty staged diff.
Raw evidence is preserved as captured, without trimming it to satisfy Markdown whitespace rules.

The implementation adds exact field support and a fixed diagnostic profile, without a full Profile
adapter. No scope deviation or blocking semantic question remains for this bounded task. Allocation
failure handling was reviewed but not fault-injected; generated evidence does not establish arbitrary
schema or full private-file compatibility. No errors were detected in the successful checks listed
here; that does not assert the absence of all semantic errors.

A duplicate learning would add no durable information. Independent review remains a separate step.
