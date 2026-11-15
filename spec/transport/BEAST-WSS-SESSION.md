# Private Beast source connection

[Русская версия](BEAST-WSS-SESSION.ru.md). English is authoritative.

The [optional Beast profile](../../cmake/beast-wss-profile/README.md) builds a private owner for
one connection attempt in a dedicated source-adapter process. One thread starts, drives, stops
and destroys the attempt. The owner retains its executor, resolver, socket, TLS context and
buffers. The connection progresses through resolution, TCP connect, verified TLS and the Beast
HTTP upgrade. Only a successful upgrade opens `READY`. Connection setup delivers no source data.

Supply a route host, port, DNS identity, HTTP request target and explicit trust PEM. DNS SAN
verification and SNI use the identity. The HTTP authority uses that identity and, for a port
other than 443, the configured port. Resolution uses the route host and numeric service. The owner
retains at most eight resolved endpoints for sequential TCP connect under one deadline. The system
resolver can use its own native storage before returning results.

## Verification and resource limits

The [TLS boundary](BEAST-WSS-TLS.md) applies to context ownership, fixed encrypted buffers, SAN
verification, explicit trust and disabled TLS features. Before decoding, the owner scans the whole
trust input of at most 512 KiB and 256 ordinary certificate blocks. Each anchor must qualify as a
CA. The profile admits RSA keys from 2048 to 4096 bits with public exponent 65537 and named
P-256/P-384 EC keys. The private provider validates each public key. Peer verification guards
the leaf and all untrusted entries before calling the OpenSSL verifier. It uses the native 64 KiB
certificate-list limit, verify depth four, at most six untrusted entries and a 16 KiB serialized
size limit per checked certificate. DNS SAN verification disables partial-label wildcards and CN
fallback. The dedicated process initializes OpenSSL without ambient configuration before any
other OpenSSL use. The attempt uses its explicit trust anchors without loading system roots.

The pinned Beast HTTP response parser limits headers to 8192 bytes and bodies to 8 MiB. Beast may
allocate temporary storage while parsing. The owner discards the response after the handler retires.
The terminal diagnostic retains the native error and a complete refusal status when available.
The WebSocket payload limit defaults to 64 KiB in each direction and accepts positive values through
256 KiB. WebSocket compression is disabled. An unsolicited extension in the upgrade response
is refused before readiness. These limits cover the respective inputs and parsers.
Native resolver, provider, parser and executor allocations also contribute to process memory.

## Established connection

The owner admits one operation at a time after the upgrade. Start the subscription write before
arming a receive. While an operation or unread text result occupies the slot, further receive,
send and close calls return busy. There is no message queue. Drive the owner until the operation
completes, then take the one result and decide when to arm another receive. A received value owns
immutable text bytes and survives teardown. The transport has no provider JSON or publication
logic. Provider text such as a status message stays opaque until classified by the caller.

Beast validates frame structure and UTF-8 and reassembles fragments. A read uses a flat buffer
with capacity at most the configured payload limit plus one byte. The extra writable byte lets
the parser process a final empty continuation when the message already fills its limit. The
separate Beast message limit still controls admitted payload bytes. On completion the owner
copies the admitted text into the result and releases the read buffer. Both can briefly exist
during the copy. Empty text completes without a result. An unsupported binary message or a Beast
protocol violation terminates the attempt without delivering bytes.
If transport teardown fails while Beast is closing a rejected stream, the native teardown error
can take precedence over the original parser error. Both outcomes are terminal.

During an active receive Beast answers ping with pong and handles peer close. Control frames
never occupy the application slot. The owner does not run background reads while the application
holds a result or drives a separate operation. A send accepts one nonempty, valid UTF-8 value up
to the same byte limit. It copies the supplied bytes before returning, retains them until the write
completion and uses a 4 KiB Beast buffer for client masking and automatic fragmentation. A
completed write does not acknowledge peer delivery.

Receive, send and local close each have an explicit positive deadline. Their defaults are
30 seconds, 5 seconds and 5 seconds respectively. A local normal close is admitted from the
idle ready state. A completed local or peer close yields `CLOSED` with a validated close code
when supplied. A transport EOF without a completed close yields an end-of-stream failure.
Reset and other transport failures preserve their native error. On stop or deadline, the
lowest socket closes and the same operation retires without retry. Terminal retirement
releases unfinished receive storage, unsent output and any unread result along with the socket,
TLS context and provider.

## Deadlines and retirement

The owner clips its executor wait to the active phase deadline. A result delivered at or after
the deadline cannot advance the phase. Stop invalidates pending resolution and closes the lowest
socket. While a native completion remains, the owner exposes `STOPPING` with the original
diagnostic and drives the executor until the completion retires. Destruction also drains handlers
before releasing backing. If the system resolver is already inside a native DNS call, its worker
can return after the configured deadline. The deadline bounds admission of the resolver result.
A caller that needs responsive ownership can retain the stopping attempt and drive retirement on
the dedicated owner thread.

The [connection fixture](../../tests/conformance/beast-wss-dependency/wss_qualification.cpp) drives
independent loopback peers on Windows and Linux. It exercises fragmented upgrade, exact TLS and
HTTP refusals, stop in each active phase and an upgrade deadline. Resolve and connect tests cut
off before the owner admits the completion. The TLS and upgrade tests hold the peer after
observing client bytes. The [message and control fixture](../../tests/conformance/beast-wss-dependency/wss_io_qualification.cpp)
covers the exact message limit, fragmented UTF-8, interleaved ping and pong, oversize, malformed
and unsupported input, both close directions, EOF and reset, blocked write, deadlines and
cancellation. Provider subscription recovery and live interoperability are subsequent integration steps.

See the [composed qualification and reproduction commands](BEAST-WSS-VALIDATION.md) for the
tested install boundary, repeated attempts and existing transport regressions.
