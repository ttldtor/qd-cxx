# ADR-0001 — Separate source provenance, local schema and directional protocol state

[Русская версия](ADR-0001-normalized-model-boundaries.ru.md). This English document is authoritative.

Status: **accepted**. Approved by the owner on 2026-09-21 after QDCXX-007.

## Context

[Requirements and compatibility matrix](../../spec/schema/NORMALIZED-MODEL.md) distinguish captured behavior, source facts and design proposals. QDCXX-005 measures 17 subcases; QDCXX-006 measures nine cases, including 32 P01 schedules. Their [schema coverage](../../tests/fixtures/schema/COVERAGE.md) and [protocol coverage](../../tests/fixtures/protocol/COVERAGE.md) do not cover entire scenario families. Java observations are not the target interchange format.

Copy timing and effective aliases depend on construction history. Local IDs/slots differ from directional wire IDs/order. An originally empty requested description differs observably from an empty selected layout. One flattened model cannot safely serve all these meanings.

## Accepted decision

1. Make the immutable resolved local schema and directional protocol descriptors/state first-class, separate artifacts. Retain source declarations and ordered overlay operations as importer provenance with original inputs and profile identity. Do not expose an executable XML model in qd-core. Core remains independent of importer execution and transport.
2. Retain semantic/full/local/property names and scoped lookups, complete serial type IDs, emitted record order/local IDs, hasTime/index roles and an explicit Java slot projection. Logical fields are separate from physical C++ storage; synthetic VOID positions remain comparable without requiring Java arrays. Preserve templates, disabled entities, docs, enums, tags, bitfields and custom types in provenance; retention does not promise their execution.
3. Preserve the measured effective-name result of copied main-alias loss in the initial pinned profile. This is justified by observable full/local-name and collapsed-lookup differences in [S08-main-alias](../../tests/fixtures/schema/expected/S08-main-alias.tsv), not by a duty to copy Java bugs. Preserve explicit-main metadata separately. Do not silently “repair” existing artifacts. A corrected profile needs explicit review and mapping/property/wire probes; wire impact of that XML copy has not been captured.
4. Adopt one deliberate diagnostic deviation: reject BYTE/SHORT local construction with a structured unsupported-type diagnostic instead of reproducing NullPointerException. [S15-byte](../../tests/fixtures/schema/expected/S15-byte.tsv) and [S15-short](../../tests/fixtures/schema/expected/S15-short.tsv) succeed at model resolution and fail at runtime; SerialFieldType factory/DefaultRecord explain the null dereference ([sources](../../spec/schema/SOURCES.md#types)). Preserve this acceptance boundary and the distinction from [S05-disabled-both](../../tests/fixtures/schema/expected/S05-disabled-both.tsv). Do not claim universal error-stage/class parity, accept those local types, or prohibit their wire codes based on this construction failure.
5. Keep incoming sender-ID bindings, original requests and selected outgoing layouts in protocol state, scoped by peer/direction and local schema. Preserve P03/P04 and first-use P05 outcomes only as measured in [CAPTURES](../../spec/protocol/CAPTURES.md). Absent, originally empty and nonempty requests remain distinct, even when the selected list is empty. Do not adopt Java writer-cache/reset behavior without probes.
6. Prefer a versioned data-only JSON interchange with ordered arrays and separate format/profile versions. Keep the final schema, canonical serialization/hash, C++ API, storage and dependencies undecided. Java TSV/digest remain evidence, not canonical normalized identity.

## Alternatives

| Alternative | Advantage | Reason not recommended initially |
| --- | --- | --- |
| Only resolved runtime descriptors | Smallest implementation surface | Loses source order, disabled metadata and explanations of copy/overlay outcomes. |
| Unified source/runtime/session object graph | All information in one graph | Couples source and session lifetimes to core and obscures ID/name/order boundaries. |
| Public lossless XML AST plus overlay engine | Editing and round-trip support | Expands initial contract into unmeasured defaults, parser quirks and overlay semantics. Retained originals suffice for provenance. |
| Fix main alias immediately | Intuitive copy semantics | Changes observed full/local names and lookups; downstream wire/provider impact unmeasured. Must be a separate approved behavior profile. |
| Emulate Java exception classes/messages | Easier literal oracle comparison | Imports accidental implementation failure into a non-Java API. Retaining rejection with structured diagnostics is more useful; this deviation is approved by this ADR. |
| Drop VOID entirely or copy Java physical arrays | Simpler logical model, or direct oracle storage matching | Dropping loses observed slots/lookups; mandatory arrays unnecessarily constrain C++ storage. A projection preserves evidence without either commitment. |
| Adopt TSV, XML interchange or binary serialization | Existing captures, source syntax reuse, or compact storage | TSV mixes observation concerns; XML does not express the three boundaries well; binary optimization is premature. JSON is a direction, not a selected dependency. |

## Consequences and compatibility impact

More explicit metadata and references are required, but importer diagnostics, local semantics and peer adaptation can be reviewed independently. Mapping/provider/property provenance is mandatory for reproducibility; Java class names do not implement C++ mappings. Full dxFeed profiles, conversions, dynamic requests and malformed/reset/PART behavior remain outside the initial profile.

No accepted wire rule or existing golden changes in this ADR. Slot projection and directional separation aim to preserve observed behavior. Main-alias preservation is a bounded compatibility choice, not normalization of an intended clone contract. The error proposal intentionally changes diagnostics for invalid local inputs; there is no successful local schema/wire output in those two captures. This does not prove absence of impact for callers that classify errors by Java exception type.

Migration proposal: keep the original captures immutable; introduce separately named profile/version and comparison expectations for structured errors when implemented. If corrected copying is later chosen, keep pinned-profile artifacts distinguishable and require explicit re-resolution/diff of names, lookups, properties and affected wire output; never silently reinterpret an existing artifact. No automatic migration or default corrected profile is authorized.

## Validation and acceptance gate

This is a documentation result; [QDCXX-007 validation](../QDCXX-007-VALIDATION.md) records checks actually run. Java, C++ and new semantic probes are not executed here. Later importer tests must compare all 17 exact schema subcases, retaining rejection stages and Java observations alongside approved diagnostic differences. Later protocol tests must compare nine exact/semantic cases and preserve the P01 distinction between ordered values and callback grouping; targeted LONG/time-index, alias-copy and request-update probes are additional work, not existing evidence.

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.
