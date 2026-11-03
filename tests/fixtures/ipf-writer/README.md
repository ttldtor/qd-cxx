# Local IPF writer observations

[Русская версия](README.ru.md). English is authoritative.

[Provenance](provenance.json) binds32 named cases to pinned QD3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK8, canonical harness/launcher sources,
production-class inventory and exact98 raw asset hashes. No random matrix is generated.

[Case index](capture/cases.tsv) contains ID, selected native status, input count and Java byte
length. Each .input.tsv lists canonical PROFILE UTF-16 hex name=value pairs; .ipf contains actual
Java writer bytes; .read.tsv records actual Java reader canonical profiles/EOF completion or
hex-encoded error. Raw CRLF, embedded zeroes and UTF-16 surrogate observations are preserved.

| Named cases | Observable boundary |
| --- | --- |
| empty, basic, empty-type, empty-symbol | Complete marker, empty/default/type/symbol records |
| all-text-fields, typed, nonfinite, zero-defaults, typed-boundaries | All standard categories, dates, extremes, canonical defaults |
| custom-order, custom-case-empty-name, custom-quotes, union-fields | Sorted exact case-sensitive custom names including empty, union formats |
| escaping, unicode, embedded-nul, bom-value, paired-boundaries, noncharacters | CSV/CR/LF/quotes, exact BMP/supplementary/null text |
| types-order, duplicate-symbols, hash-type, quoted-type | Header lexical order versus source profile order, ordinary '#' and quoted types |
| removed, removed-empty-extra, removed-standard, removed-custom | Ordinary tombstones work; Java discards only nonempty extra data |
| reserved-flush, reserved-complete, reserved-header | Java emits formats its own reader rejects; native INVALID_TYPE |
| unpaired-high, unpaired-low-name | Java replaces unpaired surrogate with '?'; native UNREPRESENTABLE_UTF16 |

[Failure observations](capture/failures.tsv) exercise actual Java output failures in the body,
marker and final flush. A final-flush failure can leave a full completion marker externally.
Native stream tests check both exception masks, partial side effects, successful-write lower-bound
counts and complete=false; no external rollback/atomicity is claimed.

Native tests compare exact bytes for25 accepted cases and round-trip all canonical text through
the native reader. The [launcher](../../../tools/upstream/ipf-writer-oracle/README.md) can then
read actual emitted native files through the production Java reader. Explicit differences are
defined in the [paired writer contract](../../../spec/ipf/WRITER.md), not hidden as skipped tests.

