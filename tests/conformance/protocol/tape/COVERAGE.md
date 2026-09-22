# Tape reader conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Reader contract](../../../../spec/protocol/TAPE-READER.md), [CLI contract](../../../../docs/TAPE-INSPECT.md),
[Java evidence](../../../fixtures/protocol/tape-reader/COVERAGE.md). These are designed C++ expectations,
not Java lifecycle/position parity. The seven reader cases execute 18,061 assertions in the final MSVC
Debug direct run. Two IO cases exercise injected faults and safe presentation; CMake supplies process checks.
All ten pre-existing suites remain active in every full matrix profile.

| Area | Executed coverage |
| --- | --- |
| Original tape | Exact seven local IDs, symbols and local-order values from unchanged constrained.qtp, late IDs, Unicode/extremes, zero-field records; all two-chunk splits and whole/1/2/3/7/17-byte schedules |
| Java stimuli / C++ policy | All 51 raw inputs at whole/1/5/13-byte schedules; explicit expected classifications, identical completed prefixes and frame/accepted/payload consumed/error positions across schedules |
| Descriptor | Required/absent/empty/null and unknown/duplicate properties, version diagnostic, opt/time/send/receive restrictions, negative counts, magic, trailing extension and complete-message truncation |
| Immutable state | Failed syntax and failed duplicate-ID candidate updates preserve the previous context pointer and plan contents; no new ID becomes visible; subsequent feeds accept zero |
| Data lifecycle | Unknown record skip work, zero-field E, message-local repeat reset, truncated current record after one completed record, terminal error and owning results after reader destruction |
| EOF/markers | Empty/header-only/no-header/no-footer, explicit-zero rejection, marker followed by data (nine records), marker followed by truncated bytes; idempotent finish |
| Positions | Noncanonical two-byte length and type accepted with actual payload start 4/end 60; independently asserted malformed empty header/count and truncated-file offsets; other schedules compare exact positions |
| Resource boundaries | Zero/exact/one-less input, body, frames, descriptions, actions, per-name/aggregate names, construction work, processed/emitted records, wire fields, values, owned/per-symbol/state units, descriptor properties/entries/per-string encoded bytes/units/aggregate units, data payload/remaining symbol span; overflowing frame configuration |
| Reused boundaries | Existing schema/binding/primitive/symbol/data suites retain their independent schema budgets, numeric width/overflow, duplicate-target/skipped-field, unsupported types/flags and ownership tests |
| Output/IO | Default TSV and opt-in human, chunk-identical output, version controls/bidi/lone surrogate escaping, row-budget preflight, exact/short output budget, injected read/write/flush failures, broken output via /dev/full on Linux |
| Process contract | No args equals exclusive help; missing/duplicate/unknown/overflow/negative arguments, zero chunk, missing file; original constrained succeeds, default/TEXT explicitly reject, truncated original fails |
| Fixture integrity | Separate PowerShell strict row validation/exact comparison rejects ten mutations including empty coverage; full negative and baseline clean Java Verify |

No allocator-injection, extreme-memory, performance, other architectures, default Quote, default RAW_DATA
TapeConnector, compression/time sidecars, transport, importer or general session claim. Diagnostic primitive
positions are C++ policy; Java callback batch dropping/continuation is not copied. A successful EOF is not
authenticated recording completeness. [Executed validation and logs](VALIDATION.md).
