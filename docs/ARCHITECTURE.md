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

No networking library, package manager, stable public API, threading model, or binary compatibility policy has been selected. The QDCXX-010 owner follow-up selects doctest as a test-only FetchContent dependency. Those decisions require scoped tasks and evidence.

QDCXX-010 implements only the encoding boundary's compact int/long primitives, with a provisional span-based API, minimal CMake/CTest and no third-party runtime dependency. See [build/API](CPP-BUILD.md) and [validation](../tests/conformance/encoding/VALIDATION.md). This does not implement or change the other proposed boundaries.

## Accepted ADR-0001 boundaries

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON is accepted for schema-description interchange; libraries, canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.
