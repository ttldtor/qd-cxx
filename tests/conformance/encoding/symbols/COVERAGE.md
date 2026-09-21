# C++ symbol conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[symbol_tests.cpp](../../symbol_tests.cpp) consumes immutable [Java observations](../../../fixtures/encoding/symbols/expected/symbols.tsv).
It requires exact counts: 3465 read rows / 693 main scenarios, 19 writer vectors, 114 sequence reads,
seven integration deliveries, completion marker and unique read identities. Missing/empty fixtures
cannot pass. Java cipher, exact UTF-16, consumption, raw flags and flag-byte count are compared.

After a failed main read, C++ continuation is checked as FAILED/zero/no value, not against Java's
partially retained state. after-reset starts a new message; sentinel gets another C++ reset because
Java's preceding undefined repeat was an error. Successful after-repeat/MRU rows compare to Java.
The integration driver selects captured type-15 symbol boundaries and skips the empty R record ID;
it compares decoded values to real Java deliveries. This test-only driver is not an event decoder.

Additional tests cover input/output/state budgets including zero and exact bounds, supplementary
unit-budget failure, length/resource precedence, unsupported option mask, truncation without borrowing
another payload, terminal/reset behavior, MRU reset, NUL/surrogates and independent result lifetime
across mutation/reset/destruction. No production writer or C++ round-trip is the oracle.

[Negative checker](../../check_symbol_negative.py) changes units, consumed count, repeat state, MRU,
cipher, removes cases/completion, supplies malformed units, empty and missing files. It requires each
failure and an unchanged passing baseline. [Validation](VALIDATION.md) records the final matrix.
Huge resources, allocation-failure injection, 32-bit targets, exhaustive penta/Unicode/flags,
resolver cache behavior, negotiation, event metadata semantics and transport are not tested.

## TSV review follow-up

Every row is schema-validated before its disposition, including Java fields excluded from the C++
terminal-state projection. The closed vocabulary and column counts (including row type) are:

| Type | Columns | Disposition |
| --- | --- | --- |
| profile | 5 | Configuration metadata only; exact bounded profile literals checked |
| writer-error | 6 | Java writer rejection only; no C++ writer comparison |
| writer | 5 | Canonical Java writer evidence, counted; corresponding main reads compare to C++ |
| writer-sequence | 3 | Supplies captured bytes for C++ sequence reads |
| sequence | 6 | Compares positions, UTF-16 and flags |
| read | 12 | Compares the documented C++ projection; raw Java string and post-error state remain Java-only evidence |
| integration | 2 | C++ reads captured symbol boundaries |
| replay | 3 | Java-only one-chunk parser schedule, production / [positive count] |
| parse.begin | 2 | Java-only parser input position |
| record | 3 | Compares delivered symbol values; validates numeric index and STREAM_DATA/0/R shape |
| callback | 4 | Java-only callback grouping; validates STREAM_DATA and counts |
| parse.end | 7 | Java-only parser summary; validates all six counts |
| complete | 3 | Requires symbols / 156 and exactly one final completion row |

Unknown types, missing/extra columns (including a trailing empty column), malformed identifiers,
hex bytes/UTF-16, unexpected nulls, invalid status literals, nonnumeric/negative counts and Java ints
outside the signed 32-bit range are rejected. Hex cannot contain signs or whitespace. Sequence and
read positions are range-checked. Exactly one complete must be the final row; duplicate complete,
any following data or blank row, missing completion and non-EOF stream failure fail the test.
Diagnostic-only types have individual explicit handlers; there is no generic ignored-row branch.

The original ten negative checks remain. Four additional temporary-copy mutations exercise
unknown-row, wrong-column-count (extra empty profile column), duplicate-complete and row-after-complete
(a valid parse.begin row). All four require their specific rejection diagnostic as well as nonzero
exit. The unchanged baseline must pass and all temporary inputs/goldens must remain unmodified.
See the [follow-up results](VALIDATION.md#tsv-review-follow-up).
