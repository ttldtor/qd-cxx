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
The WebSocket message limit defaults to 64 KiB and accepts positive values through 256 KiB.
WebSocket compression is disabled. These limits cover the respective inputs and parsers. Native
resolver, provider, parser and executor allocations also contribute to process memory.

## Deadlines and retirement

The owner clips its executor wait to the active phase deadline. A result delivered at or after
the deadline cannot advance the phase. Stop invalidates pending resolution and closes the lowest
socket. While a native completion remains, the owner exposes `STOPPING` with the original
diagnostic and drives the executor until the completion retires. Destruction also drains handlers
before releasing backing. If the system resolver is already inside a native DNS call, its worker
can return after the configured deadline. The deadline bounds admission of the resolver result.
A caller that needs responsive ownership can retain the stopping attempt and drive retirement on
the dedicated owner thread.

The [local fixture](../../tests/conformance/beast-wss-dependency/wss_qualification.cpp) drives
independent loopback peers on Windows and Linux. It exercises fragmented upgrade, exact TLS and
HTTP refusals, stop in each active phase and an upgrade deadline. Resolve and connect tests cut
off before the owner admits the completion. The TLS and upgrade tests hold the peer after
observing client bytes. Data handling, ping/close, reconnection and live provider interoperability
belong to subsequent integration steps.
