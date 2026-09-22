# Java QD tape files: evidence and a bounded next profile

[Русская версия](TAPE-FILES.ru.md). English is authoritative.

QDCXX-019 is research against QD 3.355, commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` in [upstream.lock](../../upstream.lock).
This document does not extend the [current inspector contract](OFFLINE-INSPECTION.md).
“Measured” below means the new FileWriter/FileReader runs; “source” means inspected code, not
an executed upstream test. The future profile is a proposal requiring owner approval.

## Sources and production paths

All source links pin the same commit; no upstream source was modified or copied into the harness.

| Reference | Relevant source / contract |
| --- | --- |
| F1 | [FileFormat](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileFormat.java): `valueOf`, `detectFormat`, `createQTPComposer`, `createQTPParser`, `getTimestampsType`, `isBareBones` |
| F2 | [FileWriter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriter.java), [FileWriterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java): constructor, `open`, `visitData`, `updateCurTime`, `writeData`, `writeHeader`, `writeFooter`, `close` |
| F3 | [FileWriterParams](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterParams.java), [FileReaderParams](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReaderParams.java): direct API defaults |
| F4 | [FileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java): `readInto`, `openFiles`, `process`, `createParserOnFirstChunk`, `configureParser`, `processHeartbeatMessage`, `processRecordSourceMessage`, `doNotWait` |
| F5 | [BinaryFileQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/BinaryFileQTPParser.java): optional `schemeKnown` and heuristic `resyncOn` |
| F6 | [TimestampsType](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TimestampsType.java), [TimestampedPosition](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TimestampedPosition.java), [FileUtils](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileUtils.java): time modes and sidecar naming |
| F7 | [StreamCompression](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/StreamCompression.java): NONE/GZIP/ZIP, extensions and header detection |
| F8 | [ProtocolDescriptor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java), [ProtocolOption](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolOption.java), [QDFactory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDFactory.java): tape type/version/options; null property values become empty strings |
| F9 | [AbstractQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPComposer.java), [BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java): automatic descriptions and supported flag mask |
| F10 | [TextDelimiters](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/text/TextDelimiters.java), [BlobQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/blob/BlobQTPComposer.java), [BlobQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/blob/BlobQTPParser.java) |
| F11 | [TapeConnector](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TapeConnector.java), [FileWriterHandler](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterHandler.java): connector call path and RAW_DATA default |
| F12 | [FileWriterTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/FileWriterTest.java), [TapeConnectorFlagsTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/TapeConnectorFlagsTest.java), [FileConnectorCorruptedTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/FileConnectorCorruptedTest.java): candidate broader tests, inspected only |

The harness calls `FileWriter.open → FileWriterImpl.open`, then `visitHeartbeat/visitData →
composer → writeData → ParallelWriter`, and `close` before replay. It does not compose raw QTP
directly. Replay uses `FileReader.setScheme/readInto → openFiles → createParserOnFirstChunk →
FileFormat.createQTPParser → BinaryFileQTPParser/TextQTPParser → FileReader.Consumer → observer`.
There is no parser subclass, reflection, injected clock, replaced version string or connector.
The separate envelope scanner uses `ByteArrayInput.readCompactInt`; it proves framing only.

## Source inventory and exact defaults

| Surface | Locked source behavior | Executed scope |
| --- | --- | --- |
| BINARY | Default writer format regardless of ordinary filename extension; QTP envelopes, automatic record descriptions, protocol header and empty-heartbeat footer. Default time MESSAGE. | Default and constrained uncompressed files |
| TEXT | Tab-separated text, `==` message prefix / `=` record descriptions. Default time FIELD. | One file with default time/options |
| CSV | Comma-separated Text composer/parser with `#=` descriptions; bare bones, no protocol header/footer; parser assumes STREAM_DATA. Default time FIELD. | Source only |
| BLOB | Explicit `blob:<record>:<symbol>`, fixed local record/symbol, serialized fields without QTP envelopes; composer accepts HISTORY_DATA/RAW_DATA; parser delivers HISTORY_DATA. Bare bones, default time NONE. | Source only |
| Format selection | Explicit writer `format` or BINARY. Reader explicit format or first-buffer detection: TEXT prefix first, CSV prefix next, otherwise BINARY. BLOB needs explicit configuration. Detection is not general validation. | Auto-read BINARY/TEXT |
| Compression | Writer explicit compression or filename `.gz`/`.zip`, otherwise NONE. Reader explicit/extension choice, then header detection if unresolved; GZIP `1f8b`, ZIP `504b0304`. | NONE only; no compressed captures |
| Time NONE | No timestamp storage. Header/footer remain for non-bare-bones formats. | Constrained file |
| Time LONG / TEXT | Separate `.time` file, time-millis or formatted timestamp followed by `:` and byte position. Naming replaces the data extension while retaining container extension. Reader uses sidecar positions to delimit input. | Source only |
| Time FIELD | EventTime/EventSequence metadata fields; ordinary application fields remain distinct. | TEXT EventTime, zero sequence |
| Time MESSAGE | Heartbeat timestamp per data block; reader translates it into event-time sequence on delivered cursors. | Default BINARY, three times |
| Descriptor | `type=tape`, `version=QDFactory.getVersion()`, configured `opt`; `time` if not NONE. `saveAs` adds send type. `noHeaderAndFooter` system property defaults false. | Both binary headers retained, never stripped |
| Flags/options | Supported set is `hs` absent property override. Binary composer masks flags by the current message/options; `opt=""` is EMPTY_SET. `hs` availability does not mean every record has nonzero flags. | Zero input/output flags only |
| Description placement | First use can trigger a description immediately before data using that record, even after earlier data. | S, then R, then E, interleaved with data |

Direct `FileWriterParams.Default`: split/compression/format/time/saveAs/tmpDir null,
storageTime unlimited, storageSize `Long.MAX_VALUE`, opt `SUPPORTED_SET.toString()` (`hs` here).
Resolved defaults for our `.qtp` name: BINARY/NONE compression/MESSAGE; no splitting or retention.
`TapeConnector` differs: `saveAs=RAW_DATA` by default (F11). These captures are direct FileWriter
STREAM_DATA evidence, not default connector or general dxFeed event evidence.

Direct `FileReaderParams.Default`: empty user/password; start/stop/delay `FileConnector.NA_TIME`;
compression/format/time/readAs/resyncOn/fieldReplacer null; ignoreTime/cycle/schemeKnown false;
speed 1; stats `QDStats.VOID`. Our only overrides are speed `MAX_SPEED=Double.MAX_VALUE` and
explicit cycle=false; the local scheme is supplied with `setScheme`. No replay delays, follow,
field replacers, resync or preinstalled sender schema are enabled. Single literal filenames make
`doNotWait()` true. Multiple timestamped-file following is source-inventoried, not measured.

## Measured files and replay

[Coverage](../../tests/fixtures/protocol/tape/COVERAGE.md),
[validation](../../tests/fixtures/protocol/tape/VALIDATION.md) and
[inputs](../../tests/fixtures/protocol/tape/inputs.tsv) identify the artifacts and reproduction.
The three writes consume the same seven rows / three messages. Sender IDs/layouts are
R=0(x,y,z), S=1(v), E=2(); local IDs/layouts are S=0(v), R=1(y,x,z), E=2(). All fields are ordinary
type 8 and hasTime=false. Symbols include repeated ABC, U+2603 and empty; values include both int32
extremes. Incoming heartbeats carry `1700000000123`, `1700000001123`, `1700000002123` ms.
FLAGGED_DATA cursors carry zero flags and no input event-time sequence.

| File | Writer settings | Raw size | Real Java result | Envelope types / inspector result |
| --- | --- | --- | --- | --- |
| default.qtp | No setters | 165 bytes | 7 records, 3 data callbacks, 3 timestamp heartbeats, 0 errors | `1,0,2,15,0,2,15,0,2,15,empty`; inspector exit 4 at type 1 |
| constrained.qtp | BINARY, compression NONE, time NONE, opt empty, saveAs STREAM_DATA | 137 bytes | 7 records, 1 data callback, no heartbeat callback, 0 errors | `1,2,15,2,15,2,15,empty`; inspector exit 4 at type 1 |
| text.txt | format TEXT; other writer defaults | 432 bytes | 7 records, 1 data callback, empty heartbeat callback, 0 errors | No binary scan; inspector exit 4 after interpreting initial text as type 61 |
| truncated.qtp | Diagnostic copy of constrained.qtp without last data byte and footer | 135 bytes | 4 records, 1 data callback, 1 corrupted-stream callback, readInto returned | Inspector still exits 4 at leading type 1; it does not reach truncation |

Default BINARY and TEXT deliver the same ordered local identities, symbol units, local-order values,
zero flags and event-time sequences. The sequences are `7301444403715899392`,
`7301444408010866688`, `7301444412305833984`; constrained/truncated records have sequence 0.
Callback grouping differs and is not a record boundary contract. Binary empty footer is visible
in raw bytes and envelope rows but did not produce an observer heartbeat callback; TEXT footer did.
Protocol observations include sorted properties, send/receive sets and null endpoint identity:
default/TEXT send sets empty, constrained/truncated send set STREAM_DATA; receive sets empty.
The class-directory runtime reports `version=QDS-UNKNOWN`, preserved verbatim. Upstream identity
comes from commit/tag/POM/clean-tree checks, not this runtime string; no fake version was injected.

Two clean runs have identical raw tape bytes and every one of the 22 compared artifacts, without
normalization. Logs and metadata retain different run paths and UTC execution times. No sidecars
were produced. Text file CRLF bytes are retained; observation TSV and inspector output use LF.
Java EOF probe completes within a 30-second process limit and logs 13 unparsed bytes. This is a
partial-frame EOF error, not a timeout or normal empty EOF. Complete files return without error.
No general malformed-input recovery, file-follow or resource safety claim follows from this probe.

## Current inspector gap table

| Area | Finding and gap | Evidence class |
| --- | --- | --- |
| File/container decoding | Selected BINARY bytes are directly QTP envelopes; not all tape needs an extra binary container. TEXT/CSV/BLOB and compression are separate formats absent from inspector. | Measured BINARY/TEXT; source other formats |
| Message kinds | DESCRIBE_PROTOCOL type 1 rejects both complete binary samples immediately. RAW_DATA connector output is a further excluded kind. | Measured first rejection; source connector |
| Timing | Nonempty type-0 heartbeat and FIELD/sidecar time are outside inspector. First rejection masks later gaps; none were bypassed. | Measured default heartbeat bytes; inferred inspector gap; source sidecars |
| Flags | Samples have zero flags; default `hs` does not force nonzero flags. General flag handling is unsupported and unmeasured here. | Measured zeros; source mask; existing contract |
| Schema | Both sides need an explicit local scheme; wire IDs remap, R fields reorder. No XML/JSON importer or dxFeed schema is implied. | Measured FileReader deliveries |
| Types/conversions | Only ordinary type 8; TEXT adds event time metadata. Wider wire types and conversions need separate evidence. | Measured selected fields; inferred broader gap |
| Description order | Writer's late R/E descriptions violate inspector's initial-description-only policy even in constrained sample. Current rejection remains the earlier type 1. | Measured sequence; inferred subsequent failure from contract |
| Resource/error policy | Inspector has finite budgets/terminal errors. Java single-file EOF reports corruption after a delivered prefix; optional heuristic resync/follow are separate behaviors. No equivalent Java aggregate budget was tested. | Measured truncation; source resync/follow; existing contract |

## Recommended next task, not implementation approval

Recommend one **explicit uncompressed BINARY, time NONE, EMPTY_SET, STREAM_DATA** file-reader
profile using the exact constrained writer configuration above. Retain protocol header and footer;
do not preprocess bytes. Require a caller-provided immutable ordinary type-8 local schema,
hasTime=false, zero flags and finite file/frame/description/record/field/symbol/output budgets.
Accept one initial bounded `type=tape` protocol descriptor with empty opt and absent/NONE time,
then additive first-use record descriptions and STREAM_DATA, and empty heartbeat/footer.
Permit an ID's first declaration after data, but reject any redeclaration, reset or replacement.
That is a bounded append-only mapping proposal; it does not authorize general dynamic sessions.

Before implementation, a scoped task must settle an ADR for descriptor acceptance (including
unknown properties, version-as-diagnostic, send sets and time absence), append-only binding and
immutable plan lifetimes, and EOF/completed-prefix policy versus Java batching. Add focused Java
evidence for descriptor variations, duplicate/redefined IDs, missing descriptions, unknown records,
truncation at header/description/data boundaries and complete malformed frames. Specify exact
limits, unsupported exits and empty-file/footer policy before C++ changes. Reuse this raw constrained
sample as positive conformance input; default/TEXT remain explicit unsupported cases.

Exclude compression, sidecars, MESSAGE/FIELD timing, follow/cycle/split/retention, network/transport,
writer, RAW_DATA/TICKER/HISTORY/subscriptions, `hs`/nonzero flags, resync/schemeKnown, description
replacement, converters, importer and real dxFeed event schemas. Runtime dependencies and general
tape support remain unapproved. No C++ or inspector source/build/test change is part of QDCXX-019.

## Remaining unknowns

Compressed/sidecar byte parity, CSV/BLOB replay, nonzero flag/time-sequence combinations, packaged
JAR version metadata, connector defaults in execution, follow/recovery and resource extremes are
not measured here. Source tests in F12 are leads, not independent execution evidence. See the
[open questions](../../docs/upstream/OPEN-QUESTIONS.md) for the decision/evidence gates.
