# Architecture

[Русская версия](ARCHITECTURE.ru.md). This English document is authoritative.

## Clarified goal: library and operational programs

The project includes equivalents of the necessary qds-tools utilities, not just a library/inspector.
QDCXX-020 establishes the actual inventory and order. Feeds, multiplexing, recording/replay and
diagnostics are use categories, not preapproved command names. Shared data/subscription behavior
belongs in libraries; transport handles connections; programs compose components, configuration,
management and observability. Do not turn the inspector into a universal server or duplicate shared
logic. Transport/threading and CLI/properties compatibility remain separate decisions.
[Roadmap and migration policies](NEXT-STEPS.md).

## Intent

QDCXX-025 implements the bounded [prepared event mapping boundary](../spec/events/EVENT-MAPPING.md):
immutable local schemas carry an opaque record context through generic decoding, while separate
Quote/Profile adapters own validated field selections and event values. Schema copies preserve context;
fresh schema creation creates a new identity. Wire binding and source configuration remain separate.
The [Java configuration subset](../spec/schema/CONFIGURED-EVENTS.md) provides source evidence;
it does not add a C++ loader, ambient process settings or transport to core/event libraries.

`qd-cxx` will reproduce the externally observable contracts of Java QD without mechanically reproducing its class hierarchy or historical implementation choices. The design target is a modular, testable C++23 system.

## Proposed boundaries

These are direction-setting boundaries, not yet implementation commitments:

- **qd-core**: records, field values, scheme semantics, symbols, subscriptions, and core state transitions. It must not depend on transport.
- **schema/model**: proposed immutable resolved records, fields, complete type identities, names/lookups and a Java slot projection; source operations and mapping/provider information are retained as provenance, not executable Java mappings.
- **encoding**: byte-level primitives and stateful codecs with explicit error and ownership rules.
- **protocol**: QTP messages, framing, negotiation, and protocol state machines expressed independently of a socket library.
- **transport adapters**: connections, I/O scheduling, backpressure, lifecycle, and integration with a future networking runtime.
- **tools**: importers, inspectors, fixture capture, and compatibility diagnostics.

Dependency direction should point inward: transport may depend on protocol; protocol may depend on encoding and core contracts; core must not depend on transport.

## Configurable schemas and event mappings

[ADR-0003](decisions/ADR-0003-configurable-schemas-and-event-mappings.md) makes runtime-resolved schemas a project-wide requirement.
Built-in defaults and future XML/JSON or programmatic sources use the same validated local-schema boundary.
Generic records remain available without standard event adapters; Quote/Profile mappings bind to a particular schema instead of assuming its default field order.
Source configuration, wire adaptation and event projection remain separate responsibilities.
QDCXX-025 implements the first bounded application of this decision; the decision is not a claim of existing loader or arbitrary-schema support.
The [configuration requirements](CONFIGURATION.md) apply beyond schema settings: property sources, precedence, scope, read timing and diagnostics are explicit compatibility concerns.

## Live subscription ownership

[ADR-0005](decisions/ADR-0005-live-subscriptions.md) selects the design for upcoming live subscriptions;
the [subscription contracts](../spec/subscriptions/CONTRACTS.md) define its observable barriers and budgets.
`qdcxx_core` owns deterministic identities, desired-interest maps and bounded atomic mutations.
Live session/client/runtime layers own connection epochs, queues, callbacks, I/O and lifecycle.
One logical serialization domain is selected for client dispatch.
The [pure protocol session](../spec/subscriptions/LIVE-SESSION.md) owns a fresh epoch,
immutable peer generations and whole admitted frames. Caller-supplied byte spans and work credits
keep that layer independent of runtime, clocks and application callbacks. Its partial-write cursor
does not acknowledge peer delivery.
[ADR-0006](decisions/ADR-0006-live-runtime.md) selects an optional Asio/c-ares/OpenSSL profile,
explicit SDK/bootstrap/trust requirements and its [feasibility boundary](../tests/conformance/transport/CONTRACTS.md).
The [optional TCP adapter](../spec/transport/TCP-RUNTIME.md) implements only the caller-driven,
credential-free plain subset, with explicit bootstrap, bounded SDK sources and owned-operation
retirement. TLS, execution-host dispatch and the complete subscription client remain separate work.
The ADR does not imply implementation or owner acceptance of those remaining parts.

## Design principles

- Specify observable behavior before implementation.
- Make ownership, lifetime, cancellation, threading, and error semantics explicit.
- Keep deterministic transformations separate from I/O.
- Prefer replaceable adapters at upstream, filesystem, timing, and transport boundaries.
- Preserve source traceability for behavior learned from Java QD.
- Use ADRs for consequential or difficult-to-reverse decisions.

## Not decided yet

A package manager, stable public API and general binary compatibility policy remain undecided. The selected optional networking profile and serialized ownership model are described above and in ADR-0006. The QDCXX-010 owner follow-up selects doctest as a test-only FetchContent dependency. Further decisions require scoped tasks and evidence.

QDCXX-010 implements only the encoding boundary's compact int/long primitives, with a provisional span-based API, minimal CMake/CTest and no third-party runtime dependency. See [build/API](CPP-BUILD.md) and [validation](../tests/conformance/encoding/VALIDATION.md). This does not implement or change the other proposed boundaries.

## Accepted ADR-0001 boundaries

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.
