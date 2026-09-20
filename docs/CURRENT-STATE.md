# Current state

## Phase

**Specification baseline / repository bootstrap.**

The repository currently defines working rules, documentation and specification areas, upstream pinning, and the first research tasks. There is no production C++ implementation, build configuration, dependency selection, or published API.

## Active starting tasks

- `QDCXX-001`: repository and workflow bootstrap.
- `QDCXX-002`: verify and document the pinned Java QD upstream.
- `QDCXX-003`: analyze XML schema and DataScheme semantics without implementing them in C++.
- `QDCXX-004`: inventory QTP framing and protocol boundaries.

## Near-term exit criteria

This phase is complete when the repository workflow is reproducible, upstream provenance is verified, initial schema and QTP inventories are reviewable and source-linked, and the next implementation tasks can be scoped from written contracts rather than assumptions.

## Known non-goals for now

- C++ source code or generated code;
- CMake or dependency-manager configuration;
- selection of Asio or another transport runtime;
- performance claims;
- a stable public C++ API.

Update this file whenever the project phase, active work, or major known limitation changes.
