# ADR-0003 — Configurable schemas and schema-bound event mappings

[Русская версия](ADR-0003-configurable-schemas-and-event-mappings.ru.md). English is authoritative.

Status: accepted by the owner on 2026-09-23. This accepts architectural constraints, not unimplemented compatibility.

## Context

[ADR-0001](ADR-0001-normalized-model-boundaries.md) separates source resolution, immutable local schemas and directional wire descriptions.
The current `LocalSchema` already accepts caller-provided records and lookup keys, but `mapQuote` requires eight values in one fixed local order.
That bounded default was deliberate; it must not become the architecture for all events.
Java schema construction can depend on properties and providers or external XML. Source configuration and incoming descriptions are different inputs.

## Decision

1. Configurable schemas are a project-wide requirement for libraries and future operational tools, not a Profile-specific exception. Built-in schemas are reproducible defaults using the same validated schema boundary as caller-created or imported schemas.
2. Source adapters resolve explicit configuration into an immutable local schema. Capture effective properties, provider identities and source provenance for reproducibility. Core, codecs and event mapping must not read process-global properties, environment variables or XML files themselves. Different configurations may coexist without changing each other.
3. Standard event adapters prepare a binding against a particular validated schema and record. Resolve explicit lookup keys and full type identities before processing values; distinguish required, optional and unsupported fields. A binding must not be reused against an incompatible schema merely because record ID, count or types match. Document ownership and invalidation; mutable live schema replacement remains separate work.
4. Keep wire-to-local binding distinct from local-record-to-event mapping. A missing wire field in an existing local schema is not equivalent to a field absent from that local schema. Java-observed defaults and permitted conversions require evidence at the relevant boundary.
5. A generic record/value API remains usable without any standard event adapter. Custom records and extra fields are not silently removed from that representation. A typed event is a documented projection, not the entire schema or a lossless replacement for the original record.
6. Preserve exact full type validation and finite budgets. Configurability does not authorize arbitrary coercion, inferred aliases, arbitrary XML execution or automatic reconstruction of local semantics from wire descriptions. A recognizable event name alone is insufficient to establish compatibility.
7. Compile-time event types and built-in field declarations are allowed. Fixed field positions belong only to an explicitly constrained default API or to a prepared binding for a specific schema, never to a general mapping contract. Share validation and lookup machinery where needed by multiple adapters; do not require a plugin framework, executable mapping language or reflection system.

## Alternatives

- One fixed layout per event is simple but cannot represent configured or externally supplied schemas.
- A separate layout branch for every combination duplicates mapping logic and scales with configurations.
- Recreating Java factories/reflection literally adds machinery without proving behavioral compatibility.
- Schema-bound mappings reuse the existing immutable schema boundary and allow validation before repeated event processing.

## Consequences and compatibility

Existing default profiles and their byte-level CLI output remain compatibility contracts.
Quote must gain a schema-aware path rather than be deleted or silently widened; Profile must use the same architectural approach.
XML/JSON source adapters are separate deliverables, not permanent exclusions from the product. Their future output must be usable without changing core or writing another event mapper.
The current XML-captured JSON contract is bounded and is not automatically widened by this decision.
No new wire types, indexed-record semantics, live reconfiguration, transport or dependencies are approved by this ADR alone.

## Verification and delivery

QDCXX-025 supplies Java evidence, schema-aware Quote/Profile mappings and generated integration tests.
Verify default, reordered, optional-field and external-XML-derived schemas, simultaneous configurations, generic custom records, and wrong-schema rejection.
Source configuration probes must use the pinned upstream and record their actual property/provider environment. Existing captures remain historical evidence.
This ADR does not claim those probes or the C++ extensions have already run.
