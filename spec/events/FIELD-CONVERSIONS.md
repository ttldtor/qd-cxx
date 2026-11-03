# Schema-bound event field conversions

[Русская версия](FIELD-CONVERSIONS.ru.md). English is authoritative.

Prepared Quote and Profile mappings convert complete, validated local values. The local schema
retains every exact full serial type, extra field and lookup alias. Conversion never changes a
generic `FieldValue`, its schema identity, its nullable payload or its raw bits. The fixed eight-field
`mapQuote` entry point retains its original exact-type contract.

## Supported accessors

| Event selection | Accepted exact local types | Projection |
| --- | --- | --- |
| Quote Bid/Ask Price and Size; ten Profile numeric properties | UTF_CHAR (2), INT (4), COMPACT_INT (8), DECIMAL (24), LONG (104), WIDE_DECIMAL (120) | Existing decimal fields follow `get…WideDecimal`; business getters follow direct production `getAsDouble` |
| Quote Sequence, TimeNanoPart and Bid/Ask Exchange; Profile ExdDiv.Date, Flags and Halt.StartTime/EndTime | 2,4,8,24,40,56,72,88,104,120,152 | First physical Java int slot, without representation conversion |
| Quote Bid/Ask Time | TIME_SECONDS (56), TIME_MILLIS (152) | Seconds multiplied by 1000, or unchanged milliseconds |
| Profile Description and StatusReason | UTF_CHAR_ARRAY (10), STRING (137) | Nullable exact UTF-16 String; no numeric or object-to-string coercion |

These are exhaustive typed-adapter tables. Generic `EventMapping::prepare` accepts explicit type
alternatives from the existing finite BINARY_FIELDS_V1 catalog; it does not perform a conversion
or authorize every catalog identity for a typed event. An unsupported present selection returns
`INCOMPATIBLE_TYPE`. Unknown request identities return `INVALID_INPUT`. No representation bits are
masked to make an unsupported field appear compatible.

The pinned production `SerialFieldType.createDefaultIntInstance` does not create local BYTE (1) or
SHORT (3) fields. Those identities remain supported generic binary values and are explicitly refused
by these typed adapters. This boundary does not prohibit wire BYTE/SHORT conversion into a
supported local schema and does not exclude standard events from the receiving-client direction.
VOID (0) is not a typed scalar selection or a manufactured local long tail.

## Numeric values

UTF_CHAR, INT and COMPACT_INT become integer WideDecimals. LONG uses the production integer
composition contract: a signed 56-bit significand and rank, decimal reduction with half ties toward
positive infinity, then the production normalization rules. Both signed 64 extremes are handled
without signed overflow or an intermediate `double`.

DECIMAL uses the production `Decimal.tinyToWide` path. Standard precision, extra precision,
128th fractions, reserved encodings, NaN, positive/negative infinity and noncanonical zeros follow
the raw input bits. WIDE_DECIMAL preserves every incoming bit unchanged, including noncanonical
special values. The conversion helper has no allocation or unbounded loop; supported inputs need
at most three decimal reductions and at most eight normalization steps.

The public decimal members retain the generated WideDecimal accessor result. Calling their
`WideDecimal::toDouble` converts that decimal and remains the existing raw-decimal view. The
Java-like business getters preserve the actual event value directly from the original scalar:
Quote `getBidPrice`, `getBidSizeAsDouble`, `getAskPrice`, `getAskSizeAsDouble`; Profile `getBeta`,
`getEarningsPerShare`, `getDividendFrequency`, `getExDividendAmount`, `getHigh52WeekPrice`,
`getLow52WeekPrice`, `getShares`, `getFreeFloat`, `getHighLimitPrice`, `getLowLimitPrice`.

The distinction matters for LONG 36028797018963973: the production wide getter returns raw
922337203685477759, whose decimal-to-double bits are 4854880398305394688. The actual Java business
value has bits 4854880398305394689. The typed business getter preserves the latter; the existing
decimal field preserves the former. DECIMAL follows its direct tiny-decimal getter path, including
extra precision and reserved encodings; WIDE_DECIMAL uses its production numeric view. Finite,
infinite and signed-zero business results are compared by exact binary64 bits; NaN is classified
without a promise about arithmetic NaN payload/sign.

An optional inline array of four/ten `{wideRaw, directValue}` snapshots is appended after every
existing Quote/Profile data member. It owns only numbers and adds no heap allocation or borrowed
record references. Existing aggregate initializers and legacy `mapQuote` leave it absent. Each
getter uses its source snapshot only while that property's current decimal raw bits still match.
Manual events, an absent snapshot, or an edited raw field use the current decimal view instead;
changing one field does not invalidate an unchanged sibling. Copy/move preserve the owned inline
state. This is a value object, with the same ordinary synchronization requirements for concurrent
mutation as its existing public fields. `WideDecimal::toDouble` and the legacy mapQuote contract
do not change. The ordinary [Feed example](../../examples/feed_quote/README.md) uses business getters.
No general numeric-to-string, string-to-number or object deserialization API is introduced.

## Physical slots and absence

A Java long occupies two physical int slots. Raw `getInt` access at its field index reads the
signed high 32 bits, while numeric LONG and WIDE_DECIMAL access reads all 64 bits. The native local
value remains one logical scalar; typed raw selections reproduce the first-slot result explicitly.
Profile halt fields always multiply that raw signed 32 slot by 1000, even when their exact local
type is TIME_MILLIS or WIDE_DECIMAL. They do not use Quote's time converter.

| Absent local selection | Native projection |
| --- | --- |
| Required Quote numeric field | Preparation fails with MISSING_REQUIRED |
| Required Profile Description | Preparation fails with MISSING_REQUIRED |
| Optional Profile numeric field | Raw WideDecimal 0 and business NaN; each of the ten public Java double getters is observed separately |
| Quote optional time, Sequence, TimeNanoPart | Zero |
| Quote optional Exchange | Syntactic regional record exchange, otherwise zero |
| Profile optional date, flags, halt fields | Zero |
| Profile optional StatusReason | Null |

Missing means no selected lookup. A present field with the wrong type is refused even where a
Java optional lookup would treat a different storage class as absent. Null and engaged empty
String remain distinct. Text is copied only after per-string and aggregate output bounds pass;
the borrowed projection does not retain source storage. Complete shape, canonical payload and
context checks still precede every projection, including fields not selected by the event.

## Finite preparation

The complete Quote table declares 72 exact alternatives and Profile declares 108, including all
optional selections even when absent. `EventRegistration.mappingLimits.typeAlternatives` defaults
to 128; explicit smaller limits remain meaningful. Exact 72/108 allowances succeed and one below
returns RESOURCE_LIMIT without a partial mapping. Comparison work is separately charged for all
actual name and type comparisons. The default comparison allowance conservatively includes the
new finite type budget; registry preparation still sums every declared allowance using checked
arithmetic and consumes the existing caller credits. Record 16 and field 128 receiving caps do not
change. Counts are logical preparation bounds, not a measured process-memory guarantee.

The receiver's result-byte coordinate and high-level preparation both use
`sizeof(client::SubscriptionProjection)`, so the inline snapshot increases those finite allowances
automatically. The final callback vector remains bounded by its admitted record count and actual
event sizeof. Queue frames still contain generic FieldValue data rather than typed snapshots;
their byte representation does not grow. Selection visits still inspect each numeric source once,
computing both views from that one scalar. Exact-result-byte and one-byte-below receiver tests cover
the updated owning projection. These are logical copy-work/output bounds, not allocator/RSS limits.

## Production evidence and verification

The [oracle helper](../../tools/upstream/field-conversion-oracle/README.md) uses the locked QD
revision from [upstream.lock](../../upstream.lock), actual `DXFeedScheme`, `MarketFactoryImpl`,
generated QuoteMapping/ProfileMapping, production RecordCursor and delegate/value getters.
Its configurable records preserve production field names, reverse physical order, construct
real long VOID tails and export complete logical fields. Numeric sign/rank/tie/carry/extreme
inputs, all128 low tiny-decimal flag patterns at five signed mantissa boundaries, raw-slot
variants, nullable strings, absent required/optional fields and unsupported representations are
captured in the [fixture](../../tests/fixtures/field-conversions/CAPTURES.md).

Primary conversion sources are pinned
[RecordMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordMapping.java),
[WideDecimal](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/WideDecimal.java)
and [Decimal](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/util/Decimal.java).

Native conformance checks compare production WideDecimal bits, first-slot results, business
times, text and defaults through both owned and borrowed mapping entry points. Exact preparation
boundaries, default registry use, provenance/complete-shape validation and finite text ownership
remain required. Deterministic wire delivery, installed/relocated package consumers and platform
results are separate executed gates; a source table alone does not establish them.
