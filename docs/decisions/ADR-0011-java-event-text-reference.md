# ADR-0011 — Java event text reference

[Русская версия](ADR-0011-java-event-text-reference.ru.md). English is authoritative.

Status: accepted compatibility policy. Implementation qualification is reported separately in the [validation record](../../tests/conformance/EVENT-TEXT-VALIDATION.md).

## Context and decision

Event text follows pinned QD field order, character handling and business accessors under an explicit immutable context. Numeric presentation uses the refined Java 19+ Double.toString contract, qualified against Java 25. The [Java 19 release note](https://www.oracle.com/java/technologies/javase/19-relnote-issues.html#JDK-4511638) documents tighter decimal selection and changed results. The [current specification](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/lang/Double.html#toString(double)) selects the shortest round-trip decimal, with the defined closest one-or-two-digit rule when one digit suffices. A runtime-version difference alone does not establish an upstream defect.

The [numeric difference fixture](../../tests/fixtures/event-text/numeric-version-differences.tsv) retains both observed spellings, significant lengths, exact decimal distances and each classification. Both spellings must round-trip to the original binary64 bits. Existing Candle, symbol, source and IPF canonical formats keep their established contracts.

## Confirmed TimeFormat cache defect

Pinned [TimeFormat](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeFormat.java#L515) keys second-only results by the UTC second, although legal fixed offsets may contain milliseconds. For offset 1234, calling format(1) before format(999) produces 19700101-000001 twice; a fresh formatter produces 19700101-000002 for 999. Reversing the calls reverses the stale answer. getInstance creates fresh format/cache objects and defensively copies the timezone. Production behavior is reproduced on Java 8 and 25.

C++ returns the correct history-independent local timestamp, checked against a fresh production Java formatter. The [three exact exceptions](../../tests/fixtures/event-text/upstream-cache-differences.tsv) retain cached and fresh observations for offsets 1234, -2147483648 and 2147483647. Other rows require exact equality; new differences stop capture/verification until reviewed. Valid sub-second fixed contexts remain supported.

## Alternatives and consequences

Copying historical numeric/cache behavior would preserve documented obsolete spellings or the confirmed cache defect. A different C++ text syntax would prevent ordinary log comparisons. The selected policy retains exact inputs and narrow documented differences. UTF-16 is the strict result; [UTF-8 conversion](../EVENT-TEXT.md) has an explicit malformed-surrogate policy. No generic event-text parser or universal round trip is promised.
