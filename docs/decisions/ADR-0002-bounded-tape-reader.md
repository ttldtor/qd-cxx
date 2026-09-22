# ADR-0002 — Bounded tape state with immutable snapshot replacement

[Русская версия](ADR-0002-bounded-tape-reader.ru.md). English is authoritative.

Status: implementation decision within the owner-authorized QDCXX-021 boundaries; independent review pending.

## Context and decision

[Contract](../../spec/protocol/TAPE-READER.md) records the Java evidence and explicit differences.
Use a private append-only description list and rebuild StreamDataContext for each complete update.
Charge cumulative candidate construction before copying; commit both list and context only on success.
The reader owns a bounded local-schema copy. It exposes no mutable plans or general session API.
Reuse FrameReader and existing description, binding and STREAM_DATA primitives without changing their
contracts. IO and presentation belong to the optional utility, not core/protocol.

Accept exactly one initial restricted descriptor; version is diagnostic, unknown extensions fail.
Accept late first-use IDs, reject every repeat. Empty envelopes are observable no-op markers, never
permission to ignore trailing input. Clean EOF after the header is accepted without a footer. Stop on
first error with completed data prefix. These restrictive/atomic/terminal policies are explicitly
approved by the assigned task; Java's permissive properties, partial installations and continuation
are preserved as observations, not implemented as lifecycle requirements.

## Alternatives and consequences

An append-only mutable plan table could reduce copying but needs a new decoder/context interface.
An unrestricted session API introduces replacement/reset semantics outside scope. Snapshot rebuilding
is the smallest reuse path; it costs repeated bounded scans and temporarily holds old/candidate state.
The cumulative construction budget limits repeat work as well as storage. The contract documents
quadratic lookup bounds; no throughput or allocator-memory claim is made.

A new versioned tape output avoids changing the raw-QTP inspector profile or historical output.
Existing fixtures and upstream.lock stay byte-identical. Migration is additive: users explicitly
select the new executable and diagnostic local profile. General tape, default TapeConnector RAW_DATA,
compression, time, Quote, importer and transport remain excluded. Public API is provisional.

## Validation

Two clean Java captures and nine mutation rejections preceded this decision. C++ matrix, ownership,
atomicity, resource boundaries, EOF and process IO tests are required before completion; no executed
C++ result is claimed by this pre-implementation ADR. Review remains open until separately performed.
