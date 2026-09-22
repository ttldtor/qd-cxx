# Project roadmap


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
| Next: QDCXX-020, qds-tools inventory | Complete discovered utility/alias/entry-point inventory, Java-to-qdcxx mapping, priorities and dependencies; source-backed CLI/configuration compatibility choices | Research and planning only |
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
