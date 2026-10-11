# Default Quote author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

QDCXX-022 implemented from HEAD `fa5a0b967e9c021e656846cdd3d6faee5faa4776`.
Checks below ran on 2026-09-22 UTC.
**Independent review pending.** These are author/team observations, not an independently rerun review.

The initial author runs below are historical. The subsequent presentation-boundary correction and
test additions have a separate review-fix report; independent re-review remains pending.

## Evidence and contract order

The first Java runtime capture preceded the EN/RU [value contract](../../../../spec/encoding/QUOTE-VALUES.md)
and [mapping/profile contract](../../../../spec/protocol/DEFAULT-QUOTE.md), which preceded C++ implementation.
The full default DXFeedScheme was unchanged: 331 records, composite Quote ID 0, twelve int slots,
eight logical fields 56/2/120. The original 326-byte tape contains five real Quote records through
actual QuoteDelegate.putEvent → FileWriter → FileReader → QuoteDelegate.getEvent.
Forty additional constructed tapes are focused production-parser probes; they are distinct evidence.

[Java validation](../../../fixtures/protocol/quote/VALIDATION.md) records two clean matching captures
of all 86 artifacts, nine rejected mutations, a failing full negative Verify followed by successful
unchanged baseline Verify, and the old 22-artifact tape Verify in each of four clean invocations.
Final author comparison reran strict comparison against capture A.
This last comparison is not a fifth clean Java build. Final tool hashes and A/B/Verify artifact hashes
were checked against retained metadata by the audit.

## Final C++ matrix

Both inspector options were ON in every test profile. All 17 CTest suites passed in each row;
the 13 existing suites, profiles, output/error comparisons and fixtures remain enabled.

| Profile | Actual final CTest log |
| --- | --- |
| MSVC x64 Debug | 17/17 |
| MSVC x64 Release | 17/17 |
| clang-cl x64 Debug | 17/17 |
| clang-cl x64 Release | 17/17 |
| GCC x64 Debug, WSL Ubuntu 24.04 | 17/17 |
| GCC x64 Release, WSL Ubuntu 24.04 | 17/17 |
| GCC Debug, ASan+UBSan | 17/17 |

Configure/build logs retain actual command arrays, start/end UTC and exit codes; compiler
versions are in the audit. GCC sanitizers use
`-fsanitize=address,undefined -fno-omit-frame-pointer`. The existing pinned doctest source directories
were supplied explicitly; this does not claim a fresh network dependency download. Existing Python
checks still run unchanged among legacy suites. No new Python script or dependency was introduced.
CMake's pinned-doctest minimum-version deprecation warning remains; no build/test failure was ignored.

Fresh focused checks: primitive suite, 3 cases / 7958 assertions;
tape suite, 6 cases / 54163 assertions;
IO suite, 5 cases / 109 assertions. [Coverage](COVERAGE.md) details the finite set.
All 41 original Java tapes are compared in whole/one-byte/seven-byte schedules, checking ordered
symbols, exact tags/raw values, decimal text/bits and mapped values. CLI tests compare the real tape,
including chunking, exact/short budgets, useful human decimals, old/new diagnostic grammar and help.
The IO tests additionally reject wrong schema/display profile or Quote shape before input/stdout work.

Nine C++ consumer mutations were rejected, then the unchanged baseline
passed. Five primitive mutations were also rejected during development.
Development checks exposed an incorrectly encoded synthetic empty symbol and a missing schema/display
guard; both were corrected before the final matrix above.

Four MSVC Release production combinations with BUILD_TESTING=OFF passed:
OFF/OFF, OFF/ON,
ON/OFF, ON/ON
(QTP option / tape option). Each build directory had no _deps directory and no Python3 cache discovery;
the final audit repeated those checks. Core/encoding/protocol/Quote remain independent of CLI IO.

## Formatting, preservation and handoff

Pinned Uncrustify 0.83.0 → clang-format 20.1.8 Format and
final Check passed for 46 C++ files. Java wrapper Format/Check passed for
13 files with OpenJDK 23.0.2 and pinned google-java-format 1.36.1; only the new QuoteOracle changed,
and final source/tool hashes match the last full Verify metadata. See the Java validation above.

The audit verified all 1393 pre-existing fixture/upstream.lock hashes,
all 86 new artifact hashes across A/B/Verify, four oracle tool hashes, changed/untracked Markdown
trailing whitespace and relative links, git diff --check and the staged diff separately.
No compiled source changed during final validation.
The tracked legacy Python bytecode cache regenerated
by an early test run was restored byte-for-byte; final runs disabled bytecode writes.

Three bounded internal code-quality passes considered reuse, clarity and efficiency. Applied changes
share fixed Quote field labels, remove redundant NaN canonicalization and avoid repeated decimal
conversion during output counting/writing. A separate scalar-only mapping API and a descriptor
abstraction for two fixed CLI profiles were not introduced: the existing empty-symbol projection
avoids a symbol copy, and extra public/structural abstractions are unnecessary for this bounded task.
These passes do not replace independent review.

Reproduction: use [build/API commands](../../../../docs/CPP-BUILD.md), [oracle commands](../../../../tools/upstream/quote-oracle/README.md)
and the exact compiler commands retained in logs. The C++ negative launcher is:

```powershell
pwsh -NoProfile -File tests/conformance/protocol/quote/Test-Negatives.ps1 -Program <qdcxx_quote_tests.exe> -RunDirectory <new-directory>
```

No errors were detected in the final checks above. Coverage does not establish exhaustive Unicode/
decimal behavior, all allocation failures, 32-bit platforms or throughput. General real tape,
RAW_DATA, MESSAGE timing, missing-opt policy, other/regional events, conversions, compression,
schema import, dynamic replacement and transport remain excluded. The owner's private tape was
not used, rewritten or committed. Continue with separately bounded stages in [NEXT-STEPS](../../../../docs/NEXT-STEPS.md).
