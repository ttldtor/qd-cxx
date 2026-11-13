# Bounded connection recovery

[Русская версия](RECOVERY.ru.md). English is authoritative.

This contract specifies the opt-in `TcpRecovery` and `TlsRecovery` owners in
`qd/transport/tcp_recovery.hpp` and `qd/transport/tls_recovery.hpp`. The common
types are declared in `qd/transport/recovery.hpp`. They combine finite reconnect,
liveness checks and replay of current desired subscriptions over fresh protocol
epochs.

## Build and scope

`ORG_TTLDTOR_QDCXX_BUILD_RECOVERY` defaults to `OFF`. Enabling it requires both
`ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` and `ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON`.
The `qdcxx_recovery` target provides plain recovery without TLS/OpenSSL linkage.
`qdcxx_tls_recovery` is available only when
`ORG_TTLDTOR_QDCXX_BUILD_TLS=ON` and additionally links the qualified TLS runtime.
The existing [TCP runtime](TCP-RUNTIME.md), [TLS runtime](TLS-RUNTIME.md), CLI
options and their default single-run behavior do not change.

Recovery is caller driven. It installs no worker, timer or background scheduler.
`start()` requests activation; it does not enter an attempt factory or open a
route. `poll()` spends caller-supplied credits. `nextWake()` returns the earliest
monotonic progress hint; the application must arrange the next turn itself.
This owner does not provide a whole-application graceful-drain protocol.

## Preparation, ownership and cutoff

`create()` validates immutable endpoint/schema declarations, outgoing records,
family bindings, capacities and work before final input adoption. Transferred
queues must be compatible pristine sources; existing registrations and desired
commands are permitted. Every fallible allocation, including the stable outer
operation state, precedes the final nonthrowing transfer. On failure the caller
retains the blueprint and queue ownership; already spent work is not refunded.
Preparation performs no network activation, credential-provider call or consumer
callback. `TlsRecovery` also performs no TLS SDK entry during preparation.

The application must create `RuntimeBootstrap` under its main-thread/process
preconditions and keep its stable borrowed state alive for the entire recovery
object lifetime and until every returning operation lease retires. The recovery
object retains a counted borrow in `CONFIGURED`, `BACKOFF` and `CLOSED` as well
as during native activity; final native retirement alone does not release that
borrow. Moving the bootstrap transfers its stable state without invalidating
active borrows;
replacing or destroying a state with live borrowers violates its lifetime
precondition. Recovery methods, inspection, moves and destruction are serialized
on the thread that created the recovery owner. Checked misuse reports
`WRONG_THREAD` or `REENTRANT`; these errors do not grant concurrent access.

The public recovery handle is move-only. A move transfers the same run; move
assignment first cuts off the old destination, and self-move has no effect.
A provider or consumer callback may call `requestStop()`, destroy the public
handle or replace it by move. The executing operation retains a stable lease on
the old state and rechecks its cutoff and identities after callback return.
An old continuation must not activate a replacement, start another factory or
deliver later callbacks after an abort cutoff. An explicit local drain permits only
the previously queue-admitted prefix described below.

`requestStop()` records an allocation-free monotonic cutoff without inline
native/TLS cleanup, callback dispatch or waiting. Callback destruction uses the
same request-only rule; permitted idle destructor fallback may drain owned
retirement synchronously. The bootstrap borrow follows the full-object lifetime
above; controlled-host premises remain required through retirement.
Recovery exposes detached status and queue batches;
no raw `LiveEvent`, live-session snapshot or native runtime escapes the owner.

## Explicit plain pause and restart

`TcpRecovery::requestDisconnect(clear)` fences native input and retires its actual
attempt while retaining the same Core, queues, registrations and desired interests.
It is owner-thread checked and allowed from a consumer callback; no application
callback runs inline. `poll()` must fund the continuing disposal/reset work.
`SUSPENDED` is nonterminal native quiescence. Without clear, already projected cache
remains eligible for offline cached replay with its original queue epoch. Old
undecided decoded input is released, never relabelled as a fresh source.
Adopted queues receive the generic funded pending-source cutoff before staging;
committed cache remains owned. The lower-owner action establishes that cache
boundary; an already begun nonpreemptible projection may finish. Work exhaustion
keeps pending disposal/admission fences live. See [queue source-offer disposal](../mux/QUEUES.md#discarding-an-uncommitted-source-offer).

Offline progress distinguishes actual cached INITIAL construction from old queued
source/live/initial notification. Only newly built retained-cache INITIAL records
can start callbacks; old STREAM FIFO and queued TICKER updates/initials cannot be
relabeled as replay. The immutable local origin gate survives final drain after
pause. Ordinary active/default TLS and close-without-pause behavior remain unchanged.
With clear, actual queue reset and client/global ACK are required before
`sourceResetPending` becomes false. A later independent clear after completion
starts a new actual reset/epoch/notice/ACK cycle; per-family admission flags cannot
borrow the earlier successful global ACK. Unfinished clear requests may coalesce. An accepted clear fences subsequent stale
selection immediately; state alone is not its completion barrier. Reset does not
invalidate caller-retained immutable batches or refund their original record/batch
charges. `requestReconnect()` resets the old source before actual replacement
factory entry, resetting replay convergence even for unchanged desired membership.
It cannot replenish actual attempts, work or action identities. Remaining-attempt
exhaustion rejects before mutation; final stop/drain supersedes an outstanding clear
without waiting for an application reset ACK. See [public lifecycle](../subscriptions/FEED-LIFECYCLE.md).

Only this explicit reusable plain intent classifies a Windows
`ERROR_CONNECTION_ABORTED` read completion from its subsequently owned cutoff as
expected cancellation. A failure/close already observed before the pause is not
reclassified. Prior primary/secondary, other read/write errors and fatal failures
remain evidence. Ordinary `requestClose()`/final drain and TLS defaults retain
their existing diagnostic policy, including ambiguous Windows final-close errors.
A new actual native owner starts with no inherited pause classification.
## Local admitted-prefix drain and terminal evidence

`requestDrain(deadline)` requests drain of the **local queue-admitted delivery
prefix**. Like `requestStop()`, it immediately fences factories and new native
input/output and requests native abort. It performs no inline cleanup or delivery.
Only later explicit `poll()` calls may deliver already admitted queue projections,
after the native/private attempt has actually retired. The prefix excludes a
complete DATA event still held by Core without successful queue admission, partial
native input and pending output. This is neither a wire flush nor peer acknowledgement
or TLS close-notify completion. It is not a whole-application drain protocol.

The deadline uses `std::chrono::steady_clock`. Repeated requests retain the earliest
deadline; they cannot extend it or undo `requestStop()`, destruction or fatal abort.
`now >= deadline` selects abort, including at request admission. Expiry covers all
unfinished local retirement, not only callback delivery. `nextWake()` includes an
active drain deadline. The owner samples again before each newly entered delivery
action, after potentially expensive native disposal and before terminal publication.
An already running callback, provider, SDK operation or destructor is not preempted;
the deadline is not a hard bound on arbitrary user/SDK execution time.

`RecoveryStatus::terminal` is absent until the scheduler has committed retirement,
the native/private backend is absent, no Core-held event remains and every adopted
queue has published its terminal. The frozen `RecoveryTerminal` contains the exact
optional queue results in TICKER/STREAM/HISTORY order, `deadlineExpired` and
`heldDataDiscarded`. The latter marks release of a complete Core-held **DATA** event
without queue admission during stop/final retirement; it does not invent a lost
record count from partial input. These flags and the result stay unchanged on all
later requests/polls. A moved-from handle reports CLOSED/stopped with no terminal.

`DRAINED` requires an explicit still-valid drain, every adopted queue drained and
no shutdown failure, unresolved gap or discarded Core-held DATA. An expired deadline,
explicit abort, any aborted queue, unresolved pre-fence transport/inactivity failure
or discarded held DATA yields `ABORTED`, unless a fatal current cause, failed
retirement or failed queue requires `FAILED`. Fatal causes include factory,
authentication, configuration/resource/protocol/reset and clock/identity failures.
`first`/`secondary` retain run-wide historical diagnostics; a successfully recovered
old transport failure alone does not make final shutdown fail. Exact native read/write
failures or clean EOF first **observed after** the accepted drain fence affect only
input/output outside this queue-admitted prefix. They remain in `first`/`secondary`
and native evidence, including ambiguous Windows error 1236; they are never relabelled
as cancellation. This exception does not apply to DNS/connect/deadline failures,
fatal TLS/protocol/authentication/resource/cleanup errors, or already observed
pre-fence causes, including nonterminal backend failures. Primary and secondary
causes are classified independently. Exact queue causes
remain attached to their own family; no cross-family chronological ordering is claimed.
`CLOSED` describes scheduler retirement and does not itself mean `DRAINED`.

The optional `wire` terminal preserves the most recent actually retired attempt's
`TcpTerminal`, captured before private disposal and cleared at the next actual
factory entry. For TLS its counters describe encrypted transport bytes. It retains
native primary/secondary causes, discarded unadmitted input, pending output and
local write-acceptance evidence. A local `DRAINED` result can therefore coexist with
post-fence socket errors or a discarded native suffix; it does not assert an
error-free connection, output flush or peer acknowledgement.

`nativeOperationsAtCutoff` is sampled before the first explicit drain/abort fence
requests native cancellation. Empty means no such cutoff was observed; an engaged
zero is an actual zero count. Repeated requests cannot replace it. The count covers
owned submitted native/DNS/connecting lifetimes still awaiting retirement. In a
plain CONNECTED attempt a count of two proves that its read and write/readiness
handlers still require owner progress. It does not prove kernel stalling or that
the operating system accepted no output; queued Windows completions and Linux
readiness waits retain their existing native semantics.

Detached terminal/queue identity copies and application-retained `OwnedMuxBatch`
values remain valid after owner destruction. Such values may retain charged logical
allocations; terminal quiescence does not require their resource counters to be zero.
The fixed terminal storage is part of the admitted owner `sizeof(Impl)` scope.

## Attempts and failure policy

The original endpoint's `retry.maximumAttempts` is a lifetime budget **including
the first attempt**. Successful connections and incoming traffic never reset it.
Retries use the original fixed delay; there is no exponential multiplier,
jitter or authentication-retry option. A combined writer/schema/runtime factory
entry counts once, including when that entry fails. An entered factory failure
is terminal. A lack of credits before entry consumes no attempt and permits a
later funded turn; it does not refund work already consumed by other actions.

After an established attempt fails, the owner must retire its native/private
state and complete the old-source reset barrier before starting a successor.
Backoff cannot bypass either obligation. Exhaustion ends in `EXHAUSTED`;
terminal failure ends in `FAILED`; explicit stop ends in `CLOSED` after local
retirement. `ACTIVE` means local current-demand convergence, never a peer ACK.

Retry classification is conservative:

| Cause | Recovery policy |
| --- | --- |
| Ordinary DNS/connect/read/write failure, transport EOF or a provenance-preserving native operation deadline | May retry after retirement/reset, within the remaining lifetime budget. |
| Protocol inactivity after eligible current transport input ceases | May retry under the same finite policy. |
| Authentication/credential failure, certificate/identity/trust/policy failure, corrupt or unsupported protocol input | Terminal. |
| Generic capacity, allocation, immutable-configuration, identity-exhaustion or reset failure | Terminal. |
| TLS-phase deadline without the exact nested native deadline evidence | Terminal; it is not inferred to be a transient network timeout. |

TLS transport failures retain their typed numeric provenance. A specifically
classified truncated EOF may be transient when no nested protocol failure is
present; an arbitrary handshake failure is not automatically retryable.
The existing occupied output slot and temporary protocol/auth readiness gates
can defer a composition action. They do not convert a generic resource or
security failure into a retry. First and distinct secondary diagnostics retain
bounded static/numeric component evidence without endpoint, token or peer text.

## Monotonic liveness

`heartbeatInterval` and `protocolInactivity` are optional explicit positive
finite millisecond intervals. Deadlines use `std::chrono::steady_clock`;
wall-clock changes cannot renew them. Arithmetic/identity exhaustion is typed
and must not wrap. No hard real-time deadline is promised by this clock choice.

Heartbeat admission requires the current transport, protocol and authentication
eligibility. Its interval measures local output-admission inactivity. Admitting
ordinary output rearms that interval; admission does not prove peer receipt.
An occupied output slot defers the heartbeat without creating another slot or
unbounded pending-frame list.

Protocol inactivity measures validated input belonging to the current attempt
while its transport is eligible. Raw socket bytes, stale-epoch events and local
poll activity do not refresh this deadline. Liveness processing and delivery
remain subject to the explicit work coordinates; `nextWake()` is a scheduling
hint rather than a timer callback or a latency guarantee.

## Fresh negotiation and desired replay

Each entered connection uses a fresh runtime, writer, parser, protocol epoch,
authentication exchange and outgoing descriptions. No prior sender IDs,
authentication success or partially composed output authorize the new epoch.
Replay waits for actual current verified transport readiness, a validated
current protocol advertisement and the applicable authentication readiness.
Each family's real subscription admission remains authoritative.

Replay uses the **latest owner-applied desired state** of the stable family
queues. A queued command takes effect when the queue owner applies it. Desired
changes made while disconnected therefore participate in the next replay;
an applied offline REMOVE must not be resurrected from an older emitted map.
Revision changes during replay invalidate obsolete candidate work. A frame
already admitted to the single output slot must finish or abort before its
follow-up update can be admitted. Empty demand does not require a fabricated
empty-family subscription.

TICKER, STREAM and HISTORY keep their existing
[subscription semantics](../subscriptions/CONTRACTS.md) and
[live-session readiness rules](../subscriptions/LIVE-SESSION.md). Local replay
convergence is not an acknowledgement from the producer and does not establish
upstream continuity or exactly-once delivery.

## Reset, gaps and retained batches

Stable queue registrations survive a connection change. The phased
[queue reset](../mux/QUEUES.md) freezes old intake/callbacks, disposes old pending
owners with bounded work, commits the source reset and publishes its notices.
The replacement cannot consume old protocol offers as new-epoch data.

STREAM reset uses the collector's real reset sequence and discard counts; it
does not fabricate a collector-global ACK. Each client's exact reset-notice ACK
still gates new callbacks. Global source ACK and client notice ACK are distinct;
stale notice sequences cannot acknowledge newer intervals. Reset commit is not
repeated because later notice or ACK progress needs another funded turn.
See the [STREAM collector](../subscriptions/STREAM-COLLECTOR.md) contract.

Caller-retained detached batches remain valid under their existing lease and
remain charged to the same queue reservation ledger after reset. Reset does
not free that external ownership or turn it into a new-epoch callback.
Gaps, discards and interruption remain observable. A disconnected
[HISTORY](../subscriptions/HISTORY-COLLECTOR.md) request is interrupted;
replay does not establish completion, resumable upstream history or exactly-once
delivery. Previously delivered data may overlap a fresh request.

## Observe an adopted HISTORY range

Both owners expose `historyRangeStatus(handle, key, work)` after `poll()` and its
callbacks return. The client handle must belong to the adopted HISTORY queue;
`key` is its exact schema/symbol/selector identity. The caller supplies ordinary
`mux::QueueWork`, including the existing HISTORY observation coordinates.

The query first rejects a moved owner, wrong thread or callback reentry. An absent
HISTORY queue returns `INVALID_STATE` at `recovery-history-queue`. Current observation
requires ACTIVE recovery, no cutoff and no held complete event of any kind. Actual
read-only backend facts must also report a present, connected, authenticated,
nonterminal attempt coherent with the last sampled generation/authentication.
The scheduler's ACTIVE phase alone is insufficient after deferred authentication.
The query performs no SDK entry, I/O, callback, progress or reset acknowledgement.

If these eligibility conditions do not hold, it returns `INVALID_STATE` at
`recovery-history-ineligible`, without calling the queue or changing any work,
first/secondary diagnostic, stop or replay state. This refusal means the current
range is unknown. A previously successful observation must not be reused as current.
Known retirement/reset/backoff is gated even before the queue's reset commits.

An eligible call delegates to the existing [queue range query](../mux/QUEUES.md),
returning its `QueueRangeStatus` unchanged. Queue refusals retain exact nested
`QueueFailure`; work/resource/allocation categories and actual work debits are
preserved. There is no additional work axis, implicit replenishment or retry.
Foreign/stale handle, invalid key and insufficient work follow that queue contract.

Use `completeDeliveredRange` for scoped local completion. Source coverage alone,
raw flags or successful local drain cannot establish it. A requested higher bound
can be complete while a lower aggregate snapshot is still RECEIVING; transaction,
view, queued/unstaged data, gap and reset conditions remain authoritative. See the
[HISTORY completeness rules](../subscriptions/HISTORY-COLLECTOR.md).

The result is detached AS-OF evidence for the exact key, bound, activation, revision
and epoch of the admitted prefix. It survives later progress and owner destruction
as a historical value. It proves neither peer acknowledgement, unread/future wire
input, provider-wide history nor future connection health.

## Capacity, work and qualified copy profile

All `RecoveryLimits` coordinates are explicit and finite; zero means zero.
They are simultaneous componentwise limits, not interchangeable byte credits.
The runtime-entry peak admits three owned schema copies before entry. The
caller-held source schema remains a separate borrowed-input scope. The copied
writer blueprint, adopted blueprint capacities, settings text and retained
writer configuration have independent ceilings.

Each enabled family reserves three replay entry banks and three UTF16 slabs:
latest desired, emitted state and candidate replay. Both `entries` and
`symbolUnits` apply to **each** bank/slab. Retained/adopted storage uses actual
capacities; admitted copy envelopes account for terminators and conservatively
count inline small-string capacity. Checked overflow refuses admission before
allocation or transfer.

`RecoveryCapacity` reports these logical coordinates separately. Replay byte
subtotals overlap replay counts; existing native/session/TLS byte ceilings also
overlap adopted declarations. Queue byte subtotals exclude independently
bounded desired-owner/count scopes. These values must not be added as if they
were disjoint, or presented as whole-process RSS, allocator overhead, kernel
socket storage or a hard wall-time bound. TLS retains the additional
[controlled-host/resource premises](TLS-RESOURCES.md), including persistent
external failed-prefix debt across attempts; recovery does not reset that debt.
This run reserves three private and four ambient failed-initialization prefixes
once, rather than multiplying that reserve by `retry.maximumAttempts`.
Already READY ambient generators may be reused under the qualified profile.
Each fresh private TLS context has fresh private generators and is disposed
before a successor is admitted. Successful attempts add no failed-initialization
debt; terminal failed preparation consumes the once-only three/four reserve.
Declared host totals must be at least
these reserves, but that comparison does not measure free headroom. External
and past failed-prefix debt must fit within the remaining declared totals for
the entire run.

`RecoveryPreparationWork` separates recovery, first-writer and per-family queue
credits. `RecoveryTurnWork` separates operations, visits, text units and bytes
from actual writer, runtime, session and queue work. TLS additionally consumes
`TlsWork`. No work reference escapes a call, no coordinate is implicitly
replenished and a failed action receives no refund. Pre-entry `WORK_LIMIT` can
yield; an already entered factory failure follows the terminal rule above.

The compiled copy profile is deliberately narrow:

| Profile | Qualified implementation family |
| --- | --- |
| `1` | MSVC 19.42 with the selected standard-library headers and ABI. |
| `2` | GCC 13.3 with the selected libstdc++ headers and ABI. |
| `0` | Unknown or unqualified environment; `create()` returns `UNSUPPORTED_PROFILE` before recovery allocation/adoption/activation. |

Compiler version alone is insufficient: qualification includes the selected
header bytes, ABI/macro settings and copy paths. Every participating translation
unit must use that same qualified standard-library profile and ABI. The compiled
factory gate does not attest arbitrary consumer include paths, later target
overrides or foreign template instantiations at runtime. Changing those premises
requires fresh qualification rather than assuming the capacity proof transfers.

## Required live contract capabilities

A recovery owner configured with all three queues may require TICKER, STREAM and HISTORY
as one negotiated set. Each connection attempt starts with a fresh capability set. Before
replaying any subscription, recovery requires the standard peer DATA declarations and
peer ADD and REMOVE declarations for every family. Descriptor deltas can complete the set.
The configured read duration also bounds this capability negotiation from connection
observation. On expiry, recovery reports `UNSUPPORTED_PROFILE` at `recovery-peer-contracts`.
DATA offered before mixed capability readiness is refused at ecovery-data-before-contracts.
Authentication continues to gate wire output independently. Explicit disconnect, shutdown and
backoff exclude the prior attempt's negotiation wake. The option defaults to off.
