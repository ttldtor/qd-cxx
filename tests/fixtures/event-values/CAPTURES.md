# Event value capture contract

[Русская версия](CAPTURES.ru.md). English is authoritative.

`oracle.tsv` contains deterministic raw UTF-8/LF rows from the [production oracle](../../../tools/upstream/event-values-oracle/README.md).
`provenance.json` binds the exact pinned revision, upstream source hashes, existing class inventory,
JDK8 executable hashes/version, owned harness/launcher hashes, raw fixture digest and timezone equality.
Local execution paths and full working logs do not belong to this public fixture.

| Row kind | Inputs and actual production result |
| --- | --- |
| enum | Type name, raw signed32 code, actual valueOf/getCode result or error |
| flags | Six IndexedEvent constant values |
| bits32 / bits64 | Raw word, mask, shift, replacement bits, getBits result, setBits result |
| nanos | Raw nanoseconds, floor milliseconds, nonnegative nano part |
| time | Raw milliseconds, saturated signed32 seconds, nonnegative milliseconds |
| combine | Milliseconds, signed nano part, wrapped signed64 nanoseconds |
| packed | Raw Trade timeSequence, actual getTime, getSequence |
| settime / setseq | Original raw packed word, replacement time/sequence, actual packed result or error |
| day | Year/month/day, actual DayUtil result or error |
| date | Signed packed date, actual DayUtil day ID or error |
| calendar | Raw signed32 day ID, actual DayUtil packed Gregorian date |
| schema | Standard TimeAndSale record, full ExchangeSaleConditions field name, exact serial type ID40 |
| encode | Nullable source units, EventUtil full64 code or error, decoded units or error |
| decode | Every raw64 pattern selected by the corpus, actual EventUtil decoded units |
| field | Nullable source units, signed32 standard-field parse result, field render units |

Text cells use `null` for null, `-` for engaged empty, otherwise four hexadecimal digits per UTF-16 unit.
Decimal integer cells retain signed raw bits; `error` means the actual Java operation rejected input.
Full64 encode/decode must remain distinct from the standard SHORT_STRING field's signed low32 projection.
A negative standard field is sign-extended when passed to its long decoder; it can therefore render
leading 0xFF units. No parser result is substituted with an unsigned four-byte interpretation.

The corpus covers all named/reserved enum-table entries and out-of-table codes; zero/all-one/sign-bit
masks and negative/oversized shifts; timestamp extrema, saturation and out-of-range nano parts;
sequence boundaries; Gregorian century/leap/year-zero/negative-year/arbitrary-day cases, signed32
extrema and deterministic raw calendar samples; null/empty/embedded-zero/eight/nine-unit strings,
0xFF, above-byte/surrogate rejection and each nonzero byte in each of eight positions.
Both timezone observations must be byte-identical. Capture is evidence of the called production
operations, not a claim about live feeds, future event adapters or platform/package readiness.
