# ADR-0008 — Schedule and timezone data ownership

[Русская версия](ADR-0008-schedule-timezone-data-ownership.ru.md). English is authoritative.

Status: **Accepted; selected provider/data boundary qualified.** This decision establishes ownership and dependency boundaries. It does not implement the production Schedule parser, day/session construction, filtering, or a complete client.

## Decision

Use a private, pinned [Howard Hinnant date/tz v3.0.5](https://github.com/HowardHinnant/date/releases/tag/v3.0.5) provider at commit `179a6b921ccd6179b5b04dce812a89f71a51b4df`, with [IANA TZDB 2026f](https://www.iana.org/time-zones) as the selected timezone dataset. The upstream source archive SHA-256 is `ef786edc203daec76475825640b3af247bd08e31fc52217e5ce8f76107b4bb05`; the IANA archive SHA-256 is `01b4a91cec5167ed48c11e8ab78f2048cead78ce94b0cafcad0ae8c5a3044d3d`. Preserve the provider's per-file MIT notices and IANA's [data license](https://data.iana.org/time-zones/tzdb-2026f/LICENSE).

The [QD market calendar resource](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/resources/com/dxfeed/schedule/schedule.properties) remains an independently owned, versioned input. Calendar definitions and timezone transitions are separate data domains. The Java 8/TZDB 2024a behavior remains a compatibility reference; differences introduced by TZDB 2026f must be treated as data-version differences, separately from Calendar and Schedule algorithm compatibility.

## Qualified provider boundary

The private profile uses one provider generation per process. Initialization accepts an explicit data root, admits a fixed snapshot by checking its files against approved bytes, and parses that owned snapshot without reopening source files. It does not discover data through a home directory, operating system, CURL, remote source, or ambient environment. It does not reload timezone data at runtime. Updating the provider or TZDB is a maintainer-reviewed offline data update deployed by the application with a process restart.

All lazy provider work that can affect timezone results completes before the generation is published as `READY`. Queries then use the immutable generation and return scalar values. Initialization has a bounded terminal failure state: a failed attempt publishes no usable context, and later attempts in that process are refused. An invalid argument rejected before initialization begins does not consume the attempt. Static source/archive inspection alone does not establish isolation; the qualified profile isolates provider symbols and state in a private namespace and symbol domain.

The profile enforces these logical bounds: 11 files, 1,100,000 source bytes, 1,024 bytes per line, 35,000 lines, 12,000 source records, 600 zones, 400 links, 10,000 rules, and 32,768 cumulative native model insertions. It uses the ordinary standard allocator and has no configurable hard heap or RSS cap. Measured process peak memory is an observation, not a guaranteed limit. These provider bounds do not impose a one-context limit on future public `ScheduleDataContext` objects; calendar contexts and retained calendar generations are separately owned and bounded by their own contracts.

## Compatibility and implementation boundary

The complete pinned QD Schedule grammar and date-range behavior remain the target described in [Schedule rules and timezone compatibility](../../spec/schedule/RULES-AND-TIMEZONES.md). Preserve accepted timezone IDs and aliases, custom Java `GMT+HH:MM`/`GMT-HH:MM` IDs, explicit rejection of unknown IDs, Calendar gap/overlap policy, raw-offset/day anchoring, signed 64-bit Unix milliseconds, and the established calendar arithmetic. Do not silently fall back to UTC, machine-local data, or fixed offsets.

Production Schedule parsing, market-default merging, day and session generation, query/filter behavior, and their context/cache lifetimes remain separate implementation work. Provider qualification is not evidence that these client features are implemented. The selected boundary requires no application callback or timezone-work pumping during ordinary value queries. Market-calendar updates, when implemented, must validate a complete new generation before publishing it; previously returned immutable values retain their original generation. Timezone provider/data updates remain deployment/restart operations.

## Consequences

Maintainers own the pinned provider/data manifests, notices, and review of future provider and timezone-data updates. Applications choose when to deploy an approved profile and restart. The library has no background updater. Market calendar updates remain independent of timezone-data updates, and neither data source alone guarantees all future exchange holidays or every Java Calendar result.
