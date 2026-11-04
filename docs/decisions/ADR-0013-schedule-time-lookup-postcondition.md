# ADR0013: Enforce the containing-day postcondition

[Русская версия](ADR-0013-schedule-time-lookup-postcondition.ru.md). English is authoritative.

Status: accepted implementation decision.

## Context

The pinned Java `Schedule.getDayByTime` Javadoc promises a Day containing the supplied instant.
`Day.containsTime` uses `[start,end)`. For `(tz=UTC;td=1234567;0=d00000000)` and time0, the actual
production Java lookup returns empty Day1 with start=end86400000. It does not contain0. The two
separate backward/forward loops do not recheck the first condition after advancing through an
empty Day. This is a confirmed public-contract violation, independently of timezone data versions.
The [actual witnesses and source identity](../../tests/fixtures/schedule-query/containing-day-correction.json)
retain both Java output and the matching native result before correction.

## Decision

Keep the source lookup and complete grammar. Before returning a time-selected Day, check
`containsTime(time)`. If false, return typed `ScheduleError::NO_SESSION`, as for an absent containing
session. Do not add a new search, reject the parsed definition or invent a resource failure.
Direct id/YMD factories still construct empty Days and preserve their session records. The
Schedule session-by-time operation propagates the typed refusal.

## Consequences and validation

The Day time query intentionally differs from the preserved Java fixture for this confirmed bug;
the corresponding session query already refuses the instant. The two cases are classified explicitly,
separately from the36 TZDB2026f data/reference differences. Conformance checks retain actual Java
bytes and assert the defined C++ correction. Ordinary positive/boundary queries retain the same
algorithm, method-specific ranges and bounded construction/lifetime contracts. This decision does
not establish a general permission to normalize or reject other accepted source quirks.

## Alternatives

Returning the Java non-containing Day would violate the documented contract. A new nearest-day search would add different semantics and work. Rejecting empty definitions would narrow the accepted grammar. The postcondition gate avoids all three.
