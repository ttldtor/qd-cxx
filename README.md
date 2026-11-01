# qd-cxx

[Русский](README.ru.md) · English

`qd-cxx` is a C++23 library and offline inspection tools implementing a bounded subset of
[devexperts/QD](https://github.com/devexperts/QD). Java QD at the revision in [upstream.lock](upstream.lock)
is the compatibility reference. Supported profiles and limits are explicit; full QD compatibility is not claimed.

## Documentation

1. [Getting started](docs/getting-started.md) — install the library and run a small consumer.
2. [Practical guides](docs/guides.md) — tapes, schemas, events and controlled STREAM use.
3. [API reference](docs/api.md) — public headers, link targets and ownership contracts.
4. [Architecture and protocols](spec/README.md) — component responsibilities and wire behavior.

[Current capabilities](docs/CURRENT-STATE.md) identify implemented profiles.
[Developer documentation](docs/developer.md) contains build options, tools and verification results.
[Build the documentation](tools/docs/README.md) to browse the same reading paths locally.
