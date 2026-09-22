# Upstream open questions

[Русская версия](OPEN-QUESTIONS.ru.md). This English document is authoritative.

Record questions about Java QD behavior that cannot yet be answered confidently. Link each question to the task investigating it and close it with source or fixture evidence.

## QDCXX-022 bounded resolution

The [Java runtime evidence](../../tests/fixtures/protocol/quote/COVERAGE.md) resolves the selected
default Quote schema, production round trip, missing raw-zero fields, repeated targets, UTF_CHAR
positions, signed-second mapping and finite decimal raw/text/binary64/loss observations.
[Contracts](../../spec/protocol/DEFAULT-QUOTE.md) and
[C++ author validation](../../tests/conformance/protocol/quote/VALIDATION.md) implement only the
explicit 2/8/56/120 exact-match local profile. Independent review is complete; implementation commit c26aed28f84db5c574ec5fbb9e4e5160e9dbbd8e.
TF-04 exclusions, RAW_DATA/MESSAGE, missing-opt policy, other events, conversions, regional mapping,
compression, importer and transport remain open. Earlier Quote runtime/code gates below are historical
and superseded only for this bounded increment; no exhaustive decimal/Unicode claim is made.

QDCXX-023 investigates RAW delivery, heartbeat timing propagation and absent-opt semantics before implementation.
It must distinguish replay timing, event-time sequence and Quote business fields; no answer is assumed from descriptor omission alone.
The private sample's remaining descriptions/types are inventoried without authorizing broader field decoding or publishing the sample.

## QDCXX-021 bounded resolution

TF-01 is implemented only for the explicit constrained BINARY/NONE/EMPTY_SET/STREAM_DATA/type-8 profile.
TF-02/03 now have [51 production FileReader probes](../../tests/fixtures/protocol/tape-reader/COVERAGE.md),
[TAPE-READER policy](../../spec/protocol/TAPE-READER.md) and [ADR-0002](../decisions/ADR-0002-bounded-tape-reader.md).
Unknown/duplicate properties, repeated headers/IDs, partial Java installations, missing descriptions,
unknown records and malformed/truncated EOF have bounded evidence; the C++ strict/atomic/terminal
policies are task-authorized differences. Clean EOF does not authenticate a complete recording.
Broader TF-02/03 sessions, recovery, extreme resources and all TF-04 exclusions remain open. No default
TapeConnector/RAW_DATA, Quote, compression, importer or transport claim. Author matrix passed;
[independent review is pending](../../tests/conformance/protocol/tape/VALIDATION.md). Earlier gates below
retain their historical scope and are superseded only for this assigned bounded implementation.

## QDCXX-019 tape evidence and next-task gates

QDCXX-020 update: [inventory](QDS-TOOLS.md) and [default Quote](../REAL-EVENT-BASELINE.md)
resolve the tracked command list and source-derived schema selection only. Runtime help/classpath,
full transitive option/default/error behavior, Quote production round trip (including wide decimal
special values and time loss) remain unmeasured. No earlier runtime unknown is closed by this research.
Owner gates: native per-utility CLI/configuration/output/exit policy; bounded tape descriptor,
append-only binding and EOF ADR; later Quote adapter/API and type conversions; later transport/
threading/subscriptions. [One next implementation proposal](../NEXT-STEPS.md) retains QDCXX-019's
constrained type-8 profile as an enabler, not Quote support. RMI/IPF/Launcher scope is deferred;
Python migration needs old/new negative equivalence before retiring active checks.

[Tape inventory/gaps](../../spec/protocol/TAPE-FILES.md) and
[author validation](../../tests/fixtures/protocol/tape/VALIDATION.md) add real FileWriter/FileReader
evidence: default/constrained BINARY and TEXT preserve seven local records; truncated constrained
input delivers four records and one EOF stream error. Two clean captures match without normalization.

| ID | Status / remaining question | Required next step |
| --- | --- | --- |
| TF-01 | Measured for direct writer only: header type 1 and late first-use descriptions block the current inspector even with EMPTY_SET/time NONE. TapeConnector defaults to RAW_DATA by source, not measured here. | Approve a separate uncompressed BINARY/NONE/EMPTY_SET/STREAM_DATA task; no general tape/connector claim. |
| TF-02 | Open policy: descriptor unknown properties, empty opt, absent time, version diagnostics, append-only ID declarations and plan ownership. | ADR and descriptor/redeclaration/missing-description Java evidence before C++ implementation. |
| TF-03 | Partial EOF measured; corruption recovery, follow, budgets and boundary behavior remain open. | Additional finite header/description/data truncation and malformed-frame probes; explicit terminal/completed-prefix policy, no implicit resync. |
| TF-04 | Source only: CSV/BLOB, GZIP/ZIP, time sidecars, split/cycle/retention and broader flag/time combinations. Packaged JAR version metadata and real dxFeed schemas remain unmeasured. | Separate evidence before widening the profile; no new runtime dependency is approved. |

QDCXX-019 is research only. Existing QT unknowns retain their unmeasured portions; no C++/inspector
capability, importer or transport was added. New oracle tooling is Java launched through PowerShell,
with no new Python script/dependency. Independent review is pending.

## Provenance

- **QDCXX-002 — GitHub repository authority**
  - **Status:** non-blocking provenance question.
  - The upstream root POM at the locked commit identifies the internal Devexperts Stash repository (`https://stash.in.devexperts.com/scm/mdd/qd.git`) as its SCM source, while `qd-cxx` uses the public `https://github.com/devexperts/QD.git` repository.
  - For the pinned QD 3.355 baseline, the public tag, commit SHA, root POM version, and release notes are consistent, so the locked source tree is reproducible and unambiguous.
  - `qd-cxx` therefore treats the pinned GitHub repository and immutable commit SHA as the **public reference source** and uses that Java source tree as the compatibility oracle.
  - Whether the GitHub repository is the canonical repository, an official mirror, or another form of synchronized public copy remains unverified.
  - This uncertainty does **not** block specification extraction, conformance work, or C++ implementation against the pinned baseline.

## Initial questions

- XML resources/loaders, rule layers, aliases, mappings, defaults and composition: initial source answers recorded by QDCXX-003 below; QDCXX-005 adds 17 bounded captures. Broader captures and normative normalization remain pending.
- QTP framing, directional schema exchange, codec state and candidate upstream tests: initial source answers recorded by QDCXX-004 below; QDCXX-006 measures nine bounded P01/P03/P04/P05 cases, with broader runtime coverage pending.

For each answer, distinguish explicit upstream contract from observed behavior and implementation accident.

## Schema investigation QDCXX-003

The initial loader/resource, composition, alias and mapping questions are answered at source-inventory level in [XML/DataScheme analysis](../../spec/schema/README.md) and [locked source citations](../../spec/schema/SOURCES.md). They are not final normative rules or captured oracle results. XML syntax, source model and emitted runtime are distinct; XML mappings are currently ignored and runtime mappings come from Java services.

Remaining questions, all tied to [fixture proposals](../../tests/fixtures/schema/README.md):

| ID | Status and precise question | Evidence / next step |
| --- | --- | --- |
| SC-01 | Open, parser edge cases: what is the observed model/error for prefix-qualified elements, numeric booleans, whitespace, repeated include/exclude containers, and XSD key constraints? | XmlSchemeModelReader child/attribute helpers and active XSD; S18. Capture the JDK/JAXP provider rather than assuming schema-valid means semantically consumed. |
| SC-02 | Partially measured; compatibility decision open: preserve clone main-alias loss, ignored copy doc/regionals, and main-alias removal pointer behavior? | QDCXX-005 S08-main-alias confirms copied explicit-main entry survives but effective name falls back to first alias; S08-before/after confirms snapshot timing. Doc/regionals/removal and normative decisions remain deferred. [Coverage](../../tests/fixtures/schema/COVERAGE.md). |
| SC-03 | Open, ordering/defaults: how should normalized source operations preserve defaulted XML overlay attributes, unordered overlay alias additions and absence versus empty index values? | Active XSD, SchemeRecord.override, Field.override, SchemeRecordGenerator.override; S07/S09/S11. Do not impose an intended inheritance rule. |
| SC-04 | Partially measured: BYTE/SHORT pass model resolution then throw NullPointerException; both disabled indexes throw SchemeException at runtime, while one disabled index yields VOID. | QDCXX-005 S05/S15 [captures](../../tests/fixtures/schema/COVERAGE.md). Object/long/duplicate indexes, other reserved types, generated-name and lookup collisions remain open. No universal SchemeException promise. |
| SC-05 | Open, bitfield boundary: actual behavior for alias-to-flags, duplicate bitfields and offset+size beyond 64; decide metadata scope. | DefaultEmbeddedTypes.canHaveBitfields, reader addRecordField and Field.validateState/addBitfield; S19. No runtime bitfield mapping generation was found in the traced path. |
| SC-06 | Open, environment profile: which service-provider set and property configuration defines required dxFeed parity? | RecordMappingFactoryHolder, DXFeedScheme, DXFeedPropertiesConverter and default-scheme tests; S13/S14/S16. Full legacy property matrix, Nuam coverage and third-party provider order remain follow-up work. |
| SC-07 | Open, rebuild isolation: does a sequence of withProperties calls mutate shared base entities and affect later builds? | DXScheme.configureSchemeModel and SchemeModel.overrideEntityCollection share references; S20. Source sharing is observed; sequence-dependent oracle capture remains. |
| SC-08 | Resolved for QDCXX-005 only: original tiny XML and Java harness; raw TSV retained, only input-directory URL and CRLF normalized. No upstream source/resources redistributed. | [Harness/profile/provenance](../../tools/upstream/schema-oracle/README.md), [validation](../../tests/fixtures/schema/VALIDATION.md). Future copied resources still require notice/provenance review; this observation format does not choose the normalized model. |
| SC-09 | Proposal available; owner decision open: source/overlays and metadata as importer provenance, resolved local schema and directional protocol state as separate first-class artifacts. | [QDCXX-007 requirements](../../spec/schema/NORMALIZED-MODEL.md), [ADR-0001](../decisions/ADR-0001-normalized-model-boundaries.md), status proposed. This is a qd-cxx design choice, not a resolved upstream semantic question. |

QDCXX-005 supplies initial S01/S04/S05/S07/S08/S15 subcases; unlisted portions remain open. Follow-up scope includes the remaining S01-S12/S15 matrix, parser/alias/rebuild probes S18-S20, normalized-model requirements and compatibility decisions (QDCXX-007), and a separate dxFeed provider/property parity profile S13-S17. QDCXX-006 subsequently adds the bounded QTP captures below.

## QTP investigation QDCXX-004

Initial answers are in [QTP boundaries](../../spec/protocol/README.md), [DataScheme/wire mapping](../../spec/protocol/SCHEMA-EXCHANGE.md) and [locked source index](../../spec/protocol/SOURCES.md). Source establishes sender-ID mapping by record name, ordered field descriptors, peer-requested outgoing adaptation, complete-frame buffering and message-local symbol state. QDCXX-006 adds [bounded captured observations](../../spec/protocol/CAPTURES.md), not complete compatibility guarantees.

| ID | Status and remaining question | Evidence / next step |
| --- | --- | --- |
| QT-01 | Open: exact primitive acceptance/canonicalization and limits, including compact-int truncation, overlong encodings, malformed Unicode and huge counts/IDs. | BufferedInput/Output, IOUtil, PentaCodec; [encoding dependencies](../../spec/encoding/README.md); P01/P02/P08. Bound probes to avoid allocation hazards. |
| QT-02 | Partially measured: P03 directional IDs, P04 ordering/property-alias mapping and missing compact-int raw-zero slots, P05 requested layouts captured. Complete adaptation, application defaults, alias collisions, unconvertible and object/numeric mismatch matrix remains open. | BinaryRecordDesc retries local representation bits rather than uniformly rejecting mismatches; FieldAdaptationTest covers selected numeric cases. P03–P06/P08. |
| QT-03 | Open: unknown local record with built-in event metadata, nonzero externally supplied event time or field replacers: which null-cursor paths fail and how are failures surfaced? | BinaryRecordDesc.readFields and AbstractQTPParser postprocessing can dereference null. Normal skip path is source-established; optional-path outcome requires P07. |
| QT-04 | Open: dynamic requested-layout changes and session reuse contract. Does intended compatibility preserve cached writer layout after resetSession? | BinaryQTPComposer.recordMap survives inherited reset; ConnectionQTPParser.schemeMap is reset. QDCXX-006 P05 measures four fresh connections before first writer use only; P12 and dynamic before/after writer use remain pending. |
| QT-05 | Open: PART reset/cancel lifetime, resource limits, excess fragment handling and error recovery policy. | BinaryQTPParser.resetSession does not clear partitionedMessages; assembly limit TODO; P10/P12. Do not silently copy a potential implementation accident into a normative rule. |
| QT-06 | Open: which error/close behavior is required for default versus configured consumers, partial descriptor commits, final truncated streams and unchecked exceptions? | Parser callbacks, MessageAdapter skip flags, no final-EOF signal in base parse path. P01 now records pending partial frames without errors and uses no final-EOF signal. P02/P08/P13 remain pending; file parser resync is a separate profile. |
| QT-07 | Open: complete negotiation/auth/legacy and live aggregation transition profiles, including order of first data after opt/version updates. | ProtocolDescriptor deltas and adapter paths traced; hook Javadoc contradicts source call order. P09 plus focused auth/aggregation follow-up, not a full state-machine claim. |
| QT-08 | Partially measured: P01 all 32 schedules preserve record order/values but callback grouping differs. Complete event flag/time matrix, batching requirements and timing profiles remain open. | AbstractQTPParser reorder masks, EventFlag, HeartbeatPayload and MessageAdapterConnection. P01/P11/P13; separate record order from callback grouping and clock values. |
| QT-09 | Resolved for QDCXX-006: source-built harness, exact byte/callback captures and separate semantic expectations; only CRLF normalized; P05 uses real connection path with documented instrumentation. | [Coverage](../../tests/fixtures/protocol/COVERAGE.md), [validation/provenance](../../tests/fixtures/protocol/VALIDATION.md). Broader profiles and C++ conformance remain pending. |

Suggested follow-up after QDCXX-006: P06 conversions/P09 negotiation and targeted error/reset/partition probes; then normative primitive/wire rules and a reviewed compatibility profile. QDCXX-006 does not start QDCXX-007. No transport runtime or C++ implementation is authorized by this research result.

## QDCXX-007 decision gates and missing evidence

QDCXX-007 is complete as documentation; [ADR-0001](../decisions/ADR-0001-normalized-model-boundaries.md) remains **proposed**. Earlier task-specific no-execution statements above retain their historical scope. No SC/QT runtime unknown is closed by a model proposal.

- **NM-01 — Owner decision, gates importer contract:** approve three-layer ownership, source metadata retention and versioned JSON direction (SC-09). Alternatives and consequences are in the ADR.
- **NM-02 — Owner decision, gates copy policy:** proposed pinned-profile preservation of measured main-alias loss (SC-02). Before choosing corrected copying, request a focused copy/known-mapping/property/wire probe. Copy doc/regionals/disabled, alias removal and collisions remain unmeasured. This does not block representing effective names independently of declarations.
- **NM-03 — Owner decision, gates error contract:** approve structured unsupported-local-type diagnostics in place of BYTE/SHORT NullPointerException (SC-04), retaining successful model resolution and failed local construction. Other unsupported types/index shapes and factory wrappers need distinct probes; no universal exception parity.
- **NM-04 — Evidence gate for broader protocol support:** LONG/time-index wire omission, requested y,x order/types and P06 conversions before extending static adaptation; before/after-first-writer requests and reset before dynamic layouts (QT-02/04). Existing P05 captures cover four fresh first-use connections only.
- **NM-05 — Safely deferred outside initial profile:** SC-01/03/05/06/07 and QT-01/03/05/06/07/08 retain their unresolved portions. Primitive boundary probes gate an encoding contract; full dxFeed, mapping getters, generators, rebuilds, PART/auth/timing require separate tasks. [Scoped backlog](../NEXT-STEPS.md) records dependencies; exclusion is not a compatibility claim.

## Accepted decision and QDCXX-008 update

The owner accepted ADR-0001 on 2026-09-21. This supersedes the proposed/pending decision labels above for SC-09 and NM-01–03: ownership/JSON, XML-import-only pinned copy behavior and structured BYTE/SHORT rejection are approved. SC-02/04 unmeasured behaviors remain open; approval does not supply new runtime evidence. QDCXX-008 now defines the [bounded JSON contract](../../spec/schema/JSON-CONTRACT.md) and [validated examples](../../spec/schema/examples/VALIDATION.md). NM-04/05 and native C++ construction semantics remain open. No importer or QDCXX-009 execution follows from this update.

## QDCXX-009 primitive evidence update

QT-01 is now partially measured: [PRIMITIVES](../../spec/encoding/PRIMITIVES.md) and [coverage](../../tests/fixtures/encoding/COVERAGE.md) establish bounded compact int/long writer transitions, accepted overlong reader forms, cross-width truncation, QTP UTFString code units, malformed inputs and direct ByteArrayInput consumed positions. Two independent builds and negative/positive Verify are recorded in [validation](../../tests/fixtures/encoding/VALIDATION.md). Remaining QT-01 work: accepted extreme lengths/resource allocation, other BufferedInput subclasses and IOUtil entry points, other UTF/character codecs, PentaCodec, huge protocol counts/IDs and outer parser errors. Do not infer rollback from EOF or transfer JSON Unicode policy to QTP. These need focused future tasks; no C++ or importer is authorized by this evidence.

## QDCXX-012 framing update

QT-01/QT-06 are partially measured by new [P02](../../tests/fixtures/protocol/p02/COVERAGE.md): small
length/type limits, overlong forms, incomplete-frame retention, type/payload errors, unknown type and
one default-adapter closure comparison. Header diagnostics read extra bytes, explaining Java position
dependence on partitioning. The [C++ contract](../../spec/protocol/FRAMING.md) supplies separate terminal
first-error, resource and finish policies; Java resetSession/EOF equivalence is not claimed. Full error
flag combinations, huge resources, other buffers and PART/session/transport remain open.

## QDCXX-013 description update

QT-01/QT-06 now include bounded [P08 evidence](../../tests/fixtures/protocol/p08/COVERAGE.md):
record/field header checks after compact low-32-bit projection, exact UTF-16, duplicates,
construction failures distinct from malformed descriptions and retained prior installations.
The [raw parser contract](../../spec/protocol/DESCRIBE-RECORDS.md) separates diagnostic raw observations,
actual Java mappings and C++ resource policy. Huge resources, local binding/conversions, optional
profiles, session recovery and transport remain open; no universal callback/exception parity.

## QDCXX-014 static binding update

QT-02/QT-03 are partially measured by [production parser observations](../../tests/fixtures/protocol/binding/COVERAGE.md):
exact type-8 remapping, field reorder/explicit lookup, duplicate final values, missing fields, empty
records, sparse IDs and unknown-record/field skipping with continued delivery. Undescribed ID remains
a distinct message error. [Static binding](../../spec/protocol/STATIC-BINDING.md) is an independent
owned plan, not payload decoding/session state. Intermediate duplicate writes remain source-supported;
application defaults, conversion matrix, metadata/replacers/providers, huge resources, dynamic updates,
reset/PART and transport remain open. Local subset validation does not implement JSON/XML import.

## QDCXX-015 symbol update

QT-01/QT-06 now include [bounded symbol evidence](../../tests/fixtures/encoding/symbols/COVERAGE.md):
canonical/accepted forms, exact units/positions, flags/MRU, repeat/reset and real two-message production
integration. Java partial failure state is observed separately from [C++ terminal policy](../../spec/encoding/SYMBOLS.md).
Huge allocations, resolver/other buffers, option negotiation, event/time interpretation, full recovery,
PART and transport remain open. No full event decoder or protocol compatibility is inferred.

## QDCXX-016 STREAM_DATA update

QT-01/02/06/08 now include 35 bounded production-parser observations of ordinary type-8 payloads,
exact UTF-16, skipped records/fields, repeated symbols across record IDs, message reset, cross-width
values and malformed complete messages. [Evidence](../../tests/fixtures/protocol/stream-data/COVERAGE.md).
The [decoder contract](../../spec/protocol/STREAM-DATA.md) separates Java dropped malformed-message
batches from C++ completed-prefix retention and rejects all nonzero raw flags. Dynamic session,
conversions, metadata/replacers, event/time, huge resources, recovery and transport remain open.
