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
profile. File timing does not schedule callbacks in this immediate mode.
Existing FIELD metadata, indexed flags, complete payloads and exact schema
provenance pass through the ordinary typed mappers.

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

The lifecycle moves through CONNECTING, CONNECTED when the local source is
open, CLOSING while draining, and CLOSED after clean EOF and retirement.
Unavailable/nonregular input or an I/O error reports `TAPE_IO_FAILURE`.
Malformed input, unsupported profile, and capacity failures report
`PROTOCOL_FAILURE`, `UNSUPPORTED_PROFILE`, or `RESOURCE_LIMIT`, respectively.
A failed source finishes in FAILED. An explicit close cuts off new intake and
drains admitted work under the endpoint drain policy. Use a fresh endpoint to
play again.

The local collector has a real file-source epoch. No TCP/DNS attempt, peer
descriptor, wire subscription or peer acknowledgement exists for it. Native
connection metrics are unavailable. Endpoint connection state on a tape is
local readiness only.
