# Specifications

[Русская версия](README.ru.md). English is authoritative.

This directory holds implementation-independent contracts extracted from upstream evidence and intentional qd-cxx decisions.

Specifications should distinguish:

- **normative** requirements qd-cxx commits to;
- **descriptive** Java QD behavior observed at the locked revision;
- **open** questions not yet verified;
- **intentional deviations** backed by an ADR.

Every nontrivial upstream-derived statement should be traceable. Prefer examples that can become conformance fixtures. Do not use C++ class design as the specification language unless the contract is specifically a C++ API.

Areas:

- `schema/` — records, fields, types, aliases, mappings, and scheme construction.
- `protocol/` — QTP messages, framing, negotiation, and state machines.
- `encoding/` — byte-level primitives and stateful codecs.
- [TCP transport](transport/TCP-RUNTIME.md) — caller-driven runtime ownership, progress and close, with the [DNS resource derivation](transport/DNS-RESOURCES.md).
- [STREAM collector](subscriptions/STREAM-COLLECTOR.md) — every-record staging, membership/revision barriers, whole-frame overflow and release-work accounting.
- [TICKER collector](subscriptions/TICKER-COLLECTOR.md) — shared latest cache, bounded local views, initial/live ordering, explicit retention and stale/reset barriers.
- [Aggregate interests](mux/INTERESTS.md) — exact upstream union, independent local clients, raw HISTORY bounds and atomic bounded mutations.
- [Local STREAM dispatch](subscriptions/CLIENT-DISPATCH.md) — serialized callbacks, bounded cross-thread commands, detached batches and explicit local completion; source/transport composition remains caller-owned.
- [Controlled STREAM usage](../docs/SUBSCRIPTIONS.md) — optional plain-TCP CLI and programmatic example, exact selection, generic records and prepared summaries with separate local/transport outcomes.
- [Subscriptions](subscriptions/CONTRACTS.md) — delivery ownership, resource limits and lifecycle design; [bounded desired state](subscriptions/STATE.md), [outgoing composition](subscriptions/WRITER.md), the [pure live session](subscriptions/LIVE-SESSION.md), STREAM staging and bounded TICKER cache/views are implemented, with [selected Java evidence](../tests/fixtures/subscriptions/CAPTURES.md). Full end-to-end client and the HISTORY collector remain pending.
