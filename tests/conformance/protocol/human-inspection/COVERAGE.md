# Human inspection coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

This is designed presentation policy, not new Java wire evidence. [Designed manifest](../../../fixtures/protocol/human-inspection/designed.json)
contains 84 exact cases: 69 projections of historical CLI policy cases (the three TSV-specific
output budgets are replaced by human byte-boundary checks) and 15 explicit Unicode stimuli.
The eight previous suites and all historical expectations remain unchanged. The new two suites
are `human_inspection_cli` and `human_inspection_injected_io`.

| Area | Executed coverage |
| --- | --- |
| Presentation | Header/profile, frame names/IDs/actual offsets including noncanonical headers, S.v and R.y/R.x/R.z local order, skips/duplicates/default zeros, empty symbols, zero-field records, exact completion. |
| Unicode | ASCII, Cyrillic, supplementary scalars and UTF-8 width boundaries, lone high/low and reordered surrogates, literal escape lookalikes, quotes/backslashes, every C0/DEL/C1, line/paragraph separators, all specified bidi controls, decomposed versus composed text. Strict UTF-8 decoding and absence of raw unsafe controls. |
| Partitioning/errors | Read sizes 1/2/3/7/16/4096 and file-size+1; exact completed prefixes, unsupported/malformed/sequence/resource errors, unchanged stderr, seven new format usage errors and help, real closed stdout pipe. |
| TSV regression | All 72 historical designed CLI cases compared to original exact stdout/stderr/exit for both default and explicit tsv at read sizes 1/7/4096; original Java-composed file also compared. Original CLI suite still checks 624 invocations and seven Java records. |
| Output accounting | Exact encoded-byte limits and insufficient budget at every row start/end for all 25 successful cases without extra arguments; no partial logical row. Four/six-byte UTF-16 expansion differences, header/summary costs included. Large escaped symbol with tiny budget and size_t maximum budget. |
| Tool integration | Independently supplied schema uses field[0]/field[1] despite alias order; local names use the same escaping; zero fields distinct from empty symbol; custom global numeric locale, maximum uint64 counter. |
| I/O injection | First write, pre-summary flush and final-summary flush, with and without stream exceptions. Exact old stderr and nonzero exit even when final summary bytes are delivered. These are portable streambuf tests, distinct from real process closed-pipe checks. |
| Negative verification | Nine human mutations: label, value, escape, literal escape lookalike, completion count/removal/duplication, trailing row and frame offset; unchanged baseline. Original 17 mutations/baseline rerun without editing old proof. Format mismatch rejected read-only, baseline and two-stage idempotence pass. |

[Validation](VALIDATION.md) provides commands, logs, measured limits and hashes. The fixture builder
requires a new output directory and is never invoked by CTest verification. Exact comparisons read
checked-in designed expectations; verification checks fixture hashes before/after and never blesses
C++ output. No fresh Java capture was needed: existing production composer/parser evidence is reused.
No new wire semantics, allocation-failure injection, enormous resources/RSS guarantee, 32-bit,
tape/compression/time-file support, protocol descriptors, importer, transport, dynamic sessions,
conversions, broader fields, timing or general dxFeed compatibility is claimed.
