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

- XML resources/loaders, rule layers, aliases, mappings, defaults and composition: initial source answers recorded by QDCXX-003 below; normative normalization and oracle captures remain pending.
- Where are QTP frame boundaries established, and which layer owns message boundaries?
- Which QTP negotiation and state transitions are version- or option-dependent?
- Which encodings retain state across messages or connections?
- What malformed-input behavior is externally observable and relied upon?
- Which upstream tests are suitable as direct sources for conformance fixtures?

For each answer, distinguish explicit upstream contract from observed behavior and implementation accident.

## Schema investigation QDCXX-003

The initial loader/resource, composition, alias and mapping questions are answered at source-inventory level in [XML/DataScheme analysis](../../spec/schema/README.md) and [locked source citations](../../spec/schema/SOURCES.md). They are not final normative rules or captured oracle results. XML syntax, source model and emitted runtime are distinct; XML mappings are currently ignored and runtime mappings come from Java services.

Remaining questions, all tied to [fixture proposals](../../tests/fixtures/schema/README.md):

| ID | Status and precise question | Evidence / next step |
| --- | --- | --- |
| SC-01 | Open, parser edge cases: what is the observed model/error for prefix-qualified elements, numeric booleans, whitespace, repeated include/exclude containers, and XSD key constraints? | XmlSchemeModelReader child/attribute helpers and active XSD; S18. Capture the JDK/JAXP provider rather than assuming schema-valid means semantically consumed. |
| SC-02 | Open, copy/alias compatibility: should eventual conformance preserve clone main-alias loss, ignored copy doc/regionals, and main-alias removal pointer behavior? | SchemeRecord.copyFrom, Field.copyFrom/override and reader parseRecord; S08/S20. Source behavior is recorded; executable output and a reviewed compatibility decision remain. |
| SC-03 | Open, ordering/defaults: how should normalized source operations preserve defaulted XML overlay attributes, unordered overlay alias additions and absence versus empty index values? | Active XSD, SchemeRecord.override, Field.override, SchemeRecordGenerator.override; S07/S09/S11. Do not impose an intended inheritance rule. |
| SC-04 | Open, errors at runtime: capture unsupported recognized types, object/long/duplicate indexes, disabled indexes, generated-name and lookup collisions. | SerialFieldType factories, DXScheme.buildOneRecord/addIndexField, DefaultRecord/DefaultScheme constructors; S05/S07/S11/S15. Do not promise all failures are SchemeException. |
| SC-05 | Open, bitfield boundary: actual behavior for alias-to-flags, duplicate bitfields and offset+size beyond 64; decide metadata scope. | DefaultEmbeddedTypes.canHaveBitfields, reader addRecordField and Field.validateState/addBitfield; S19. No runtime bitfield mapping generation was found in the traced path. |
| SC-06 | Open, environment profile: which service-provider set and property configuration defines required dxFeed parity? | RecordMappingFactoryHolder, DXFeedScheme, DXFeedPropertiesConverter and default-scheme tests; S13/S14/S16. Full legacy property matrix, Nuam coverage and third-party provider order remain follow-up work. |
| SC-07 | Open, rebuild isolation: does a sequence of withProperties calls mutate shared base entities and affect later builds? | DXScheme.configureSchemeModel and SchemeModel.overrideEntityCollection share references; S20. Source sharing is observed; sequence-dependent oracle capture remains. |
| SC-08 | Open, fixture provenance: choose capture harness/output normalization and review upstream resource redistribution before adding golden files. | Fixture policy and schema fixture proposals. No captured Java outputs or copied resources exist in QDCXX-003. |
| SC-09 | Open, normalized-model scope: retain resolved runtime descriptors only, or also source overlays, templates, disabled metadata, docs, enums, tags and bitfields? | Analysis follow-up section. This is a qd-cxx design decision for a separate task, not an upstream unknown or C++ design established here. |

Suggested follow-up tasks (proposed, not yet assigned task IDs): a Java oracle capture harness with S01-S12/S15; targeted parser/alias/rebuild probes S18-S20; normalized-model requirements and compatibility decisions; a dxFeed provider/property parity profile S13-S17. QDCXX-004 retains QTP schema-exchange and wire-boundary research.
