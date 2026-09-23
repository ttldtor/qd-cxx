# Project roadmap

## Current priority: configurable schemas and settings

1. QDCXX-025: apply the accepted [ADR-0003](decisions/ADR-0003-configurable-schemas-and-event-mappings.md) to schema-bound Quote/Profile, with properties/external-XML Java evidence and existing tape integration. Built-in defaults are retained, not promoted to the only possible schemas.
2. Inventory project-wide settings and specify typed configuration resolution before general loading or operational-tool configuration. Use the owner-supplied wiki, pinned source consumers and release history per [CONFIGURATION](CONFIGURATION.md); include initialization/file/API sources, aliases, precedence, read timing, units and sensitive values. QDCXX-025's schema subset feeds this ledger. Assign this follow-up separately; no blanket implementation of every Java property is authorized.
3. Implement external schema/configuration sources against those contracts, then continue event coverage and the operational feed/replay/multiplexer tools. Arbitrary live reconfiguration and unsupported representations need explicit compatibility tasks.

QDCXX-025 is implementation work, not a request to rewrite only architecture documents. Earlier sequencing below is historical where this section supersedes it; historical verification remains unchanged.

[Русская версия](NEXT-STEPS.ru.md). English is authoritative.

## Current sequence after QDCXX-023 acceptance

The owner requested continued implementation toward real tape support, not permanent synthetic-only
coverage. Independent review accepts QDCXX-021's
bounded scope. QDCXX-022 started from committed HEAD `fa5a0b967e9c021e656846cdd3d6faee5faa4776`.

1. QDCXX-022, done; independent review complete: exact UTF_CHAR/TIME_SECONDS/WIDE_DECIMAL and default
   composite Quote end to end, Java production observations then C++, explicit Quote profile and
   useful rejection diagnostics. Existing profile/output compatibility is retained. Implementation commit:
   `c26aed28f84db5c574ec5fbb9e4e5160e9dbbd8e` in master; author evidence remains separate from independent checks.
2. QDCXX-023, done, independently accepted, High — production Java
   evidence and bounded C++ RAW_DATA/MESSAGE support, owned heartbeat timing and measured missing-opt
   semantics. [Author validation](../tests/conformance/protocol/raw-tape/VALIDATION.md) and
   [remaining gaps](REAL-TAPE-GAPS.md) retain the distinction between replay/business/event time and
   structural inventory versus full-file compatibility. Independent acceptance
   records separate fresh checks. Committed by the owner as `48fd76bfc1b7c8fd8d32fba9159e91c4cafe1176`.
3. QDCXX-024, implemented, High; independent review pending: exact
   UTF_CHAR_ARRAY/SHORT_STRING/SEQUENCE/DATE (10/40/72/88), production Java evidence followed by bounded
   C++, owned nullable fields, cumulative budgets and explicit generic version-4 inspection.
   [Coverage](../tests/conformance/protocol/field-types/COVERAGE.md) and
   [author validation](../tests/conformance/protocol/field-types/VALIDATION.md) separate executed checks
   from review. Existing profiles are preserved; unchanged standard Profile is field-level evidence.
   Full Profile event mapping remains separate. The next task has not started.
4. Profile and further real-event adapters: build on reviewed field support and the remaining-gap inventory.
   Assign their actual schemas, mappings, optional fields and business-value comparisons explicitly;
   additional types, indexed records or conversions need their own scope. Do not claim whole-file support
   from a structural inventory or a budget-limited replay.
5. External schema adapters: normalized JSON and then scoped XML/import/overlay/provider behavior,
   feeding validated loader-independent core. Wire descriptions alone are not a complete local schema.
6. Operational replay/publication/feed/multiplexer tools follow the existing 20-tool inventory and
   separately specified transport/core semantics. Python removal remains a parallel staged workstream.

Stages 2 and 3 are scoped by QDCXX-023 and QDCXX-024; stages 4–6 require separate tasks. Every implementation requires evidence, not permission to weaken
current strictness. Compression, sidecars and broader session updates get explicit profiles when needed.
The local `external-input.tape` is optional private integration input, never a required CI fixture;
do not commit/upload it without separate permission. Its header version is diagnostic, not proof of
producer identity or authorization to change upstream.lock. Reproduce relevant cases with locked Java;
if genuine version differences arise, document them and seek a compatibility decision.

## Historical QDCXX-021 author implementation checkpoint

The owner assigned QDCXX-021 after QDCXX-020. The bounded reader and qdcxx-tape-inspect are now
implemented and author-validated; [independent review remains pending](../tests/conformance/protocol/tape/VALIDATION.md).
The first row below has reached its bounded author acceptance endpoint. The next real-event milestone
remains the unmodified default Quote with 56/2/120; it still requires its own task. The unassigned
proposal below is retained as historical QDCXX-020 text, superseded by the QDCXX-021 assignment.

## QDCXX-020 result and dependency order

The [locked inventory](upstream/QDS-TOOLS.md) accounts for 20 registered tools plus the separate
MICBuilder maintenance entry. [Default Quote](REAL-EVENT-BASELINE.md) needs TIME_SECONDS,
UTF_CHAR and WIDE_DECIMAL (including sizes); it is not supported by the existing type-8 decoder.
The earlier workstream table below records the owner's direction; this sequence refines its order.

| Order | Bounded outcome and dependency | Acceptance endpoint |
| --- | --- | --- |
| 1 — recommended next implementation | Uncompressed constrained binary tape reader and optional qdcxx-tape-inspect, based on QDCXX-019; descriptor/additive binding/EOF decisions first | Read the unchanged constrained Java S/R/E file; reject excluded profiles explicitly; no claim of real-event support |
| 2 — default Quote through the same reader | Capture actual default Java scheme and QuoteDelegate → FileWriter → FileReader → QuoteDelegate; specify and implement exact 56/2/120 fields, long slots, decimal conversion and bounded Quote mapping | Original default composite Quote tape → C++ API values/output compared to Java, plus synthetic boundary/negative cases; no schema simplification |
| 3 — first record/replay and publication programs | Bounded writer, subscription/data composition, explicit stream contract and transport lifecycle; choose transport only in its own approved scope | qdcxx-connect/qdcxx-post with deterministic local source, recorded file and replay; finite drain, shutdown and failure checks |
| 4 — subscription distribution | Core aggregation/isolation, wildcard/ticker/stream contracts, reconnect and bounded per-client buffers | qdcxx-feed/qdcxx-multiplexor with two clients, changing subscriptions, slow consumer and disconnect/reconnect cases |
| 5 — operational diagnostics and separate domains | Reuse preceding schema/file/transport capabilities | Compare/FileAnalysis, subscription dump/replay and load tests; IPF separate; RMI/JVM/Launcher decisions remain deferred |

Python removal follows the [ordered migration plan](PYTHON-MIGRATION.md) alongside these stages:
logging/acquisition, CLI checkers, negative/evidence families, JSON/designed artifacts, then a clean
no-Python validation. It does not authorize a bundled rewrite or delay working programs indefinitely.

## One proposed next implementation task (unassigned)

**Bounded uncompressed binary tape reader with qdcxx-tape-inspect.** No new task ID is assigned.
This is a concrete proposal for owner assignment, not authorization supplied by QDCXX-020.

Goal: consume the exact QDCXX-019 constrained file, without stripping the header/footer or moving
late descriptions. Inputs: caller-provided immutable ordinary type-8 local schema, hasTime=false,
BINARY/no compression/time NONE/EMPTY_SET/STREAM_DATA/zero raw flags. Reuse the current primitives
and bindings; keep IO/CLI outside qdcxx_core and preserve qdcxx-qtp-inspect's exact behavior/output.

Before C++: obtain focused Java production-parser evidence for descriptor variations, unknown
properties, missing/duplicate/redefined IDs, late additions, malformed/truncated header/description/
data and EOF. Review an ADR defining descriptor acceptance (version diagnostic, time/opt/send sets),
one initial header, append-only ID mapping/owned plan lifetimes, completed-prefix errors and empty
file/footer policy. Specification and fixtures accompany any protocol change.

Implementation boundaries: finite file/frame/description/record/field/symbol/output budgets,
terminal explicit errors, no ID replacement or general session reset. Add one optional executable
qdcxx-tape-inspect with an explicit profile; define its own output/error contract, no Java CLI parity.
No-args and standalone --help must match on stdout with empty stderr/exit 0 and no file IO; invalid
nonempty args and failed output retain nonzero exits. New acceptance tooling must not add Python.

Acceptance: unchanged QDCXX-019 constrained positive input and preserved hashes; Java-focused
positive/negative Verify with two clean captures; C++ conformance and process/help/IO/budget/error
tests; explicit default-BINARY timing/TEXT/unsupported-type rejection; production BUILD_TESTING=OFF
without test dependencies; applicable formatting/build checks and paired EN/RU validation. Preserve
all old synthetic fixtures. A later Quote acceptance is required for the first real-event milestone.

Exclude compression/sidecars/follow/rotation/retention/resync, MESSAGE/FIELD time, RAW/TICKER/HISTORY,
subscriptions/nonzero flags, writer/network, type conversion, importer and real-event schema support.
No new runtime dependency without explicit approval. If the ADR requires wider semantics, narrow
or split the task before implementation rather than silently extending this profile.

Owner decisions: approve this bounded implementation and new executable/output contract; approve
per-utility native CLI/configuration policy from the inventory separately. Quote adapter/public API,
broader wire conversions and transport/threading remain later gates. Research does not grant them.


## Approved project direction and next work

Owner decisions recorded on 2026-09-22. This roadmap supersedes the historical backlog below;
individual implementation scopes still require assigned tasks.

The product is both a reusable C++23 library and operational command-line programs analogous in
purpose to Java qds-tools: feeds/publishers, multiplexers, recording/replay and diagnostics.
Inventory actual upstream utilities before deciding mappings; the goal is to implement the necessary
equivalents in stages, not to assume only these four categories exist. Command names, argument syntax,
address strings and properties compatibility are separate decisions, not automatically promised.

| Order / workstream | Required result | Authorization |
| --- | --- | --- |
| Completed research: QDCXX-020, qds-tools inventory | Complete discovered utility/alias/entry-point inventory, Java-to-qdcxx mapping, priorities and dependencies; source-backed CLI/configuration compatibility choices | Research and planning only |
| Real event baseline | Select a standard event such as Quote or Trade after inspecting its actual locked scheme/mapping. Generate deterministic Java events and replay them through production tooling; enumerate every field/type/flag/time requirement and C++ gap | Separate task; do not simplify a schema and call it full event support |
| Bounded binary tape reader | Use QDCXX-019 constrained file unchanged; define protocol descriptor, additive descriptions, EOF and resource policies with focused Java evidence/ADR before C++ | Separate approved task; no general tape support yet |
| Remove mandatory Python | Inventory active dependencies; replace UTC logging, audits and process checks incrementally with C++/CTest, Java and small PowerShell/CMake scripts; demonstrate equivalent positive/negative coverage | Separate migration tasks |
| First operational programs | Use inventory and real-event evidence to scope feed/publish/replay, then multiplexer subscriptions/routing, connection lifecycle, bounded buffering and observability | Separate protocol/transport/core tasks; no networking library selected |

New Python scripts/dependencies are prohibited. Existing ones may be used unchanged temporarily;
the target is no Python requirement for current build, tests or tools. Preserve historical evidence,
logs and hash snapshots; remove an old script from active workflows only after replacement validation.
Archived provenance may still contain Python: do not rewrite history to claim it never existed.

Keep small synthetic records for precise boundary/error tests AND real-schema end-to-end cases.
Controlled values in a standard Java event are valid real-schema evidence and require no live feed.
Unavailable demo endpoints must not block acceptance: offline Java publication and recorded tapes
are preferred reproducible inputs. External XML/JSON schema support remains planned; local core
models and protocol decoding must stay independent of schema loaders.

Communicate clearly in Russian: prefer functionality/capabilities, API/method/parameter rather than
vague managerial jargon. The existing term “budget” remains unchanged by explicit owner choice.
Use the current naming and CLI rules in AGENTS.md; preserve fixed output identifiers.

## Historical backlog after QDCXX-007

[Русская версия](NEXT-STEPS.ru.md). This English document is authoritative.

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.

B01 is complete. The contract portion of B03 is complete in QDCXX-008, with [validation](../spec/schema/examples/VALIDATION.md); B04 is complete in QDCXX-009, with [bounded primitive validation](../tests/fixtures/encoding/VALIDATION.md). Other rows remain backlog. Importer/C++ implementation requires a separate task.

| Work item | Dependency / missing evidence | Reviewable exit |
| --- | --- | --- |
| B01 — Review compatibility profile and artifact ownership | Owner choices on provenance, copied main alias, structured errors and JSON | Accepted or revised ADR; explicit deviations and exclusions. No implementation required. |
| B02 — Focused schema probes | Existing schema harness; before adding alias overlays capture collision/removal, defaulted attributes and copy docs/regionals/disabled; before repaired main alias capture known mapping/property/wire effects. SC-01/02/03/04/07 remain bounded separately. | Exact stimuli, two reproductions, negative/positive verify, paired coverage/validation; retain failures and stages. Only probes needed for the next importer subset, not all families. |
| B03 — Minimal importer contract, then separately authorized importer | B01; B02 for any added constructs. Start with the 17 existing schema subcases, explicitly reject/report unsupported scope. Custom types/generators/imports require additional evidence before support. | Versioned interchange specification, original-input/profile provenance, proposed normalized projections reviewed against unchanged Java expectations; separate structured diagnostics. Code requires a new approved task. |
| B04 — Primitive encoding evidence and contract | Existing encoding source inventory; P01 is insufficient. Probe compact width/sign boundaries, truncation/overlong forms, UTF null/empty/Unicode and bounded malformed lengths. | Source-linked byte vectors and acceptance/error/partial-input rules; no library choice. Enables a later explicitly approved first encoding component. |
| B05 — Static protocol adaptation extension | B01/B04; capture LONG/WIDE_DECIMAL tails and missing time-index wire omission; requested y,x order, P06 numeric/object mismatches and supported skip paths. | Incoming versus outgoing mappings and exact bytes/values, no generic defaults/coercion claim. Existing P03/P04/P05 remain a regression subset. |
| B06 — Session/layout lifetime contract | Before any dynamic layout/session reuse claim: requests before/after first writer use, re-description and reset; P09 negotiation and relevant P12 probes. | Measured cache/lifetime/error behavior and separate compatibility decision. No automatic preservation of source-observed cache retention. |

Full dxFeed provider/property parity, generated/regional schemas, mapping getters, PART/auth/sockets/timing and broad rebuild sequences can remain outside the initial profile. They require independently scoped tasks, not silent extension of B03 or B05. qd-core stays transport-independent; no C++, importer, build system or dependency is introduced by this backlog.
