# Architecture

[Русская версия](ARCHITECTURE.ru.md). This English document is authoritative.

## Clarified goal: library and operational programs

The project includes equivalents of the necessary qds-tools utilities, not just a library/inspector.
The [utility inventory](upstream/QDS-TOOLS.md) records the upstream commands and dependencies.
Feeds, multiplexing, recording/replay and
diagnostics are use categories, not preapproved command names. Shared data/subscription behavior
belongs in libraries; transport handles connections; programs compose components, configuration,
management and observability. Do not turn the inspector into a universal server or duplicate shared
logic. The selected runtime and current ownership contracts are described below;
CLI/properties compatibility is defined per utility.
[Roadmap and migration policies](NEXT-STEPS.md).

## Intent

QDCXX-025 implements the bounded [prepared event mapping boundary](../spec/events/EVENT-MAPPING.md):
immutable local schemas carry an opaque record context through generic decoding, while separate
Quote/Profile adapters own validated field selections and event values. Schema copies preserve context;
fresh schema creation creates a new identity. Wire binding and source configuration remain separate.
The [Java configuration subset](../spec/schema/CONFIGURED-EVENTS.md) provides source evidence;
it does not add a C++ loader, ambient process settings or transport to core/event libraries.

`qd-cxx` will reproduce the externally observable contracts of Java QD without mechanically reproducing its class hierarchy or historical implementation choices. The design target is a modular, testable C++23 system.

## Component boundaries

These logical boundaries guide the implementation. [Current capabilities](CURRENT-STATE.md)
identify the implemented profiles and their remaining limits:

- **qdcxx_core**: records, field values, scheme semantics, symbols, subscriptions, and core state transitions. It must not depend on transport.
- **schema/model**: immutable resolved records, fields, complete type identities, names/lookups and a Java slot projection; source operations and mapping/provider information are retained as provenance, not executable Java mappings.
- **encoding**: byte-level primitives and stateful codecs with explicit error and ownership rules.
- **protocol**: QTP messages, framing, negotiation, and protocol state machines expressed independently of a socket library.
- **transport adapters**: connections, I/O scheduling, backpressure, lifecycle, and integration with the optional networking runtime.
- **tools**: importers, inspectors, fixture capture, and compatibility diagnostics.

Dependency direction should point inward: transport may depend on protocol; protocol may depend on encoding and core contracts; core must not depend on transport.

## Configurable schemas and event mappings

[ADR-0003](decisions/ADR-0003-configurable-schemas-and-event-mappings.md) makes runtime-resolved schemas a project-wide requirement.
Built-in defaults, programmatic construction and the optional [native JSON loader](../spec/schema/NATIVE-JSON.md)
use the validated local-schema boundary. The [offline Java bridge](../tools/schema-export/README.md)
resolves its supported XML/configuration inputs before C++ loads the exported schema. Native XML
construction and general Java-property resolution remain separate work.
Generic records remain available without standard event adapters; Quote/Profile mappings bind to a particular schema instead of assuming its default field order.
Source configuration, wire adaptation and event projection remain separate responsibilities.
QDCXX-025 implemented the first bounded mapping contract. Later loaders and mappings retain their
own explicit schema/type limits; ADR-0003 does not imply arbitrary-schema support.
The [configuration requirements](CONFIGURATION.md) apply beyond schema settings: property sources, precedence, scope, read timing and diagnostics are explicit compatibility concerns.

## Live subscription ownership

[ADR-0005](decisions/ADR-0005-live-subscriptions.md) establishes the ownership design for live subscriptions;
the [subscription contracts](../spec/subscriptions/CONTRACTS.md) define its observable barriers and budgets.
`qdcxx_core` owns deterministic identities, desired-interest maps and bounded atomic mutations.
Live session/client/runtime layers own connection epochs, queues, callbacks, I/O and lifecycle.
The local dispatcher has one logical serialization domain. The current typed Feed runtime owns a
shared worker for its endpoints; its callback and shutdown rules are defined in the
[Feed guide](FEED-API.md#runtime-callbacks-and-shutdown).
The [pure protocol session](../spec/subscriptions/LIVE-SESSION.md) owns a fresh epoch,
immutable peer generations and whole admitted frames. Caller-supplied byte spans and work credits
keep that layer independent of runtime, clocks and application callbacks. Its partial-write cursor
does not acknowledge peer delivery.
The [STREAM collector](../spec/subscriptions/STREAM-COLLECTOR.md) adds pure bounded staging
over admitted events and owned desired state. It preserves event order without conflation,
filters retired membership activations before offering records and reports whole-frame overflow.
Its staging, desired-state and shared epoch bounds are separate scopes to aggregate at the upper owner.
The [TICKER collector](../spec/subscriptions/TICKER-COLLECTOR.md) owns a shared latest cache
and finite independent local views. Explicit retention, per-view initial/update metadata,
whole-frame publication and stale/reset barriers keep cached evidence distinct from fresh delivery.
Local view interests do not export upstream demand or add a callback/runtime owner. The separate pure `qdcxx_mux` component captures a bounded desired-interest set per local client and publishes their exact aggregate union. Its per-client selection predicate remains separate from wire reconciliation, collector staging and callback delivery; the caller owns those components and any source-session integration.
The [HISTORY collector](../spec/subscriptions/HISTORY-COLLECTOR.md) adds bounded per-view projection over whole admitted source events, with shared raw-indexed storage and independent requested-range completeness. It consumes events admitted by a session; source sessions, upstream demand and callbacks remain separate owners. A complete requested range for one view does not imply a globally complete source snapshot.
The [local STREAM dispatcher](../spec/subscriptions/CLIENT-DISPATCH.md) owns one collector,
one guarded consumer, bounded command/result storage and detached immutable batches.
Only its command handles, results and detached leases cross threads; source event leases stay
in the serialized owner domain. The caller owns the actual session/runtime, explicitly routes
events and subscription output, and combines local completion with transport/output shutdown.
[ADR-0006](decisions/ADR-0006-live-runtime.md) selects an optional Asio/c-ares/OpenSSL profile,
explicit SDK/bootstrap/trust requirements and its [feasibility boundary](../tests/conformance/transport/CONTRACTS.md).
The [optional TCP adapter](../spec/transport/TCP-RUNTIME.md) implements only the caller-driven,
credential-free plain subset, with explicit bootstrap, bounded SDK sources and owned-operation
retirement. The [controlled STREAM tool/example](SUBSCRIPTIONS.md) connects this actual runtime
to the dispatcher with explicit add/remove output and separate local/transport terminal evidence.
The separate [TLS runtime](../spec/transport/TLS-RUNTIME.md) and [typed Feed API](FEED-API.md)
implement their documented receiving profiles, including verified TLS and optional Basic authorization.
The plain TCP adapter retains its own narrower contract. The [receiving qualification](../tests/conformance/subscriptions/DELIVERY-VALIDATION.md)
records the composed checks; neither the ADR nor component checks establish full QD compatibility or release.

The [server execution strategy](../spec/transport/SERVER-EXECUTION.md) compares the current
owner loop with a candidate adapter on the existing Asio pin, including ownership,
shutdown and the evidence required before switching.

## Accepted direction for further implementation

The next architecture extends the shared record path across receiving, processing and publication.
A schema-based `RecordFeed` and the typed `Feed` share an `Endpoint`; immutable owning record blocks
preserve all fields, schema identity and source generation. Prepared bindings support event projection.
Callbacks borrow views; explicit retention and copying have separate ownership and capacity charges.
These are accepted requirements for further implementation, beyond the current [Feed profiles](FEED-API.md).

Logical channels will separate schema, routes, permitted data/subscription families and lifecycle within
an endpoint. Bidirectional exchange and publisher support require independent send/receive readiness
and authorization. Source identity, ingress generation and qualified route provenance remain explicit;
merging independent inputs does not establish a global source order.

Input, collector state, consumer queues, retained data and output require separate configurable byte
budgets with coordinated reservations. Handoff size is independent of input capacity and QTP frame size;
ready data proceeds without waiting to fill a batch. Each mutable processing partition has one execution
owner. Notifications for one logical subscription remain serial and non-reentrant, while independent
subscriptions may execute concurrently through a shared executor outside the network owner and internal
locks. This direction does not change the current shared-worker contract described above.

Implementation must establish observable ordering, completion, failure and resource behavior, then
measure comparable receiving/processing/publication paths against Java QD. An accepted design does not
establish implementation, measured performance or release. See the [roadmap](NEXT-STEPS.md).

## Design principles

- Specify observable behavior before implementation.
- Make ownership, lifetime, cancellation, threading, and error semantics explicit.
- Keep deterministic transformations separate from I/O.
- Prefer replaceable adapters at upstream, filesystem, timing, and transport boundaries.
- Preserve source traceability for behavior learned from Java QD.
- Use ADRs for consequential or difficult-to-reverse decisions.

## Not decided yet

A package manager, stable public API and general binary compatibility policy remain undecided. The selected optional networking profile and serialized ownership model are described above and in ADR-0006. The QDCXX-010 owner follow-up selects doctest as a test-only FetchContent dependency. Further decisions require scoped tasks and evidence.

The original QDCXX-010 increment implemented only the encoding boundary's compact int/long primitives, with a provisional span-based API, minimal CMake/CTest and no third-party runtime dependency. See [build/API](CPP-BUILD.md) and [validation](../tests/conformance/encoding/VALIDATION.md). Its validation record retains that original scope.

## Accepted ADR-0001 boundaries

Accepted by the project owner on 2026-09-21. The three boundaries are logical ownership boundaries; they do not require three file formats or three serialization subsystems. Core can consume a resolved local schema without loading XML or its history. Preserve copied-main-alias behavior only for XML import under the pinned QD 3.355 profile; it is not a requirement for future native C++ schema construction. Retain Java slot projection without prescribing C++ storage. Approve structured unsupported-local-type diagnostics for BYTE/SHORT while retaining local-construction rejection; this does not ban their wire codes. Keep original requests and selected layouts separate. JSON was accepted for schema-description interchange. The later native JSON profile selects its private pinned loader dependency; canonical hashes and whole-session serialization remain undecided. Existing oracle evidence is unchanged.
