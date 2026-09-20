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


## Accepted initial profile (ADR-0001)

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.

Unmeasured conversions, dynamic layouts/reset and dxFeed provider profiles remain open. Separate diagnostic expectations are added during implementation; Java captures are not rewritten.
