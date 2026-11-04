# Private bounded byte operation

[Русская версия](BYTE-OWNER.ru.md). English is authoritative.

The private `RawByteOwner` and `TlsByteOwner` provide caller-driven application byte windows over
the existing [TCP native owner](TCP-RUNTIME.md). They have no installed header or public loader API.
They do not parse HTTP or IPF, select credentials, follow redirects, create threads or invoke user
code. Native EOF, verified TLS close, HTTP message completion and IPF completion remain distinct.

## Ownership and serialized progress

One stable `RuntimeCore` owns the socket, DNS channel, native context, arenas and completion state.
Its private session slot selects either the existing QTP `LiveSession` or a raw-state borrow. Boolean
slot inspection recognizes only QTP. The raw alternative constructs no schema, composer or QTP
session. Compile-time size/alignment checks preserve the existing session slot storage on the
qualified platforms; the new separately allocated byte windows have their own full reservation.

`RawByteOwner` retains an outer `BootstrapLease` before its byte state and native child. The native
child acquires its existing independent lease. Both are counted borrows of the same application
bootstrap, including configured idle gaps; no additional global c-ares/WSA initialization occurs.
The application retains its bootstrap until all operations retire, on its original cleanup thread.

All mutable calls, inspections and destruction are serialized on the creating owner. Checked calls
reject a foreign thread before touching native/SDK state. Normal calls reject reentry; close remains
request-only. Destroying an owner during its own active method is a precondition violation. A future
callback facade must retain a stable operation lease across user code, invoke callbacks outside
owner locks/native handlers/SDK entries, and recheck liveness, attempt identity and cutoff afterward.
The current mechanism installs no application callback or scheduler.

## Input and immutable output

Raw input has a separately owned finite window. A poll copies only an eligible prefix from native
input or verified engine plaintext, charging one copy operation and the exact copied byte count.
No new block replaces a retained block until `consumeInput` explicitly releases the full suffix.
Partial consumption keeps the remaining bytes stable. An excessive consume count fails without
mutation. Backpressure yields while preserving the native/engine suffix. Published input remains
readable after native EOF, failure or abort until explicitly consumed or the byte owner retires.
Unpublished native suffixes at abort are reported as discarded input, rather than silently delivered
after the cutoff. A preceding verified engine prefix is copied before processing a later native EOF.

Output admission copies the whole block into one finite owned slot before publication. A busy or
undersized slot rejects the whole block. Copy work is explicit; caller memory may be changed or
released immediately after return. The published address, length and bytes remain immutable through
native short writes and exact TLS WANT retries. Native ciphertext acceptance and engine application
acceptance advance independently. Output is reusable only after all application bytes are accepted.
Local acceptance never proves peer receipt.

## Cancellation, completion and terminal evidence

Native handlers retain stable core state, record only completion evidence and never rearm I/O or
invoke user code. `requestClose` sets only the enclosing operation's admission cutoff; it does not
close a socket, enter an SDK, invalidate a buffer or drain a callback inline, including during reentry.
A subsequent safe poll applies the existing core close path. A raced successful write still credits
its accepted prefix against the original backing before that backing can retire. `CLOSED` requires
both pending native handlers and callback depth to be zero. Engine/WANT and application output
owners outlive that drain. Destructor fallback performs the same owned drain on its creating thread;
native scheduling and fallback teardown carry no hard latency guarantee.

Published terminal evidence is immutable. Subsequent poll/abort requests return the existing CLOSED
observation without changing abort flags, diagnostics or phase deadlines. Terminal native evidence
retains EOF, abort, primary/secondary errors, reported write acceptance,
ambiguous native write acceptance, discarded native input and pending output. Raw EOF is a byte
boundary, including an empty boundary, rather than a protocol-success claim. Application input
already published before the terminal observation remains a separate explicitly consumed prefix.

## TLS profile and reservations

`TlsByteOwner` owns bounded trust/reference settings and handshake/close deadlines through engine
retirement. It reuses the existing `TlsEngine`, `TlsPolicy`, finite BIO windows and exact WANT tuple;
only chain- and identity-verified plaintext can enter the raw input window. It has no Basic provider
or QTP authorization workflow. Even a null credential pointer with a shared ownership control block
is rejected before copying settings; no external provider graph or deleter is retained. Pending
application output/WANT arguments forbid orderly close. Repeated orderly-close requests are refused
once closing begins, preserving the original absolute deadline and cached WANT state. Phase clocks
stop at terminal publication, including clean peer and local orderly close.
Abort remains request-only; orderly close entries consume the existing close vector.

The existing [TLS resource profile](TLS-RESOURCES.md) applies unchanged: recompute `tlsRequirements`
from the same-image host maxima, admit F+R before SDK allocation, retain copied R through final graph
retirement, fund H once and E/close for admitted SDK entries, and expire each entry on every exit.
No caller credit pointer is retained or refunded. The wrapper neither resets nor discovers shared
query/property/ERR/thread/failure history. All users of that SDK image participate in the same host
declarations, including failed preparation prefixes. Shared storage is funded once at that host
scope; a per-attempt reported shared allowance does not establish another isolated accounting domain.
External host truth remains an embedding precondition, not an enumeration claim.

Native backing includes the actual core/bootstrap sizes, input and complete context/handler arenas,
plus the existing DNS envelope for named endpoints. Raw backing additionally includes its actual
owner size and both full application windows, retained after close. Native preparation retains the
existing arena visits and three whole-backing byte passes; raw preparation adds three byte passes
over its own complete backing. TLS counts its wrapper, engine/policy and owned settings separately,
including conservative inline-string slack checked against the actual qualified copies before SDK
allocation, and both existing SDK allowances. Native and whole-TLS resident ceilings are independent.
Exact and one-below admission/work cases are checked without weakening existing QTP limits.

See [raw byte tests](../../tests/conformance/transport/raw_byte_tests.cpp),
[TLS byte tests](../../tests/conformance/transport/tls_byte_tests.cpp) and the existing
[TCP conformance tests](../../tests/conformance/transport/tcp_tests.cpp). These contracts do not
establish HTTP support, a public transfer API or complete source-transfer readiness.

The private factories also accept an already retained stable bootstrap lease. Independent child
borrows use the same provenance/capacity counter at the original factory admission checkpoint.
Moving the application's bootstrap facade does not change that identity; the moved-to owner still
outlives every idle lease, child and final drain. No facade address is retained by this path.
Native-error plain prefixes use a dedicated cache-only RawByteOwner drain, never ordinary poll.
Completed Windows bytes/core input enter the existing raw input window under original native-step
and copy-operation/byte credits. Zero credits yield before mutation; no context poll, new socket
operation, codec step or output acceptance/submission occurs. Pending handlers/output remain owned
through later retirement, and the original first cause is unchanged.