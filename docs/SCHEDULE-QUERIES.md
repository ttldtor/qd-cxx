# Schedule days and sessions

[Русская версия](SCHEDULE-QUERIES.ru.md). English is authoritative.

`qdcxx_schedule` constructs owning `Day` and `Session` values from the complete
[rule grammar](SCHEDULE-RULES.md). Queries use the shared
[explicit timezone data component](TIME-ZONE-DATA.md); parser-only contexts remain file-free.

```cpp
#include <qd/schedule/schedule.hpp>

int main(int argc, char** argv) {
    using namespace org::ttldtor::qdcxx::schedule;

    if (argc != 2) {
        return 2;
    }

    auto data = ScheduleTimeZoneData::initialize(std::filesystem::path(argv[1]));

    if (!data) {
        return 1;
    }

    auto context = ScheduleDataContext::create(*data);

    if (!context) {
        return 1;
    }

    auto schedule = Schedule::getInstance(*context, u"XNYS()");

    if (!schedule) {
        return 1;
    }

    auto day = schedule->getDayByYearMonthDay(20241104);

    if (!day) {
        return 1;
    }

    return day->isTrading() ? 0 : 1;
}
```

The complete [sample](../examples/schedule_queries/README.md) accepts a native Windows wide path,
prints session values and provides input-free EN/RU help. The data capability can be copied into
several independent calendar contexts. Initialization is a one-attempt process operation; reuse the
successful capability rather than initializing a second native registry.

## Identity, snapshots and updates

A `Schedule` is a live identity. `Day` owns that identity and an immutable parsed calendar generation;
`Session` owns a Day and an index into its immutable records. There is no ownership cycle. Copies and
moves retain ownership without duplicating date/session models; moved-from handles remain usable.
Metadata and direct queries through Schedule observe its current atomically published generation.
A returned Day's values and navigation stay on its original generation. `Day::getSchedule()` returns
an owning live Schedule, so a subsequent direct query through it can observe a newer generation.

The Java source keeps old Day fields but navigates through the live Schedule after an update. C++
explicitly pins navigation too, extending snapshot stability. The actual hot-update oracle preserves
Java's old/new results and the conformance test checks this distinction, rather than rewriting Java.
Day equality follows the source identity plus day-id rule even across calendar generations; equal
Day handles can therefore contain different versioned values. Session equality additionally checks
type and endpoints. Use `getDefaultsVersion()` when the generation matters.

Typed `setDefaults` rebuilds all retained identities and commits one complete bundle. It changes no
live identity, metadata or defaults generation on failure. Ordinary factories can evict inactive
cache entries before a later refusal; this is not a rollback guarantee for cache/accounting state.
A Day or Session pins its Schedule as a live handle, preventing accidental eviction of that identity.

## Calendar behavior and ranges

`DayFilter` combines a weekday bit mask (Monday is bit 1, Sunday bit 7; zero ignores
weekday) with optional holiday, short-day and trading flags. `SessionFilter` combines
an optional type and trading flag. Every specified condition must match. Named values
cover the source's common filters; contradictory conditions simply match nothing.
Empty and inverted sessions participate in filtering in their stored order.

The `find` methods return an empty optional when no match is found, and preserve typed
query failures separately. The corresponding filtered `get` methods return `NO_MATCH`
for an exhausted search; this is distinct from the `NO_SESSION` time-lookup failure.
Day navigation excludes the current day and searches at most
366 labels in the requested direction. Session navigation first searches the remaining
records of the current day, then at most 366 neighbouring days. Day-local first/last
searches inspect only that day. Navigation retains the originating calendar snapshot.
The query work and staging budgets are cumulative across each navigation, so a finite
resource limit or the supported day-id range can stop a search before its 366-day bound.

The implementation preserves noon-anchor Calendar construction, proleptic DayUtil labels, complete
weekday/date overrides, holiday and short-day strategies, early close and joined non-trading days.
Nonexistent local times move forward through the gap; overlaps choose the later occurrence. A
skipped civil date keeps its original Schedule label. Zero-duration trading records remain present
and trading by type. `Session::isEmpty()` means `start >= end`; direct no-trading definitions can
produce inverted intervals during a DST gap. Empty/inverted intervals contain no instant.

| Factory | Inclusive argument range |
| --- | --- |
| `getDayById` | -719160 through 2932894 |
| `getDayByYearMonthDay` | 10103 through 99991229; invalid fields use source normalization |
| `getDayByTime` / `getSessionByTime` | -62135424000000 through 253402128000000 Unix milliseconds |

The upper time argument can return label 99991230 although that label is refused by the YMD/id
factories. Previous/next Day applies the direct day-id range. Session navigation preserves source
record ordering, including empty records, then crosses to the adjacent pinned Day. Membership is
half-open `[start,end)`. `Day::getSessionByTime` returns `NO_SESSION` when no record contains the instant.

One model-constructor raw-reference instant is captured outside publication locks. An absent override
captures the current clock once for a new model; cache hits and all date queries do not recapture it.
One typed defaults rebuild uses one instant for all candidate models. Set
`ScheduleQueryOptions::rawOffsetReferenceTime` for deterministic comparison or snapshot contexts.
This reference affects constructor raw metadata/noon anchoring, not the range of queried dates.
Pre-1900 actual offset uses the final standard base, independently of a historical constructor reference.

## Finite work and retention

`ScheduleLimits` separately bounds query inspection/conversion work, cumulative staging bytes,
records per Day, retained Days, retained session records, weak cache entries and retained cache
snapshots. A zero `dayCacheEntries` disables caching. Old generation pins consume the same context
ledger; callers must release handles or choose adequate limits before constructing more models.
Cache contention may return a fully leased uncached Day. No worker, callback, implicit file load or
unbounded retry occurs. Every search charges `queryWork`; a non-containing time-lookup result returns
`NO_SESSION` under the containing-day postcondition.
All owning query factories return typed resource/allocation/range/data errors. `Day::getSessions()`
creates a caller-owned vector bounded by that Day's record count and can throw `std::bad_alloc`;
`getFirstSession` and `getLastSession` copy only handles.

Logical model accounting excludes standard allocator/control-block/capacity overhead and the shared
native timezone backing; it is not a configurable exact heap or RSS cap. Arbitrary caller-owned
handle/vector copies are outside context retention counts. Failed candidates publish no partial
Day/Session. Initialization and standard allocator overhead are described separately in the timezone guide.

## Compatibility evidence

The [actual Java oracle](../tools/upstream/schedule-query-oracle/README.md) records 2303 projections
from pinned production Schedule/Day/Session classes, all 631 accepted identities and complete SystemV
transition facts. The [unaltered Java fixture](../tests/fixtures/schedule-query/oracle.tsv) uses Java8
TZDB2024a. Exactly 36 [contextual data expectations](../tests/fixtures/schedule-query/tzdb2026f-provenance.json)
classify selected Canada, Cuba, Morocco and Paraguay outcomes from the installed TZDB2026f source.
These are per-case data/reference differences, not blanket tolerances or confirmed Java bugs.
The corpus includes DST gaps/overlaps, historical/pre-1900 and 1582 labels, endpoint normalization,
60000-day shifts, session boundaries, strategies and hot updates. It does not claim exhaustive
comparison of every timezone transition in both datasets.

The [containing-day correction](../tests/fixtures/schedule-query/containing-day-correction.json)
records a confirmed public-contract violation separately from timezone data differences. Java's
`getDayByTime(0)` for `(tz=UTC;td=1234567;0=d00000000)` returns empty Day1, which cannot contain0,
despite the method's containing-day Javadoc. C++ checks the postcondition after the same two-pass
lookup and returns `NO_SESSION`. Direct id/YMD queries still return valid empty Day models; session
lookup already refuses this instant. Both actual Java witnesses and pre-correction native results
are retained; the Java fixture is not rewritten. No extra search or parser validation is introduced.
