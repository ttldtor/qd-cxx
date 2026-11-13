# qd-cxx documentation {#documentation}

[Русский](index.ru.md) · English

Use qd-cxx as a C++23 library for market-data clients or through its offline inspection tools.
Start with a concrete task;
[capabilities and limits](CURRENT-STATE.md) identify the supported profiles.

1. **[Getting started](getting-started.md)** — build, install and run a small library consumer.
2. **[Practical guides](guides.md)** — receive market data, query snapshots, work with instruments and inspect tapes.
3. **[API reference](api.md)** — find headers, link targets, ownership and error contracts.
4. **[Architecture and protocols](../spec/README.md)** — understand components and wire behavior.

[Developer documentation](developer.md) contains build options, verification profiles and conformance
results. [Compatibility policy](COMPATIBILITY.md) explains which Java QD behavior the library preserves.

- [Schedule days and sessions](SCHEDULE-QUERIES.md): owning generation snapshots, calendar queries, limits and actual oracle.
- [Explicit timezone data](TIME-ZONE-DATA.md): shared CORE binding, optional backend, pinned offline dependency and relocation.
