# Event text contract

[Русская версия](EVENT-TEXT.ru.md). English is authoritative.

## Reference and fields

[ADR-0011](../../docs/decisions/ADR-0011-java-event-text-reference.md) defines the Java 25 numeric profile and reviewed cache-defect corrections. [Provenance](../../tests/fixtures/event-text/provenance.json) identifies the pinned QD source. qdcxx_event_text depends one-way on event leaves, value/source helpers and shared qdcxx_time_zone_data; event values do not depend on Schedule.

Quote order: symbol, eventTime, time, timeNanoPart, sequence, bidTime, bidExchange, bidPrice, bidSize, askTime, askExchange, askPrice, askSize. Profile: symbol, eventTime, description, SSR, status, statusReason, haltStartTime, haltEndTime, highLimitPrice, lowLimitPrice, high52WeekPrice, low52WeekPrice, beta, earningsPerShare, dividendFrequency, exDividendAmount, exDividendDay, shares, freeFloat. Trade/TradeETH: symbol, eventTime, time, timeNanoPart, sequence, exchange, tradeId, price, change, size, day, dayVolume, dayTurnover, direction, ETH.

Numeric fields use business accessors and their valid source binary64 snapshots. Profile shares/freeFloat use the Java double-to-WideDecimal presentation projection. Text changes no stored decimal bits and appends no extra raw fields. Reserved flags remain in the event and can be printed separately with formatEventFlags; business enum projections keep their existing accessor semantics.

C++ owned event symbols cannot represent Java null; comparisons supply the identical non-null string to Java, including empty. Nullable Profile strings print quoted literal null. Quotes, controls and individual surrogates remain literal. Exchange units 32..126 are literal; NUL becomes backslash-zero; others become backslash-u plus four lowercase hex digits.

## Numbers, dates and canonical values

Double text preserves negative zero, NaN and signed infinities using Java spelling. Plain notation applies for leading decimal exponents -3..6; scientific notation uses uppercase E, no positive exponent sign and a fractional digit. Decimal selection uses standard shortest scientific to_chars, explicit nearest-two-digit selection when one digit suffices, and Java layout. Actual Java comparisons qualify both required C++ implementations; library equivalence is not assumed. No Java runtime algorithm source or new numeric dependency is embedded.

Time uses yyyyMMdd-HHmmss.SSS; Profile halt times omit .SSS. Zero prints 0. Java's inclusive pre-guard is [-62135683200000,253402387200000]; timestamps outside it print the original signed64 integer. Within it, local Gregorian dates outside years 1..9999 also print the raw number. Day IDs use existing signed32 DayUtil-compatible arithmetic independently of timezone.

Enums, exact nullable source names and unsigned lowercase flag hex have dedicated overloads. Existing CandlePeriod/CandleSymbol/Candle-attribute/decimal APIs retain canonical text and documented parsing guarantees. No general event parser or universal round trip is added.

## Ownership, failures and finite cost

EventTextLimits is mandatory. inputUnits counts combined copied/inspected symbol, nullable-string and source-name units. outputUnits bounds the complete UTF-16 result. utf8Bytes bounds UTF-8 bytes and is unused by UTF-16-only calls. work charges one per inspected source unit, one per copied UTF-16 output unit, 128 per formatted integer/double/character/date/WideDecimal scalar, and one per UTF-16 unit plus each UTF-8 byte during encoding. Literal/enum text charges copied output units. Credits are cumulative per call; no internal retry occurs. These logical bounds are not CPU-instruction, wall-clock, allocator-capacity or RSS quotas.

Double results have at most 24 units; exchange rendering has at most six. Numeric temporaries have fixed binary64/integer bounds; dates use fixed stack buffers. Profile share composition has at most eight rank comparisons and bounded decimal-power conversion. UTF-8 convenience temporarily retains up to outputUnits UTF-16 units plus utf8Bytes bytes; standard strings may reserve extra allocator capacity. Failed allocation returns ALLOCATION, exceeded logical/host string capacity returns RESOURCE_LIMIT. No partial result escapes.

Contexts own immutable binding pins; source/provider handles can be destroyed after binding. UTC/fixed contexts allocate no binding storage. Views are retained only during synchronous calls. Independent const calls are reentrant/concurrent while inputs stay unchanged; concurrent handle replacement requires external synchronization. There are no application callbacks, lazy format caches, queues, reloads or process-setting mutations.

UTF-16 output preserves malformed units exactly. UTF-8 conversion explicitly rejects an unpaired surrogate or substitutes U+FFFD for each. Valid pairs become one standard UTF-8 scalar; NUL remains a zero byte. Error results contain no bytes. The application writes a stream after obtaining the complete string.

Profile share scaling uses a fixed two-word significand product and explicit nearest-even multiplication/addition rounding. It gives the same text under all four supported caller rounding modes and never changes the caller thread rounding mode. The tests do not establish preservation of floating-point exception flags.

Summary order is symbol, eventTime, day, dayOpen, dayHigh, dayLow, dayClose, dayCloseType, prevDay, prevDayClose, prevDayCloseType, prevDayVolume, openInterest. Prices/volume use the same business binary64 accessors; OpenInterest prints the complete signed64 accessor value. The production text oracle sets that long directly, including 3000000000 and both signed64 extrema, independently of the corrected delegate-width issue in [ADR-0010](../../docs/decisions/ADR-0010-summary-open-interest-width.md).

Named context construction borrows the optional TimeZoneData pointer only for the factory call and owns a ZoneBinding plus one shared context holder on success. maxIdUnits bounds accepted copied ID units; shared immutable provider backing follows the [timezone contract](../../docs/TIME-ZONE-DATA.md), not per-format output limits. Construction may allocate; formatting only reads the retained binding and copies scalar offsets. The explicit raw-reference instant is never read from the clock by EventText. Actual total offsets, including the provider final-raw convention before 1900, are queried independently of constructor/reference metadata. Provider unavailable/invalid-ID/range/data failures are mapped explicitly without OS/data fallback.