# Container production captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The [maintained oracle](../../../../tools/upstream/tape-container-oracle/README.md) executed Capture,
an independent repeat Capture and Verify against the first capture for each production pin on
2026-10-30, 19:54:40–19:55:32 UTC. This is a repeated author capture, not independent code review.
The original [locked reference](../../../../upstream.lock) remains unchanged.

| Producer | Commit | Runtime/classpath |
| --- | --- | --- |
| Java QD 3.355 | `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` | Zulu OpenJDK 1.8.0_422-b05, fourteen existing production roots |
| Separate Java QD 3.356 | `f71e4cb390d1586f2d4e3a83d9b9067e3326ef19` | Same JDK, separate fourteen production roots |

Every run compared all 1317 classpath file entries per pin (1290 `.class` files and 27 resources)
before/after; no file changed. No Maven or upstream
source build/write occurred. The helper and launcher were frozen after Java Format/Check and
PowerShell syntax/help checks. Their actual hashes, JDK hashes and safe capture intervals are in
[baseline provenance](baseline/provenance.json) and [current provenance](current/provenance.json).
Full local command/stream receipts are retained separately from these relative provenance summaries.

[manifest.json](manifest.json) identifies the exact retained production NONE/GZIP/ZIP files. Raw
ZIP entry timestamps are preserved. Canonical [baseline](baseline/production.tsv) and
[current](current/production.tsv) outcomes record actual FileReader callback counts separately from
direct production StreamCompression results. The helper asserts both expected direct byte counts
and failure classes, then Verify compares canonical bytes exactly. Physical ZIP regeneration hashes
need not be identical because metadata timestamps can differ.

Valid NONE/GZIP/ZIP each produce one STREAM record; the two gzip members produce two ordinary-file
callbacks. Header truncation produces no record. CRC/trailer damage can produce the complete record
before a logged I/O failure; FileReader's parser-error count remains zero. ZIP reads the first file
only, while ordinary buffered gzip reads concatenated members. One-byte direct gzip reads with
available=0 can stop after the first member. Trailing bytes and truncated ZIP EOCD are accepted by
these Java paths. These observations must not be mistaken for successful strict container closure.

Primary source paths at each pin are `dxlib/.../StreamCompression.java` and
`qds-file/.../FileReader.java`/`FileWriterImpl.java`. For example,
[locked StreamCompression](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/StreamCompression.java)
defines the first-file ZIP wrapper. [The C++ profile](../../../../spec/protocol/TAPE-CONTAINERS.md)
deliberately validates the full finite container and rejects unsupported/malformed closure.
