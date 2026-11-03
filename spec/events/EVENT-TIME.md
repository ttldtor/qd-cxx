# Common event time fields and regional Quote records

[Русская версия](EVENT-TIME.ru.md). English is authoritative.

This extends [prepared mappings](EVENT-MAPPING.md) using the locked production Java
`MarketFactoryImpl`, `QuoteMapping`, `QuoteDelegate`, `ProfileMapping`, `ProfileDelegate`,
`MarketEventMapping`, `MarketEventDelegateImpl` and `Quote` getters. The reproducible
[oracle](../../tools/upstream/event-time-oracle/README.md) retains generated logical schemas,
raw cursor values and actual delegate results in `tests/fixtures/event-times/oracle.tsv`.
Physical Java VOID tails are omitted from logical schemas without changing long values.

## Fields, units and absence

| Event property | Exact local identity | Projection and absent-local default |
| --- | --- | --- |
| Quote `bidTimeMs`, `askTimeMs` | TIME_SECONDS (56), TIME_MILLIS (152) | Signed seconds ×1000, or signed milliseconds unchanged; absent means 0 |
| Quote `sequence` | SEQUENCE (72) | Low22 bits of unsigned raw Sequence; absent means 0 |
| Quote `timeMs` | Optional SEQUENCE plus bid/ask time | Floor the maximum business time to whole seconds, then add unsigned Sequence bits31..22 (0..1023); absent Sequence adds 0 |
| Quote `timeNanoPart` | COMPACT_INT (8) | Raw signed nanoseconds within a millisecond; absent means 0; the mapper preserves out-of-range raw values as Java does |
| Quote exchange codes | UTF_CHAR (2) | Present low UTF-16 unit, including zero; absent uses the record's syntactic exchange suffix, or zero for composite |
| Profile halt start/end | TIME_SECONDS (56) | Signed seconds ×1000; absent means 0; these generated fields do not use Quote's selectable TIME_MILLIS representation |
| Quote/Profile `eventTime` | Explicit caller metadata | Milliseconds since the Unix epoch; default0; independent of business time, Sequence, indexed identity and route origin |

The existing required Quote price/size and Profile Description fields, numeric defaults,
full-type matching, string ownership and errors remain in force. A present wrong full type
is an incompatible mapping, not an absent field. `dxscheme.bat=millis` is a Java schema
construction option; C++ receives an already validated schema and reads no process properties.

Quote `timeMs` reproduces Java long arithmetic, including two's-complement wrapping at
the signed64 boundaries. Negative milliseconds use mathematical floor, not truncation toward0.
Sequence's ten high bits are preserved even when greater than999; neither Sequence nor
TimeNanoPart is silently clamped. The legacy eight-field `mapQuote` remains unchanged.
Profile has no Quote Sequence/nanosecond business property; extra generic fields stay generic.

At the receiving boundary `FIELD_METADATA` selects original FIELD event metadata when present.
Its optional metadata result distinguishes absence from a timestamp of0; the legacy numeric
event member uses0 when unspecified. `UNSPECIFIED` does not synthesize metadata from business
fields or indexed record time. A delivered event owns its symbol and strings independently of
the schema, decoder frame, callback and reconnect generation.

## Regional binding and finite scope

`PreparedQuote` derives its exchange from the selected local record name using the same
syntactic suffix rule as `MarketEventSymbols.getExchangeCode`. It restores that suffix on
the owned event symbol, preserving a trailing attribute block. Present exchange fields still
win over the record fallback. Exchange identity is unrelated to Order `#source` or route origin.

An explicitly configured `EventRegistry::create` may include composite Quote, regional Quote
records and Profile within the existing16-record/128-field receiving limits. The built-in
registry remains the original composite Quote/Profile subset. No complete regional catalog
or general selector implementation is established by this contract.

For each supported event kind, a logical symbol with a registered matching regional exchange
selects that record and removes the suffix from the physical QD symbol. A composite route is
used when no regional route matches, retaining the exact logical symbol, as Java's delegate-set
fallback does. The selected record plus physical symbol participates in existing desired-state
membership, remove, reconnect and delivery filtering. For example, configured `Quote&Q` with
subscription `AAPL&Q` sends `Quote&Q`/`AAPL`; its received Quote owns `AAPL&Q`. A record for another
exchange cannot satisfy that physical membership. Mixed composite/regional subscriptions retain
separate record identities. This is deterministic local-wire compatibility; a public-service
demo is a separate observation.

The native high-level API requires exact representability for a selected regional route.
Java transformations are not always invertible: `AAPL&N&Q` becomes physical `AAPL&N`
and restores as `AAPL&Q`; `AAPL{x=y}&Q` restores as `AAPL&Q{x=y}`. With a matching regional
Quote route, `addSymbols`/`setSymbols` refuse these spellings synchronously with
`ErrorCode::INVALID_ARGUMENT` at `subscription-symbol-representation`. The complete attempted
mutation is refused; previous selectors and wire interests remain unchanged. The worker
repeats the check before physical allocation or request publication. These are explicit native
subset refusals; Java's lossy transformations are captured separately in
`tests/fixtures/event-times/representability.tsv`. Composite fallback and Profile preserve
literal spellings. Ordinary `AAPL&Q{x=y}` and representable literal attributes before an
additional trailing block remain valid.

## Evidence boundary

The oracle covers default, Sequence-only, nano-only, both, selectable millisecond business
times, optional-field absence, reordered fields and required-field failures. It uses actual
generated Quote, Quote&Q, Quote&z and Profile declarations and production delegates, with
signed32/signed64 boundaries, negative time, raw Sequence/nano boundaries and independent event
metadata. Both UTC and Pacific/Honolulu runs must produce identical bytes. Native conformance
compares owning and borrowed projections with these results and separately checks incompatible
types, provenance, exact symbol bounds and real installed-Feed wire routing.

Additional production `MarketEventDelegateImpl` and `MarketEventDelegateSet` observations cover
`AAPL&N`, `AAPL{x=y}` and `AAPL&N{x=y}`. Regional conversion replaces an existing suffix and
preserves attributes; subscription conversion removes only the suffix. It must use
`changeExchangeCode`, not `getBaseSymbol`, which also removes attributes. The actual delegate-set
exchange-index lookup chooses the matching exchange when registered and its composite entry when
that exchange is absent; the composite delegate leaves `AAPL&Q` unchanged. These calls are retained
as `route` and `selection` rows alongside the value observations.
