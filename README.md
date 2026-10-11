# qd-cxx

`qd-cxx` is a C++23 implementation of a bounded, behaviorally compatible subset of
[devexperts/QD](https://github.com/devexperts/QD). The project supplies reusable libraries and
offline inspection utilities. Java QD at the revision in [upstream.lock](upstream.lock) is the
compatibility oracle; its internal architecture does not prescribe the C++ design.

The implementation includes compact integers, exact UTF-16 strings and symbols, bounded QTP framing
and record binding, selected STREAM/RAW tape profiles, schema-bound Quote/Profile mappings, optional
native JSON schema loading and an offline Java schema exporter. Full QD, tape and transport
compatibility is not claimed.

Start with:

1. [Build and API instructions](docs/CPP-BUILD.md) ([Russian](docs/CPP-BUILD.ru.md)).
2. [Current capabilities and limits](docs/CURRENT-STATE.md) ([Russian](docs/CURRENT-STATE.ru.md)).
3. [Architecture](docs/ARCHITECTURE.md) and [compatibility policy](docs/COMPATIBILITY.md).
4. [Schema export and consumption](tools/schema-export/README.md) ([Russian](tools/schema-export/README.ru.md)).
5. [Product roadmap](docs/NEXT-STEPS.md) ([Russian](docs/NEXT-STEPS.ru.md)).

[CONTRIBUTING.md](CONTRIBUTING.md) describes contribution and validation requirements.
Normal configure/build/CTest uses CMake and C++ without Python. The
[native developer tools](tools/dev/README.md) ([Russian](tools/dev/README.ru.md)) provide UTC logging
and portable formatting; optional PowerShell conveniences remain available. The
[migration ledger](docs/PYTHON-MIGRATION.md) describes remaining manual Python workflows.

Observable contracts live in `spec/`, architecture and compatibility decisions in `docs/`, and
reviewed inputs, expected results and conformance checks in `tests/`. Protocol and schema changes
must update the corresponding contracts and evidence together.
