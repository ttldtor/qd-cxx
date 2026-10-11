# Bounded synchronous offline replay

[Русская версия](OFFLINE-REPLAY.ru.md). English is authoritative.

This contract defines one finite in-process replay session over a strictly admitted RAW_MESSAGE_V1
tape. [TAPE-WRITER](TAPE-WRITER.md) and [TAPE-WRITE](../../docs/TAPE-WRITE.md) define admission and
owned bytes. It preserves generic records and their exact timing/values without network services,
collectors, ticker/history fanout, event adapters, a thread pool or scheduler framework.
It defines a library API; a user-facing replay CLI is deferred because this component's operational
utility is optional. The selected strict rewrite CLI remains fully specified in TAPE-WRITE.

## Evidence and timing identities

Existing [RAW-TAPE](RAW-TAPE.md) and production
[RAW observations](../../tests/fixtures/protocol/raw-tape/COVERAGE.md),
[packing.tsv](../../tests/fixtures/protocol/raw-tape/expected/packing.tsv) and outgoing
[batch](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message-wire.tsv)/
[singleton](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-singletons-wire.tsv)
witnesses establish timestamp propagation and Java cursor packing. Locked
[TimeSequenceUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/util/TimeSequenceUtil.java)
and [TimeUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java)
are primary sources. This contract's wait/cancellation/outcome policy is deliberate C++ behavior;
it is not Java FileReader speed/seek/reconnect compatibility.

Three independent quantities remain visible: optional exact signed64 raw milliseconds from the
last accepted heartbeat, saturated Java cursor `eventTimeSequence`, and tagged business fields
such as Quote bid/ask time or field SEQUENCE. No business field controls scheduling. Absence is
distinct from explicit zero. Timestamp saturation never changes raw milliseconds. Multiple records
in one data frame share its `TapeTiming`. Empty heartbeats do not reset time; bits1 replaces it
exactly, including negative/equal/decreasing/extreme values. Trailing heartbeats have no record
callback but are parsed; markers do not authenticate EOF.

## API, ownership and callback lifetime

The optional library target `qdcxx_replay` and header `qd/replay/tape_replay.hpp` use namespace
`org::ttldtor::qdcxx::replay`. CMake option `ORG_TTLDTOR_QDCXX_BUILD_REPLAY` defaults OFF.
It depends on `qdcxx_protocol` and core types, never filesystem/JSON/transport. Core/codecs/reader
retain no clock dependency. New dependencies are unnecessary. No runnable replay utility/CLI
option is implied by enabling this library.

`replayTape(const protocol::AdmittedTape&, ReplayObserver&, ReplayClock&, ReplayCancellation&, ReplayLimits, ReplayPolicy)`
returns `ReplayResult`. The immutable admitted tape and callback objects are borrowed for this
synchronous call only; all must outlive it and remain unchanged. No pointers are retained after
return, no callback runs on another thread, and no background work or detached wait survives.
The caller owns callback object destruction. Independent calls share no mutable replay state.
Reentrant/concurrent use of the same callback objects is outside this single-threaded contract.

| Public type/method | Meaning |
| --- | --- |
| `ReplayPolicy {IMMEDIATE, RELATIVE}` | Explicit required policy; no inferred speed, scaling, seek, loop, business-time or auto mode. |
| `ReplayRecordView` | Borrowed `const StreamDataRecord& record`, owning-by-value `TapeTiming timing`, numeric frame index, within-frame record index and sender ID. Record reference lives only until `onRecord` returns. Local provenance stays tied to admitted schema; indices are zero-based envelope/record order. |
| `ReplayObserver::onRecord(const ReplayRecordView&) -> ObserverResult` | Synchronous result OK/FAILED. Copy anything retained before returning. A failure may follow external side effects and is never retried. |
| `ReplayObserver::finish() -> ObserverResult` | Exactly once after clean parser EOF and all deliveries, for final output flush/completion. Never called on cancellation/input/wait/resource/record-callback failure. |
| `ReplayClock::nowNs() -> expected<int64_t, ClockError>` | Nonnegative monotonic nanoseconds, finite 0..INT64_MAX; no wall-clock timestamp. Negative/decreasing samples are CLOCK_FAILURE. |
| `ReplayClock::waitUntilNs(int64_t deadline, ReplayCancellation&) -> WaitResult` | Synchronous READY/CANCELLED/FAILED. May sample cancellation on this thread. READY means this supplied deadline has been reached; verify with nowNs. It must return within the bounded requested slice/cancellation polling policy. |
| `ReplayCancellation::isCancelled() -> bool` | Caller-supplied poll only; once true, replay stops and never resumes that invocation. It must return promptly and not throw. |
| `ReplayResult` | Owns outcome, first enum/static checkpoint and numeric frame/record position, delivered-record count, charged parse/wait/callback work, and `cleanEof`/`observerFinished`. No borrowed diagnostic text. |

Callback interfaces have virtual destructors, project enum/expected results and no ownership
transfer. They do not throw as part of valid use. Engine catches unexpected observer/clock/poll
exceptions and returns OBSERVER_FAILURE/CLOCK_FAILURE/CANCELLATION_FAILURE respectively;
allocation/length exceptions in its own storage return ALLOCATION_FAILURE. It cannot guarantee
bounded execution if user code blocks forever; honoring the prompt-return/wait contract is a
caller precondition. No external callback rollback is promised.

The engine uses a fresh reader over `AdmittedTape`'s owned immutable bytes with separately charged
ReplayLimits. Strict admission authenticates the complete source sequence, but actual replay EOF
requires this reader's finish after exhausting all bytes, not a caller boolean. Reader/feed records
and current frame are engine-owned until consumed/discarded; no views across snapshot update.
Unique layout local IDs allow exact sender identity lookup. Final-reader errors remain errors even
if admission succeeded; replay cannot silently ignore them as an alleged impossible state.

## Immediate and relative scheduling

IMMEDIATE delivers records in wire order without sampling the clock or waiting. It still polls
cancellation and obeys parse/record/callback budgets. All raw time/cursor/business metadata remains
unchanged, including int64 extremes.

RELATIVE schedules **data frames**, once before their first record. Heartbeat/description/marker
frames cause no wait. An empty data frame causes no delivery/wait.
Empty data frames are excluded from **all** scheduling anchor/high-water/relative arithmetic,
clock sampling and waiting: their time metadata is still parsed, preserved and charged by the
reader, but cannot establish t0, advance a target or trigger TIMING_RANGE. Only a nonempty
data frame with present timestamp establishes or updates scheduling. Thus empty timed 100
followed by nonempty timed 110 anchors at 110 immediately; an extreme empty timestamp does
not affect scheduling, while later nonempty timestamps still receive all range checks.
Before the first nonempty data frame with a present timestamp, untimed records deliver
immediately. The first present nonempty data timestamp `t0`
anchors at a fresh validated `nowNs()==m0`; its target is m0, so it does not wait for an epoch.
Record time is not averaged or interpreted as absolute clock time. Later present timestamp `t`
has mathematical elapsed `max(0,t-t0)` milliseconds. Scheduling elapsed is the maximum of all
such elapsed targets so far. Equal/decreasing timestamps never move the target backward, delay
an additional time or reorder records; late clock samples cause immediate delivery. Every record
in that frame uses the same target. No per-record sleep is introduced.

Compute positive signed timestamp difference without signed overflow, then compare against
`maxRelativeMs`, multiply by 1000000 only after checking int64 representability, and check
`m0 + elapsedNs` before addition. Overflow/excess yields TIMING_RANGE before any delivery from
the affected frame. Raw metadata is retained; an extreme timestamp is not clamped to make waiting
work. A first timestamp of either int64 extreme anchors normally; a later huge positive difference
can fail. A timestamp below t0 has elapsed zero without subtracting. Relative scheduling is distinct
from saturated cursor sequence and accepts negative anchors with valid bounded differences.

Wait toward the target in slices ≤ `maxWaitSliceNs`, each computed by checked min(remaining,slice).
Before each slice poll cancellation, check `maxWaitCalls`, then call wait and validate READY via a
nondecreasing nowNs sample at/after the slice deadline. A READY sample before that deadline fails
CLOCK_FAILURE instead of spinning. CANCELLED terminates even if the deadline simultaneously arrives.
FAILED/exception yields WAIT_FAILURE/CLOCK_FAILURE as appropriate. A clock reaching INT64_MAX has
no representable later deadline. Work/call budgets prevent a hostile endless sequence of short waits.
Only caller's injected clock/wait implements real waiting; deterministic tests use a fake clock.

## Cancellation, EOF and failure precedence

Poll before initial parse, before each frame feed, after a wait, before every record callback and
after actual EOF validation before observer finish. Cancellation detected at any of these checkpoints
wins over not-yet-started work. Completed callbacks remain counted; discard the remaining owned
records of the current frame and all future input without callbacks. There is no drain mode or
resume token. A cancellation reported by wait has the same pending-record disposition. If onRecord
fails, report OBSERVER_FAILURE before any later cancellation poll. Observer finish failure wins after
it starts; an eventual cancel does not disguise a flush error.

One reader result that is FAILED may contain a completed record prefix. Replay reports INPUT_FAILURE
and delivers none of that result's records. Already delivered earlier frames remain observable.
Frames whose parsing succeeded deliver in exact order. This fail-closed per-frame rule differs from
the reader's preserved prefix exposure and must be covered explicitly. No partial record escapes.

| Outcome | EOF/completion facts |
| --- | --- |
| COMPLETED | Actual reader finish FINISHED; final cancellation poll false; observer finish OK. cleanEof=true, observerFinished=true. |
| CANCELLED | Pending records discarded. cleanEof=true only if actual finish had already succeeded before the final cancellation poll; observerFinished=false. Never successful completion. |
| INPUT_FAILURE | Reader/feed/finish error with original owning diagnostic; cleanEof=false, no finish callback. |
| OBSERVER_FAILURE | Record or finish callback failed; cleanEof reflects actual parser state; observerFinished=false. Earlier side effects are not rolled back. |
| CLOCK_FAILURE / WAIT_FAILURE / TIMING_RANGE / CANCELLATION_FAILURE | Stop before affected delivery; no finish callback. |
| RESOURCE_LIMIT / ALLOCATION_FAILURE | Engine finite budget/storage failed; retain completed counters and first checkpoint, no partial callback. |

Order at a frame is cancellation → frame/input work bounds → existing reader syntax/profile/resource
checks → failed-result refusal → relative arithmetic → clock/wait/cancel → per-record delivery budget
→ cancellation → observer callback. Final order is actual byte exhaustion → reader finish → cancellation
→ observer finish. First detected error is sticky. Each outcome destroys engine-owned pending values
and buffers before return; caller observer/clock/cancellation objects remain alive and untouched by
destruction. Cancellation is not authenticated EOF; marker/header-only valid files still require finish.

## Finite budgets and checks

`ReplayLimits` contains explicit `TapeLimits`, `deliveredRecords`, `callbackCalls`, `workUnits`,
`maxRelativeMs`, `maxWaitSliceNs`, `maxWaitCalls`, and `clockCalls`. Count fields are finite size_t;
relative milliseconds are finite uint64; wait slice is positive int64 nanoseconds for RELATIVE.
Zero is valid for counts/maximum elapsed and rejects the first needed operation. RELATIVE with
zero/negative wait slice is INVALID_CONFIGURATION before parsing. IMMEDIATE ignores slice/delay
clock values and never calls clock. No library defaults or unlimited sentinel.

Reader bounds independently cover each frame and cumulative parse/work/value/symbol/string/timing
charges; released frames do not refund cumulative work. Replay work additionally counts sender-layout
lookup elements, frame/record attempts, callback/poll/clock/wait calls; check each before invoking user
code. `callbackCalls` includes finish and onRecord, counts attempts even on failure. `deliveredRecords`
counts successful onRecord only; failed attempt count is separate. Counter arithmetic uses remaining
allowance before addition. Peak owning input/schema/layout storage belongs to AdmittedTape bounds;
fresh reader buffers/current returned frame+records retain existing TapeLimits ownership bounds.

Required fake-clock checks include absent/zero/negative anchors, repeated/decreasing timestamps,
int64 extremes and overflowing differences/deadlines, multiple records per frame, delayed observer
causing late clock, exact slice/call bounds, decreasing/negative clock, premature READY, wait error,
cancel before parse/during wait/between records/after EOF, failed current-frame prefix, observer record
and finish/flush errors, budget zero/exact/excess, independent sessions and borrowed-view lifetime.
No test uses wall-clock sleeps/manual window closure or leaves owned processes/threads alive.
Compare delivered raw time/cursor sequence and exact values to existing production Java evidence;
fake-clock targets are designed C++ policy, not Java speed parity. Independent ownership/time/state
review and normal formatting/build/CTest/optional-dependency gates remain implementation requirements.
