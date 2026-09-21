# Codec readability refactoring validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 UTC under the owner's separate readability request, before QDCXX-012.
QDCXX-012 remains unstarted; no framing, importer or transport work is included. This directory
contains new evidence only; prior validation documents, logs and oracle evidence are preserved.

## Changes and compatibility review

Reviewed both headers in `include/qd/encoding/` and both implementations in `src/encoding/`.
Public headers/API are byte-for-byte unchanged. Implementation-only constants name compact byte
widths, masks, full-width prefix and maximum size; UTFString constants name null length, prefix
capacity, sequence sizes, lead/continuation masks, Unicode and surrogate ranges. Constants remain
in unnamed namespaces, with separate names for distinct semantic uses of equal values.
`compactLongSize` isolates the existing length selection, and `sequenceSize` isolates the existing
UTF lead classification. Comments explain seven-bit compact growth, the nine-byte full-width
form, prefix payload placement, UTF lead payload bits and the Java body-length ceiling.

Source review confirms equivalent compact range/shift formulas and unchanged UTF validation order,
resource-policy precedence, consumed/written positions, accepted overlong/surrogate forms and
failure atomicity. No normalized/repaired UTF path or new allocation was introduced. Obvious
counters/indices remain literal. Tests and their exact byte examples are unchanged. The new
[EN/RU style rule](../../../../docs/CPP-STYLE.md#numeric-format-semantics) and AGENTS guidance require
meaningful names or local explanations for format semantics, without a generic framework.

## Executed checks

All seven configurations pass the existing four doctest cases: **43,375 assertions**, including
**832 compact + 314 UTFString = 1146 Java observations**, zero exclusions. They compare exact bytes,
UTF-16/null, statuses, consumed/remaining positions and exercise capacities, canaries, resource
limits and atomic output. Every verbose CTest log retains the full file/kind/case/method inventory.

| Configuration | Build | Test |
| --- | --- | --- |
| MSVC 19.42.34444.0 Debug | [build](msvc-Debug-build.log) | [CTest](msvc-Debug-test.log) |
| MSVC 19.42.34444.0 Release | [build](msvc-Release-build.log) | [CTest](msvc-Release-test.log) |
| ClangCL 19.1.5 Debug | [build](clang-Debug-build.log) | [CTest](clang-Debug-test.log) |
| ClangCL 19.1.5 Release | [build](clang-Release-build.log) | [CTest](clang-Release-test.log) |
| GCC 13.3.0 Debug | [build](gcc-Debug-build.log) | [CTest](gcc-Debug-test.log) |
| GCC 13.3.0 Release | [build](gcc-Release-build.log) | [CTest](gcc-Release-test.log) |
| GCC Debug ASan + UBSan | [build](asan-build.log) | [CTest](asan-test.log) |

Windows uses the existing Visual Studio 2022 x64 builds and SDK 10.0.26100.0; Linux uses the existing
WSL Ubuntu 24.04.4 x86_64 builds and Ninja. CMake is 4.4.2; doctest remains pinned at 2.4.12.
The existing `.work/utf-asan` configuration uses GCC Debug with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. ASan/UBSan actually executed without reported
errors; this is separate from ordinary bounded-storage assertions. All requested compiler families
were available. Windows/WSL build access was granted through the tool's sandbox escalation.

- Pinned Uncrustify 0.83.0 → clang-format 20.1.8: [Format](format.log), then [Check](format-check.log), passed.
- Existing encoding evidence audit: [UTC log](evidence-audit.log), passed; no Java rerun or hashed Java edit.
- SHA-256 before/after: fixtures, upstream.lock, public headers and all pre-existing encoding
  validation artifacts unchanged; [check report](checks.json) records counts and inventory digest.
- Relative Markdown links and `git diff --check`: [final audit](final-audit.log), passed.
- Naming, helper Doxygen, private scope, arithmetic and control-flow equivalence reviewed manually;
  formatters alone cannot enforce those properties.

## Reproduction and limits

Use the existing [build commands](../../../../docs/CPP-BUILD.md) with new log paths in this directory;
use `ctest --verbose` to retain each case. Each build/test log records its exact command, working
directory and UTC start/exit. New logs use `tools/run_utc.py`, without relabeling old timestamps.
No build-system, dependency, public API, wire-contract or fixture changes are part of this refactor.

The checks cover the existing bounded corpus, not every possible input or huge allocation. No new
fuzzing, allocator-failure injection, 32-bit/macOS/other architecture coverage or performance claim.
Existing provisional-API and alternate-BufferedInput limitations remain. No commit was created.
