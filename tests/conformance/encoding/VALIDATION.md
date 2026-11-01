# C++ encoding validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

## QDCXX-011 current validation — 2026-09-21 UTC

All 1146 unchanged Java observations pass, including 314 UTFString and 832 compact regressions;
four doctest cases / 43,375 assertions. Earlier QDCXX-010 evidence below is historical.

| Executed check | Evidence |
| --- | --- |
| MSVC 19.42.34444.0 Debug / Release | [Debug](utf-msvc-Debug.log), [Release](utf-msvc-Release.log) |
| ClangCL 19.1.5 Debug / Release | [Debug](utf-clang-Debug.log), [Release](utf-clang-Release.log) |
| WSL Ubuntu 24.04.4, GCC 13.3.0 Debug / Release | [Debug](utf-linux-Debug.log), [Release](utf-linux-Release.log) |
| GCC Debug AddressSanitizer + UndefinedBehaviorSanitizer | [Actual execution](utf-asan.log), no reported errors |
| UTF byte/value/parser/empty-coverage/CTest negatives, MSVC Release | [Report](utf-negative-proof.json), [log](utf-negative.log) |
| Same UTF negatives, GCC Release | [Report](utf-linux-negative-proof.json), [log](utf-linux-negative.log) |
| Compact negatives plus formatter mismatch/idempotence/missing-tool regression | [Report](utf-compact-negative-proof.json), [log](utf-compact-negative.log) |
| Library-only build | [GCC build](utf-library.log), BUILD_TESTING=OFF, no `_deps` directory |
| Reuse decision before implementation; exact candidate follow-up | [Initial](utf-reuse-initial.log), [exact probe](utf-reuse.log), [contract](../../../spec/encoding/UTFSTRING.md) |
| Oracle evidence audit | [UTC log](utf-audit.log) |
| Final Format → Check, links, diff and immutability | Report |

All requested compiler environments were available. CMake 4.4.2, Windows VS 2022/SDK 10.0.26100.0
and Linux Ninja 1.11.1 match the [build instructions](../../../docs/CPP-BUILD.md). Sandboxed WSL and
Windows SDK access initially failed; approved elevated runs succeeded. One initial WSL shell loop
lost its build-path argument before compilation; the corrected script performed the recorded runs.
Sanitizer flags were `-fsanitize=address,undefined -fno-omit-frame-pointer`; the sanitizer configure
used the existing pinned doctest source via `FETCHCONTENT_SOURCE_DIR_DOCTEST`, without changing it.

No Java rerun: hashed Java tools and inputs are unchanged. Every original fixture file and
upstream.lock matches the pre-change SHA-256 inventory. No golden was regenerated. New logs use
`tools/run_utc.py`; historical logs retain their original timestamps. Naming, Doxygen, self-contained
headers, integer arithmetic and borrowing/ownership were reviewed separately from formatting.

Repeat UTF negatives with `python tests/conformance/check_utf_string_negative.py
.work/msvc/Release/compact_tests.exe --report .work/utf-negative-proof.json` (one command), or the
Linux executable path. Repeat the existing compact negative driver as documented below. Both mutate
only temporary fixture copies, demand the expected diagnostic/case, verify unchanged copies and
rerun the true baseline. S-ascii writer mutation is `03414243 → 03414244`; reader units change
`004100420043 → 004100420044`. Release CTest must fail on the temporary mismatch.

No huge allocation, allocator-failure injection, 32-bit target, macOS, other Linux distributions,
other BufferedInput implementations, performance benchmark, full Unicode/QTP or transport/importer
was tested. No production UTF dependency was added. The API remains provisional.

Initial implementation: 2026-09-21 local date. Owner follow-up: 2026-09-20 UTC
(exact UTC instants in the fresh logs below). [Build/API instructions](../../../docs/CPP-BUILD.md)
record exact tools, generator, SDK and configure/build/test commands. MSVC 19.42.34444.0 and
ClangCL 19.1.5 both passed Debug and Release (one CTest per configuration), each executing
96 writer/736 reader oracle rows. Ubuntu 24.04.4 WSL2 x86_64 with GCC 13.3.0/libstdc++ also
passes Debug/Release: two doctest cases and 28,358 assertions per run. CMake 4.4.2 and Ninja 1.11.1
were used on Linux. This checks the measured Windows/Linux profiles, not universal portability.
Sanitizers, macOS, other Linux distributions and other architectures were not run. No Java rerun was needed: hashed Java tools, oracle inputs and captures are unchanged.

## Results and retained evidence

| Check | Result/evidence |
| --- | --- |
| Java-vector comparison | All 832 selected observations pass; 101 UTFString rows explicitly excluded. [Exact case IDs](cases.tsv), [coverage](COVERAGE.md). |
| MSVC / ClangCL Debug/Release | Fresh UTC CTest logs: [MSVC Debug](msvc-debug.log), [MSVC Release](msvc-release.log), [ClangCL Debug](clang-debug.log), [ClangCL Release](clang-release.log). |
| Linux GCC Debug/Release | [Linux Debug](linux-debug.log), [Linux Release](linux-release.log); same unchanged cases and doctest assertions. |
| Writer storage bounds / reader sentinel | 1056 bounded writes and 144 appended-sentinel reads pass. |
| Temporary expected byte/value mutations | Both detected with case ID and expected/actual details; [negative-proof.json](negative-proof.json). |
| Parser failures | Invalid decimal, hex, status, exception category and empty coverage rejected. |
| CTest failure propagation | Temporary registration with changed writer expectation fails with exit 8 and expected/actual diagnostic. |
| Formatting | Pinned Format then Check passed for all three C++ files; a subsequent read-only Check passed. Temporary mismatch rejected without rewriting; Format/Check/Format/Check proves idempotence on the temporary sample. Missing tool rejected. |
| Style review | Naming, explicit braces, standalone public-header requirements, namespace comments, Doxygen and bounded ownership reviewed separately. |
| Existing encoding evidence audit | Passed all hashes/provenance and prior Java observations. |
| Immutability | SHA-256 comparison of 339 initially present fixture files plus upstream.lock (340 entries) passed. No goldens/configurations were edited. |
| Documentation / whitespace | EN/RU claims and reciprocal/relative links reviewed; git diff --check passed. |

## Reproduce supplementary checks

Run after the builds above, from the repository root:

```powershell
python tools/format/setup_cpp.py
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/check_compact_negative.py .work/msvc/Release/compact_tests.exe --report .work/negative-proof.json
python tests/conformance/check_encoding_evidence.py
git diff --check
```

The [negative-check driver](../check_compact_negative.py) uses temporary copies, requires specific
semantic diagnostics (not just any process failure), verifies real and mutated expectations remain
unchanged and reruns the baseline. The report retains fixture hashes. The temporary writer mutation
is `I-zero` byte `00 → 01`; the reader mutation is value `0 → 1`. Temporary directories are removed.
[Formatter lock](../../../tools/format/cpp-tools.lock.json) pins downloaded archives and hashes;
[formatting workflow](../../../tools/format/CPP.md) describes scope and version checks.

## Limits and next boundary

The API is provisional and covers only bounded compact int/long. Java direct-reader EOF maps to
`INSUFFICIENT_INPUT` with measured consumption; Java exception text is not a C++ API promise.
No wire semantics were changed. Existing schema/protocol/encoding fixtures and upstream.lock remain
byte-for-byte unchanged. No production runtime dependency, broad CI, UTF codec, importer, QTP framing,
socket layer, packaging or performance claim was introduced. Later UTFString work needs explicit
surrogate-preserving representation. Later integration still needs supported Java tape-format checks
and local qds-tools replay or a Java publisher where appropriate; none was implemented here.

## Owner follow-up evidence

- [Windows negative proof](negative-proof.json) was regenerated after doctest migration; [Linux negative proof](linux-negative-proof.json) and [Linux negative log](linux-negative.log) confirm the same
  byte/value/parser/CTest mutation failures. Linux explicitly skips the Windows formatter checks.
- `BUILD_TESTING=OFF` Linux configure/build succeeds and creates no `_deps/` directory, confirming
  the doctest download is confined to tests. Production codec source and original fixture hashes are unchanged.
- Public API and test helpers now carry explicit `@param`, `@tparam` and non-void `@return` descriptions.
  Pinned Format/Check and the unchanged Java evidence audit pass after these changes.
- [UTC runner](../../../tools/run_utc.py) prefixes log lines with full ISO 8601 UTC timestamps and
  sets the child timezone. An intentional exit-7 command confirms failure propagation and UTC output.
  Both CTest LastTest.log and the newly retained C++ logs use UTC. New negative reports carry
  `executedAtUtc`; historical Java oracle logs are preserved without timestamp edits.

The updated [build instructions](../../../docs/CPP-BUILD.md) include Windows/WSL commands and
UTC logging. doctest's upstream CMake minimum-policy deprecation warning is non-fatal and retained
in local configure logs; no dependency source patch or version substitution was used.
