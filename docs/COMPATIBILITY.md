# Compatibility policy

[Русская версия](COMPATIBILITY.ru.md). This English document is authoritative.

## Oracle

The pinned Java QD revision in `upstream.lock` is the initial compatibility oracle. Compatibility claims must name the upstream revision and the observed surface being compared.

## Compatibility surfaces

The project expects to evaluate compatibility separately for:

- wire bytes and framing;
- message sequencing and negotiation;
- schema/DataScheme interpretation;
- symbol and field encodings;
- validation, defaults, aliases, and mappings;
- error handling and malformed-input behavior;
- state transitions visible to peers or API consumers.

Source/API compatibility with Java is not a goal. Internal class layout and algorithms are not compatibility surfaces unless they affect observable behavior.

## Evidence

Each compatibility rule should eventually have:

1. a normative or explicitly descriptive statement in `spec/`;
2. a traceable upstream source, test, or captured observation;
3. a reviewed fixture when byte-level or structured examples are practical;
4. a conformance test that can run against the C++ implementation.

Golden fixtures must record how they were generated, including upstream commit and relevant options. Hand-authored fixtures must be labeled as such.

## Changes and deviations

Protocol or schema behavior must not change only in code. Update the specification, documentation, and conformance fixtures together. Intentional deviations from Java QD require an ADR describing motivation, interoperability impact, migration strategy, and test coverage.

Unknown behavior must be recorded as an open question rather than converted into an accidental contract.

## Proposed initial model profile (QDCXX-007)

The policy above is unchanged. [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.md) is **proposed**, not an approved deviation. The [requirements matrix](../spec/schema/NORMALIZED-MODEL.md) recommends preserving the exact measured schema/protocol subcases, retaining copied-main-alias effective names within the pinned profile, and representing VOID slots as a compatibility projection without prescribing physical storage. It proposes explicit unsupported-local-type diagnostics instead of the captured BYTE/SHORT NullPointerException, while preserving model recognition and local-construction rejection.

Original requests and selected outgoing layouts remain distinct; P05 evidence covers fresh first-use connections only. Copy repair, broader error parity, dynamic layouts/reset, conversions, application defaults and dxFeed provider/property profiles remain undecided or unmeasured. Source-only VOID wire omission is not a captured LONG/time-index wire result. Java observations are immutable evidence; any later approved deviation needs separate profile expectations and conformance work. No wire behavior or existing fixture is changed here. [Validation](QDCXX-007-VALIDATION.md) and [next steps](NEXT-STEPS.md) state the limits and decision gates.
