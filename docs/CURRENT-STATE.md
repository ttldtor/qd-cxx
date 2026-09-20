# Current state

## Phase

**Specification baseline / repository bootstrap.**

The repository currently defines working and contribution rules, documentation and specification areas, upstream pinning, and the first research tasks. There is no production C++ implementation, build configuration, dependency selection, or published API.

## Task status

- Completed: `QDCXX-001`, repository and workflow bootstrap.
- Completed: `QDCXX-002`, verified Java QD 3.355 provenance and documented reproducible upstream inspection.
- Completed: `QDCXX-003`, initial source-linked XML schema and DataScheme investigation; runtime oracle captures and normalized-model decisions remain follow-up work.
- Completed: `QDCXX-004`, source-linked QTP framing/protocol inventory and local DataScheme/wire-schema mapping; executable oracle validation remains follow-up work.
- Completed: `QDCXX-005`, Java schema oracle tooling and 17 captured subcases, with independent reproducibility and positive/negative verify checks.
- Proposed: `QDCXX-006`, in-memory QTP captures, dependent on QDCXX-005 tooling.
- Proposed: `QDCXX-007`, normalized-model requirements and proposed compatibility ADR, dependent on QDCXX-005/006 evidence.

## Next milestone: executable oracle baseline

The schema portion now has an [executable oracle baseline](../tools/upstream/schema-oracle/README.md), scoped by [exact coverage](../tests/fixtures/schema/COVERAGE.md). Task-scoped Java tooling is permitted; production C++ and dependency selection remain deferred.

QDCXX-006 protocol bytes/observations is a proposed next session, then QDCXX-007 model proposals. Neither was started in QDCXX-005. Source findings alone do not establish executable compatibility. Remaining edge cases, negotiation, conversion matrices, reset/PART and dxFeed provider profiles stay open for later focused work.

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

## Upstream baseline validation

Checked for `QDCXX-002` on 2026-09-20:

- [x] `https://github.com/devexperts/QD.git` is fetchable and contains the locked commit.
- [x] Annotated tag `qd-qds-3.355` peels to `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
- [x] The root POM and release notes at that commit identify version 3.355.
- [x] Exact detached-checkout and lock-update review procedures are documented in `docs/UPSTREAM.md`.
- [x] Relevant top-level modules are inventoried without attempting the schema or QTP semantic analysis assigned to later tasks.
- [x] The public-GitHub/internal-SCM metadata ambiguity is recorded in `docs/upstream/OPEN-QUESTIONS.md`.

Update this file whenever the project phase, active work, or major known limitation changes.

## Executable schema baseline

Recorded for `QDCXX-005` on 2026-09-20:

- [Java harness and Windows launcher](../tools/upstream/schema-oracle/README.md) verify the locked origin/commit/tag/version and source cleanliness, rebuild the external source with JDK 8/Maven, and use fresh JVMs with optional property overlays disabled.
- [17 subcases](../tests/fixtures/schema/COVERAGE.md) cover only S01/S04/S05/S07/S08/S15 portions: empty/no input, field ordering/disabled entities, index/VOID variants, aliases/lookups, copy timing/main-alias loss, storage slots and BYTE/SHORT rejection.
- [Validation evidence](../tests/fixtures/schema/VALIDATION.md) retains original inputs, raw observations, normalized expectations, exact commands and provenance. Two independent captures reproduce all expectations; verify rejects a temporary 0-to-999 count mutation and passes the unchanged baseline.
- Runtime confirms the source-predicted copy-main-alias quirk and distinct exception families. No unexplained semantic discrepancy remains in these subcases. The observation format is not a normalized-model decision.
- Existing Zulu JDK 8 matches the upstream enforcer requirement. Maven cache location and JAXB temporary working directory are explicit accommodations; no upstream source/POM patch or substitute QD binary is used.
- Upstream tests were skipped, and full schema/provider/property parity is not claimed. QTP captures, parser/rebuild edge cases, C++ implementation and C++ conformance remain unexecuted.

## Schema investigation baseline

Recorded for `QDCXX-003` on 2026-09-20:

- [XML/DataScheme inventory](../spec/schema/README.md) traces the locked Java source from resource loading through runtime construction and separates XML syntax, model semantics and dxFeed conventions.
- [Source inventory](../spec/schema/SOURCES.md) records immutable citations, relevant symbols and checkout verification.
- [Schema fixture proposals](../tests/fixtures/schema/README.md) identify 21 capture cases; no expected output or Java execution is claimed.
- [Schema open questions](upstream/OPEN-QUESTIONS.md#schema-investigation-qdcxx-003) cover parser/default/alias quirks, runtime failures, property/provider profiles, rebuild isolation and normalization scope.
- All five acceptance criteria for the initial investigation are addressed. No C++ code, build dependencies, normalized format or wire semantics were introduced.

## QTP investigation baseline

Recorded for `QDCXX-004` on 2026-09-20:

- [QTP boundary inventory](../spec/protocol/README.md) separates stream/frame/message/session/transport responsibilities, partial input, batching, negotiation, heartbeat, PART and error handling.
- [Local DataScheme and wire descriptions](../spec/protocol/SCHEMA-EXCHANGE.md) explains sender-ID to receiver-local-ID mapping by record name, ordered field names/types, requested outgoing layouts and differing schemas.
- [Locked source index](../spec/protocol/SOURCES.md), [encoding dependencies](../spec/encoding/README.md), [13 proposed capture families](../tests/fixtures/protocol/README.md) and [QT-01–QT-09 questions](upstream/OPEN-QUESTIONS.md#qtp-investigation-qdcxx-004) distinguish source facts, documentation/test assertions, inferences and unknowns.
- Source review identified non-atomic record-description errors, message-local symbol state, retained writer cache/PART state across resetSession, and protocol-hook documentation/source ordering disagreement. These need captures and compatibility review rather than silent normalization.
- Verified locked upstream checkout and documentation links/diff; no Java build/test or runtime capture. All five inventory acceptance criteria are addressed. No production C++, dependencies, wire change or networking choice was added; byte-level compatibility is not yet demonstrated.
