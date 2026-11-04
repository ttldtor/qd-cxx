# ADR-0009 — Live IPF connection ownership and retirement

[Русская версия](ADR-0009-live-ipf-connection-ownership.ru.md). English is authoritative.

Status: implementation design subject to independent lifetime/concurrency review.
This record does not state product acceptance or universal Java source/API compatibility.

## Context

The pinned QD connection starts a handler thread and exposes callback-driven close.
A C++ facade containing a joinable worker would self-join if deleted from that worker's
callback. Moving process SDK cleanup into the worker would violate the existing
pre-thread bootstrap/creator-cleanup contract. Native cancellation also requires drain
before releasing handler/input/output/configuration owners.

QD Handler assigns Last-Modified after processing, before its resource close. A later
close failure can therefore coexist with committed metadata. The existing C++ source
reader preserves first error and commits only after native retirement.

Primary sources:
[QD connection at the pinned revision](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java),
[actual JDK8 witness/provenance](../../tests/fixtures/ipf/live/connection-provenance.json),
[existing transfer contract](../../spec/ipf/TRANSFER.md).

## Decision

Use an explicit creator-owned InstrumentProfileConnectionRuntime, one lazy worker and
one non-reusable lifetime connection slot. A connection facade retains separate stable
shared control/port state, without a joinable thread or SDK cleanup. Close/delete from
state/collector/auth callbacks requests cutoff; the worker operation survives through
drain. Creator separately closes/joins runtime, then releases outer bootstrap lease and
owned process SDK. Borrowed mode retains stable explicit host identity; no hidden
ApiRuntime bootstrap or global initialization domain is introduced.

Callbacks, capture destruction and collector publication occur outside owner/native
locks. Started connections serialize state notifications on worker; never-started
final retirement occurs on creator. Ordinary concurrent facade destruction and
worker-side runtime destruction remain prohibited caller behavior.

Preserve first native/parser/HTTP/auth/admission error and already published owning
prefix. Commit changed-source metadata only after successful native retirement.
Keep semantic completion sticky separately from active attempt, HTTP framing,
container integrity, IPF EOF and native retirement. Preserve witnessed QD pending
carryover, generation/removal, capability, conditional HTTP and retry behavior.

All work/backing/attempt/lifetime/history allowances are explicit and finite.
The initial supported runtime capacity is exactly one lifetime slot; it is not a
promise of a reusable slot pool. QD inf remains a signed64 numeric delay, bounded
operationally by independent connection lifetime/work/attempt grants.

## Alternatives

A self-owning facade thread cannot safely support callback deletion. Detached workers
lose creator cleanup and joined retirement guarantees. Application-driven polling
moves SDK delivery machinery into normal client code. Repeated whole-response snapshot
loads cannot provide live prefix delivery or failed-attempt pending carryover.

## Compatibility and migration

Java source/API and arbitrary callback-thread identity are not promised. Applications
use an explicit runtime and creator shutdown rather than putting join in a connection
destructor. Metadata is deliberately stricter than Java assignment-before-close:
a later native/TLS close error leaves prior Last-Modified unchanged. Existing published
revisions and semantic completion evidence remain available. This can cause a later
conditional check to request the source again; it does not discard accepted updates
or falsely skip a failed transfer.

No wire extension is introduced. The request uses the existing X-Live capability and
source-reader origin/auth/compression rules. Pure collector/value users acquire no
worker/SDK dependency. Tests cover before-EOF delivery, pending carryover, generation
replacement,304/date skip, close/reentry/delete, first cause, finite admission,
moved bootstrap identity and separate installed/moved consumers.

Connections reserve non-reused nonzero domains under the collector mutex, with finite identity exhaustion. Public collector integer tags retain domain zero. Removal matches both domain and attempt generation; equal values refresh both without notification. Independent runtimes sharing a collector cannot remove one another or manual domain-zero updates. The internal model entry carries an additional8-byte domain field; collector limits remain logical counts/UTF-16/work bounds, not exact allocator-byte/RSS bounds. Reserving a domain consumes one lifetime connection work unit before publication.

## First-snapshot waiting adaptation

Pinned waitUntilCompleted registers an advisory listener, returns sticky completed even after close,
and restores the Java interrupt flag on interruption. The bounded JDK8
[wait witness](../../tests/fixtures/ipf/live/completion-wait-provenance.json) observes those cases and
a finite worker-side CONNECTED wait returning false because its own producer cannot advance during
that wait. Reproducing an indefinitely blocking self-wait would prevent the explicit runtime from draining.

C++ therefore returns distinct completed/closed/cancelled/deadline outcomes, uses a caller stop token
rather than JVM interruption, and refuses a would-block wait on its producer worker. Immediate
completion/cutoff/stop/expired queries retain their normal priority even on that worker. This does not
change first-snapshot generation/metadata rules or authorize callback-owned runtime destruction/join.
Finite concurrent-slot and per-call-turn grants, with actual fixed-frame backing in resident admission,
make waiting capacity explicit. Applications migrate listener/polling readiness glue to awaitCompleted
and keep creator-thread closeAndAwaitTermination for shutdown. Tests separately cover terminal-before-
registration, cancellation handoff, callback close/self-wait, bounds and reconnect replacement.
