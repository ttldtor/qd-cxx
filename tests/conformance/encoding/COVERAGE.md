# C++ encoding conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

## QDCXX-011 current coverage

The existing [doctest executable](../compact_tests.cpp) now selects every observation in
integers.tsv, forms.tsv and strings.tsv: **1146 observations, zero exclusions**. The original
832 compact observations and their capacity/sentinel regressions remain. The sections below
this update retain QDCXX-010 historical counts; their UTFString exclusions no longer apply.
[Fresh case log](utf-cases.log) lists every file/kind/ID/method and totals.

| UTFString file | Writers | Readers | Strict prefixes | OK reads | EOF | Invalid encoding | Invalid length |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| strings.tsv | 13 | 200 | 187 | 13 | 187 | 0 | 0 |
| forms.tsv | 0 | 101 | 79 | 10 | 80 | 8 | 3 |
| Total | 13 | 301 | 266 | 23 | 267 | 8 | 3 |

All 13 S IDs and 22 F IDs, including every captured strict prefix, are mapped in
[Java coverage](../../fixtures/encoding/COVERAGE.md); the fresh log supplies exact expanded IDs.
Each read compares exact UTF-16 units/null, status, consumed and remaining bytes against unchanged
TSV. No C++ round trip supplies expectations. Malformed prefixes use actual Java categories.
Every UTF writer exercises capacities zero through golden size + 2 with guards, untouched excess
and unchanged failure buffers. No sentinel is appended to incomplete UTF input.

The fourth doctest case covers zero budgets, null/empty, isolated surrogates, pair budgets,
incremental limits, invalid-length-before-policy, incomplete/malformed-before-unit-budget,
body overrun and trailing bytes, owning results, SIZE_MAX budgets and the maximum Java length,
maximum + 1 and signed-long maximum without large allocation. Writer byte/unit policy failures
are atomic. A maximum declared body with no body returns EOF under sufficient policy, without
reserving the declaration. Fake huge spans are excluded; arithmetic safety is also source-reviewed.
Four doctest cases execute **43,375 assertions**. Parsing rejects malformed units/hex/decimals,
unknown methods/statuses/categories, duplicate IDs, incomplete files and empty selected coverage.

The [UTFString contract](../../../spec/encoding/UTFSTRING.md) separates operational policy from
Java observations. This is not huge-allocation, allocator-failure, 32-bit, universal Unicode,
other BufferedInput, framing, importer or transport coverage.

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
