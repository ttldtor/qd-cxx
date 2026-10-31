# ADR-0006 — Optional live runtime and verified TLS profile

[Русская версия](ADR-0006-live-runtime.ru.md). English is authoritative.

Status: selected design for upcoming adapters; qualification scope is recorded below.
This decision does not implement a live transport, establish provider compatibility or record owner acceptance.

## Context and scope

[ADR-0005](ADR-0005-live-subscriptions.md) keeps subscription/session logic independent of I/O and gives
an external execution host responsibility for progress through quiescence. The next adapters need one
concrete Windows/Linux dependency, cancellation and trust policy. A cancelled public operation is not
necessarily an exited resolver worker; returning from an event loop is not close completion.

The selected implementation direction is C++23, standalone Asio for executor/TCP/timers, c-ares for
asynchronous DNS, and optional OpenSSL for TLS. None belongs in core, encoding, event mapping or the
pure protocol/session implementation. Existing tape profiles and APIs remain unchanged.

## Dependency pins and build profile

The release bytes, license and build settings are part of the profile; a newer or differently configured
SDK is not qualified merely because its API links. Acquisition is explicit and project-local; configure
must not silently fetch network dependencies. No package manager or global installation is required.

| Dependency | Pin | License | Selected role/build |
| --- | --- | --- | --- |
| Standalone Asio | 1.38.2, commit `8806a6803cde7054c3049d3666d3ec36786568c5` | Boost Software License 1.0 | Header-only standalone executor/TCP/timers; Windows IOCP, Linux epoll; no built-in resolver. |
| c-ares | 1.34.9 upstream commit `bc91829cce983a35ec01f5dbc05465858a3df469`, release archive SHA-256 `9364b70a12eb59a06830fb488d9357f90100dff0d98b23ea3593a183d43e6f2f`; qualified `1.34.9+qdcxx1` patch required | MIT | Static, `CARES_THREADS=OFF`, caller-driven channel, no event thread/system monitor, bounded socket callbacks; distinct qualified SDK rather than an interchangeable stock build. |
| OpenSSL | 3.5.9, commit `45e844fa2a14ec92d146bd8f5778ac130b6625fb`, release archive SHA-256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a` | Apache 2.0 | Optional static SSL/Crypto, `no-shared no-tests no-docs no-autoload-config`; built-in default provider. |

OpenSSL's [release list](https://www.openssl-library.org/source/) identifies the 3.5 LTS support horizon
as 8 April 2030. A pin is a reproducibility point, not a promise to retain a vulnerable patch: dependency
updates require security review, changed acquisition hashes and affected qualification.
Preserve upstream license/notice files when distributing SDKs or linked artifacts.
The [DNS lock and patch](../../tests/conformance/transport/runtime-feasibility/dns/dependencies.lock.json)
and [Asio/OpenSSL lock](../../tests/conformance/transport/runtime-feasibility/tls/dependencies.lock.json)
record acquisition URLs and exact archive/patch hashes. The DNS patch modifies two upstream files;
its application reproduces the qualified source tree, without changing the original archive.

## Executor and TCP ownership

Use one caller-driven `asio::io_context` with one active owner thread per execution host. The application
may run that owner on a thread it owns. Windows IOCP timers also use an SDK timer-wait helper: it only
wakes IOCP, not application callbacks. Its thread/stack/lifetime count toward runtime resources. Context
shutdown wakes and joins it after owner operations drain, outside callbacks. This is a single callback
serialization domain, not a promise that the process has only one thread. Use a monotonic runtime clock
for deadlines; pure components receive explicit
time inputs. No public API requires an Asio/native socket/TLS type.

All session, socket, timer and TLS operations are serialized on the owner. Other threads submit bounded
commands through the host's control port. Read/write owners, operation state, context and epoch leases
remain alive until their completion handlers have drained. One read and one write may be outstanding;
their state is independent and each buffer survives its own completion.

Stop from a callback requests retirement and returns. On the owner, reject new work, cancel deadlines,
cancel/close owned sockets and retire the DNS channel. Keep pumping through every outstanding completion
before reporting close completion or releasing an epoch. A raced successful completion still belongs
to its original epoch and must not publish into a replacement. Socket close cannot retract bytes
already accepted by the kernel and does not mean that the peer acknowledged or applied a message.

[`io_context::stop`](https://think-async.com/Asio/asio-1.38.2/doc/asio/reference/io_context/stop.html)
stops dispatch; it is not a cancellation/drain implementation. Neither graceful TLS shutdown nor a
blocking host/thread join may run inside a user callback. Graceful close gets a deadline, then follows
the defined abort path without falsely reporting a flushed result. A blocking user callback remains
outside a preemptive shutdown guarantee; see ADR-0005's host progress precondition.

## DNS and process bootstrap

Do not use `asio::ip::tcp::resolver` for this profile. In the pinned implementation, cancellation resets
a token, while `socket_ops::background_getaddrinfo` tests it before entering blocking `getaddrinfo`;
resolver pool shutdown joins its worker. This is source evidence, not a measured OS lookup timeout.

Use a caller-driven c-ares channel without `ARES_OPT_EVENT_THREAD`. Event-thread retirement can join
system-configuration work; that would reintroduce an OS-work wait into close. Prepare the channel at the
explicit configuration boundary before activation and keep it under the external host's resolver
lifetime for one upstream run/independent retirement domain across that run's connection epochs.
`ares_cancel` cancels a whole channel: never share a cancelling channel between independent runs.
New requests carry fresh epoch/operation identities. No background
configuration monitor or periodic `ares_reinit` runs during active operation.

While a query is pending, an owner timer drives bounded fd snapshots, zero-timeout readiness polling and
c-ares progress. There is no second descriptor owner or DNS polling thread. C callbacks capture bounded
nonthrowing results on that same owner; they never invoke application code or destroy their channel.
Account for sockets, query/answer/cache state and old-plus-retiring ownership, including SDK state.
Disable query caching and address sorting. Use per-channel socket-function callbacks to enforce shared
per-turn byte, datagram and native syscall quotas; exhaustion returns WOULD_BLOCK without another native
read. Preserve incomplete TCP frames and full UDP datagrams. Charge SDK input-buffer spare capacity,
power-of-two growth and simultaneous old/new reallocation, not just received bytes. Adapter qualification
must enforce finite fd/answer/storage and per-turn work limits; counting an opaque SDK call as bounded
work is not sufficient. Use at most one outstanding wire query per channel; resolve IPv4 and IPv6
sequentially rather than selecting `AF_UNSPEC`, and never submit a new query from a C callback.
The [qualified DNS profile](../../tests/conformance/transport/runtime-feasibility/dns/profile.json)
records the tested ceilings and conservative input-buffer/reallocation envelope. Total SDK heap is not
yet implemented or measured as a production resource budget.

**Application bootstrap is a supported-profile precondition.** The documented
[`ares_library_init`](https://c-ares.org/docs/ares_library_init.html) contract requires initialization
on the main thread before other threads are active. Its matching
[`cleanup`](https://c-ares.org/docs/ares_library_cleanup.html) is after other threads have finished.
Use an explicit application-owned bootstrap lifetime, default allocators and coordinated reference
counts with every other c-ares user in the process. A client/epoch destructor must not perform global
cleanup. Late first use in an already multithreaded application without that bootstrap is unsupported;
serializing only this library's constructors does not satisfy the upstream precondition.

Stock c-ares reads system/environment/NSS configuration even when explicit values override it; some
temporary file reads are not bounded by the application's limits. Its hostname-derived search fallback
is another ambient input. Pre-activation is not an excuse to put these allocations outside the budget.
The qualified SDK therefore disables these paths and requires a bounded explicit numeric IPv4 DNS-server
list before initialization. IPv4 nameservers can resolve both IPv4 and IPv6 peer addresses. Empty server
configuration fails rather than falling back to localhost. Preserve the upstream pin and the exact
qualified patch separately; do not label the modified SDK as an unmodified upstream release.

Use DNS-only lookup (`lookups="b"`), `NOSEARCH` and `NOALIASES`; literal numeric peer endpoints bypass DNS.
Automatic OS DNS discovery, hosts/NSS/search-domain behavior, resolver environment overlays and automatic
server refresh are not selected. Applications supply validated finite settings; an automatic system
provider would need its own bounded implementation and qualification. Do not mutate process environment
to force isolation. Reconfiguration requires quiescence and an explicit new configuration boundary;
repeated requests still resolve current records against the selected servers.
Cancellation and retirement must handle both UDP and TCP DNS paths. Cancellation callbacks can execute
inline on the cancelling owner; defer application delivery until the C call has returned.

Native OS cryptographic randomness must be available and initialized: the qualified Linux build uses
`arc4random_buf`, Windows uses `RtlGenRandom`. Unsupported entropy/file/RC4 fallback paths are not
qualified. The adapter's bootstrap must check native readiness and reject an unavailable service;
small timing measurements do not establish a hard wall-clock bound for OS primitives.

## TLS identity, trust and authentication

TLS is explicitly enabled per endpoint. Negotiate TLS 1.2 or TLS 1.3 with mandatory peer, chain, validity
time and identity verification. Require a caller-selected PEM CA bundle, loaded into an instance-local
context before connection. Missing/unreadable/invalid trust material fails before network activation.
Do not load implicit default CA paths, `SSL_CERT_FILE`/`SSL_CERT_DIR`, or automatically import Windows
stores. The application owns trust-bundle selection, distribution and updates.

Use a private `OSSL_LIB_CTX`, its explicitly loaded built-in default provider, and `SSL_CTX_new_ex`;
configure the context before creating streams. Bound trust-bundle bytes/anchors and certificate-message
bytes before SDK parsing; verification depth alone does not bound certificate decoding. Use finite
approved key/group/signature input limits and a source-derived conservative SDK allowance. Handshake
deadlines are cooperative between SDK calls, not preemption of cryptographic work. Logical limits must
cover SDK buffers, objects and old-plus-candidate peaks; measurements alone do not prove an upper bound.
Library context/provider/context owners outlive every
stream and completion; Asio takes ownership of its supplied native `SSL_CTX`. Do not alter the process
default provider, trust store, property query or initialization flags, and do not call global
`OPENSSL_cleanup` from a client. The pinned SDK's `OPENSSL_NO_AUTOLOAD_CONFIG` build property disables
automatic configuration loading; this is not a claim that OpenSSL has no normal process initialization.
An ordinary system SDK lacking that build property is outside this profile.
The application must not manually preload a process-global SSL `system_default` configuration:
`SSL_CTX_new_ex` can consult that registry even with a private library context. This shared-process
precondition is explicit; context ownership alone is not complete configuration isolation.
Application-registered custom ENGINEs, alternate key methods and external providers affecting this SDK
are also outside the profile; built-in decoder paths remain part of its resource allowance.

The production TLS policy uses ECDHE groups `X25519`, `P-256`, `P-384`; TLS 1.2 ECDHE-RSA/ECDHE-ECDSA
AEAD suites (AES-128/256-GCM or ChaCha20-Poly1305), and TLS 1.3 AES-128/256-GCM or ChaCha20-Poly1305.
Accept RSA keys of 2048–4096 bits with public exponent 65537, or EC keys on named P-256/P-384 curves;
reject explicit EC parameters and other key families. Select SHA-256/384/512 RSA-PSS, TLS 1.2 RSA-PKCS1
SHA-256/384/512, and matching ECDSA P-256/SHA-256 or P-384/SHA-384 signatures. No DHE or RSA key exchange.
Configure `SSL_CTX_set1_groups_list`, `SSL_CTX_set_cipher_list`, `SSL_CTX_set_ciphersuites` and
`SSL_CTX_set1_sigalgs_list`, checking their results. These are production requirements for the upcoming
adapter; ordinary RSA-2048 feasibility handshakes do not qualify every allowed or rejected key case.

Use a maximum 64 KiB Certificate message and verification depth 4; cap the supplied PEM bundle at
512 KiB and 256 anchors before SDK parsing. Asio's tested input/output record buffers are 17 KiB each;
additional BIO/native SSL storage is accounted separately. `SSL_CTX_set_max_cert_list` limits parsing
input; `SSL_CTX_set_verify_depth` limits chain verification, not preliminary certificate decoding.
OpenSSL can decode public keys while parsing X.509 before a certificate-verification callback. Account
for that initial built-in decoder work/storage under the pinned SDK envelope. The
`SSL_CTX_set_cert_verify_callback` policy inspects SPKI/key/parameter limits before delegating to
`X509_verify_cert`; it does not prevent every earlier key decode. Adapter acceptance requires hostile
DER, explicit-parameter and key-size cases and a source-derived conservative peak/work allowance.
Deadlines do not preempt an executing decoder. The feasibility fixture implements selected size/depth
mechanisms and ordinary keys, not that complete production admission policy.

For a DNS endpoint, verify its nonempty ASCII/A-label reference identity and send SNI. Require SAN
matching, disable partial-label wildcards, and do not fall back to Common Name. For an IP endpoint,
verify the IP SAN explicitly and omit DNS SNI. DNS answers/CNAMEs never replace the configured reference
identity. Check every identity/trust setup return value; an empty identity cannot silently disable
verification. OpenSSL's [context](https://docs.openssl.org/3.5/man3/SSL_CTX_new/) and
[identity APIs](https://docs.openssl.org/3.5/man3/SSL_set1_host/) define these setup obligations.

No verification bypass, opportunistic plaintext downgrade, automatic AIA fetching, online revocation,
OS trust-policy emulation, record/certificate compression, TLS renegotiation, TLS early data,
FIPS-provider claim, client certificates, TLS proxy or QUIC
is selected. Deployments requiring one of these need an explicit additional profile, not an implicit
success label. TLS graceful shutdown is bounded; timeout/abort is reported distinctly from orderly
`close_notify` completion.

The selected read-side QTP authentication profile is the ordinary negotiated peer characterized in
[subscription compatibility](../../tests/conformance/subscriptions/VALIDATION.md): instance-local
credential handoff after verified TLS, explicit peer readiness/authentication, bounded auth wait and
redacted diagnostics. Select the characterized QTP `authorization` property with a `Basic` token;
other schemes require separate peer qualification. Plain TCP is a separate explicit credential-free mode. No credentials are sent
before TLS verification; reconnect obtains credentials through the configured endpoint callback, does
not log them and does not reuse another epoch's pending auth operation. Session auth state is implemented
separately from this TLS decision.

## Optional build and consumer boundary

Future runtime and TLS facilities are separately OFF by default. Runtime enables only the selected
Asio/c-ares/Threads/native socket closure; TLS additionally enables OpenSSL SSL/Crypto. TLS requires
runtime. Neither option may change JSON, recording, replay or pure-library dependencies. Public headers
retain standard/project types; a static consumer still receives the explicit native link closure.

OFF configure/build/consumer operations must not find, fetch, include or link runtime/TLS SDKs, start
resolver workers, initialize TLS or require their build tools. Test dependencies remain independently
controlled by `BUILD_TESTING`. Enabled builds accept explicit verified SDK/source roots and fail clearly
for missing, wrong-version, wrong-architecture or incompatible SDKs. Download/build is an explicit
developer step, never an unannounced configure side effect.

Preserve CMake 3.25+, C++23 and existing CLion toolchains/presets. Supply dependency roots in local
configuration rather than editing global paths. Source-built SDKs with native tools support MSVC x64
and Linux GCC without requiring installed Conan; native consumer linkage and install/export behavior
must be checked when the adapters/package integration actually exist. This ADR does not pretend those
future production options or package exports already exist.

## Alternatives and consequences

| Alternative | Decision and reason |
| --- | --- |
| Standalone Asio plus native worker-based resolver | Use executor/TCP only; blocking resolver worker quiescence is not bounded by public cancellation. |
| Boost.Asio | Same networking model; additional Boost integration is unnecessary for the selected standalone closure. |
| libuv with `uv_getaddrinfo` | Its [request contract](https://docs.libuv.org/en/v1.x/request.html) cannot cancel an already executing lookup and retains request memory until completion. Does not close the DNS lifetime issue. |
| Direct IOCP/epoll/native DNS backends | Feasible future work, but requires more platform-specific ownership and cancellation implementation than this profile. |
| Schannel on Windows and a different Linux TLS engine | Native stores are useful, but two TLS backends and verification policies are unnecessary for this first profile. |
| OpenSSL automatic default stores/configuration | Reject hidden trust/configuration selection; use explicit PEM and context-local settings. |

Only Windows x64/MSVC and Linux x86-64/GCC are initial qualification targets. Other OS/architectures,
MinGW, io_uring, alternate providers/allocators, late uncoordinated global initialization and arbitrary
system SDKs remain unqualified. See the [transport verification contract](../../tests/conformance/transport/CONTRACTS.md)
for executed evidence versus source facts and required adapter checks. Kernel scheduling and arbitrary
application callback duration are not hard real-time guarantees inferred from small loopback probes.
