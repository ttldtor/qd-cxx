# Local tape as a Feed source

[Русская версия](FEED-TAPE-SOURCE.ru.md). English is authoritative.

`Endpoint::connectTape` selects one regular binary file for the endpoint.
It owns the supplied path after the call and opens the file asynchronously on
the existing runtime worker. Supply a stable regular path that remains a
regular file throughout playback. No URI parsing or file discovery is performed.
A read is synchronous at the OS boundary and its completion time depends on
the underlying file system.

The adapter uses the [binary catalog reader](TAPE-READER.md), the endpoint's
exact receiving schema and the existing typed subscription path.

| Input | Selected endpoint delivery |
|---|---|
| TICKER_DATA, STREAM_DATA, HISTORY_DATA | The matching endpoint contract receives the records. Other families are parsed and skipped. |
| RAW_DATA | The endpoint's explicitly selected contract receives the records. |
| Descriptions, descriptor deltas, markers, heartbeats | Validated as tape input. No typed application event is created. |

The qualified binary profiles accept supported header or self-described
headerless input and inline NONE, MESSAGE or FIELD timing. Companion-time
files, compressed containers, text and blob modes are outside this source
profile. Actual FIELD timing supplies each typed event's `eventTime` when
available. Otherwise a preceding timestamped MESSAGE heartbeat supplies its
Java cursor time. Untimed input keeps the event type's unspecified value.
This metadata is independent of price, trade, candle and indexed time fields.
Registry routes with `EventTimePolicy::NONE` keep their original behavior.

Configure subscriptions before connecting. The worker applies their initial
desired commands and synchronizes collector registrations before reading. It
uses the normal filtering, batching, callback execution, and retained-history
boundaries. Each successful tape envelope is admitted whole. A rejected envelope
contributes no records, including when its decoder found a successful prefix.
Older accepted events can finish delivery.

`TapeSourceOptions` defaults to 64 MiB of file bytes and 65,536 envelopes.
Each buffered read requests at most 4096 bytes. Per-frame limits come from the
endpoint's existing receiving profile. Reader construction, cumulative decode
work and event ownership have separate finite allowances. The worker retains
at most one unadmitted tape result or qualified event, plus the existing
collector and subscription reservations.

## Pacing and pause

`TapePlaybackPolicy::IMMEDIATE` is the default. For relative playback, select
`TapePlaybackPolicy::SOURCE_TIME` and set `speedPercent` from 1 through 10,000
(100 is normal speed). The first timed eligible envelope starts the runtime's
monotonic clock. An envelope with multiple timestamps is due at its latest
record time. The adapter uses physical FIELD time when present and the exact
inherited MESSAGE milliseconds otherwise. Untimed envelopes add no delay.
Backward timestamps use the elapsed-time high water mark and preserve file
order. The scaled delay is rounded up to the next whole millisecond.

`maximumSourceSpan` limits forward source time from the first timed envelope.
It defaults to 24 hours and must be positive and at most 365 days in relative
mode. An excessive span or unrepresentable deadline reports `TAPE_TIMING_RANGE`.
The worker holds one future envelope and can wake for its deadline, source
control or other active endpoints.

Set `initiallyPaused` to open the file without reading envelopes. Use
`Endpoint::setTapePaused(true)` and `Endpoint::setTapePaused(false)` to hold
and resume intake on the same endpoint. The worker applies the latest intent,
shifts the relative clock by its observed pause duration, and lets previously
admitted work finish. Early close releases the pending envelope and does not
wait for the playback deadline.

## Restart and stop

Call `Endpoint::restartTape()` while the tape is connected, including while
paused. An accepted request publishes CONNECTING, fences new selections from
the old source, and joins any restart already in progress. The worker cancels
old not-started callbacks, waits for running callbacks to retire, then resets
the collector and its HISTORY coverage. It reopens the same path at byte zero
with a fresh playback clock and source epoch. Desired subscriptions, listeners
and pause intent stay on the same endpoint. CONNECTED reports the new open.

The endpoint's `drainTimeout` bounds the restart attempt. Expiry reports
`TAPE_RESTART_TIMEOUT` in FAILED. A reopen error reports its own diagnostic.
Actual callback and operating-system I/O quiescence can take longer. An old
queued task cannot invoke a listener in the new epoch. A reset can leave a
continuity gap in final accounting even if the new pass completes.

`Endpoint::stopTape()` starts the normal terminal drain. The existing close
policy cancels selected custom-executor callbacks that have yet to start.
`stopTape(TapeStopMode::ABORT)` discards queued delivery and can escalate an
unfinished drain. Stop or close takes precedence over a pending restart.
Use `closeAndAwaitTermination()` to read the detached `CloseResult` after
native and callback retirement.

The lifecycle moves through CONNECTING, CONNECTED when the local source is
open, CLOSING while draining, and CLOSED after clean EOF and retirement.
Unavailable/nonregular input or an I/O error reports `TAPE_IO_FAILURE`.
Malformed input, unsupported profile, and capacity failures report
`PROTOCOL_FAILURE`, `UNSUPPORTED_PROFILE`, or `RESOURCE_LIMIT`, respectively.
A malformed suffix ends in FAILED after previously admitted work drains under
the endpoint drain policy. A clean EOF completes in CLOSED after that drain.
After CLOSED or FAILED, use a fresh endpoint to play again. Restart in this
file profile always starts at byte zero. The profile offers no arbitrary seek.

The local collector has a real file-source epoch. No TCP/DNS attempt, peer
descriptor, wire subscription or peer acknowledgement exists for it. Native
connection metrics are unavailable. Endpoint connection state on a tape is
local readiness only.
