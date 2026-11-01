# TLS runtime contract

[Русская версия](TLS-RUNTIME.ru.md). English is authoritative.

[`TlsRuntime`](../../include/qd/transport/tls_runtime.hpp) implements verified TLS/authentication
handoff over the common caller-driven TCP owner. Focused native tests exercise the backend, credential
gate and lifetime boundaries. See [qualification and remaining gates](../../tests/conformance/transport/TLS-VALIDATION.md).
Existing `TcpRuntime`, `runStream`, CLI and `EndpointOptions` behavior remains unchanged. Reconnect,
a full client facade and application-wide graceful stop are separate capabilities. Native observations,
source-qualified bounds and final integration acceptance are reported separately.

## Profile and application inputs

The first adapter must use the controlled same-image host and qualified SDK/native assumptions in
[TLS resources](TLS-RESOURCES.md). The embedding application chooses that profile and supplies true
finite declarations for all relevant users/history. A private context alone cannot establish them.
TLS-disabled independence from OpenSSL is checked by the separate build/package regression profiles.

The selected first protocol profile is TLS 1.2/1.3 with explicit trust and fixed permitted groups,
ciphers and signature algorithms. Resumption, early data, client certificates, renegotiation and
post-handshake authentication are excluded. Fixed provider/query/RNG and complete-key policy checks
must precede activation. No mutable system configuration, provider/RNG hook, system roots or foreign
issuer resolver may silently widen the profile.

Keep the configured DNS/IP reference identity distinct from the connection route and resolved
addresses. Verify DNS SAN or numeric IP SAN, with no CN fallback. DNS SNI follows the configured DNS
identity; numeric IP does not acquire DNS SNI. A successful TCP connection or completed encrypted
exchange alone does not open the authentication gate.

## Trust before key decode and verification

Scan the complete admitted immutable trust bytes before any PEM/X509/key decode. Accept only
the selected ordinary `CERTIFICATE` block framing and count every block. Reject an excess block at
this pure preparse boundary; do not call a PEM reader that first invokes `d2i` and only then discovers
capacity overflow. Keep the byte/framing/count snapshot immutable through decode. The final `A+1`
reader opportunity is EOF/framing validation only, never another certificate/public-key decode.

Guard every anchor before insertion, then the leaf and every untrusted chain entry before exactly
one default verification on the same prepared context. The guards must bind allowed types, complete
keys, raw SPKI/PSS encodings, normal flags and depth. The selected proof uses depth 4, chain bound 6
and normal TRUSTED_FIRST behavior; no user callback may override verification failure. System roots,
DANE, revocation/network issuer retrieval and foreign issuers are outside the first profile.

Certificate, chain and configured SAN identity verification must succeed before any credential
provider invocation. A fresh token is requested only when that verified original attempt needs the
selected protocol authentication. Prepay bounded token validation/copy/output work and secure an
output slot before calling user code. Do not cache credentials across attempts or retry a callback
to repair already spent work. Translate failure into bounded static diagnostics without token,
trust text, authenticated address or arbitrary provider exception echo; use synthetic test secrets.

## Owner, callback and cutoff

The public owner is move-only; stable internal ownership and an in-flight lease protect operations.
All methods, inspection, moves, destruction and protocol-owner release are serialized on the creating
thread. Checked operations report `WRONG_THREAD`; inspection/move/destruction retain that precondition. A callback also holds its executing provider alive. No user callback or
deleter runs under owner locks.

Move construction transfers ownership of the same attempt; destination replacement cuts off the
destination's old attempt. Destruction and replacement immediately clear the old public-owner-live
gate even if an operation still holds its internal lease. After user code, use only the original
local internal/provider leases and recheck live public ownership, unchanged attempt, verified phase,
cutoff and deadline. Never access members of the possibly moved/replaced/destroyed public object.
A returned credential can authorize only that same still-live original attempt, never another one.

The reserved `requestClose` control, destructor and destination-owner cutoff must remain legal
inside an active credential/provider callback. They consume owned `R`, are allocation-free,
idempotent and request-only, and neither begin a new work turn nor require the caller's current
budget. The cutoff step performs no inline SSL/BIO/provider destruction, SSL shutdown or native join. Actual
cleanup waits for the required in-flight/native owners and consumes its copied reserve once.

The active operation lease prevents disposal of that attempt inside its provider callback. Outside
the callback, destruction of the last owner or replacement of an idle destination may synchronously
run the fallback native drain and SDK disposal on the owner thread. That fallback can require host
progress and has no hard time bound. Use explicit `requestClose` and `poll` to a terminal result when
observable quiescence is required before releasing the public handle.

Normal SDK-entering methods remain nonreentrant. `requestTlsClose` is itself request-only and pays
SDK close work in later `poll` calls, but rejects reentry from the credential provider. It is not the
reserved `requestClose` busy exemption. Do not reset the active turn during a nested request. A public
cutoff is neither callback quiescence nor native completion. No caller work pointer survives a call.

## Backpressure and pending writes

Use the selected bounded memory-BIO engine over the separately qualified raw TCP path, without
hidden SDK socket reads or retry loops. Wire input/output and both BIO directions have their own
17 KiB windows; SSL objects, native record/handshake backing, certificates, provider/cache/RNG state
and plaintext/session owners are separately accounted. At most one native read and one native write
may be outstanding, with each backing retained until its own completion. Preserve encrypted and
plaintext suffixes under BIO/session backpressure.

For a WANT retry, retain the exact pending write pointer, length, contents and backing owner until
that operation succeeds or aborts. Draining ciphertext from a BIO is not plaintext acceptance.
Advance application output only by the successful SSL-reported count; retain pending ciphertext.
This selected invariant is stricter than optional moving-buffer behavior. See
[official write/retry requirements](https://docs.openssl.org/3.5/man3/SSL_write/).

Perform prerequisite queries first, clear ERR immediately before the I/O operation, then classify
its exact return on that same thread before any other OpenSSL call. Diagnostics and BIO inspection
come afterwards. Do not preserve foreign stale errors as part of this API. See
[official error-classification rules](https://docs.openssl.org/3.5/man3/SSL_get_error/).

## TLS close and limits of progress

For `SSL_shutdown`, 0 means pending without `SSL_get_error`, 1 means TLS shutdown completed, and a
negative result requires immediate classification. Each later orderly attempt requires new close-entry
work. This is TLS record closure, not peer application acknowledgment or full subscription drain.
See the [official return-value contract](https://docs.openssl.org/3.5/man3/SSL_shutdown/).

The implementation must reject an undrained application-output slot rather than discard it and
report graceful success. A deadline/cutoff follows explicit abort and native-completion retirement;
event-loop stop alone is not cleanup. Credits bound qualified work per admitted entry. They cannot
preempt crypto/native calls, bound user callback duration or promise hard wall-clock completion.

## Qualification boundary

The focused native suite exercises TLS 1.2/1.3, ordinary trust/identity/key-policy/malformed cases,
credentials, diagnostics, work turns and ownership across callbacks/moves/destruction.
Refreshed Windows and Linux packages each contain 57 files. After physical relocation, an independent
consumer with two `find_package` calls compiled and executed TLS 1.2 and TLS 1.3 on each OS.
It uses only installed public headers and an explicit qualified SDK, keeps bootstrap alive beyond
runtime retirement, supplies finite host declarations, constructs the actual eight-field Quote/writer,
receives a heartbeat and performs bounded retirement of its operations. This exercises the public API;
it does not establish a full subscription facade or peer delivery.

Not every protocol boundary was executed: the [qualification matrix](../../tests/conformance/transport/TLS-VALIDATION.md)
explicitly distinguishes source-only CV/NST/CertificateRequest, exact P-384 maximum, ECDSA peer,
full PSS cross-product and SSL-write WANT reasoning. Complex ASN.1/SPKI corpora, fault injection and
fuzzing are outside ordinary scope. All four final regression profiles passed (58/58 with JSON and 57/57 without JSON on each OS).
Generated-reference verification is recorded separately. No external endpoint, reconnect, broader facade or released-version claim follows.

## Construction, progress and observable values

Keep `RuntimeBootstrap` alive through all runtime/in-flight retirement. `create` borrows a usable
immutable `client::EndpointOptions` TLS snapshot, a matching `SubscriptionWriter` and explicit
`TlsOptions`; retained endpoint/options/DNS data is owned independently after preparation. Composer
ownership transfers only on success. No DNS/network activation or credential call occurs in factory
preparation; chain/identity verification belongs to the later active attempt. `start` activates the
single connection sequence and does not itself certify TLS or invoke credentials.

`TlsOptions` contains `limits`, `host`, `dns`, `sdkInstanceBytes`, `sdkSharedBytes`, `residentBytes`.
Supply explicit valid ceilings and the complete host declaration. Compute `tlsRequirements` first;
SDK allowances must admit its private/shared requirements, and the whole-run allowance also covers
adapter, retained snapshot, transport and session scopes. Zero-initialized options are not working
memory defaults. For named routes `TlsDnsOptions::servers` borrows 1..4 numeric IPv4 servers and
`port` is explicit; numeric routes bypass DNS. The factory copies retained DNS values.

Each `poll` uses independent mutable `TcpWork`, `TlsWork`, `protocol::LiveWork` values. It reports
`TcpPollStatus` and at most one owning `LiveEvent`. Credits are not implicitly reset/refunded and
no work pointer escapes. `composeProtocol`, `composeHeartbeat`, `describe`, `subscribe` compose local
output under their existing session readiness/authentication rules; none is peer acknowledgement.
`peer` returns an owning snapshot with serialized release, `authentication` observes pure auth state,
`requirements` copies the run qualification, `dnsProgressWorkRequirement` describes common DNS work,
and `nextWake` is a monotonic deadline/cadence hint rather than a registered notifier.

| `TlsState` | Meaning |
| --- | --- |
| `CONFIGURED` | Prepared, no network activation |
| `RESOLVING` / `CONNECTING` | Common bounded route resolution/address attempts |
| `HANDSHAKING` | TCP connected; chain and reference identity not yet both verified |
| `AUTHENTICATING` | Verified TLS with selected protocol authentication pending |
| `CONNECTED` | Verified TLS; selected protocol authentication does not currently block |
| `CLOSING` | Admission cut off; owned progress/completions remain |
| `CLOSED` | Native operations/internal callbacks retired |

A moved-from `state()` sentinel is `CLOSED` without terminal evidence. Observe `terminal()` for real
completion. `TlsTerminal::primary`/`secondary` preserve ordered static/numeric causes; `wire` carries
encrypted native acceptance/retirement evidence. `acceptedPlaintextBytes` counts SSL-accepted bytes,
`pendingPlaintextBytes` retains the unaccepted suffix/WANT tuple, and `pendingCiphertextBytes` counts
known queued wire output. `tlsDataPending` separately warns of possible additional private SDK data.
`closeNotifyReceived`, `orderly` and `aborted` distinguish peer TLS alert, fully drained TLS/native
closure and explicit cutoff. None proves peer application receipt. CLOSED does not imply all externally
retained storage has vanished.

`TlsResources` reports `transport`, `sdk`, `adapterBytes`, `snapshotBytes`, `totalBytes`, `peakBytes`.
These are conservative admitted requested-capacity reservations, including retained owners after close,
not RSS or exact current allocator usage. Accounting qualification boundaries are stated above.

`TlsDiagnostic` contains `error`, static `stage`, numeric `sdkCode`/`verifyCode` and detached `transport`
cause. Categories distinguish configuration/state/thread, resource/work/allocation, transport/policy,
trust/identity/handshake, credential/authentication, protocol/deadline/close failures. No peer/provider
formatted error string or credential text enters these fields.

## Declaration excerpts and example boundary

The following excerpts are exact declarations inside `TlsRuntime` in namespace
`org::ttldtor::qdcxx::transport`. They are not function definitions or a standalone program. The public
header declarations are not a standalone example. A complete installed consumer with genuine
bootstrap/snapshot/composer/host qualification has separate native TLS evidence as described above.

```cpp
[[nodiscard]] static std::expected<TlsRuntime, TlsDiagnostic> create(RuntimeBootstrap &bootstrap,
                                                                         const client::EndpointOptions &endpoint,
                                                                         protocol::SubscriptionWriter &&composer,
                                                                         TlsOptions options, TlsWork &tlsWork,
                                                                         protocol::LiveWork &sessionWork);
```

```cpp
[[nodiscard]] std::expected<TlsPollResult, TlsDiagnostic> poll(TcpWork &runtimeWork, TlsWork &tlsWork,
                                                                   protocol::LiveWork &sessionWork);
```
