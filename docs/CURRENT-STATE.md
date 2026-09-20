# Current state

[Русская версия](CURRENT-STATE.ru.md). This English document is authoritative.

## Phase

**Specification baseline / repository bootstrap.**

The repository currently defines working and contribution rules, documentation and specification areas, upstream pinning, and the first research tasks. There is no production C++ implementation, build configuration, dependency selection, or published API.

## ADR approval and next tasks

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.

Completed: QDCXX-008 — JSON contract and examples; next: QDCXX-009 — ready, primitive encoding vectors. Earlier records below describe the state at each task's completion.

## Task history

- Completed: `QDCXX-001`, repository and workflow bootstrap.
- Completed: `QDCXX-002`, verified Java QD 3.355 provenance and documented reproducible upstream inspection.
- Completed: `QDCXX-003`, initial source-linked XML schema and DataScheme investigation; runtime oracle captures and normalized-model decisions remain follow-up work.
- Completed: `QDCXX-004`, source-linked QTP framing/protocol inventory and local DataScheme/wire-schema mapping; executable oracle validation remains follow-up work.
- Completed: `QDCXX-005`, Java schema oracle tooling and 17 captured subcases, with independent reproducibility and positive/negative verify checks.
- Completed: `QDCXX-006`, nine in-memory QTP P01/P03/P04/P05 captures, real connection requested-layout path and reproducibility/verify evidence.
- Completed: `QDCXX-007`, documentation-only normalized-model requirements, proposed ADR and scoped follow-up backlog. ADR-0001 remains **proposed**, pending owner decisions.

## Next milestone: executable oracle baseline

The schema portion now has an [executable oracle baseline](../tools/upstream/schema-oracle/README.md), scoped by [exact coverage](../tests/fixtures/schema/COVERAGE.md). Task-scoped Java tooling is permitted; production C++ and dependency selection remain deferred.

QDCXX-006 supplies protocol bytes/observations. QDCXX-007 now supplies reviewable model proposals, not accepted compatibility rules. Source findings alone do not establish executable compatibility. Remaining edge cases, negotiation, conversion matrices, reset/PART and dxFeed provider profiles stay open for later focused work.

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
- Upstream tests were skipped, and full schema/provider/property parity is not claimed. QTP captures were subsequently added by QDCXX-006 below; broader parser/rebuild edge cases, C++ implementation and C++ conformance remain unexecuted.

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

## Executable QTP baseline

Recorded for `QDCXX-006` on 2026-09-20, High reasoning:

- [Protocol harness](../tools/upstream/protocol-oracle/README.md) reuses locked-source clean builds, provenance and capture/verify conventions. All builds on the shared checkout ran sequentially.
- [Nine cases](../tests/fixtures/protocol/COVERAGE.md) cover P01 (32 schedules), P03 both ID-remapping directions with data/subscription, P04 reordered/absent/alias-property fields, and P05 none/zero/x/absent-z requests using real MessageAdapterConnection parser/composer paths.
- [Exact captures and separate semantic expectations](../tests/fixtures/protocol/expected/) preserve bytes, callbacks, raw values, local slots and chunk schedules. Fragmentation preserves ordered records while callback batching differs. Zero original fields selects all fields; nonempty absent-z selects zero outgoing fields but still delivers a record.
- [Validation](../tests/fixtures/protocol/VALIDATION.md) records independent clean-build reproduction and negative/positive verify with unchanged expectations. Metadata identifies tool/input source hashes, upstream, commands, JDK and providers.
- [Measured specification](../spec/protocol/CAPTURES.md) bounds the observations. P06 conversion, P09 negotiation, malformed/reset/PART, auth, sockets and timing remain pending; P05 metadata instrumentation is explicit. No QDCXX-007 or C++ work was performed.

## Normalized-model proposal

Recorded for `QDCXX-007` on 2026-09-21, High reasoning:

- [Requirements](../spec/schema/NORMALIZED-MODEL.md) separate source/overlay provenance, resolved local schema and directional wire descriptions/state. Worked examples use actual subcases; source-derived VOID wire omission is labeled separately.
- [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.md) is **proposed**: retain measured copy-name results within a pinned profile, retain a Java slot projection without fixing C++ storage, and replace accidental BYTE/SHORT null dereferences with explicit diagnostics only if approved. Versioned JSON is a recommendation, not an adopted format/dependency.
- [Backlog](NEXT-STEPS.md) identifies decision gates and missing probes for importer/encoding work. Dynamic requested layouts, provider/property parity and remaining SC/QT questions stay open.
- [Documentation validation](QDCXX-007-VALIDATION.md) records evidence/link/status checks and bilingual review. No Java rerun, new runtime capture, C++ implementation, importer or dependency was added; existing fixtures remain unchanged. Completion of the documentation task does not accept the ADR.

## JSON contract baseline

Completed QDCXX-008 on 2026-09-21 with High reasoning. The [version 1 contract](../spec/schema/JSON-CONTRACT.md) defines resolved local schemas, scoped lookups, logical fields, Java slot ownership, ordered provenance references and structured construction diagnostics under accepted ADR-0001. [Coverage](../spec/schema/examples/COVERAGE.md) maps all 17 subcases to designed JSON projections; [validation](../spec/schema/examples/VALIDATION.md) records syntax, invariants, TSV comparisons and negative checks. EN/RU documents are synchronized. Original schema/protocol goldens are unchanged. No importer, production C++, dependency, Java rerun or QDCXX-009 work was added. Native construction, broader profiles and session serialization remain open.
