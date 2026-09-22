# Current state

[Русская версия](CURRENT-STATE.ru.md). English is authoritative.


## Owner-requested naming migration

Current C++ namespaces use `org::ttldtor::qdcxx`, project CMake targets use `qdcxx_`, and owned
cache variables/options use `ORG_TTLDTOR_QDCXX_`. Include paths `qd/...` and fixed output identifiers are unchanged; the utility executable
is now `qdcxx-qtp-inspect`. [Migration and future rules](NAMING.md).
The preceding human-output work is now committed as `e1e19e5`; earlier no-commit statements below
are historical. This naming migration adds no independent review claim for that commit.


## Current status

QDCXX-019 research is complete and ready for independent review, without a commit. Real Java
FileWriter/FileReader captures cover default/constrained BINARY, TEXT and truncated EOF; two clean
runs match all 22 raw artifacts, 19 negative cases and negative/baseline Verify behave as expected.
[Inventory, gap table and proposed next profile](../spec/protocol/TAPE-FILES.md),
[author validation](../tests/fixtures/protocol/tape/VALIDATION.md). C++ and inspector capabilities,
historical fixtures and upstream.lock are unchanged. New oracle work uses Java/PowerShell; no new
Python script/dependency. The bounded future reader and runtime dependencies still require approval.

QDCXX-010–016 are implemented. QDCXX-016 is **done**, independently reviewed and committed as
`8c9e82223e9bd783e281babf3e791cfaf4767555`. The subsequent commits `9562ec2`, `94f3542`
and `128b8bf` update comment guidance, comments, licensing and copyright headers.
The review checks below predate those follow-ups; this status update does not claim a new test run.

Implemented: bounded C++23 compact int/long, UTFString, incremental QTP framing, raw DESCRIBE_RECORDS,
validated local type-8 schemas/static binding, message-local EMPTY_SET symbols and complete STREAM_DATA
payload decoding with a static context and zero raw flags. APIs remain provisional. Transport, dynamic
sessions/negotiation, importer, writer, conversions and broader event profiles remain unimplemented.

### QDCXX-016 evidence and review

The author's [validation report](../tests/conformance/protocol/stream-data/VALIDATION.md) records
35 production Java observations, two matching clean captures and negative/baseline Verify.
Independent review reran MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan, all six CTest suites,
production-only build, 15 negative cases and baseline, formatting and evidence checks.
No errors were detected in those checks. Java captures were not independently rerun.
Local review logs remain in `.work/review-016/` (untracked; not portable repository evidence).
Historical fixtures and `upstream.lock` were preserved.

QDCXX-015 review is closed, including strict fixture validation and **14** negative cases;
committed baseline: `8bc9838efa3be8c157ec7d9a9b13e476baeff385`. Its follow-up review reran
MSVC Release, five suites, the original unknown-row reproduction, negatives and evidence audit;
other final follow-up profiles remain author-run results.

### QDCXX-017 and QDCXX-018 reviewed

QDCXX-017 is done and committed as `a0944f0ac361202b298d2b2acf1edc6adb0e1f5a`.
Its prior independent review reran MSVC Release and GCC ASan+UBSan (eight suites), 17 mutations
and baseline, hash/diff checks. No errors were detected in those checks. Other profiles and Java
captures remain author checks; [historical author evidence](../tests/conformance/protocol/offline-inspection/VALIDATION.md)
is preserved. The accepted API-GATE only added charged record/field counters.

QDCXX-018 adds opt-in `--output-format human`, safe lossless quoted UTF-8, explicit local labels,
bounded encoded-byte accounting and separate tool-only formatting. Default/explicit TSV, stderr,
wire profile and decoder libraries are unchanged. [Author validation](../tests/conformance/protocol/human-inspection/VALIDATION.md)
records the seven-profile, ten-suite matrix, production tool ON/OFF, formatting, 9 human and 17 legacy
mutations/baselines, historical hashes and bilingual checks. QDCXX-018 is independently reviewed and
committed as `e1e19e5`: prior review reran MSVC Release and GCC ASan+UBSan, ten suites each,
format Check and hash/diff checks. Migration review at `4cb1d8c` reran MSVC Release ten suites only;
other author checks remain author checks. This status correction does not claim new review runs.
This is raw QTP inspection, not QD tape or general dxFeed support.
Importer, compression/time files, transport, dynamic sessions and broader wire types remain excluded.

## Historical completion records

The sections below preserve the state at each earlier handoff, not today's scope or task status.
Statements such as “no commit”, “proposed”, and earlier missing components describe those milestones.
The current summary above takes precedence.

## Completed: bounded QTP symbol reader

QDCXX-015 — done, High; handed off for review without a commit. Java evidence preceded C++:
156 inputs, 693 primary reads plus state probes, two equal clean captures and negative/baseline Verify.
The qd_encoding reader owns exact UTF-16/cipher/raw flags, keeps explicit message-local repeat/MRU
state and uses bounded atomic success / terminal failure. Real composer/parser integration confirms
repeats and reset across two messages. Final MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan,
five CTest suites, production-only no-fetch, formatting and ten negative fixture checks pass.
Historical fixtures/upstream.lock remain unchanged; EN/RU are synchronized.
[Validation and limits](../tests/conformance/encoding/symbols/VALIDATION.md). API remains provisional; no writer, event decoder, negotiation,
importer, incoming-ID installation, PART or transport. The next implementation task needs approval.

## Completed: bounded static incoming binding

QDCXX-014 — done, High. Production BinaryQTPParser evidence came first:
13 cases, two identical clean captures and negative/baseline Verify. The bounded planner accepts a
validated immutable loader-independent local subset, returns owned ordered type-8 targets/skips and
keeps unknown records distinct from invalid input. MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan,
BUILD_TESTING=OFF, four-target regressions and 11 negative fixture checks pass. Historical fixtures
and upstream.lock are unchanged. [EN/RU validation](../tests/conformance/protocol/binding/VALIDATION.md).
No importer, event decoder, conversions, session table or transport was added.

[Русская версия](CURRENT-STATE.ru.md). This English document is authoritative.

## Completed: DESCRIBE_RECORDS

QDCXX-013 — done, High. Java evidence was captured first: 81 bounded observations,
two independent clean captures and negative/positive Verify. The complete type-2 payload parser
returns owned ordered raw descriptions, exact UTF-16 and full type IDs, preserves completed records
before later errors and enforces explicit budgets. Structural validity, serialization operations and
local-field construction remain separate. MSVC/ClangCL/WSL GCC Debug/Release and GCC ASan+UBSan,
primitive/framing regressions, BUILD_TESTING=OFF and negative fixture checks pass.
[EN/RU results](../tests/conformance/protocol/descriptions/VALIDATION.md). Historical oracle files and upstream.lock are unchanged.
No local binding, event decoding, transport or frame writer was added.

## Completed: QTP framing

QDCXX-012 — done, High. First captured 17 Java P02 subcases / 117 schedules,
two independent captures and negative/baseline Verify; then implemented a bounded incremental C++
reader with opaque payload, retained incomplete input, exact accepted counts, owned frames,
backpressure and explicit finish/terminal error. All MSVC/ClangCL/WSL GCC Debug/Release and GCC
ASan+UBSan checks pass with compact/UTFString regressions. Historical oracle goldens and upstream.lock
are unchanged. [EN/RU results](../tests/conformance/protocol/VALIDATION.md).
Transport, payload decoding and a frame writer were not added.

## Completed: UTFString

QDCXX-011 — done, High. Standard facilities, UTF-CPP 4.0.6 and simdutf 5.2.8 were
assessed before implementation; existing Java vectors demonstrate incompatibilities. A small
QD-specific codec preserves UTF-16 units/null, Java bytes and consumed positions with explicit
caller budgets and atomic writes. All 314 UTFString observations plus 832 compact regressions pass
MSVC/ClangCL/WSL GCC Debug/Release and GCC ASan+UBSan. No production dependency or oracle change.
[Contract and reuse decision](../spec/encoding/UTFSTRING.md),
[validation](../tests/conformance/encoding/VALIDATION.md). API remains provisional; huge allocation,
other BufferedInput profiles, framing/importer/transport remain outside scope.

## First C++ component

QDCXX-010 — done, High: minimal C++23 build, compact int/long codec, Java-vector conformance and pinned formatting workflow. Implemented and verified on MSVC and ClangCL in Debug/Release: 832 selected Java observations, pinned formatting and negative checks. [Build/API](CPP-BUILD.md), [validation](../tests/conformance/encoding/VALIDATION.md). Existing fixtures and upstream.lock are unchanged. UTFString is completed by QDCXX-011; importer and transport still require separate approved tasks.

## Phase

**Specification baseline with bounded C++23 encoding, QTP framing, raw descriptions and static type-8 binding.**

The repository defines specifications and Java oracle evidence, plus QDCXX-010 compact int/long, QDCXX-011 UTFString, QDCXX-012 framing, QDCXX-013 raw descriptions and QDCXX-014 static binding, minimal CMake/CTest and pinned C++ formatting. The API is provisional; no third-party runtime dependency is selected.

## ADR approval and next tasks

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.

Completed: QDCXX-008 — JSON contract and examples; QDCXX-009 — primitive encoding vectors and contract. Further C++/importer work needs a separate approved task. Earlier records below describe the state at each task's completion.

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

- production components beyond the approved compact integer/UTFString scope or generated code;
- dependency-manager configuration;
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

## Primitive encoding baseline

Completed QDCXX-009 on 2026-09-21 with High reasoning. The [contract](../spec/encoding/PRIMITIVES.md) separates canonical writer output, additional reader forms and measured errors/consumed positions for compact int/long and QTP UTFString. [Coverage](../tests/fixtures/encoding/COVERAGE.md): 109 writer and 1037 reader observations, including 858 strict prefixes. [Validation](../tests/fixtures/encoding/VALIDATION.md) records two independent clean builds, negative/positive Verify, unchanged real/temporary expectations and unchanged earlier schema/protocol goldens. Results are synchronized EN/RU. Exact QTP methods are traced; JSON Unicode policy, other UTF codecs and PentaCodec remain separate. Huge accepted lengths, other buffer profiles and full protocol behavior remain unmeasured. No C++, importer or dependency was introduced.

## Java tooling style

The owner adopted Google Java Style after QDCXX-009. AGENTS.md, [.editorconfig](../.editorconfig) and the [pinned formatter wrapper](../tools/format/README.md) define the persistent workflow. All three Java harnesses now pass google-java-format 1.36.1; the formatter uses JDK 21+, while the oracle remains on JDK 8. [Validation](../tools/format/VALIDATION.md) records read-only negative/positive format checks, successful schema/protocol Verify and refreshed independent encoding captures/Verify. All expectation bytes remain unchanged; historical schema/protocol provenance is retained separately from current-source evidence.

## Future C++ style

The owner adopted the reference-derived [C++ style contract](CPP-STYLE.md), pinned to ttldtor/dxfcxx_event_test commit d96ef76f3c4c6649437ac438bd0fa30f91c12f48. Naming, documentation and source-layout rules supplement the copied root formatter configurations: UpperCamelCase types, lowerCamelCase functions/public fields, lowerCamelCase_ encapsulated members and UPPER_SNAKE_CASE enumerators/named constants. The future complete formatting pipeline is Uncrustify 0.83.0 followed by clang-format 20.1.8; the configurations alone cannot enforce naming. Reference/configuration provenance and copied-license notices are retained. This documentation/configuration update does not start C++ implementation or select a build/test dependency; no C++ formatter was run.

## QDCXX-010 owner follow-up

Doxygen parameter/return tags, test-only doctest 2.4.12 through pinned FetchContent, Ubuntu 24.04.4
WSL2 GCC 13.3 Debug/Release conformance and UTC logs are implemented. Windows MSVC/ClangCL tests
also pass after migration; all 832 selected Java observations and original fixtures remain unchanged.
See [validation](../tests/conformance/encoding/VALIDATION.md).
