# Live instrument-profile collector

[Русская версия](COLLECTOR.ru.md). English is authoritative.

`InstrumentProfileCollector` is an independent, thread-safe repository in `qdcxx_ipf`.
It needs no transport, Feed, runtime, TLS or transfer module. Its input values are the
existing [instrument-profile model](PROFILE.md); all standard numeric/text/date values
and nonempty custom fields are copied without conversion or projection.

## Publication and generations

Symbols use exact UTF-16 identity, including empty symbols. Each current symbol has one
non-removed immutable revision and an explicit unsigned generation; zero means no generation.
A changed input receives a monotonically increasing revision. An equal value changes only
its generation, without advancing the revision, modification time or listener stream.
Equality uses the value model's NaN/signed-zero rules.

Inputs apply in span order as one atomic admission. Superseded revisions within a span
are excluded from not-yet-started deliveries. TYPE `REMOVED` removes a present symbol
and publishes the complete supplied removal value; removing an absent symbol does nothing.
`removeGenerations` ignores zero and publishes fresh tombstones containing only TYPE
`REMOVED` and SYMBOL. No previous custom, description or numeric fields are retained in
a generated tombstone. A failed admission or allocation preserves model, generations,
revision, modification time and queued publication. Allocation failure throws
`std::bad_alloc`; checked errors use `ProfileCollectorError`.

`lastUpdateTime()` reports system-clock milliseconds, initially the creation time.
Only a changed call updates it to the greater of current clock time and the last reported
time plus 1000 milliseconds. Equal updates and absent removals do not change it.
Arithmetic exhaustion is explicit; counters and generations are never silently reset.

## Owning observations and delivery

`snapshot()` returns an immutable owning batch in exact UTF-16 symbol order. Every revision
contains an independent complete `InstrumentProfile`. Batches and individual revision
references remain valid through replacement, removal, collector close and destruction.
A profile span and every input profile must remain alive and unmodified for the entire
call; collector synchronization does not protect caller-owned source mutation.

Registration supplies the current snapshot when nonempty. Later changed calls enqueue
owning batches for current listeners. Before a callback starts, superseded revisions are
filtered out; an empty batch causes no callback. A started batch is stable even if its
callback changes the same symbol. No duplicate initial empty notification is emitted.
Batch ordering follows committed changes; snapshot symbol order is specified separately.

One synchronous dispatcher invokes callbacks serially outside the state mutex. It runs on
the thread starting the outer dispatch cycle. A nested or concurrent update joins that
cycle and may return before its notification. There is no worker, application polling,
executor or completion-wait API. A callback can update, register/remove listeners, close
or destroy the facade. Each public operation first retains stable internal state and
does not dereference the facade after invoking a callback.

Listener removal and close prevent callback starts that have not passed the dispatch
cutoff. A callback already selected under the mutex may finish, including when close
happens immediately before actual invocation. Close is request-only and does not wait.
Callbacks/capture destructors never run under the collector mutex. Callback exceptions
are contained; the saturating `callbackFailures()` count records them and delivery
continues. Ordinary concurrent destruction of the facade still requires external
synchronization with callers accessing that facade; deletion from its own callback is
supported. Captured references remain the application's responsibility.

## Finite resource contract

`ProfileCollectorLimits` contains actual finite allowances, including zero:

| Limit | Meaning |
| --- | --- |
| modelProfiles | Current symbols, excluding removals. |
| liveRevisions / retainedUnits | All live revisions and their retained UTF-16 text/name lengths, including staged and externally retained revisions. |
| customFields | Nonempty custom fields in each accepted profile. |
| stagingProfiles | Input span length or generation-removal model scan size; also bounds tag span length. |
| listeners | Registered callbacks; pending removed callbacks are additionally bounded by the dispatch cycle. |
| liveBatches / batchReferences | All live batches and their total owned revision references, including external observations. |
| workUnits | Per-call index, scan, comparison, copy and task-reference admission work. |
| deliveryCalls | Total callback tasks admitted in one outer dispatch cycle, including nested/concurrent additions. |

The retained ledger is independent of collector state. A lease is released only after its
profile/vector backing is destroyed; closing does not refund externally retained values.
Batch reference reservations retain their original pre-filter slot count until the batch dies; filtering does not renew admission. Failed staging reservations unwind normally. Each mutation may temporarily hold two model
maps, two task vectors and bounded staging vectors. Model nodes are bounded by modelProfiles;
staged revision vectors by stagingProfiles; pending task counts by deliveryCalls. Snapshot
vectors are bounded by modelProfiles. Fixed standard-field backing, custom map nodes,
allocator metadata and control blocks are structurally bounded by these counts and lengths.
These units/counts are **not** an exact heap-byte or RSS budget. User callback/capture
allocations, allocator internals, OS scheduling and arbitrary callback runtime are outside
this ledger. No hard wall-clock latency is promised.

Batch counters are reserved before vector/control-block allocation and before model gathering. One generated removal temporary is bounded by admitted model symbol lengths. Consumed task slots remain empty until the finite cycle ends. Close/destruction allocate no backing: only internal profile/ledger payloads are cleared under the mutex; user captures are retired outside it.

Work units conservatively debit input/current text and field scans, model-copy/index
work, tag comparisons and listener/task reference copying before publication. Removal,
close, delivery filtering and observer methods have finite loops bounded by the explicit
model/listener/reference/task caps. Reentrant publication that would exceed the current
cycle's deliveryCalls fails atomically; accepted work is always drained by the existing
dispatcher, with no leftover application pump requirement.

## Java evidence and C++ ownership boundary

The locked Java `com.dxfeed.ipf.live.InstrumentProfileCollector`, connection and update
listener define symbol replacement, equal-value generation refresh, removal, initial
notification, superseded-entry skipping and reported-time semantics. The bounded
[collector oracle](../../tests/fixtures/ipf/collector/README.md) records actual production
class observations at the revision in `upstream.lock`.

Java stores caller references by default and offers a protected defensive-copy hook. Its reference-retaining base publishes an unchanged same-instance resubmission, while a defensive-copy collector suppresses equal external resubmissions. The C++ API is an immutable defensive-copy/value profile: object addresses never trigger publication, including resubmission of snapshot.profile. This is an ownership adaptation, not universal parity with the reference-retaining base.
C++ always supplies owning immutable revisions. Java's weak concurrent iterator may
expose a removal during traversal; the C++ snapshot captures one committed state and
keeps it stable. Java executor scheduling and iterator object identity are not C++ API
contracts. No connection, source reload, completion wait or scheduler is implemented here.
