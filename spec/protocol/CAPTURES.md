# Measured QTP evidence (QDCXX-006)

[Русская версия](CAPTURES.ru.md). This English document is authoritative.

Evidence **C** means executed Java capture, bounded by the [coverage manifest](../../tests/fixtures/protocol/COVERAGE.md) and [validation/provenance](../../tests/fixtures/protocol/VALIDATION.md). Source labels elsewhere remain source evidence unless linked here. Baseline is QD 3.355 / `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. These are descriptive observations, not normative C++ decisions.

## Framing and callback grouping

**C — P01:** [exact capture](../../tests/fixtures/protocol/expected/P01.tsv), [ordered values](../../tests/fixtures/protocol/expected/P01.semantic.tsv). The 31 bytes contain description [0,12), data [12,23) and data [23,31). Whole input yields one callback with three records; split at 23 yields two callbacks containing two then one record. Every split (1..30) and bytewise delivery yields the same ordered values. Incomplete frame bytes remain pending without callbacks/errors for that frame; a completed description produces no record callback. This does not establish a final-EOF truncation error: no final-EOF API is called.

The first data frame uses repeat-symbol `ff` for its second ABC record; the subsequent frame encodes ABC again (`0443`). Values/positions come from Java composition and parsing, not a hand-authored codec. Sources: BinaryQTPParser.parseImpl, AbstractQTPParser.processPending, BinaryQTPComposer and PentaCodec Reader/Writer in [SOURCES](SOURCES.md).

## Directional mappings and fields

**C — P03:** [A to B](../../tests/fixtures/protocol/expected/P03-A-B.tsv) sends ID 0 and delivers ID 1; [B to A](../../tests/fixtures/protocol/expected/P03-B-A.tsv) sends ID 1 and delivers ID 0. Both data and STREAM_ADD_SUBSCRIPTION resolve R by name, with unchanged local R/S ordering. Each direction has its own fresh parser/composer. Sources: BinaryQTPParser.remapRecord/parseSubscription and BinaryQTPComposer.describeRecord.

**C — P04:** [reordered fields](../../tests/fixtures/protocol/expected/P04-order.tsv) maps wire x,y=(11,22) into local y,x,z=(22,11,0). The absent compact-int z has raw bits zero and formats as 0 in this specific field class; this says nothing about application defaults, decimals, XML defaults or object fields. [Alias case](../../tests/fixtures/protocol/expected/P04-alias.tsv) serializes sender local x.dot as property xdot and resolves receiver alias xdot to local x slot 1. DefaultRecord.addFieldAlias also makes xdot the receiver field's property name; the capture preserves this detail. No alias table is transported. Sources: BinaryRecordDesc field lookup, AbstractDataField.getDefaultPropertyName, DefaultRecord.addFieldAlias.

## Real connection requested layouts

**C — P05:** [none](../../tests/fixtures/protocol/expected/P05-none.tsv), [zero](../../tests/fixtures/protocol/expected/P05-zero.tsv), [x](../../tests/fixtures/protocol/expected/P05-x.tsv), [z](../../tests/fixtures/protocol/expected/P05-z.tsv). Incoming peer description/subscription runs through MessageAdapterConnection.processChunks / ConnectionQTPParser. Reply retrieval runs through MessageAdapterConnection.retrieveChunks / ConnectionQTPComposer. No requested descriptor is injected. The [driver instrumentation](../../tools/upstream/protocol-oracle/README.md#driver-and-instrumentation) suppresses only nondeterministic heartbeat/protocol metadata, and is not a timing or handshake experiment.

| Incoming request before first writer use | Outgoing wire description | Audit raw x,y,z |
| --- | --- | --- |
| Absent | x,y | 11,22,0 |
| Originally zero fields | x,y | 11,22,0 |
| x only | x | 11,0,0 |
| Nonempty z only, absent locally | zero fields | 0,0,0 |

The empty intersection does not become the all-fields signal: R/ABC is still delivered, but with no field payload. Incoming peer R ID 1 resolves to publisher R ID 0; reply describes publisher ID 0 and peer parser delivers local ID 1. This confirms the bounded source interpretation. Sources: ConnectionQTPParser.remapRecord/getRequestedRecordDesc, MessageAdapterConnection.getRequestedRecordDesc, ConnectionQTPComposer.getRequestedRecordDesc, BinaryQTPComposer.getRecordDesc and BinaryRecordDesc.isEmpty. Dynamic layout changes, cache/reset, P06 conversions and P09 negotiation remain unmeasured.
