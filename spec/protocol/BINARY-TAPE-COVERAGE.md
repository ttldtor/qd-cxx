# Finite generic binary tape coverage

[Русская версия](BINARY-TAPE-COVERAGE.ru.md). English is authoritative.

Status: finite field/session/dispatch implementation and optional tool container component; sidecar/chain/operational integration and full-catalog acceptance remain pending.
The [decision](../../docs/decisions/ADR-0004-broad-binary-tape-reading.md),
[71-row support matrix](../../tests/fixtures/protocol/real-tape/support-matrix.tsv) and
[verification contracts](../../tests/conformance/protocol/real-tape/CONTRACTS.md) define the same target.
A source-derived row or planned test is not a passed compatibility test.

## Target and evidence

The finite reference set is Java QD **3.355** at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and **3.356** at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. The first remains the lock baseline;
selecting the second does not change `upstream.lock` or claim later-version compatibility.
Each matrix row links immutable production sources at both commits and identifies actual evidence
separately from required positive/negative observations.

The [producer fixtures](../../tests/fixtures/protocol/real-tape/producer/README.md) and
[producer validation](../../tests/conformance/protocol/real-tape/producer/VALIDATION.md)
establish three named positive Java FileWriter/FileReader cases and a derived truncation per pin.
They cover compact/Quote, NONE/MESSAGE, zero flags and ordinary STREAM_DATA. They do not establish
the new types, indexed layouts, nonzero flags, headerless files or other families below.
The producing harness is Windows-qualified; portable fixtures have no Windows runtime dependency.
C++ consumption and its Windows/Linux verification remain separate acceptance requirements.

Generic output preserves source sender identity, field order/full type, symbol, raw scalar bits,
exact UTF-16 units or opaque nullable bytes, data flags and the explicitly supported timing metadata.
This is a canonical value projection, not preservation of every redundant byte encoding.
Java object execution, business conversions, event adapters, ticker/history collectors, subscriptions,
RMI operations and transport are outside this target. Private recording contents are not public fixtures.

## Profile boundaries

| Existing selection | Exact existing field policy | Broader behavior |
| --- | --- | --- |
| `COMPACT_INT_V1` | 8 | Unchanged |
| `QUOTE_FIELDS_V1` | 2/8/56/120 | Unchanged |
| `EXTENDED_FIELDS_V1` | 2/8/10/40/56/72/88/120; ordinary records | Unchanged |
| `TapeMode::LEGACY` | Existing descriptor/STREAM/zero-flag policy | Unchanged |
| `TapeMode::RAW_MESSAGE_V1` | Existing RAW/MESSAGE policy, selected local profile, zero data flags | Unchanged |
| Ordinary wire discovery | Existing exact ordinary types, append-only IDs and strict repeated-sender refusal | Unchanged |
| `BINARY_FIELDS_V1` | 19 exact read-only identities; ordinary and explicit indexed local records | Bounded fields/indexed subset implemented |
| `TapeMode::BINARY_CATALOG_V1` | Four generic families, NONE/MESSAGE/headerless, known descriptors/options/flags/removal and immutable generations | Finite opt-in implemented; FIELD/containers/chains remain separate |
| `FieldTimePolicy::READ` | Exact raw leading EventTime/full56 and optional EventSequence/full72 with separate physical REMOVE prefix | Explicit binary-catalog selection; old signatures remain DISABLED; see TAPE-TIME |

The core field profile and separate protocol mode are explicit selections. The current
[indexed contract](../schema/INDEXED-SCHEMAS.md) and [message policy](BINARY-MESSAGES.md) define the finite subset. No existing constructor/default
starts broader discovery, accepts indexed layouts or enables new container semantics implicitly.
The initial broader reader can use an explicitly supplied validated local schema. Any new broader
discovery entry must be explicit and cannot reconstruct `hasTime` from record names.
Core remains independent of file I/O, transport, configuration sources and protocol mode selection.

## Exact field catalog

The serialization mask is `~0xf0`, not `0x0f`; representation selectors are alternatives.
Shared serialization bits never authorize an unlisted full identity. Named aliases such as TIME,
PRICE and MMID add no new identity. The source is
[SerialFieldType at 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L60-L83),
[SerialFieldType at 3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L60-L83).

| Full ID | Name | Physical read / logical raw value | Current bounded support; new work |
| --- | --- | --- | --- |
| 0 | VOID | No serialized value; explicit wire field may project raw zero | Implemented scalar subset |
| 1 | BYTE | Signed 8-bit | Implemented scalar subset |
| 2 | UTF_CHAR | Existing UTFChar code point contract | Existing exact profiles |
| 3 | SHORT | Signed 16-bit, big endian | Implemented scalar subset |
| 4 | INT | Signed 32-bit, big endian | Implemented scalar subset |
| 8 | COMPACT_INT | Compact long, then signed low 32 bits | Existing exact profiles |
| 9 | BYTE_ARRAY | Nullable byte count and opaque bytes | Implemented owned-payload subset |
| 10 | UTF_CHAR_ARRAY | Nullable character count and exact UTF-16 units | Existing extended profile |
| 24 | DECIMAL | Compact long, then signed low 32 raw bits | Implemented scalar subset |
| 40 | SHORT_STRING | Compact long, then signed low 32 raw bits | Existing extended profile |
| 56 | TIME_SECONDS | Compact long, then signed low 32 seconds | Existing Quote/extended profiles |
| 72 | SEQUENCE | Compact long, then signed low 32 raw bits | Existing extended profile |
| 88 | DATE | Compact long, then signed low 32 day-ID bits | Existing extended profile |
| 104 | LONG | Compact signed 64-bit | Implemented scalar subset |
| 120 | WIDE_DECIMAL | Compact signed 64 raw bits | Existing Quote/extended profiles |
| 137 | STRING | UTFString byte count and exact nullable UTF-16 units | Implemented owned-payload subset |
| 152 | TIME_MILLIS | Compact signed 64 milliseconds | Implemented scalar subset |
| 168 | TIME_NANOS | Reserved upstream | Unsupported; no implementation candidate |
| 233 | CUSTOM_OBJECT | Nullable opaque bytes | Implemented owned-payload subset |
| 249 | SERIAL_OBJECT | Nullable marshalled serialization bytes | Implemented owned-payload subset |

The broader field target contains 19 candidates. All other full IDs are outside the selected
profile, including otherwise readable Java representation/base combinations. Negative or greater
than 255 wire descriptor type IDs violate the production descriptor grammar. A valid Java input
outside the profile receives an unsupported diagnostic, not a claim that Java considers it malformed.

Physical decoding follows production
[BinaryRecordDesc at 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L434),
[at 3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L434).
Signed fixed widths, low-32 projection and two-slot long storage are required physical rules.
Decimal/int/wide/long/shares/time-unit conversion to a different local type is excluded.
BYTE/SHORT have no default field-factory instance; their production probes must exercise an explicit
requested wire description and a suitable production consumer, rather than invent a default schema.
Physical VOID tails used for Java long storage never become extra logical values. Only an actual
explicit wire VOID field may receive a raw-zero generic value.

## Payload ownership and grammar

`FieldValue` gains a trailing nullable owned byte payload. IDs 9/233/249 use raw zero, absent text
units, and distinct null/empty/content bytes. STRING137 reuses nullable exact UTF-16 units with raw
zero and absent bytes. Scalar tags have no text/byte payload. Existing aggregate member meanings
and all old-profile value invariants remain unchanged.

There are four different length grammars:

| Value | Production length rule | Required distinction |
| --- | --- | --- |
| BYTE_ARRAY9 / default CUSTOM_OBJECT233 | Compact **long**, validated in -1..INT_MAX | Reject full out-of-range length before proportional allocation |
| SERIAL_OBJECT249 through MarshalledObjField | Compact **int** projection, then -1..INT_MAX validation | High/overlong compact forms can project differently from byte-array lengths |
| STRING137 | UTFString encoded-byte count | Preserve accepted UTF/CESU/overlong behavior and exact UTF-16 units |
| UTF_CHAR_ARRAY 10 | Character count followed by BMP UTFChar units | Preserve existing primitive/message boundary distinction |

The marshalled length rule is explicit in
[Marshaller at 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/Marshaller.java#L162-L175),
[at 3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/dxlib/src/main/java/com/devexperts/io/Marshaller.java#L162-L175).
The generic reader retains marshalled bytes and never calls Java object deserialization.
Unknown class/tag contents remain opaque; malformed framing/length is still an error.
Legacy PlainObjField execution is outside this target. A local Java object-field class selects the
consumer decoder, so a wire serialization base alone is insufficient evidence of object semantics.

New finite limits distinguish per-value encoded/payload bytes, cumulative examined encoded bytes,
cumulative admitted payload work, and completed/current/temporary retained payload bytes.
Existing string counters keep their meaning. Zero is a zero allowance. Admission precedes every
proportional allocation/copy; skip, replacement, failure and cancellation do not refund admitted work.
Published values own their contents and survive source, reader and schema destruction.

## Indexed layout and description generations

DESCRIBE_RECORDS contains sender ID, record name and field names/full IDs; it has no `hasTime` bit.
Indexed interpretation therefore requires caller-supplied or source-proven explicit local metadata.
The supported finite layouts are two int32 logical contributors (`PAIR_INT 32`) and one logical
int64 contributor (`SINGLE_INT 64`) for Java's first two integer storage slots. Both have actual
source/exporter and parser observations in [indexed validation](../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.md);
neither is inferred from a familiar name.
Additional layouts remain pending until characterized. No synthetic VOID tail is inserted into values.

The indexed validation records equal/redefined sender IDs, record/type/order changes and actual Java reset.
The new mode stages a complete sparse active-sender table, local schema and context, then publishes
them together through nonthrowing operations. Old owned results retain their immutable provenance;
borrowed schema/description/context views expire at successful publication or owner move/destruction.
A failed parse, validation, resource admission or allocation retains the previous complete generation.
The active view has bounded current entries in stable first-seen sender order, not unbounded history.
Existing ordinary discovery still rejects repeated sender IDs.

For independent closed sources, construct a new reader explicitly. Old results remain valid.
No header/footer implicitly resets state. In-place reset remains pending if production requirements
need it; its framing/schema/descriptor/heartbeat/terminal-state and budget semantics need separate
review. Replacing a description or changing a source must not reset cumulative work to evade limits.
This stronger atomic publication policy differs deliberately from Java's incremental remapping.

## Symbols, options and flags

Known symbol forms are Penta 15/20/30/35-bit encodings, FC UTF-8, FD CESU-8, FE empty, FF repeat,
F8 MRU flags and F9 new compact-int flags. Reserved C0..DF and FA/FB forms fail. Each data message
starts fresh symbol state and MRU flags 1; two flag prefixes before one symbol fail.
The [matrix](../../tests/fixtures/protocol/real-tape/support-matrix.tsv) links both PentaCodec sources
and links the [bounded flag integration](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md).

`hs` is the only known protocol option. Absent, empty and present values require separate producer
observations. The Java writer masks flags according to negotiated options/family; its symbol reader
ignores the option set and reads raw flags. The new reader preserves the known data mask even when
the descriptor omits `hs`; it must not invent a parser rejection from writer masking. Unknown option
features remain explicitly unsupported. Existing modes retain zero-data-flag refusal.

| Bit | Data metadata |
| --- | --- |
| 0x01 | TX_PENDING |
| 0x02 | REMOVE_EVENT |
| 0x04 | SNAPSHOT_BEGIN |
| 0x08 | SNAPSHOT_END |
| 0x10 | SNAPSHOT_SNIP |
| 0x40 | SNAPSHOT_MODE |

The finite data mask is **0x5f**: all 64 subsets require production observations across all four
data families, including combinations a normal writer filters. Bit0x20 is reserved; REMOVE_SYMBOL
0x80 is not a data-writer flag; other bits are outside this profile. Raw parser acceptance of such
bits is recorded separately from selected C++ refusal. No snapshot/transaction collector is added.

REMOVE_EVENT changes the payload grammar: under explicit FIELD READ, all leading effective Event*
actions, including ordinary business fields, precede one compact-long record time; later ordinary
fields keep defaults. DISABLED retains its previous time-only removal handling. Optional `removalTime` on the owned record
preserves this exact time without confusing it with FIELD event-time metadata. Configured indexed
layouts project the time into their local words; other missing ordinary values keep fresh defaults.
The existing SymbolValue already carries raw flags; no duplicate flags member is introduced.
Builtin FIELD event metadata requires the separate explicit READ selection in
[TAPE-TIME](TAPE-TIME.md). It owns full physical compact-long words and source ordinals separately
from indexed time and MESSAGE state. Existing constructors remain DISABLED and refuse FIELD text;
old formatter versions refuse any engaged metadata. The
[FIELD validation](../../tests/conformance/protocol/field-types/FIELD-TIME-VALIDATION.md) states the
measured producer pins, resource boundaries and intentional Java unknown-consumer difference.

The bounded implementation uses `decodeStreamDataWithEventPolicy` under explicit BINARY_CATALOG_V1
for RAW/TICKER/STREAM/HISTORY with NONE/MESSAGE metadata. Supplied indexed layouts remain explicit;
a distinct TapeDiscovery constructor discovers ordinary binary fields without hasTime inference.
Both pins retain actual all64-subset flag observations and the original TimeAndSale evidence.
Absent/empty/exact hs are accepted; unknown/duplicate/whitespace option tokens are refused.
Old modes and payload entry points remain zero-only. Old formatter grammars refuse nonzero flags
or any removalTime before buffering, including0; explicit zero prefixes preserve old output.
See [BINARY-MESSAGES](BINARY-MESSAGES.md) and [dispatch validation](../../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.md).
## Message and descriptor policy

| IDs | Target behavior |
| --- | --- |
| 5 RAW_DATA, 10 TICKER_DATA, 15 STREAM_DATA, 20 HISTORY_DATA | Shared charged decoding, one wire-record sequence and original family identity |
| 0 HEARTBEAT | Empty/timestamp-only metadata with aggregate timingEvents; other variants refused |
| 1 DESCRIBE_PROTOCOL | Known DXP3 full/delta subset; unknown/receive/valid endpoint extensions explicitly refused |
| 2 DESCRIBE_RECORDS | Exact field descriptions and reviewed generation policy |
| 3 DESCRIBE_RESERVED | Bounded ignore event; no records or state reset |
| 4 PART | Unsupported reassembly feature |
| 11/12/16/17/21/22 | Unsupported subscription payload operations |
| 49/50/51/52/53/54/55/56 | Unsupported RMI payload operations |
| 61 TEXT_FORMAT | Unsupported nonbinary marker |
| 75 ZIP / 139 GZIP | Outer container markers; optional tool-owned [GZIP/ZIP component](TAPE-CONTAINERS.md), not inner data messages or implicit CLI input selection |
| Other IDs | Typed unknown-message refusal |

Source family already exists on the returned envelope; no duplicate per-record type is needed.
Shared charged decoding must serve the four data families without changing existing direct
`decodeStreamData`/`decodeRawData` contracts. RAW does not fan out into three C++ collector callbacks.
Wire order does not promise Java collector delivery order.

The normal descriptor is binary DXP3. Both pins characterize tails, properties and repeated deltas.
Known properties/sends and deltas are accepted within finite old+candidate+temporary limits; valid
unknown properties/message descriptors, nonempty receives and endpoint extensions use selected
UNSUPPORTED diagnostics. Malformed examined primitives/magic/tails fail separately; a late failure
cannot authenticate completion. The exact grammar and resource formula are in BINARY-MESSAGES.

Headerless self-described binary has actual FileWriter positive evidence and is enabled only by
this explicit mode. Every data sender still needs a prior description. Clean EOF needs an accepted
header or committed nonempty description batch, with no mandatory footer. Empty descriptions,
markers and heartbeats alone do not authenticate EOF. CSV/BLOB are nonbinary alternatives;
schemeKnown/readAs/resync/ignore/continue/cycle and implicit numeric local IDs remain excluded.
Valid Java alternatives are refused explicitly; completed-prefix/error coordinates remain exact.
## Future gates and acceptance

NONE, MESSAGE and explicitly selected FIELD READ are bounded capabilities. The optional tool-owned
[closed GZIP/ZIP component](TAPE-CONTAINERS.md) validates bounded concatenated gzip members or strict
single-file ZIP32, including integrity and physical EOF. Its OFF-default dependency remains outside
core/protocol. It does not add implicit wire modes or operational CLI container selection.
LONG/TEXT sidecars (047), finite split sequences (049) and broader operational streaming/CLI hookup
(050) retain their separate integration gates. Any composed sink must preserve callback lifetime,
stop/exception checkpoints, completed-prefix publication and layered source/close/EOF completion;
container closure alone cannot authenticate final COMPLETE.

Full generic support requires every applicable pending matrix row to gain production positive and
negative evidence at both pins, matching C++ canonical observations, exact resource/ownership/error
checks and independent final-source review. Existing bounded results remain valid only in their
documented profiles. No unfinished row is closed merely because a structural walk reached EOF.

Owned byte/string/object fields have separate [validation](../../tests/conformance/protocol/field-types/OBJECT-VALIDATION.md).
The generic exact 249/137 grammars also apply while discarding values and deliberately
differ from Java unknown-local-field skipping; opaque bytes establish no Java object semantics.
