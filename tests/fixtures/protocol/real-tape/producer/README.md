# Producer-specific offline fixtures

[Русская версия](README.ru.md). English is authoritative.

The [generator](../../../../../tools/upstream/real-tape-corpus/README.md) uses separate exact
Java QD baseline/current producers. Each producer directory contains `expected/` with seven
unmodified generated artifacts and `provenance.json` with portable source, runtime, classpath
and artifact hashes. Actual invocation paths and complete UTC text logs remain private. `corpus.tsv` inventories
these producer-specific files; the parent shared corpus inventory is not modified.

The producing launcher is Windows-qualified with an explicit native UTC runner. Its logs merge
decoded Java streams with timestamps; separate raw Java streams are not claimed. The portable
artifact files do not acquire a Windows runtime dependency. Native Linux fixture consumption is
a separate integration check; Linux/macOS producer execution and POSIX child cleanup are unqualified.

| Artifact | Origin and expected result |
| --- | --- |
| `compact-none.qtp` / `.tsv` | Production BINARY/NONE/EMPTY_SET/STREAM_DATA, NONE time; three compact integer records, exact values and cipher/raw symbols |
| `quote-none.qtp` / `.tsv` | Production writer and full default Quote delegate/scheme, NONE time; three exact records |
| `quote-message.qtp` / `.tsv` | Same full default Quote, fixed MESSAGE times; three exact records and event-time sequences |
| `quote-truncated.qtp` | Derived from `quote-message.qtp` by removing two final bytes; production reader rejection and a two-record prefix, no complete claim |

TSV is UTF-8/LF. Record rows contain ordinal, event-time sequence, UTF-16 hex record/symbol,
event flags and every local integer slot as `UTF16-field-name:full-type:raw-int`. Wide values
include their actual local void-tail slots; these are local observations, not additional wire
fields. `complete` requires exact expected records, one descriptor, zero parser errors and byte
consumption equal to physical file size. There are no object fields in these selected cases.
No symbols or bytes originate from private market recordings.

`provenance.json` records exact QD identity, source hashes normalized to LF and actual source-byte
hashes, JDK/Maven observations, executable/configuration hashes, source/helper hashes, classpath
content digests, build dependency inventory and artifact hashes. A classpath digest hashes sorted
`relative-path<TAB>sha256` lines joined by LF without a final LF. Runtime metadata retains each
individual classpath file hash privately. Builds use source classes, so tape descriptors identify
`QDS-UNKNOWN`; provenance comes from the selected commit and controlled build, not that string.

Capture never changes expectations. Promotion consists of reviewing a fresh private capture,
copying its exact artifact set here, creating portable provenance and running fresh Verify for
both producer pins. Negative checks reject five altered artifact sets and parse the derived
truncation through production FileReader. [Validation](../../../../conformance/protocol/real-tape/producer/VALIDATION.md)
records the executed scope. These files establish only their named Java production cases;
they do not establish arbitrary QD input support or C++ decoding parity. Optional live demo
acquisition is not implemented or run.

