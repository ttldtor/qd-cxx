# Live IPF connection contract

[Русская версия](CONNECTION.ru.md). English is authoritative.

## Compatibility inputs

The primary source is pinned QD commit0ddc0a09779fb61d74cc0b37c6449ff47dcb715f:
[InstrumentProfileConnection](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java),
[TimePeriod](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimePeriod.java).
The [actual JDK8 capture](../../tests/fixtures/ipf/live/connection-outcomes.tsv) separates
private Handler.checkAndDownload instrumentation from a public asynchronous lifecycle
witness. Neither is a claim of universal source/API or concurrency equivalence.

## Execution and ownership

- One explicit creator-owned runtime owns one lazy worker and one non-reusable lifetime
  connection slot. maximumConnections values other than1 reject before SDK preparation.
- Owned bootstrap startup precedes application threads; explicit borrowed startup retains
  stable BootstrapLease identity, not a pointer to a movable facade. The application keeps
  the moved-to bootstrap owner alive through creator join and cleanup.
- Native child create/poll/input/output/drain/destruction, persistent transfer host and
  attempt/parser/sink live on the worker. SDK process cleanup stays on creator after join.
- Connection facade owns only a stable Port. Worker, operation and selected callbacks
  retain their own state; callback-side facade close/delete neither joins nor destroys
  native ownership. Runtime destruction on another thread is a contract violation.
- start publishes CONNECTING atomically with successful worker creation; failure leaves
  NOT_CONNECTED and permits retry. Runtime shutdown seals starts before joining.
- close is allocation-free request-only cutoff from any thread. Every finite outer turn
  rechecks cutoff before more native/provider/output work. Native handlers never invoke
  application callbacks. Selected user work may finish; no hard latency bound covers
  filesystem progress, OS scheduling, SDK internals or arbitrary callback work.
- State/auth callbacks execute outside native/owner locks on the worker; collector notifications
  follow its independent outside-lock dispatcher, possibly on another current updating thread.
  Never-started retirement dispatches CLOSED on creator. Callback capture destruction
  also occurs outside locks. Runtime cannot be deleted/joined by its own callbacks.
- Listener nodes reserve their finite independent ledger before allocation. Removed
  selected nodes remain charged until their last selection/capture retires; there is
  no Port↔ledger ownership cycle. Listener tokens never repeat.

## Incremental and generation semantics

The normal snapshot pipe remains its own instantiation with unchanged layout/behavior.
The live wrapper uses IpfReadWork.records=1 per call, including header/comment/control
rows; IpfReadLimits.records is unchanged. It stages at most one owned profile or control,
then invokes the outer sink before another row/turn. feed and finish obey the same rule.
The one-slot wrapper and its copy/move handoffs have explicit additional backing.

Pending profiles preserve all standard/custom values through defensive copies.
Input/source/request objects must remain alive and unmodified through the creating call;
shared authorization providers must obey their existing callback/lifetime contract.
The collector owns immutable published revisions. No full HTTP response or expanded
payload collection is accumulated.

Flush on batch size, `##`, `##COMPLETE` or clean EOF. A failed attempt does not flush pending;
the pending suffix carries into the next attempt. A generation is allocated lazily at
first flush; one attempt uses that same generation after COMPLETE. On every attempt end,
its used generation joins old generations, including after failure. COMPLETE first flushes,
then removes old generations and marks initial completion; clean nonlive EOF also completes,
while clean live EOF only flushes. Failure never authenticates EOF or forces completion.

## HTTP, metadata, retry

Send X-Live: yes, Accept-Encoding: identity and existing bounded source-reader headers.
Only exact provided is live; other/absent values follow fallback completion. Normal
source reader redirects/origin/auth/container rules remain in force. Authorization runs
on the worker as the native attempt creator; secrets do not appear in observations.

Only fallback HTTP with prior nonzero Last-Modified sends If-Modified-Since and accepts304.
A200 response with equal nonzero Last-Modified skips body before capability/CONNECTED
publication and preserves existing generations. Last-Modified commits only after successful
attempt/native retirement. This preserves the stronger source-reader guarantee; Java
assigns metadata after process but before resource close. See [ADR0009](../../docs/decisions/ADR-0009-live-ipf-connection-ownership.md).

The first completion phase is sticky across reconnects; current attempt activity and last
retired HTTP/native/container/IPF evidence are separate observations. A valid marker can
precede a later framing/container/TLS error without erasing published owning revisions.
First attempt cause wins over cleanup failures; collector admission is separately identified.

A normal attempt waits updatePeriod; failures initially wait1000ms, then double retry
delay to min(current updatePeriod, doubled prior retry), without reset on success.
A changed updatePeriod affects a future wait, not an already latched one. Zero is a finite
immediate retry subject to lifetime/attempt/work bounds. QD TimePeriod syntax and signed64
numeric delay, including inf and witnessed JDK8 Unicode decimal integer parts, are preserved.
The full delay is never added directly to steady clock: the wait is clamped to the finite
connection deadline. Negative or syntactically invalid values reject.

## Admission and failure boundaries

All grants are explicit finite values. Pending profile objects/UTF-16 units, old tags,
listener nodes/selections, resident requested backing, attempts and lifetime are independent.
workUnits irreversibly charges one per profile plus its copied UTF-16 units, one per
collector update and one plus visited old tags per generation removal. A failed copy or
later publication does not refund admission. Collector per-call model/comparison/delivery
work and retained revisions remain separately bounded by its explicit limits.

The transfer host preserves native/byte/TLS work, HTTP framing/parser/copy counters,
wire/redirect/attempt counts and decoder input/output/call/work/member/metadata consumption
across attempts. Parser formats may restart; resource grants and shared same-image TLS
history do not. Cleanup reserve is partitioned from the original native grant for every
attempt and never renewed. After first native failure only the existing cache-only raw
owned-prefix/verified TLS plaintext paths can drain; no ordinary poll, new output, provider
call or fabricated EOF is permitted.

Resident admission accounts requested object/vector/string/workspace backing and explicit
control-block allowances. STL allocator metadata/RSS and arbitrary application callback
capture backing are not exact measurements. Overflow, one-below admission, OOM, callback
reentry, cross-thread close and moved bootstrap identity are separate regression surfaces.
A returned failure never claims a complete snapshot; already published revisions remain
valid. Pending unpublished suffix remains private until admitted on a later attempt.

## Build and observation scope

BUILD_IPF_LIVE is optional and requires explicit BUILD_IPF_TRANSFER and its existing
qualified TLS/native/SDK profile. No new dependency or hidden SDK startup is introduced.
Pure IPF/collector remains SDK-free; OFF configuration cannot fetch transfer dependencies.
An installed consumer links qdcxx_ipf_live with its actual exported archive closure.

Targeted concurrency checks use the actual private connection control implementation
without native/TLS SDK archives. Whole connection/HTTPS behavior requires the qualified
native/sanitizer profiles and lifetime review; helper-only TSan is not a whole-SDK claim.

Connections reserve non-reused nonzero domains under the collector mutex, with finite identity exhaustion. Public collector integer tags retain domain zero. Removal matches both domain and attempt generation; equal values refresh both without notification. Independent runtimes sharing a collector cannot remove one another or manual domain-zero updates. The internal model entry carries an additional8-byte domain field; collector limits remain logical counts/UTF-16/work bounds, not exact allocator-byte/RSS bounds. Reserving a domain consumes one lifetime connection work unit before publication.

Pending copy admission reserves aggregate requested backing before allocation: a possible empty-map sentinel, custom node/key/value objects, every standard/custom string capacity plus terminator and a conservative library alignment-request allowance. Supported tree headers use a four-pointer allowance; actual allocation-counting regressions bind the MSVC/GCC profile. Inline string capacity/padding are conservatively counted again. Fixed vector objects and the existing logical pending-unit reservation are included once in resident admission; unused resident backing, including that reservation, funds simultaneous copies. Dense custom data may exhaust resident backing before logical unit/profile limits. Flush releases copy bytes only after destroying pending values; failed copy restores storage reservation without refunding work. This is new connection-owned backing, without changing the normal source-reader or collector logical-budget contracts.

Request-only close publishes its cutoff under the port mutex, releases that mutex, and serializes notification with the runtime control mutex used for retry waits. This prevents a notification from being lost between a false predicate check and the wait handoff. Stop requests occur outside both owner locks. Worker retirement does not depend on creator shutdown sending a second notification.

## First-snapshot wait contract

The public absolute steady-clock wait preserves QD's ordinary sticky-first-completion semantics.
COMPLETED has priority over CLOSED, caller stop and deadline at one port-mutex observation.
CLOSED has priority over stop/deadline; stop has priority over deadline. Immediate results require
no waiter grant. A pending wait reserves a finite slot before registering a stack stop callback.
Its independent per-call turns are finite and never refill on notifications, retries or reconnects.

The fixed frame retains Control, an atomic slot lease, stop callback, gate lock and remaining turns.
Preflight uses the actual sizeof(frame) for every admitted slot, with checked arithmetic.
The lock is declared after the callback and releases first; callback deregistration finishes before
slot release and final Control release. Cancellation registers outside Control, then locks the same
Control gate before notifying. Snapshot publication and close release Port before taking Control.
Waiters use Control→Port for predicate checks, so false-predicate/atomic-wait handoffs cannot lose
completion, close or cancellation. No listener budget, native/provider/user invocation, SDK cleanup,
worker join or new connection progress occurs in the wait or its stop callback.

Worker identity is published under Control before state callbacks. A would-block wait on that worker
is refused; immediate terminal/cancel/expired queries remain usable there. This is the explicit C++
owner adaptation recorded in ADR-0009, not Java API/thread-interruption parity. The caller must not
hold application locks required for producer progress. Other facade destruction requires synchronized
method entry; an admitted wait operates only on its retained Port/Control leases.

A successful wait describes the first application snapshot, not the latest attempt. Existing per-
collector domain/generation replacement remains unchanged: failed prefixes and old generations survive
until a completed replacement;304/equal Last-Modified checks cannot invent a new completed generation.
Later clean replacement removes stale domain/generation entries while already owning revisions survive.
Last-Modified commits only after successful native retirement, preserving the existing first-cause rule.

Live HTTP delivery drains already owned decoded/parser rows before requesting more native input. Each progress/event gets a new outer checkpoint and a fresh bounded single-record work slice; no multiple-row callback loop runs inside parser/native code. NEED_INPUT without a debit/event falls through to ordinary input, so incomplete UTF-8, control rows or compressed headers cannot cause a drain spin. Pending data that cannot make progress under the original slice is refused without increasing/refunding any grant. The snapshot reader instantiation/layout and cumulative HTTP/decoder/source counters remain unchanged.
