# Current state

## Phase

**Specification baseline / repository bootstrap.**

The repository currently defines working and contribution rules, documentation and specification areas, upstream pinning, and the first research tasks. There is no production C++ implementation, build configuration, dependency selection, or published API.

## Task status

- Completed: `QDCXX-001`, repository and workflow bootstrap.
- Ready: `QDCXX-002`, verify and document the pinned Java QD upstream.
- Ready: `QDCXX-003`, analyze XML schema and DataScheme semantics without implementing them in C++.
- Ready: `QDCXX-004`, inventory QTP framing and protocol boundaries.

## Near-term exit criteria

This phase is complete when the repository workflow is reproducible, upstream provenance is verified, initial schema and QTP inventories are reviewable and source-linked, and the next implementation tasks can be scoped from written contracts rather than assumptions.

## Known non-goals for now

- C++ source code or generated code;
- CMake or dependency-manager configuration;
- selection of Asio or another transport runtime;
- performance claims;
- a stable public C++ API.

## Bootstrap validation

Checked for `QDCXX-001` on 2026-09-20:

- [x] Repository entry points (`README.md`, `AGENTS.md`, `CONTRIBUTING.md`, and `tasks/README.md`) exist and describe how to discover current work and project rules.
- [x] Documented repository areas under `docs/`, `spec/`, `tasks/`, `tests/`, and `tools/upstream/` exist.
- [x] Relative Markdown links in tracked documentation resolve to repository files.
- [x] Documentation-only validation and local-file ignore rules are recorded without introducing production code, build configuration, or dependencies.
- [x] Deferred architecture and dependency choices remain explicitly undecided in `docs/ARCHITECTURE.md`; upstream semantic unknowns are listed in `docs/upstream/OPEN-QUESTIONS.md` and assigned research is scoped in `tasks/`.

Update this file whenever the project phase, active work, or major known limitation changes.
