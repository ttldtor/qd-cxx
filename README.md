# qd-cxx

`qd-cxx` is a C++23 reimplementation of [devexperts/QD](https://github.com/devexperts/QD), designed to remain behaviorally compatible with the Java upstream while using an architecture appropriate for modern C++.

The project begins with specifications and conformance evidence, not a line-by-line port. Java QD is treated as a compatibility oracle; its internal architecture is reference material, not a mandatory design.

## Current phase

The specification and Java-oracle baseline now includes the first bounded C++23 compact int/long component. See [build/API instructions](docs/CPP-BUILD.md) ([Russian](docs/CPP-BUILD.ru.md)) and [conformance validation](tests/conformance/encoding/VALIDATION.md) ([Russian](tests/conformance/encoding/VALIDATION.ru.md)). The API is provisional; UTFString, importer and transport require later approved tasks.

Start with:

1. [`docs/CURRENT-STATE.md`](docs/CURRENT-STATE.md)
2. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
3. [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md)
4. `tasks/README.md`
5. The assigned task under `tasks/`

Contributor workflow and documentation-only validation are described in [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Initial workflow

1. Pin and inspect the Java upstream recorded in `upstream.lock`.
2. Extract observable contracts into `spec/` with source traceability.
3. Capture representative Java behavior as reviewed fixtures.
4. Add C++ components only after their boundaries and acceptance evidence are understood.
