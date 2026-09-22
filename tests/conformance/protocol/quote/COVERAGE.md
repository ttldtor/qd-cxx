# Default Quote C++ coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The [Java evidence](../../../fixtures/protocol/quote/COVERAGE.md) preceded the
[value](../../../../spec/encoding/QUOTE-VALUES.md) and
[mapping/tape](../../../../spec/protocol/DEFAULT-QUOTE.md) contracts and implementation.
The original 326-byte production tape remains unchanged. The 40 other tapes are diagnostic
inputs replayed by production Java, distinct from the actual QuoteDelegate/FileWriter round trip.

| Suite | Scope |
| --- | --- |
| quote_java_values | All strict primitive rows: 14 UTF_CHAR writes, 27 reads/positions, five time bounds, 19 raw decimals and 13 composition observations. Writer capacity/atomicity, raw compact-long identity, exact text/double bits, NaN/infinities/zero, independent decimal boundaries. No C++ decimal composition implementation. |
| quote_java_tape | All 41 unmodified Java tapes in whole/one-byte/seven-byte schedules. Every ordered symbol, field tag/raw value, decimal text/bits and mapped Quote row is compared, including missing/reordered/duplicate/unknown inputs and observed losses. Strict case/row arity/order/count/completion validation. |
| quote_java_tape, designed cases | Explicit local profiles; unsupported full identities and known-field type mismatch; exact supported skips; repeated targets; absent decimal NaN; compact low-32 projection; truncated fields and invalid UTF_CHAR positions; completed-prefix retention; owned output after input/schema/reader destruction; map shape/range/symbol bounds. |
| quote_java_tape, lifecycle/budgets | Exact/zero/short record/field/value/symbol and input/construction bounds; all strict original tape prefixes; late descriptions; repeated IDs; failed multi-description update publishes nothing; cumulative values across messages; fresh per-message symbols and terminal failure. Underlying legacy tape/schema tests retain their broader bound matrix. |
| quote_inspection_cli | Original real tape compared with Java raw and mapped rows; chunk schedules; no-argument/help equivalence, invalid/incomplete args and diagnostics versions; exact/short TSV/human output, record/value/symbol budgets; explicit new profile and legacy diagnostic opt-in. |
| quote_inspection_io | Safe exact symbol/exchange escaping; fractional/special decimal display; exact pair-of-rows output budget; injected read/write/flush failure; bounded TIME/OPTIONS/MESSAGE/TYPE/SEQUENCE reasons with unchanged numeric positions and legacy diagnostics. |
| 13 legacy suites | Existing compact/UTFString/framing/descriptions/binding/symbol/STREAM_DATA/tape and both inspector regression paths remain enabled. Original profiles still reject the newly supported identities unless explicitly selected. |
| [Test-Negatives.ps1](Test-Negatives.ps1) | Nine copied C++ consumer mutations: full type, raw value, record ID, field order, row arity, mapped bits, missing completion, unknown row, binary byte. Unchanged baseline rerun. Java comparator has its own nine mutations and full negative/baseline Verify. |

Review follow-up adds eight independent mapQuote tag substitutions: each correct slot is replaced
with supported COMPACT_INT and must fail INVALID_SHAPE. Existing shape/range checks remain.
The IO suite now exercises DESCRIPTOR for type=xape and OPTIONS for nonzero raw event flags,
with exact diagnostic rows/positions in versions 1/2 and TSV/human output. Earlier nonempty-opt
OPTIONS coverage is retained. New executed checks are separate from the initial matrix.

The IO suite also rejects schema/display profile mismatches and wrong Quote name/type shape before
reading input or emitting stdout, with LOCAL_PROFILE diagnostics.

Resource policies, immutable contexts, explicit exact-type rejection and completed-prefix behavior
are designed C++ contracts, not claims of identical Java exception/batching policy. Zero decimal raw
means NaN; finite zero sign and all non-NaN binary64 values are compared exactly. Unknown supported
records consume every field with field/work charges but allocate no result values.

No general tape/RAW_DATA/MESSAGE timing, external schema loading, regional events, conversions,
dynamic replacement, recovery, compression, transport, throughput, allocator injection, 32-bit or
universal Unicode/decimal exhaustive coverage. [Executed checks and limits](VALIDATION.md).
