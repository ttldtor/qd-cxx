# QDCXX-034 admission and offline rewrite validation

[Русская версия](ADMISSION-VALIDATION.ru.md). English is authoritative.

## Implemented boundary

`qdcxx_protocol` exposes `admitTape` and move-only `AdmittedTape` in
`qd/protocol/tape_admission.hpp`. Admission copies one complete caller byte sequence under a finite
input limit, checks it with a fresh `RAW_MESSAGE_V1` reader through explicit finish, validates every
description including unused records, and constructs an exact complete outgoing inventory. A token
is published only after all checks succeed. The token owns the exact original bytes, descriptor,
validated schema/layouts, and limits; a moved-from token becomes inactive. Failed reader results
never admit their completed record prefix, and no decoded history is retained.

`AdmissionLimits` separates reader limits, layout limits, extra nonrefunding work, simultaneous
logical scratch bytes, and simultaneous UTF-16 scratch units. The admission ledger charges copies of
retained schema records, fields and lookups, along with source, description, binding and candidate
storage. Limits describe logical ownership, not allocator metadata/capacity or an exact heap cap.
Reader-private frame and pending buffers remain in the independently bounded `TapeLimits` domain.
Strict admission rejects ambiguous or unrepresentable descriptors and refuses sender ID
`INT32_MAX`; it does not renumber identifiers.

The optional `qdcxx-tape-write` CLI defaults OFF and composes only an admitted complete source.
It loads an explicitly selected built-in profile or the optional native JSON schema before tape
I/O. The source is read to physical EOF and closed before admission and before any output temporary
is created. A second fresh reader pass composes original envelope positions with bounded writer
chunks. Publication creates the destination only after checked writes, reader/writer completion,
flush and close, using same-directory temporary ownership and create-only hard-link publication.
Existing and racing destination entries are refused. There is no overwrite or rename fallback.

## Executed author checks

The archived raw inputs, outputs, Java observations and selected logs are indexed in the
admission evidence archive; `snapshot.tsv` and `inventory.tsv`
identify the source revisions and verify each archived file's exact bytes.

The frozen admission implementation was formatted and checked with Uncrustify 0.83.0 followed by
clang-format 20.1.8. An isolated Windows MSVC 19.42 x64 Release target build passed. Its admission
test passed 6 cases / 1,388 assertions with emission enabled and 6 / 1,384 without emission. The
emitted B34 witness contains 128 records and measured a 9,479-unit peak logical admission scratch
value; checks covered 0, 9,478, 9,479 and 9,480 units. Independent work-ledger cases cover a
descriptor-only tape with 0, 1 and 3 schema fields and the exact boundary formula. Separate
mutations that removed schema-field copy charges or changed reader allocation-failure translation
failed their expected assertions; the restored implementation passed.

The CLI author checks covered both JSON-enabled and JSON-disabled MSVC Release production builds,
with `BUILD_TESTING=OFF`; JSON-disabled configuration did not acquire simdjson. The CLI regression
checked all 38 help options/defaults and numeric grammar, five native corpus inputs, exact canonical
rewrites, standard Quote/Profile, finite input/output refusals, existing/same destinations, Unicode
paths, and normalization of an overlong compact integer to 61 output bytes. A Windows argument
bridge defect for an input file literally named `--output` was reproduced, corrected, and rechecked
with a fresh native invocation. The CLI and changed C++ files passed the required native formatting
pipeline.

An independent focused Windows MSVC 19.42 x64 Release review passed its build and 3/3 CTest tests:
admission 6 cases / 1,384 assertions, filesystem 12 / 437, and CLI coverage of help, all 38 options,
refusals, Quote/Profile rewrites and Unicode paths. It independently closed the overlong sender
case and the literal `--output` input-path regression after the CLI correction. The source hashes
checked by that review matched the frozen final files. This was a focused review run, not a complete
project matrix.

## Full project acceptance

The final Windows MSVC x64 Release build passed and the full CTest suite
passed **50/50** tests in 285.37 seconds. Separate Windows production-only Release builds also passed with `BUILD_TESTING=OFF`
and JSON OFF: one with tools OFF and one with QTP inspection, tape inspection, and tape-write ON.
The configuration/dependency record found no
acquired dependencies in either profile. The final
97-file C++ format check passed.

The five final CLI rewrites were accepted by the locked Java production FileReader with 18 STREAM
records, 54 callback projections, 152 rows and zero parser errors. The exact inputs, normalized
outputs, Java observations, provenance and raw comparison logs
are in the archive. Focused Linux GCC ASan+UBSan file-boundary tests passed 12 cases / 436 assertions.
The final Linux Debug full-suite run
passed **50/50** under Ubuntu 24.04 / WSL2 with GCC 13.3, AddressSanitizer and UndefinedBehaviorSanitizer.
CTest reported 865.93 seconds; the logged command interval was 2026-10-11T23:23:12.960Z–23:38:14.459Z.
Both Linux production-only Release profiles built successfully with JSON disabled: minimal tools OFF, and the QTP inspection, tape inspection, and tape-write tools ON. The final checks confirmed that neither profile created `_deps`; see the archived Linux dependency-gate logs and tools profile gate. Both builds emitted the existing GCC `-Wstringop-overflow` warning at `src/core/wide_decimal.cpp:68` and exited successfully. The independent reviewer did not run
Linux, JSON ON, the Java oracle, or the complete project suite independently.

Doxygen exited 0, generated the admission API page, and emitted a zero-byte warnings file. The
configuration enables `EXTRACT_ALL` to include free functions; neither the empty file nor generated
HTML establishes completeness of all API documentation. See the archived
Doxygen log and
warnings file.

No macOS, isolated-surrogate Windows path, hostile parent replacement, race-free publication under
hostile concurrent mutation, or crash/power-loss durability certification is claimed. Logical
scratch limits exclude allocator overhead/capacity, and the measured B34 value is not a universal
heap bound.
