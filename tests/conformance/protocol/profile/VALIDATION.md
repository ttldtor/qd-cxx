# Configured events: author validation and handoff

[Русская версия](VALIDATION.ru.md). English is authoritative.

Follow-up: Java braces review correction records the current source hashes and
new Verify run. The original author-run evidence below remains a historical snapshot.

QDCXX-025 started from clean HEAD
`e5a3764992ba3a90a07d3d092a4383e37a901de2`.
All retained run dates are 2026-09-23 UTC. This is author verification and self-review, not
independent acceptance. Independent review remains pending. No errors were detected in the successful
checks below. [Coverage](COVERAGE.md) traces R1–R10 and distinguishes evidence from broader claims.

## Java first

[Java validation](../../../fixtures/events/mapping/VALIDATION.md) records 21 isolated JVM scenarios,
two clean builds/captures with 135 byte-identical artifacts, 14 rejected mutations, full negative
Verify and subsequent positive Verify. Production delegates/FileWriter/FileReader, direct boundary
probes and source-resolution observations are labelled separately. Contracts preceded production C++.
Full24/full137 remain negative witnesses; compatible XML uses explicit full10 without weakening the
unchanged default/dxlink witnesses. No acceptance blocker required widening the approved scope.

The root author checked current hashes of six oracle tools and all 135 expectations against the
last positive Verify metadata: provenance audit. Final Java Format
then Check passed 17 sources using pinned google-java-format 1.36.1 on JDK 25.0.1; the earlier oracle
formatter used JDK 23.0.2 and oracle execution used JDK 8. Format,
Check. Formatting did not change any hashed Java source.

## Executed C++ matrix

Both inspectors are ON in all seven test configurations. Each runs all 31 CTest suites: 25 previous
suites plus six new ones. MSVC 19.42.34444 and ClangCL 19.1.5 use Visual Studio 17 2022 x64 and
Windows SDK 10.0.26100.0; GCC 13.3.0 runs in Ubuntu-24.04 WSL. CMake is 4.4.2; test-only doctest
2.4.12 is supplied by existing pinned caches. Exact commands are retained in adjacent configure/build logs.

| Configuration | Build / CTest | Evidence |
| --- | --- | --- |
| MSVC Debug | PASS / 31 of 31 | log |
| MSVC Release | PASS / 31 of 31 | log |
| ClangCL Debug | PASS / 31 of 31 | log |
| ClangCL Release | PASS / 31 of 31 | log |
| GCC Debug | PASS / 31 of 31 | log |
| GCC Release | PASS / 31 of 31 | log |
| GCC Debug ASan+UBSan | PASS / 31 of 31 | log |

Sanitizers use `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie` and linker `-no-pie`.
Other architectures, sanitizer configurations and allocation-failure schedules were not exhausted.
Final ordinary builds/tests were repeated after retaining old `Limits` aggregate member order and
adding its compatibility assertion; sanitizer and production builds include that final source.

All four GCC Release `BUILD_TESTING=OFF` QTP/tape ON/OFF combinations configured and built.
Audit confirms no `_deps` directory or Python/doctest cache entries;
`prod-<QTP>-<TAPE>-configure.log` and build logs retain commands. No new Python code or production
dependency was added. Existing Python tests and UTC launcher ran unchanged. GCC retains its historical
`WideDecimal::toDouble` buffer warning, already present in the QDCXX-024 Release log; that source is
unchanged. The documented bounded significand/exponent fits its buffer. This is not a warning-free claim.

## Bounds, output and preservation

The detailed suite log records the focused counts. The two configured oracle
tests made 68,335 assertions over direct observations and both chunk schedules.
The shared helper has 10 cases/135 assertions, Profile 6/137, and Quote 9/54,245 including historical
files. Profile IO has four cases/266 assertions after the aggregate compatibility assertion. Strict
configured-fixture checks reject 13 mutations; CLI checks compare six complete outputs and reject 84
mutations. Sample outputs retain both modes/formats and wire omission.
Failed first flush and failed final flush after completion are separate tests; completion text alone
does not authenticate success. Output limits reject the complete raw/event pair before its first write.

Legacy audit compares stdout/stderr/exit bytes for 18 invocations against a
fresh preimplementation baseline rebuild and 25-suite run. Both snapshots are retained in
`evidence/legacy-before/` and `evidence/legacy-after/`. Full old CLI suites also run in every final matrix.
Historical audit checks all 2188 pre-task fixture/lock files against
initial hashes; all remain identical. No private tape was read.

Pinned Uncrustify 0.83.0 then clang-format 20.1.8 Format/Check passed all 66 discovered C++ files;
the new test helper header was formatted and checked separately. Format,
Check. Naming, Doxygen, exact identities, ownership, error ordering,
finite arithmetic, legacy behavior and changed-file scope were also reviewed manually.

Development checks exposed test-construction issues before final verification: an extra diagnostic
symbol byte, an unchanged numeric mutation, omitted-wire field-count expectation and an invalid
negative schema. These tests were corrected against original evidence. Self-review strengthened final
flush testing and guarded formatter field shape. Windows SDK/Python execution required sandbox
elevation; malformed launch arguments were corrected. None of these failed attempts counts as PASS.

## Handoff scope

Implementation U1–U5 is complete within the assigned subset. New APIs own schema-bound selections
and exact event values; fresh schemas cannot silently reuse old contexts. Copies and immutable wire
snapshot rebuilds preserve the original local context. Generic extra/custom values remain available.
Source loading, complete settings parity, full dxlink, additional identities, indexed/regional events,
wire coercion, compression, nonzero QTP flags, transport and replay scheduling remain separate work.
Independent review is open;

The nontrivial configuration timing, absent/default distinctions and context ownership reasoning are
already in the bilingual normative contracts.  Document audit covers changed/untracked authored
Markdown whitespace, reciprocal language links and local relative targets. Raw evidence was not normalized.
Source hashes identify the checked sources.
