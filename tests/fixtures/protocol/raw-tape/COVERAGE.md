# RAW tape Java coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The 94 bounded cases are observations of locked QD production FileReader; six original files are
produced through FileWriter or the actual TapeConnector path. The other 88 are explicitly identified
diagnostic wire inputs, never represented as unmodified producer output. Each case retains its input,
complete callback/value/timing/error TSV, and supplemental envelope TSV: 282 files, plus settings
and independent packing observations, total 284. No private bytes, symbols or values are included.

| Cases | Measured scope |
| --- | --- |
| raw-quote, raw-synthetic, raw-hs, stream-synthetic | FileWriter BINARY/NONE compression/MESSAGE, exact Quote or S/R/E, zero flags; real RAW and STREAM |
| connector-quote, connector-default | DXEndpoint publisher → TapeConnector → FileWriterHandler, explicit empty or default hs; unchanged full Quote delegate and supplied eventTime |
| opt-* | Absent, empty, null, hs, unknown and hs,unknown in descriptor, effective options and real RAW delivery together |
| matrix-* (24) | Descriptor time absent/NONE/MESSAGE/FIELD × send list empty/RAW/STREAM × actual RAW/STREAM |
| timing-sequence | Data before timestamp; zero; repeated/decreasing timestamp; consecutive heartbeats; state through late description and multiple messages; EOF after heartbeat |
| boundary-* (14), packing.tsv | Millisecond boundaries, negative floor division, int32 seconds saturation, both int64 endpoints and exact packed sequence |
| empty-envelope, explicit-zero-empty, heartbeat-absent | Marker versus explicit type zero with no payload versus bits 0, presence preserved |
| heartbeat-fields, heartbeat-marks-no-time | Timestamp and all known optional fields, signed-int boundaries, fields without timestamp |
| heartbeat-unknown-bit, heartbeat-negative-bits, heartbeat-extension | Unknown/negative bits and trailing extension: Java permissiveness observed, bounded policy refuses |
| heartbeat-truncated-0..20 | Every strict prefix of the full bits-15 heartbeat, covering each compact primitive; empty prefix is legal empty payload |
| timestamp-truncated-1..9 | Every nonempty strict prefix of bits-1 plus maximum int64 timestamp, including content-only |
| heartbeat-preserves-time | Bits 0 does not erase prior timestamp/sequence |
| mixed-raw-stream, unsupported-message | Actual dispatch, Java callbacks/unknown callback and continuing delivery |
| nonzero-flags, nonzero-flags-empty-opt | Real parser delivers flag 1 even with empty opt; C++ rejects before record fields |

RAW type 5 fans out to callbacks 10/15/20; the record projection for C++ comparisons is callback 15.
All delivered raw fields, exact symbols/flags and event-time sequences are recorded. Quote rows also
record business bid/ask timestamps, exchange codes, exact mapped binary64 bits, mapped eventTime,
sequence and nano part. Original inputs are separate: replay timestamps 1600000000123 + i*1000;
business times 1700000000000 + i*2000 / ask +1000; sizes 10.5+i / 12.25+i. No schema is simplified.

The harness does not implement a decoder to replace the parser oracle. Envelope scanning proves only
framing; `ProtocolOption.parseProtocolOptions` and `TimeSequenceUtil` supplementary calls expose
upstream public transformations alongside actual record deliveries. Java's permissive matrix is not
the bounded acceptance matrix; [RAW-TAPE](../../../../spec/protocol/RAW-TAPE.md) states the stricter
policy and terminal/atomic differences. C++ implementation, resource policy and CLI require separate
checks; this report makes no claim about their results or general/private tape compatibility.
