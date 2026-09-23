# Serial type model readability refactor

[Русская версия](TYPE-MODEL-REFACTOR.ru.md). English is authoritative.

Baseline: `bfdeca0d6450533bc8ef1c5a9cd098d62039a375`, branch `task/qdcxx-024-field-types`.
Owner-requested follow-up: make base serialization and representation explicit and avoid copying
full numeric identities into production consumers. No additional supported type or wire behavior.

## Acceptance and integration — 2026-09-23 (UTC)

The serial type refactor is accepted; P2 is independently closed. No findings remain within the
reviewed scope. This section records the independent reviewer reports supplied by the owner;
documenting their conclusions is not another code review or test run.

- Review of `a3d9c20db589a5730703f3a2c55bfb14a06961e5` against the baseline above found one
  P2 gap in direct representation-selector checks and no other findings. The reviewer reported fresh
  incremental MSVC Release and GCC Debug ASan+UBSan builds with 25/25 CTest suites in each,
  pinned Format Check for three C++ files, and whitespace/link checks for six Markdown documents.
- Correction `8f2ee55a9444aecc7cd7887a3efb60d6e7ef6379` changes only the test and this EN/RU
  report pair. The targeted follow-up reviewer confirmed all six independent selector expectations,
  retained earlier checks, unchanged production code, and consistent EN/RU reports and logs.
  The reviewer confirmed the author's mutation evidence: the full ID remains 40, the new check fails
  with `40 == 32`, and the restored clean build passes. The stale-binary intermediate run remains
  documented below.
- The targeted follow-up review freshly ran the existing MSVC Release target binary:
  7/7 test cases, 15,846/15,846 assertions, exit 0. Its diff whitespace check passed.
  Mutation, builds and complete 25/25 regressions were not rerun in that review; those results were
  checked in the author's logs. It did not re-review all of QDCXX-024.

The first review did not repeat production-only builds, clean builds, the full platform matrix or
Java captures; it used neither the private tape nor an external cross-model pass. The targeted
follow-up does not extend that coverage. Historical author results below retain their original scope.

The owner integrated the result into `master`. Before this documentation update, read-only Git checks
confirmed branch `master`, HEAD `8f2ee55a9444aecc7cd7887a3efb60d6e7ef6379` and a clean working tree.
The pending-review statements below describe earlier stages and are superseded by this acceptance
record. Historical commands, logs and hashes are retained; this update does not run staging, commit
or merge.

## Changes and rationale

- `SerialType` names the three implemented bases; `FieldRepresentation` names their implemented
  representation selectors. Selectors are alternatives, not independent flags.
- `SerialFieldType` preserves names, int32 underlying type and values 2/8/10/40/56/72/88/120;
  definitions now compose named components. Raw incoming IDs remain int32.
- The masks and extraction helpers preserve unknown high bits. Composition does not validate a
  pairing. The exact-ID `supportsSerialType` gate remains authoritative for schema and binding;
  its switch lists each full identity once without repeated casts/comparisons.
- The tape diagnostic schema reuses the full-type enum instead of maintaining numeric IDs.
  Independent numeric expectations remain in conformance tests to detect accidental ID changes.
- Decoder/value layout, ownership, budgets, output formats, Java tooling and goldens are unchanged.

Source basis: [FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.md), including pinned Java masks
and component definitions. No new Java semantic hypothesis or capture is introduced.

## P2 correction — 2026-09-23 (UTC)

Independent review of `a3d9c20db589a5730703f3a2c55bfb14a06961e5` against the baseline above
identified one P2 test gap: full IDs and decomposition of numeric inputs did not directly pin the
representation enum values. This follow-up adds six direct underlying-value checks in
[field_type_value_tests.cpp](../../field_type_value_tests.cpp): PLAIN=0x00, SHORT_STRING=0x20,
TIME_SECONDS=0x30, SEQUENCE=0x40, DATE=0x50 and WIDE_DECIMAL=0x70. Expectations are independent
literals, not derived from production constants, masks or composition. Existing full-ID,
decomposition and profile checks are retained. Production code, Java tooling and fixtures are unchanged.

### Newly executed correction checks

These are correction-author checks, separate from the historical refactor checks below and from the
preceding independent review. No repeat independent review of this correction has been performed.

- Pinned Format followed by Check passed for the changed C++ test (Uncrustify 0.83.0 then
  clang-format 20.1.8).
- MSVC Release incremental build, targeted `field_type_java_values` (1/1), and complete CTest
  regression (25/25) passed; both inspectors were enabled.
- GCC Debug ASan+UBSan incremental build in WSL, the same targeted test (1/1), and complete CTest
  regression (25/25) passed; both inspectors were enabled.
- Mutation test used a separate `git archive` copy of `a3d9c20`, overlaid with the updated test,
  and a separate GCC 13.3.0 Debug/Ninja build without sanitizers. The existing pinned doctest source
  was reused. The unmodified-copy target passed. Only the copied header was then changed from
  `SHORT_STRING = 0x20` to `0x28`. A separate compile-time probe still asserted full
  `SerialFieldType::SHORT_STRING == 40`. The target built successfully and failed exactly the new
  selector check: `CHECK(40 == 32)`, with 15,845 assertions passing and one failing.
- Restoring the copied header initially preserved its old timestamp, so Ninja reused the mutated
  binary and the first rerun still failed. `--clean-first` rebuilt the isolated target and the
  restored test passed (1/1), completing pass–fail–pass. The shared working header was never mutated;
  its SHA-256 remained `9B3880E92121DD048B201AB34B33A5707D16E9A6A22AE8A921BC444D2C01ACF7`.
- Final diff/self-review, relative file links and trailing whitespace checks passed for the changed
  files, including the EN/RU report pair; the index remained unchanged.

Local evidence is under `.work/field-representation-p2-20260923T130039Z/logs/` (ignored, not committed):
`msvc-{build,target,regression}.log`, `gcc-{build,target,regression}.log`,
`mutation-{configure,baseline-build,baseline-test,build,test,restored-build,restored-test,restored-clean-build,restored-clean-test}.log`
and `mutation-evidence.txt`; the isolated source/build and `full-id-probe.cpp` are adjacent.
Logs use UTC. The isolated configure emitted only the existing doctest CMake minimum-version
deprecation warning. No errors were detected in the positive checks; the mutation failure was expected.

P2 is implemented and locally verified, not independently re-reviewed. The full platform matrix,
production-only build, Java recapture and allocator-failure injection were not rerun; the private tape
was not used. No staging, commit or merge was performed. `ce-compound` was considered and skipped:
the focused test change and this report already capture the reason and evidence.

## Historical author verification

Before implementation, the existing MSVC Release `field_type_java_values` passed. After adding
the new test but before production changes, rebuilding that target failed on the missing
`SerialType`, `FieldRepresentation` and helper declarations. After implementation it passed.

New checks independently assert all eight full IDs and their components, all 256 byte-sized IDs,
unknown high bits, signed extremes, unsupported component pairings and constexpr evaluation.
The existing exhaustive profile checks (-1 through 256), value checks and Java fixtures remain.

MSVC Release incremental rebuild and all 25 CTest suites passed, including fixture negatives and
existing tape/QTP CLI checks. The three changed C++ files passed the pinned Format pipeline.
GCC Debug ASan+UBSan incremental rebuild and all 25 suites also passed in WSL.
GCC Release production-only (`BUILD_TESTING=OFF`, both inspectors ON) rebuilt successfully.
An initial attempt to use that Linux build directory from Windows CMake failed on its cached
Linux generator path; the correct WSL invocation then passed. No cache was rewritten to bypass it.
Format followed by read-only Check passed for all three changed C++ files.

Three read-only simplification passes (reuse, quality, efficiency) found no needed changes in
the header/test diff. The author also inspected the tool's enum substitution and final diff.
The separate `ce-code-review` run could not launch its reviewer: the session agent-thread limit
was reached, and the harness exposes no release operation. No independent-review completion
or merge approval is claimed. `Code review: skipped (ce-code-review unavailable)` applies only
to this failed final review attempt, not the completed simplification passes. Independent review
in a fresh session remains pending. No cross-model CLI was available and no peer job was started.

Only these two test configurations and one production configuration were rerun, not the original
seven/four matrix. Java captures were not rerun because Java, wire semantics and expectations are
unchanged. No allocator-failure injection, additional platform or private-tape testing is claimed.
No new Python code or dependency was added; existing CLI tests used the existing Python scripts.
`ce-compound` was considered and skipped: the durable rationale is already in FIELD-TYPES.

## Historical reproduction

```powershell
cmake --build .work/field-types-msvc-024 --config Release --parallel 4
ctest --test-dir .work/field-types-msvc-024 -C Release --output-on-failure
wsl -- bash -lc 'cd /work/qd-cxx && TZ=UTC cmake --build .work/field-types-gcc-asan --parallel 4 && TZ=UTC ctest --test-dir .work/field-types-gcc-asan --output-on-failure'
wsl -- bash -lc 'cd /work/qd-cxx && TZ=UTC cmake --build .work/field-types-prod-ON-ON --parallel 4'
```

Build directories are existing local configurations, not committed artifacts. This report is a new
refactor record; it does not rewrite historical QDCXX-024 validation or its source hashes.
No staging or commit is performed by this change.

The final MSVC Release rerun after comment formatting also passed 25/25. Relative file targets
and trailing whitespace were checked in all six changed/new Markdown documents; `git diff --check`
passed. Git reports no changes to fixtures, upstream.lock or Java tooling. The Python bytecode cache
regenerated by existing tests was restored to its initially clean HEAD version, not staged.

## Historical owner commit instructions

These commands were not executed. Inspect any pre-existing staged work separately before committing.

```powershell
$files = @(
    'include/qd/core/serial_field_type.hpp',
    'tools/tape-inspect/inspect.cpp',
    'tests/conformance/field_type_value_tests.cpp',
    'spec/encoding/FIELD-TYPES.md',
    'spec/encoding/FIELD-TYPES.ru.md',
    'docs/CURRENT-STATE.md',
    'docs/CURRENT-STATE.ru.md',
    'tests/conformance/protocol/field-types/TYPE-MODEL-REFACTOR.md',
    'tests/conformance/protocol/field-types/TYPE-MODEL-REFACTOR.ru.md'
)
git -C C:/qd-cxx add -- $files
git -C C:/qd-cxx diff --cached --check
# Run commit only after the check succeeds.
git -C C:/qd-cxx commit -m "refactor(core): separate serial types and representations"
```
