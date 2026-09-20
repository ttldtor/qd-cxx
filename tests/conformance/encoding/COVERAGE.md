# C++ compact integer conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

QDCXX-010, 2026-09-21. Oracle: Java QD 3.355, commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. The doctest-based
[test executable](../compact_tests.cpp) reads unchanged
[integers.tsv](../../fixtures/encoding/expected/integers.tsv) and
[forms.tsv](../../fixtures/encoding/expected/forms.tsv) directly. Selection uses the method column,
not ID prefixes. [cases.tsv](cases.tsv) records every included and excluded row as file/kind/case ID/method,
with exact totals; this is a C++ execution log, not a replacement golden.

| File | writeCompactInt | writeCompactLong | readCompactInt | readCompactLong | Excluded UTFString |
| --- | ---: | ---: | ---: | ---: | ---: |
| integers.tsv | 34 | 62 | 176 | 328 | 0 |
| forms.tsv | 0 | 0 | 116 | 116 | 101 |
| Total | 34 | 62 | 292 | 444 | 101 |

832 selected observations: 96 writer and 736 reader rows. Readers include 592 strict-prefix EOF
cases (396 integer and 196 noncanonical-form prefixes); 144 complete reads succeed. Excluded rows
are the 101 `readUTFString` observations in forms.tsv, including all F-case prefixes. The separate
strings.tsv family (13 writers/200 readers) is outside the two-file test input; no UTFString C++
implementation or conformance is claimed. Exact string IDs remain in the
[Java coverage](../../fixtures/encoding/COVERAGE.md).

## Exercised contracts

All selected IDs, including I/L signed transition boundaries/extrema, X cross-width, XI as-long,
N0–N15 overlong 0/−1, N16/N17 nonzero high bits and their captured strict prefixes, are executed.
Each writer compares exact Java bytes. Each reader compares value or insufficient-input category,
consumed bytes and remaining bytes. Java EOF strings are evidence; C++ returns a status, not Java exceptions.

For each of 144 successful reader rows, appending sentinel `25` preserves the value and consumed
position and leaves the sentinel as the next byte. For each of 96 writer rows, capacities 0–10
(1056 calls) cover every insufficient capacity, exact capacity and excess capacity. Guard bytes
before/after output and unused storage stay unchanged; failed writes report zero and change nothing.
These are supplementary bounded-storage tests; no independently calculated expected codec bytes
or round-trip-only substitution is used.

The parser preserves empty hex fields, uses exact decimal integers, rejects malformed selected rows,
unknown methods/statuses/categories, duplicate row identities, missing completion markers and empty
selected reader/writer coverage. Standard `assert` is not used; doctest fatal assertions remain active in Release. Failures include the case identity and
expected/actual byte/value/position details. CTest propagates nonzero results in Release too.

## Boundaries

The [primitive contract](../../../spec/encoding/PRIMITIVES.md) remains unchanged. Direct-reader EOF
consumption is not transactional framing or resumability. No UTFString, PentaCodec, decimal codec,
schema/importer, networking, full QTP, fuzzing, benchmark or cross-platform guarantee is supplied.
[Validation](VALIDATION.md) distinguishes executed Windows checks and unexecuted coverage.

## doctest follow-up

Two doctest `TEST_CASE`s group integers.tsv and forms.tsv. `REQUIRE_MESSAGE` checks parsing and
codec/storage results; row-level `INFO` keeps file, case ID and method in failure diagnostics.
All 28,358 assertions pass on MSVC, ClangCL and WSL GCC in Debug/Release. The selected/excluded
case log remains identical after migration; no fixtures were rewritten. doctest 2.4.12 is a
hash-pinned test-only FetchContent dependency, with no production target dependency.
