# ADR-0016: Whole-publication queue

[Русский](ADR-0016-whole-publication-queue.ru.md).

Status: selected design; publisher integration and comparative measurements are pending.

## Context

Applications can use copies of a `Publisher` from several threads. The publisher
worker is the sole owner of collector, Mux and socket progress. This establishes
a multiple-producer, single-consumer handoff. The
[publication contract](../../spec/subscriptions/PUBLISHER.md) distinguishes local
admission from delivery to a remote application.

The current catalog STREAM path in
[`publisher.cpp`](../../src/api/publisher.cpp) holds the port mutex while validating,
allocating and copying input. A shared payload already avoids separate allocations
for each row in eligible calls, but the mailbox still stages and removes per-row
owners. Shortening the copying path requires a corresponding change to ownership,
byte reservations and concurrent close.

## Decision

Prepare one owning publication outside the mailbox mutex. Link the prepared
publication with a short mutex-protected operation. The worker takes ownership of
ready publications and processes them outside that mutex. Validation, allocation,
payload copies, destruction and application callbacks stay outside the commit
critical section.

Use the shared record representation described in the
[ownership contract](../../spec/schema/RECORD-OWNERSHIP.md). A publication retains
its immutable schema and source metadata. Its queued state contains no endpoint,
socket or runtime owner. Avoid an additional per-row owning representation when
moving the same publication from the producer to the worker.

### Reservation and failure

Before allocating, compute and atomically reserve the complete publication's
required capacity. Include the queue node, descriptors, strings, byte arrays,
alignment and shared control allocations. Schema capacity follows its documented
accounting model. Overflow in size arithmetic is a refusal.

Physical block capacity, input admission and each consumer's quota are separate
accounts. A shared block reserves its physical capacity once, while each consumer
pays for the capacity it can keep alive. The complete input credit stays reserved
through queue detachment and every partial worker step. Return it only after the
target collector has consumed every row of that publication, or the publication
has been definitively discarded. Detachment or consumption of a prefix does not
return any of that credit; remote delivery is not a prerequisite for returning it.
Backing credit remains until its final owning reference is released. Closing the
endpoint does not erase outstanding retained capacity.
Allocator bookkeeping, OS buffers and thread stacks retain their documented
measurement boundaries; the budget does not establish an exact RSS ceiling.

Reserve the resources needed to commit the whole publication before making any
row visible. A validation, allocation or capacity failure admits no prefix and
releases the caller's reservations. A request larger than the configured capacity
is distinguishable from temporarily occupied capacity. No fixed record count is
introduced as a replacement for byte admission.

### Ordering and close

The successful queue link is the admission point. Non-overlapping calls from one
producer preserve their call order. Overlapping producers are ordered by queue
commit; reservation order and wall-clock start time do not define their order.
The records in one admitted call retain their input order and remain contiguous
at this handoff. Collector-specific conflation and HISTORY transaction semantics
continue to apply after admission.

Close and queue commit use the same fence. Once closed, the queue accepts no new
publication. A producer already preparing data can finish safely using its owned
port, immutable schema and reservations, then observe `CLOSED` at commit. Release
its rejected data outside the mutex. The port and budget controls may outlive the
endpoint and contain no native runtime dependency.

Shutdown detaches abandoned queue ownership under the fence and destroys it
outside the critical section. Destruction of a long chain is iterative. Existing
close behavior remains: worker termination does not promise delivery of every
previously admitted record. The worker must stop accessing native resources before
the endpoint's termination barrier completes; detached record owners can survive
that barrier.

### Worker scheduling

Transfer already-ready data without waiting for a fuller publication. Detach a
ready chain in constant time, then advance its worker-owned cursor under the
existing service budget. Keep input capacity, delivery window, QTP frame size and
generator call size independent. A work quantum can bound a progress step without
rejecting a larger valid publication.

The wait predicate and commit notification must prevent a missed wakeup when a
producer commits between the worker's progress step and its wait. Shared control
and statistics updates must not pull copying or destruction back into the commit
critical section. This handoff introduces no extra worker or queue between the
collector and Mux when the same execution owner operates both.

## Alternatives and consequences

| Option | Consequences | Decision |
| --- | --- | --- |
| Short mutex around a prepared publication | One ordering and close fence; expensive preparation is independent; straightforward failure cleanup | Implement and measure first |
| General lock-free multiple-producer queue | Requires separate publication, reclamation and close proofs; contended byte reservations still exist | Reconsider if measurements attribute material remaining cost to the short commit fence |
| One single-producer queue per caller | Needs producer registration, queue merging and fairness; topology changes with callers | Use only where a separately established producer/consumer topology warrants it |
| Larger per-row FIFO | Can absorb a burst while retaining row staging and a larger waiting backlog | Does not implement this ownership handoff |

The selected design removes copying from the shared fence and exposes a complete
publication as the handoff unit. Its throughput and tail latency remain outcomes
to measure. It does not change QTP encoding, create exchange order across unrelated
sources, or increase the number of worker threads automatically.

## Compatibility and acceptance

Preserve typed event publication, arbitrary catalog records and mixed contracts.
Retain the existing error and close behavior unless a separately documented public
contract explicitly changes it. Legacy configuration remains usable. The generator's
batch size is a workload parameter; it is independent of queue capacity and output
framing.

Integration must verify atomic admission failures; concurrent producers with exact
fields, per-producer order and publication boundaries; close during preparation
and commit; allocation failure; retained data after endpoint destruction; and byte
accounting through the final owner. Existing typed, record and mixed publication
tests remain required, along with supported Windows and Linux sanitizer builds.

Use the [Quote/STREAM comparison](../../tools/nettest/benchmark/QUOTE-STREAM.md)
and [metric definitions](../../tools/nettest/benchmark/METRICS.md) for interleaved
baseline, candidate and Java QD 3.357 runs. Keep toolchain, generator batch 8,
receiver, record content and client count identical within each comparison.
Start with one client and then exercise fan-out. Check exact delivery before
comparing sustainable rate, per-client p99, per-thread CPU, allocations and memory.
Keep profiling runs separate from throughput runs. An unsuccessful optimization
does not replace the measured baseline.
