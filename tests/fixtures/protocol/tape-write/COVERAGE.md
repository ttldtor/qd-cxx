# Outgoing witness coverage and limits

[Русская версия](COVERAGE.ru.md). English is authoritative.

The [corpus](CAPTURES.md) characterizes production Java; it does not implement or admit a C++ writing
profile. Its custom record retains exactly full IDs 2/8/10/40/56/72/88/120, two type-10 object fields
and the physical VOID tail of WIDE_DECIMAL. Original Quote/Profile schemas are used unchanged.

The inputs cover int32 and int64 minima/maxima, compact boundaries, zero and negative raw values;
UTF_CHAR zero, surrogate units, U+FFFF and U+10FFFF; null/empty strings, empty/nonempty `char[]`, NUL,
unpaired surrogates, surrogate pairs and control units. Symbols include cipher-backed IBM, repeated
symbols, empty symbol, explicit raw string, long symbol, wildcard and raw UTF-16. Cipher0/null is a
production negative, not an admitted null symbol. A standalone wire repeat marker is not a nullable
application symbol. Type-10 parser output is String even when input was `char[]`; exact units and
null/empty distinction are the asserted semantics, not preservation of Java object class.

All eleven production cases assert eight ordered STREAM_DATA projection records, zero parser errors
and exact field values. All symbol units match their inputs; raw input versus parsed cipher/string
representation may differ. MESSAGE timing retains milliseconds, not the low event-sequence bits;
NONE loses timing and FIELD is a distinct descriptor layout. FileWriter batch grouping is measured
here; no promise is made that a future bounded writer reproduces its entire file byte layout.

## Source-derived facts

All links below refer to the [locked revision](../../../../upstream.lock); source hashes are retained
in [provenance](expected/provenance.tsv). These are source inspection facts corroborated where noted
by production callbacks or separately labelled descriptor scans.

| Fact | Locked source | Corroboration |
| --- | --- | --- |
| Default outgoing names use property names; integers precede objects and VOID is omitted. | [BinaryRecordDesc:182](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L182), 190/198/204 | Custom [descriptor scan](expected/extras/extras-raw-message-wire.tsv); Quote/Profile scans. |
| Adding a new lookup alias changes its field's property name. Existing lookup keys remain. | [DefaultRecord:125](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java#L125), 143 | `Raw.Count`, `RawCount`, `FirstLookup` resolve to the same field; outgoing name is `OutgoingCount`. |
| A descriptor writes the scheme record ID and ordered name/full-type pairs. | [BinaryQTPComposer:209](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L209) | Actual wire IDs Quote0, Profile5, custom0; full identities and ordered fields in scans. |
| Type 10 writes String or char-array units; parser reads String. | [StringField:94](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/StringField.java#L94), 121/126 | Production input/read values and `text-input` rows. |
| Cipher0/null is invalid; identical previous symbols use the repeat encoding. | [PentaCodec:430](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java#L430), 443 | Production rejection and repeated input symbols; retained exact `.qtp` bytes. |
| Timestamped cursors derive milliseconds and cause timestamp changes within a batch. | [FileWriterImpl:629](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L629), 644/646 | Three RAW frames for timestamped batch; one for regular heartbeat input. |
| FIELD activates built-in time fields; footer is an empty heartbeat. | [FileWriterImpl:254](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L254), 482/485 | FIELD scan prepends EventTime56/EventSequence72; each nonempty captured file has a zero-length terminal frame. |
| Close logs footer I/O errors and uses a best-effort close helper. | [FileWriterImpl:411](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L411), 441/444 | Source fact only; no injected storage failure or durability claim. |

## Explicit boundaries

Full24/full137 remain negative C++ admission witnesses even though production Java accepts them.
NONE/FIELD timing, STREAM output and Java's wider conversion behavior do not expand the proposed
RAW/MESSAGE subset. Outgoing descriptors are explicit wire facts; native JSON lookup keys do not
establish canonical outgoing names. Existing omission/repeated-target diagnostic evidence remains
in the earlier field/event corpora; this task does not recreate or relabel it as production writing.

There is no new event adapter, field identity, production C++ code, compression, transport,
connector session, private tape, disk fault injection or publication guarantee. Successful
FileWriter.close alone is insufficient evidence of output completion; these captures additionally
parse the completed bytes. Finite successful examples do not prove all metadata combinations or
future strict rewrite admission. The [validation report](../../../../tests/conformance/protocol/tape-write/VALIDATION.md)
separates actual positive/negative checks from those limits.
