# First real-event baseline: default composite Quote

[Русская версия](REAL-EVENT-BASELINE.ru.md). English is authoritative.

## QDCXX-022 runtime and implementation result

The [production capture](../tests/fixtures/protocol/quote/CAPTURES.md) now confirms the unchanged
331-record default scheme, composite Quote ID 0, twelve int slots and eight logical fields with
full identities 56/2/120. Actual putEvent/FileWriter/FileReader/getEvent output establishes raw values,
decimal views and default-scheme time/sequence/nano losses. The separate
[C++ validation](../tests/conformance/protocol/quote/VALIDATION.md) covers the bounded implementation;
independent review is pending. The source-only statements below describe earlier QDCXX-020 research,
not the current evidence level. No broader tape or event support follows from this result.

## Historical source baseline

QDCXX-020 research; QD 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
All event findings here are **source-only**. No new event generation, schema dump, publication or
replay was executed. The earlier [tape observations](../spec/protocol/TAPE-FILES.md) use synthetic
S/R/E records and do not establish Quote compatibility.

## Selection and source chain

Select the composite `Quote` record for the first useful offline tape-to-event inspection example.
It exercises real prices, fractional sizes, exchange characters and bid/ask timestamps without
requiring the indexed-event transaction/snapshot model. Trade additionally requires TradeId,
sequence, date, volume, turnover and flags; its smaller-looking name does not make it a simpler profile.

Pinned sources:

- [DXFeedScheme](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedScheme.java): system-property snapshot, registered event factories and PentaCodec.
- [MarketFactoryImpl.buildScheme/createDelegates/createStreamOnlyDelegates](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java): fields, defaults and delegate contracts.
- [SchemeProperties](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeProperties.java): `isWide`, `selectDecimal`, `selectTime`, `isEventPropertyEnabled`, `getExchanges`.
- [SchemeBuilder](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeBuilder.java): insertion order, optional fields, two-slot long projection and `hasTime`.
- [SerialFieldType](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java): IDs, named types and local field classes.
- [QuoteMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/QuoteMapping.java) and [QuoteDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/QuoteDelegate.java): field lookup and both event directions.
- [Quote](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/Quote.java), [EventDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegate.java), [MarketEventDelegateImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketEventDelegateImpl.java): symbol, event time and API time/sequence.

## Exact unmodified default record

Use the locked built-in DXFeedScheme with no schema/property overrides. Crucially,
`dxscheme.wide` defaults to **true**: both prices AND sizes select WIDE_DECIMAL, despite the
factory's DECIMAL/COMPACT_INT fallback arguments. Do not set `dxscheme.wide=false` or replace
types with 8 to fit the current decoder. `dxscheme.price`, `dxscheme.size`, `dxscheme.bat` and
event-property enablement would create different profiles and must be recorded if ever used.

Field names below are local schema names; full names have `Quote.` prepended. Indices are the
source-derived local Java integer-slot projection, not wire record IDs or C++ storage requirements.

| Field | Full type ID / wire primitive | Java slots | Quote API mapping |
| --- | --- | --- | --- |
| Bid.Time | TIME_SECONDS 56 / compact int | 0 | BidTime in milliseconds, stored in seconds |
| Bid.Exchange | UTF_CHAR 2 / UTF character | 1 | BidExchangeCode (`char`); explicit nondefault property alias |
| Bid.Price | WIDE_DECIMAL 120 / compact long | 2–3 | BidPrice through decimal conversion |
| Bid.Size | WIDE_DECIMAL 120 / compact long | 4–5 | BidSizeAsDouble; preserve fractional size |
| Ask.Time | TIME_SECONDS 56 / compact int | 6 | AskTime in milliseconds, stored in seconds |
| Ask.Exchange | UTF_CHAR 2 / UTF character | 7 | AskExchangeCode (`char`); explicit nondefault property alias |
| Ask.Price | WIDE_DECIMAL 120 / compact long | 8–9 | AskPrice through decimal conversion |
| Ask.Size | WIDE_DECIMAL 120 / compact long | 10–11 | AskSizeAsDouble; preserve fractional size |

Do not confuse these dotted local names with outgoing property names. The default binary writer
uses `BidTime`, `BidExchangeCode`, `BidPrice`, `BidSize`, `AskTime`, `AskExchangeCode`, `AskPrice`,
`AskSize`, in that order. [AbstractDataField](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/AbstractDataField.java)
removes dots for default property names; [DefaultRecord](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java)
applies QuoteMapping's exchange aliases. [BinaryRecordDesc](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java)
uses getPropertyName and skips void tails. [BinaryQTPComposer.isWideDecimalSupported](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java)
returns true for this direct file composer, independently of EMPTY_SET; network composer overrides
must be investigated separately. Capture actual descriptions as well as local field names.

There are eight logical serialized fields, twelve local int slots, no object slots and
`hasTime=false`. The second slot of each long is a local void tail, not another wire value.
`Sequence` (SEQUENCE 72) and `TimeNanoPart` (COMPACT_INT 8) are optional and disabled by default;
their mapping reads return zero and writes do nothing. Bid/ask time and exchange are optional
in mapping construction but enabled in this default record; prices/sizes are required.
Do not omit enabled fields merely because a mapping tolerates their absence.

`Quote.getTime()` uses the larger bid/ask second plus milliseconds encoded in the high ten bits
of `timeMillisSequence`; the low 22 bits are sequence, range 0..4194303. The default missing
Sequence field therefore loses the milliseconds/sequence on round trip. Initial positive values
must use whole seconds, sequence zero and nano part zero; a separate lossy-input case must
demonstrate the Java result instead of expecting exact retention. Quote is not an indexed event;
no history time-index slots or indexed-event flags are required. Cursor raw flags and optional
EventTime/EventSequence metadata remain distinct from Quote business timestamps. The initial
file profile requires zero raw flags, EMPTY_SET, time NONE and event time zero.

The complete default scheme also constructs `Quote&A` through `Quote&Z` (see
[MarketEventSymbols.DEFAULT_EXCHANGES](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java)).
Regional records omit both Exchange fields, infer exchange from record suffix and have six logical
fields / ten int slots. Sequence/nano defaults are unchanged. The first test selects composite
symbols only; it does not remove regional records from the Java scheme or claim regional support.
Capture the actual complete scheme identity and selected record layout before asserting slot/ID results.

## Proposed deterministic production-path test

1. Build locked Java sources in two clean runs; record JDK, classpath/service providers, all effective
   scheme properties and hashes. Enumerate the actual default record and mapping. Fail if its enabled
   fields/types differ from this source-derived table; investigate rather than simplifying it.
2. Create real `Quote` objects for `IBM`, `MSFT`, then `IBM` again. Set bid/ask times to explicit whole
   seconds around `1700000000000`, exchange codes `N`/`Q`, prices `100.25`/`100.5`, sizes `10.5`/`12.25`,
   then a second changed quote. Set event time, sequence and nano part to zero. No clock/random/feed input.
3. Use the actual QuoteDelegate `putEvent` selected from the default scheme's stream-only delegates,
   followed by production FileWriter configured BINARY, NONE compression, time NONE, EMPTY_SET,
   STREAM_DATA. Preserve its descriptor, automatic descriptions and footer; close before replay.
   This is an event-to-production-file test, not a DXPublisher endpoint/network observation.
4. Replay the unchanged file through production FileReader into the same full default scheme and
   actual QuoteDelegate `getEvent`. Retain raw bytes, raw field bits, symbol units, record order and
   mapped API values independently. Compare two clean captures; callback grouping is not contractual.
   Then test the future bounded C++ reader and event presentation against those Java observations.
5. Add finite positive/error cases: empty/truncated files, late descriptions, unknown/reordered fields,
   UTF character boundaries, wide decimal finite/NaN/infinity/negative-zero behavior, fractional sizes,
   missing/redefined descriptions and budget exhaustion. Expected outcomes come from Java production
   observations plus separately specified C++ resource/error policy. Include a mutation that changes
   one type, decimal value or field order and prove the verifier rejects it; verify the baseline again.

This proposal is not an executed capture. A later endpoint test should publish with STREAM_PUBLISHER
and replay through STREAM_FEED to avoid ticker conflation, with explicit drain/shutdown and bounded
timeouts. TapeConnector RAW_DATA/default protocol options must be handled explicitly before that test;
it is not equivalent to the constrained direct FileWriter profile above. No live endpoint is needed.

## C++ gaps and implementation order

Existing compact-long and UTFString primitives do not implement WIDE_DECIMAL semantics or UTF_CHAR.
Core local validation, binding and STREAM_DATA currently accept only ordinary type 8; they need exact
56/2/120 support, long-value ownership/skip behavior, decimal conversion and Quote mapping/presentation.
Full type identity must survive even when serialization shares a compact primitive. No general
conversion matrix, schema loader, transport, writer or ticker collector exists.

The [recommended next implementation](NEXT-STEPS.md) remains the narrow QDCXX-019 tape reader:
descriptor, additive bindings and EOF enable an unchanged production file now, using existing S/R/E
fixtures. It is a prerequisite, **not** the real-event acceptance milestone. Follow immediately with
the scoped Quote type/mapping extension and the production-path test above; split its components if
the API/evidence gate requires it. Do not label the first type-8 tape reader as general dxFeed support.

External XML/JSON loading is a separate adapter that supplies validated immutable local schema input.
Wire descriptions contain incoming record IDs, names and serial types; they neither load XML nor
reconstruct the complete local mapping/default/provider policy. The first Quote profile may use an
explicit reviewed built-in local adapter, but must retain the default Java field types and publish its
limitations. A generic importer is not a prerequisite for this one event; owner approval is needed
for that adapter and for any public event API.
