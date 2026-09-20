# Architecture

[Русская версия](ARCHITECTURE.ru.md). This English document is authoritative.

## Intent

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

## Design principles

- Specify observable behavior before implementation.
- Make ownership, lifetime, cancellation, threading, and error semantics explicit.
- Keep deterministic transformations separate from I/O.
- Prefer replaceable adapters at upstream, filesystem, timing, and transport boundaries.
- Preserve source traceability for behavior learned from Java QD.
- Use ADRs for consequential or difficult-to-reverse decisions.

## Not decided yet

No networking library, build system layout, package manager, test framework, public API, threading model, or binary compatibility policy has been selected. Those decisions require scoped tasks and evidence.

## QDCXX-007 proposal

[Normalized-model requirements](../spec/schema/NORMALIZED-MODEL.md) and [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.md), status **proposed**, separate source declarations/overlays as importer provenance, resolved local schema as a first-class core artifact, and incoming sender-ID mappings plus requested/selected outgoing layouts as first-class protocol state. Local schema identity does not include peer/session state. Logical values and Java-compatible slot observations do not prescribe C++ physical storage.

Versioned JSON interchange is recommended, not adopted; format/profile versions, immutable snapshots and scoped references are proposed without choosing dependencies or public APIs. [Follow-up gates](NEXT-STEPS.md) separate owner decisions, focused probes, importer contract and encoding work. No importer or C++ implementation is authorized by this proposal.
