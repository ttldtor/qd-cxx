# Plain local IPF grammar observations

[Русская версия](README.ru.md). English is authoritative.

[Provenance](provenance.json) binds the 42 named cases to pinned QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK 8, formatted harness/launcher sources,
production-class inventory and exact input/outcome hashes. The [case index](capture/cases.tsv)
contains IDs, input byte lengths and the number of emitted profile/control events.

Each `<id>.ipf` is raw uncompressed input. Its `<id>.tsv` lists whole `PROFILE` events with exact
UTF-16 hex name/value pairs, ordered `FLUSH`/`COMPLETE`, and `EOF` or hex-encoded first `ERROR`.
A final `READER`/`READER_ERROR` row records the actual public Java reader's list size/completion
flag or first exception. Raw CRLF, embedded zeroes and malformed UTF-8 are retained unchanged.

Coverage is a fixed named corpus, not random CSV generation:

| Cases | Contract |
| --- | --- |
| basic, header-only-eof, header-only-newline, profile-eof, zero-fields | Real format/record and final-row EOF behavior |
| blank-lines, empty, empty-format, empty-type | Blank/empty type/header boundaries |
| quoted, multiline, unicode | Quoted comma/escaped quote/newline and exact supplementary/null text |
| unquoted-quote, quoted-suffix, quoted-space, quote-unfinished | Exact CSV failure kinds and valid emitted prefix |
| short-row, extra-row, extra-bad-quote, invalid-number-first, invalid-date | Field counts, conversion and error priority |
| header-redefine, duplicate-fields, empty-custom-name, metadata-as-type | Redefinition, skipped empty duplicates, custom names and lookup priority |
| comments-controls, complete-eof, complete-then-error, control-malformed-tail | Ordered controls, marker versus EOF and later errors |
| unknown-type, unknown-after-prefix, forbidden-flush, forbidden-complete, forbidden-header | Undefined/reserved format failures |
| bom, utf8-stray, utf8-overlong, utf8-surrogate, utf8-bad-middle, utf8-bad-third, utf8-out-of-range, utf8-truncated | Actual JDK UTF-8 replacement grouping and preserved BOM |

Examples: [basic input](capture/basic.ipf), [basic outcome](capture/basic.tsv),
[multiline input](capture/multiline.ipf), [unfinished-quote outcome](capture/quote-unfinished.tsv).
The [oracle launcher](../../../tools/upstream/ipf-reader-oracle/README.md) captures/verifies these
files using the pinned default missing-completion strategy (warn). Native tests compare every
byte split and tiny work turns, then check declared budgets, logical positions, local binary files,
throwing stream ownership and explicit REQUIRE policy. C++ accepted-prefix results and logical
cursors are defined by the [reader contract](../../../spec/ipf/READER.md); Java buffer prefetch
is not presented as a native byte offset.
