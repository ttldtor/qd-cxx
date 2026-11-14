# Caller-driven TCP listener

[Русская версия](TCP-LISTENER.ru.md). English is authoritative.

`qd/transport/tcp_listener.hpp` supplies the plain accepted-byte transport on
Windows x64 and Linux x86-64. It belongs to the optional runtime target and
shares the [explicit process bootstrap](TCP-RUNTIME.md) with outgoing TCP.
Create, drive, move and destroy a listener on one serialized owner thread, while
the process bootstrap remains alive. Checked operations reject a foreign
thread. The listening owner retains its bootstrap borrow through destruction.

## Binding and admission

Create the listener with one numeric IPv4 or IPv6 address and a port. Port zero
requests an ephemeral port available from `port()`. An IPv6 listener serves
IPv6 only. The default address is IPv4 loopback. The backlog is a positive
request to the operating system. The kernel may cap it and complete a TCP
handshake before application admission. `maxConnections` bounds accepted
descriptors in the listener itself, with a fixed table allocated at creation,
from 1 through `TcpListenerOptions::MAX_CONNECTIONS`. At capacity, `accept()`
returns `RESOURCE_LIMIT` before dequeuing any native connection.

Each accepted socket has a distinct `TcpConnectionId`. It identifies one live
slot generation within one listener and cannot address another listener or a
replacement peer. Moving the listener on its owner thread preserves its active
identities. When the kernel has no connection ready, `accept()` returns an
empty optional. Interrupted syscalls yield to the caller and other native
accept failures report a detached diagnostic.

## Per-connection byte I/O and retirement

`read()` and `write()` each attempt at most one native nonblocking operation
on the chosen connection. The requested prefix is capped by the configured read
or write limit. A successful operation reports exactly its positive byte count.
No progress is reported as `WOULD_BLOCK` with zero bytes. Empty spans are
rejected. The caller owns unread input processing and any unsent output suffix.
A positive write is local socket acceptance. Neither a positive write nor
shutdown establishes peer receipt.

The accepted socket has no user-space byte queue, pending callback, registered
readiness wait or worker thread. The caller decides drive cadence, fairness,
deadlines, and retirement on I/O failures. A caller running QTP retains one
independent [frame reader](../protocol/FRAMING.md) and application
protocol state per accepted identity, and applies its own finite queue policy.
A slow or incomplete stream consumes only its own slot and caller-selected
state. Other accepted sockets remain independently driveable.

Read EOF is sticky and ends the peer's send direction. Writes on that identity
can still proceed until local retirement or their own error. To retire one
connection, call `retire()`. To stop the listener, call `shutdown()`, which
fences new acceptance and ordinary I/O before closing the listener and every
active connection. It reports its first close failure after attempting all
owned descriptors. Repeating shutdown is safe and can retry descriptors still
owned after a native failure. Destruction attempts to close remaining owned
descriptors. It terminates if a Windows close fails and the descriptor remains
live. Use explicit shutdown to receive the diagnostic and retry while the
listener still owns it. Native kernel socket allocation and backlog storage
are outside the fixed user-space table budget.

The [loopback conformance test](../../tests/conformance/transport/tcp_listener_tests.cpp)
drives two separate QTP framers, retires active connections and exercises
backlog admission, stale identities and binding failures.
