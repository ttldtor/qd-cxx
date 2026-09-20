# Upstream open questions

Record questions about Java QD behavior that cannot yet be answered confidently. Link each question to the task investigating it and close it with source or fixture evidence.

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
| SC-09 | Open, normalized-model scope: retain resolved runtime descriptors only, or also source overlays, templates, disabled metadata, docs, enums, tags and bitfields? | Analysis follow-up section. This is a qd-cxx design decision for a separate task, not an upstream unknown or C++ design established here. |

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
