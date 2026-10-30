# Bounded RAW_DATA / MESSAGE tape policy

[Русская версия](RAW-TAPE.ru.md). English is authoritative.

This QDCXX-023 contract extends [TAPE-READER](TAPE-READER.md) only through explicit
`raw-message-v1`. The local schema selection is independent: synthetic S/R/E exact type 8 or
unchanged default composite Quote exact full types 2/8/56/120. No other type, event adapter,
nonzero event flags, compression, sidecar, FIELD timing, transport or replay scheduler is added.

## Production evidence and projection

The locked QD in upstream.lock is the oracle. [Harness](../../tools/upstream/raw-tape-oracle/RawTapeOracle.java)
uses unchanged `FileWriter` and `FileReader`, and separately real
`DXEndpoint(PUBLISHER) → TapeConnector → FileWriterHandler → FileWriterImpl` with supplied
Quote event times. `DXFeedScheme` and `QuoteDelegate.putEvent/createEvent` remain unchanged,
including the complete default Quote fields and fractional sizes. Explicit empty options and the
connector's default `hs` both produce actual type-5 RAW_DATA files. FileWriter STREAM control
produces type 15. The wire files include original descriptors, descriptions, heartbeats and markers.

`FileReader` delivers each RAW wire record to TICKER_DATA (10), STREAM_DATA (15) and HISTORY_DATA
(20), in that order within each flushed block. This is measured production-parser behavior, not
an invented RAW callback. C++ inspection emits one wire record and compares it against the Java
STREAM_DATA projection; it does not implement ticker/history delivery or triple its output.
Callback grouping is not a wire record boundary contract. Supplemental `*-frames.tsv` only scans
envelopes. Diagnostic probes explicitly identify mutated/constructed input as `diagnostic-wire`.

## Descriptor and dispatch matrix

The old `legacy` mode keeps its existing contract and output. For the new mode all other existing
descriptor syntax, duplicate/null/unknown property, count, receive-list and extension checks remain.
Version is diagnostic text only; it supplies no option or producer inference.

| C++ mode | Descriptor time | Send list | Actual data | opt | Policy |
| --- | --- | --- | --- | --- | --- |
| legacy | absent/NONE | empty or singleton STREAM_DATA | STREAM_DATA | present empty | Existing acceptance |
| raw-message-v1 | MESSAGE or message | empty or singleton RAW_DATA (5), matching name, no properties | RAW_DATA (5) | absent, empty, or exact hs | Accept zero flags and supported exact fields |
| raw-message-v1 | absent/NONE/FIELD/other | any | any | any | Reject descriptor |
| raw-message-v1 | MESSAGE/message | STREAM_DATA, mixed or other | any | any | Reject descriptor |
| raw-message-v1 | MESSAGE/message | allowed | STREAM_DATA/mixed/other | allowed | Reject actual message at dispatch |
| raw-message-v1 | MESSAGE/message | allowed | RAW_DATA | explicit null/unknown/hs,unknown/other | Reject descriptor |

Production descriptors spell time `message`; diagnostic probes also measure `MESSAGE`.
The full Java matrix independently crosses absent/NONE/MESSAGE/FIELD, empty/RAW/STREAM send sets,
and RAW/STREAM actual messages: Java accepts these cases, including mismatch. The bounded policy
is deliberately narrower and never infers policy from the actual message or header.

Absent opt has no property pair; explicit null has a pair with null UTFString; empty has a pair
with empty UTFString. They are not structurally interchangeable. Production FileReader observations
plus `ProtocolOption.parseProtocolOptions` show effective EMPTY_SET for absent/null/empty and
unknown-only, and hs for hs or hs,unknown. Only absent/empty/exact hs are accepted by this policy.
Java delivers nonzero RAW flags even with empty opt in the diagnostic probe. C++ refuses any
nonzero flags before decoding that record's fields; hs acceptance does not authorize flags.

## Heartbeat grammar and timing

An empty envelope has no message ID. An explicit type-0 frame has a payload: zero bytes are
permitted, or compact-int content bits followed by fields in bit order. Java `HeartbeatPayload`
defines bit 1: compact-long signed millisecond timestamp; bits 2/4/8: compact-int timeMark,
deltaMark, lagMark. Unknown bits and extra bytes are accepted/ignored by Java in measured cases;
this policy refuses them rather than discarding unrepresented information.

The accepted subset is empty type-0 payload, content bits 0 with no further bytes, or bits 1 with
one complete compact-long timestamp and no further bytes. Bits 2/4/8, unknown/negative bits and
trailing extensions are unsupported. A missing required primitive is malformed. Compact primitive
rules, noncanonical encodings and signed projections follow the existing encoding contracts.
Empty payload is distinguishable from explicit bits 0. Java emits no callback for empty envelope
or empty type-0 payload, but emits an empty callback for bits 0. Inspection preserves these wire
distinctions and each accepted heartbeat, including one at EOF without following records.

Initial timing is absent timestamp and event-time sequence 0. A complete bits-1 heartbeat atomically
replaces timestamp and sequence. Explicit zero is present, distinguishable from absence despite
sequence 0. Repeated/decreasing timestamps replace state exactly. Consecutive heartbeats are retained;
state persists over descriptions and multiple data frames, and empty heartbeat forms leave it intact.
No state crosses reader instances. A result's records share that data frame's owned timing value.

Timestamp is the exact signed int64 raw milliseconds. Packed Java cursor event-time sequence is:
`seconds = clamp(floor(timestamp / 1000), INT32_MIN, INT32_MAX)`;
`millis = floorMod(timestamp, 1000)`; packed bits are signed seconds in bits 63..32 and millis in
bits 31..22, with low 22 bits zero. Saturation, not wrapping, applies outside the seconds range.
Both int64 extremes are observed. Keep raw timestamp and packed sequence separately: saturation
can lose the original timestamp. Negative floor division/remainder and bit construction must avoid
C++ signed overflow and negative left shift. `TimeSequenceUtil` and `TimeUtil` are authoritative
locked implementations; `packing.tsv` captures their boundary values.

Replay timestamp, cursor sequence and Quote business bid/ask time are distinct. `QuoteDelegate`
produces mapped eventTime from the cursor sequence; at normal range it equals replay milliseconds,
but at saturation it may differ. The unchanged `mapQuote` third argument remains a symbol-copy
budget. Inspection exposes measured timing separately from the old untimed Quote mapping.

## Atomicity, limits, positions and refusal precedence

Keep TAPE-READER ownership, append-only immutable descriptions, cumulative reconstruction charges,
completed data prefixes, offsets and terminal errors. Charge frames including markers and heartbeat-only
streams; bound emitted heartbeat metadata cumulatively. Timing is fixed-size owning state; it must not
accumulate hidden unbounded history. Caller-owned returned events remain caller responsibility.

Framing/input/frame-budget errors precede dispatch. Header sequence and descriptor policy precede
message handling. At a heartbeat, parse the content primitive before validating its bits; for accepted
bits parse the required timestamp before trailing-byte policy. Publish timing only after full accepted
payload and resource checks. Primitive failure identifies primitive start; unsupported bits identify the
bits checkpoint; extension refusal identifies the first trailing byte. No failed heartbeat partially
updates state. Descriptor/description/data errors preserve existing checkpoints and completed prefix.
Java may report malformed complete heartbeat then continue; C++ remains terminal. Clean EOF requires
an accepted header and complete framing, never a footer marker; it does not authenticate recording
completeness. EOF after a valid heartbeat succeeds and preserves that event.

## Owned API and metadata charges

`TapeReader(schema, limits, TapeMode::RAW_MESSAGE_V1)` opts in; the default `TapeMode::LEGACY`
preserves the previous behavior. `TapeDescriptor::options` and `time` retain the exact accepted
optional strings, and `explicitRawSend` records the explicit singleton RAW send set. The old
descriptor fields remain available. No local schema is inferred.

`TapeTiming` owns optional `timestampMs` and signed `eventTimeSequence`; `eventTimeMs()` unpacks
the delivered cursor time from the sequence. `TapeHeartbeat` owns optional `content` and
`timestampMs`. Absent content means an explicit type-zero frame with no payload; present zero is
the empty content primitive. `TapeFeedResult::heartbeat` retains each accepted explicit heartbeat.
`TapeFeedResult::timing` is present for each RAW data frame, including a completed prefix followed
by a data error. Every record in that result has the same timing. Empty envelope markers remain
represented by the existing frame and marker count. `TapeReader::timing()` exposes the current
fixed-size state; copy it to retain it beyond reader lifetime.

`TapeLimits::timingEvents` defaults to zero and is a real finite cumulative metadata bound. Before
examining an explicit heartbeat or RAW data payload, check and charge one timing event. A failed
budget check precedes payload syntax and publishes no metadata. A charged later-failing attempt
counts, like record/field attempts; `timingEvents()` exposes the total. Accepted data with zero
records also counts. Fixed current state does not accumulate history. All legacy traffic ignores
this new budget. Initial constructor rejects an invalid TapeMode before consuming input.

## Timed inspection grammar, version 3

The tool's explicit `--tape-mode raw-message-v1` selects version 3 for either existing local profile.
Omitted mode and `--tape-mode legacy` preserve previous output and refusal behavior. Diagnostic
versions 1/2 and their existing profile defaults remain independent. `--max-timing-events` sets
the new cumulative bound, with CLI default 4096. All existing IO/output bounds still apply.

TSV uses LF, no BOM, canonical decimal, lowercase UTF-16 hex and `-` for absent/empty cells as
identified below. Rows are in wire order:

- Header: `format`, `qdcxx-tape-inspect`, `3`, local profile, `raw-message-v1`.
- Existing `frame` rows retain their exact grammar and actual type ID, including 5 and 0.
- `descriptor-v3`, version-present (0/1), version hex or `-`, exact time text (`MESSAGE` or
  `message`), send (`5` or `-`), receive (`-`), options (`absent`, `empty`, or `hs`).
- `heartbeat`, frame index, content (`-`, `0`, or `1`), timestamp milliseconds or `-`.
- `timing`, frame index, last timestamp milliseconds or `-`, signed packed event-time sequence,
  delivered event-time milliseconds. This row precedes records of each RAW data frame, including
  empty data frames and retained prefixes of failed data frames.
- Existing `record` or `typed-record` rows keep their local identity/value grammar. For Quote,
  the following `quote` row keeps the version-2 cell order but its final `eventTime` cell is the
  delivered time from that frame's timing, matching Java QuoteDelegate. `mapQuote` itself remains
  an untimed field mapper. Bid/ask time, sequence and nano fields keep their existing meanings.
- Existing `markers` and `complete` rows retain their grammar. Success still requires exit 0
  after the final flush; no completion row follows a detected error.

Human header is `qdcxx-tape-inspect human 3 profile=<profile> tape-mode=raw-message-v1 identities=local`.
The descriptor uses `Descriptor: type=tape opt=<absent|empty|hs> version=<quoted text|absent>
time=<MESSAGE|message> send=<5|-> receive=-` on one line. Heartbeat line:
`Heartbeat: frame=N content=<-|0|1> timestampMs=<signed milliseconds|->`.
Data timing line: `Timing: frame=N timestampMs=<signed milliseconds|-> eventTimeSequence=S eventTimeMs=M`.
Existing safely escaped record lines follow; Quote's `eventTime` is the delivered value. RAW frames
are named `RAW_DATA (5)` and explicit type-zero frames `HEARTBEAT (0)`; empty markers remain distinct.
All rows are counted against encoded-byte limits before dynamic expansion, including version text
and every timing-only row. Presentation remains outside protocol/core and never waits for replay.

This contract does not claim full support of any private tape; remaining structural gaps require the
separate bounded inventory and cannot be inferred from successful generated fixtures.

## Explicit additional field profile

[FIELD-TYPES](../encoding/FIELD-TYPES.md) defines EXTENDED_FIELDS_V1 with exact 2/8/10/40/56/72/88/120,
owned nullable UTF-16, logical field positions independent of Java slots, and cumulative string work/ownership bounds.
Both existing tape modes keep their descriptor, flags, timing, terminal-error and append-only policies.
Earlier type exclusions below describe the older profiles; only this explicit profile extends them.
No Profile event adapter, indexed schema, importer or implicit discovery is added.

## Catalog flag evidence and unchanged RAW gate

The [flags production corpus](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md)
characterizes all 64 known subsets and composer/parser option differences in RAW5, TICKER 10,
STREAM 15 and HISTORY20. RAW callback fanout is labelled and compared before selecting one STREAM
wire projection. These observations do not extend RAW_MESSAGE_V1 or `decodeRawData`: both still
refuse nonzero data flags. The new `decodeStreamDataWithEventPolicy` and BINARY_CATALOG_V1 enable
only STREAM/NONE known flags and owned removal time under their explicit contract.
Broader family dispatch and versioned output remain separate work; old serialized output is unchanged.


