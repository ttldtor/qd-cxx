# Trading schedule rules

[Русская версия](SCHEDULE-RULES.ru.md). English is authoritative.

The optional `qdcxx_schedule` library provides owning `Schedule` identities, complete parsing of the
pinned trading-hours grammar, explicit calendar contexts and typed calendar-default updates.
[Day/Session queries](SCHEDULE-QUERIES.md) use an explicit ready timezone capability; parser-only
contexts perform no provider initialization. Filters remain a separate capability.

## Use an explicit context

An application creates a `ScheduleDataContext`, then uses the familiar factory directly:

```cpp
#include <qd/schedule/schedule.hpp>

int main() {
    using namespace org::ttldtor::qdcxx::schedule;
    auto context = ScheduleDataContext::create();
    if (!context) {
        return 1;
    }
    auto schedule = Schedule::getInstance(*context, u"XNYS()");
    if (!schedule) {
        return 1;
    }
    const auto name = schedule->getName();
    const auto zoneId = schedule->getTimeZoneId();
    return name == u"NYSE" && zoneId == u"America/New_York" ? 0 : 1;
}
```

The context uses the embedded [pinned market defaults](../resources/schedule/manifest.json).
Initialization and getters perform no file, environment, network, callback or worker-progress work.
There is no global default context. Context copies share one domain; separate `create` calls are
independent. Schedule copies retain their identity and context, including after the original context
handle is destroyed. Moves preserve the source handle. Assignment to one handle needs external
synchronization when other threads access that same handle object.

`getName()` and `getTimeZoneId()` return independent owning UTF-16 copies from a completely published
generation. Each call pins its own generation; two separate getter calls can straddle an update.
`getDefinition()` borrows the exact selected UTF-16 definition from the schedule identity and remains
valid while that handle is alive and unchanged. A result-copy allocation can throw `std::bad_alloc`.

## Instrument portions and grammar

`Schedule::getInstance(context, text)` selects the first portion using the production preceding `)` or
`;` boundary. The overload with a final `venue` argument selects the first exact named portion or
returns `MISSING_VENUE`. `getTradingVenues(text)` uses the reference's separate ASCII-word regex-find
semantics, preserves order and duplicates, and does not validate each returned schedule. These three
operations deliberately do not share an invented stricter instrument tokenizer.

The [grammar specification](../spec/schedule/RULES-AND-TIMEZONES.md#schedule-text-and-defaults) covers
venue inheritance, chained/repeated properties, weekday fill, minute/second session groups, day shifts,
day/reset boundaries, named date-list algebra and ordered per-date strategies. The parser constructs
owned local rule definitions for the entire pinned source family. It does not reduce schedules to
one UTC interval. Unknown ordinary properties and the source's unknown-strategy fallback paths remain
accepted. An otherwise invalid recognized property is refused as in the production constructor.

The comparison corpus captures actual
[Schedule factories and constructed models](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/schedule/Schedule.java),
including unusual direct `TimeDef` fields, zero-duration definitions and property ordering. Native
limits can refuse additional construction as `RESOURCE_LIMIT`; they do not silently normalize source
values or change an accepted grammar rule.

## Calendar generations and updates

`CalendarDefaults` supplies a typed signed millisecond comparison version and complete ordered
`CalendarDefaultsEntry` values. Entry kinds correspond to `hd`, `sd`, `xd` and `tv`. Names and values
are borrowed only during `setDefaults`; the context copies all retained data. Named date-list
references resolve only against earlier entries in the same namespace. Duplicate named entries and
invalid venues are refused. The baseline resource has version `1774285200000` and source date
`20260323-170000+0000`.

```cpp
using namespace org::ttldtor::qdcxx::schedule;
const CalendarDefaultsEntry entries[] = {
    {CalendarEntryKind::HOLIDAY_DAYS, u"Closed", u"20261225"},
    {CalendarEntryKind::VENUE, u"Local", u"name=Local market;tz=UTC;hd=Closed;0=09001700"}
};
auto result = context.setDefaults({1780000000000LL, entries});
```

An update validates the complete candidate and rebuilds every live identity before one publication.
Unused cached identities may be discarded in that publication. Invalid data, exhausted limits,
allocation failures and a conflicting transaction leave the published defaults and live metadata
unchanged. Validated older versions return `OLDER_IGNORED`; equal-version changed defaults are
eligible for replacement. Old immutable reader pins keep their original models and calendar data.
This atomic C++ boundary is stronger than the reference's sequential global reinitialization.

The API does **not** advertise the Java `setDefaults(byte[])` resource loader or a raw-byte
`TimeFormat.GMT` date-header parser. Applications provide the typed version explicitly. The embedded
baseline is a separately hash-verified resource; it is not a permissive runtime loader for other
files. Calendar updates do not change timezone-provider data. The provider's offline deployment and
restart policy remains in [ADR-0008](decisions/ADR-0008-schedule-timezone-data-ownership.md).

## Identity profile and dependency boundary

The deterministic identity profile contains all 631 named IDs accepted by the actual pinned
Java 8/TZDB 2024a `TimeUtil` reference, plus normalized `GMT+HH:MM` and `GMT-HH:MM` custom IDs with
hours 00–23 and minutes 00–59. Requested spelling and signed zero forms are preserved. Unknown IDs,
case changes and shortened custom offsets are refused. This table validates identities; it contains
no timezone transitions and is not a claim that IANA aliases alone implement Java ID policy.

The [manifest](../resources/schedule/manifest.json) records source/runtime/data hashes. No Java runtime
is required to build or use the C++ library. The selected production transition dataset remains IANA
TZDB2026f with the maintained controlled private provider. The target links the shared
[timezone backend](TIME-ZONE-DATA.md), while parser-only contexts perform no native initialization.
The build requires no acquisition, CURL or runtime SDK.

## Finite resources and concurrency

`ScheduleLimits` configures separate finite input, construction-work, staging and retention scopes.
They count logical owned fields and modeled work, not exact heap or RSS consumption. Standard
allocator/control-block/container-capacity overhead and caller-owned input/output storage are outside
the logical byte counter. Compiled resource/identity backing is fixed and shared across contexts.

| Setting | Default and scope |
| --- | --- |
| `inputUnits`, `defaultUnits` | 65,536 units per instrument/venue; 1,048,576 combined typed-default UTF-16 units |
| `defaultEntries`, `properties` | 4,096 named entries; 1,024 distinct merged properties |
| `dateValues`, `dayDefinitions`, `sessionsPerDay` | 100,000 cumulative date-value construction credits; 8,192 day constructions; 1,024 sessions per abstract day including gaps |
| `work`, `stagingBytes` | 16,000,000 cumulative inspection/copy credits; 33,554,432 cumulative logical construction bytes per operation |
| `retainedBytes` | 67,108,864 logical bytes across the shared retention domain |
| `retainedGenerations`, `retainedSchedules`, `retainedRuleModels`, `retainedBundles` | 4 calendar generations; 256 identities; 1,024 identity/generation models; 16 publication bundles, including candidates/reader pins |
| `inactiveCacheTarget` | 64 inactive identities after explicit cache collection; all entries always remain under `retainedSchedules` |

Removed/replaced construction does not refund work credits. Retained charges survive context-handle
destruction while models are pinned. `getUsage()` reports this shared domain; extra copies of an
already retained handle are not distinct identities. Limits must admit the built-in baseline before
a context can be created. Updates also need room for a complete staging generation while old values
remain live.

Factories may first collect inactive cache entries through a complete publication that preserves the
calendar and all live identities. A later factory failure can therefore reduce cache/resource
accounting; it does not promise complete cache rollback. Typed default updates have the stronger
single-commit failure guarantee described above. Construction and retired-model destruction occur
outside the brief commit lock. No application callback or user-owned destructor is invoked there.

Concurrent conflicting writers return `CONCURRENT_CHANGE` after one bounded transaction rather than
silently retrying indefinitely. Independent immutable readers are safe. There are no background
threads, automatic updates or application progress pumps.

## Build and installed use

Select `ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON`; it defaults to OFF. The selected target is
`qdcxx_schedule`. A separate installed consumer uses:

```cmake
find_package(qdcxx CONFIG REQUIRED COMPONENTS schedule)
target_link_libraries(app PRIVATE qdcxx_schedule)
```

No private source/helper headers are required. OFF performs no schedule-resource generation or
dependency acquisition. The package installs the inspectable calendar/identity manifest and MPL2
calendar notices alongside the normal project license. Build/run the
[complete console example](../examples/schedule_rules/README.md) for a finite client application.
The [production oracle](../tools/upstream/schedule-oracle/README.md) is optional developer tooling.
