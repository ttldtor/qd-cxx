# TLS resource and runtime qualification

[Русская версия](TLS-VALIDATION.ru.md). English is authoritative.

Date: 2026-11-01 UTC. The [resource contract](../../../spec/transport/TLS-RESOURCES.md) and
[runtime API](../../../spec/transport/TLS-RUNTIME.md) have different evidence boundaries.
Pure resource arithmetic and the TLS runtime are implemented. The focused current-source suite passed
39 native tests: Windows MSVC 19.42.34444 x64 Release `/MD`, 11,486 assertions; Ubuntu 24.04
GCC 13.3 x64 Debug ASan+UBSan, 11,625 assertions. LSan is disabled; the SDK is not instrumented.
Full regression passed on each OS: 58/58 tests with JSON enabled and 57/57 with JSON disabled. Generated-reference verification is recorded separately.
The refreshed installed package passed four TLS 1.2/1.3 cases across both OSes.
This is not security certification, acceptance of a complete subscription client or a released version.

## Executed resource checks

These standalone checks compile the actual resource header/implementation/tests. They perform no
OpenSSL operation, TLS handshake, callback, native peer or runtime activation.

| Profile | Result | Scope |
| --- | --- | --- |
| Windows MSVC 19.42.34444, x64 Release `/MD` | 8/8 cases; 835 assertions; exit 0 | Source-bound resource tests |
| Ubuntu 24.04 GCC 13.3, x64 Debug ASan+UBSan | 8/8 cases; 835 assertions; exit 0 | Source-bound resource tests |
| Independent MSVC native resource oracle | 32 cases passed | Expected quantities derived separately from the paper equations |
| Independent MSVC resource-suite rerun | 8/8 cases; 835 assertions passed | Actual compiled resource executable |
| Runtime public header | MSVC self-contained compile-only check passed | No runtime method linking/execution or TLS backend qualification |

Linux used `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; LSan was excluded. The additional MSVC resource
runs are separate executions. The Linux resource receipt was inspected without a second Linux run.
Resource arithmetic and the runtime declarations were source-checked at that point; full runtime
execution is reported in its own matrix below.

The resource checks cover five input ceilings, twenty copied host maxima and 25 work coordinates;
63 nonzero one-short phase cases; zero/exact/excess, shape mismatch, copied input independence,
aliased `need`, detached retirement values, all-coordinate add overflow and requirement overflow.
One hundred source-derived golden quantities agree with the C++ results. The independent cases
cross small/maximal trust/anchor/identity ceilings and name/query/ERR/failure-history boundaries.
This is value/admission evidence, not proof of actual SSL allocations, callback lifetime or SDK progress.

The synthetic maximum arithmetic fixture gives 601,607,223 private instance bytes, 44,048,771 shared
bytes and 645,655,994 total bytes. Its twenty host numbers are not observed host state or defaults.
The values are conservative source allowances, not allocated arenas, measured RSS or recommended
memory settings. Other valid host ceilings require their own computed requirements. Fixed native
frames, allocator/sanitizer overhead and hard wall time remain outside the stated SDK metric.

## Executed native checks and remaining boundaries

The focused suite executes TLS 1.2/1.3, explicit trust, SAN/SNI verification separately from routing,
ordinary validity/identity/key-policy negatives and tiny malformed DER `30 00`. It covers Certificate
bodies at 65,536/65,537 bytes, depth-4/depth-5 chains, credential gating, reentry, owner moves/replacement/
destruction, 64 fresh attempts, two endpoints, 128 large frames, a 512-byte RSA-4096 signature,
two ordinary TLS 1.3 tickets and raw ServerHello headers at/over the cap. Local byte acceptance does
not establish peer application delivery.

Ordinary certificates and exactly two intentionally public test server keys are in the
[fixture catalog](../../fixtures/transport/tls/README.md); [provenance](../../fixtures/transport/tls/provenance.json)
records generation, properties and expected outcomes. Certificate CLI checks are separate from runtime
execution. CA private keys are absent.

| Case | Actual evidence boundary |
| --- | --- |
| Encrypted deliberately oversized/malformed CertificateVerify | Source reasoning only; not executed natively and not claimed unreachable |
| Maximum/oversized/malformed NewSessionTicket | Source cap only; two ordinary received tickets do not exercise that maximum |
| CertificateRequest 65,536-byte ceiling | Source only; the no-client-certificate profile does not execute that ceiling |
| Valid P-384 ServerKeyExchange exactly 617 bytes | Source-derived maximum; native RSA-4096/X25519 exchanges are smaller |
| ECDSA-authenticated TLS peer | Not executed; P-256/P-384 factory acceptance is narrower evidence |
| Full PSS hash/MGF/salt/encoding cross-product | Representative native guards and source review, not exhaustive combinations |
| `SSL_write` WANT_READ/WANT_WRITE | Selected-profile reachability and retry-tuple source reasoning; native socket backpressure is distinct |
| Complete partial-record/claimed-body EOF corpus | Not executed; ordinary interrupted handshake/EOF is not that corpus |
| Complex ASN.1/SPKI, fuzzing, allocator/provider/RNG hooks | Outside ordinary conformance scope |

Independent critical review conditionally approved the current policy source subject to final gates.
Preparation touches two ambient DRBGs (primary/private) and three private-context DRBGs
(primary/public/private), retaining READY/provider/CTR-DRBG/AES-256/DF checks. It introduces no extra
ambient-public-DRBG prewarming requirement. SDK composition, host declarations, the resource metric
and fixed-frame exclusions remain conditional contracts. Native tests and an instrumented caller do
not establish RSS, hard wall time or sanitizer qualification of an uninstrumented SDK.

The package was requalified after the policy change: 57 files per OS, physical relocation with every
hash preserved and original prefixes absent, fresh independent consumer builds and four successful
TLS 1.2/1.3 runs. Each OS executed two `find_package` calls and checked 24 installed qd headers and
15 imported archives; direct targets are `qdcxx_tls`/`qdcxx_quote`. Consumer compilation has no
SDK/Asio/test headers or source/build-tree fallback. `/MT` and an incorrect SDK were rejected at
configure time without building the consumer workload. Linux uses ASan+UBSan with LSan OFF; the SDK
is uninstrumented. All four final regression profiles passed: 58/58 with JSON and 57/57 without JSON on each OS.
Generated-reference verification is recorded separately.
[TLS build instructions](../../../docs/CPP-BUILD.md#tls-source-profile-cpp-build-en) specify the
component option and qualified SDK.

Historical [feasibility observations](VALIDATION.md) and [TCP validation](TCP-VALIDATION.md) retain
their original scopes. This page does not establish an external server, reconnect, a full subscription
facade, throughput, RSS or hard wall time.
<a id="internal-test-peer"></a>

## Internal test peer

The test-only `qdcxx_tls_test_peer` is called by the parent
[`TlsTestPeer` helper](tls_test_peer.hpp), not installed as a user utility. Its
[entry point](tls_test_peer_main.cpp) accepts standalone `--help`, no arguments, or
`--control ABSOLUTE_OWNED_FILE`. Help/no-argument output is identical and starts no file I/O, TLS
initialization or socket. Invalid arguments return 2 before workload. A valid control invocation
reads the parent's bounded binary-v4 control (compatible v2 is also accepted) and serves one `127.0.0.1:0` connection with explicit
ordinary PEM fixtures, an explicit test TLS version, byte scripts and deadlines. It performs no fixture discovery.

Limits are 4 MiB per individual byte script (initial write, expected read, reply), 4,096 UTF-8 bytes per path, 128 KiB per PEM fixture and
1..60,000 ms per timeout. API defaults are TLS 1.3, clean close_notify, empty initial/reply, no expected read,
zero delays, 5,000 ms startup/handshake/read/write, 15,000 ms overall lifetime and 250 ms parent stop
grace. Close modes are clean, abrupt TCP EOF and hold; handshake/read delays are explicit test inputs.
No certificate/key contents, application bytes or formatted SDK error text are printed/reported.

After handshake the server writes `initialBytes`, waits the optional read delay, reads the exact
expected byte count, then writes the reply. Empty `initialBytes` by default preserves the previous
read/reply sequence. Initial and reply writes total at most 8 MiB; each script separately remains
limited to 4 MiB. This supports a two-phase challenge/response exchange without a second connection.

On POSIX the caller preserves waitable child semantics: `SIGCHLD` must not be `SIG_IGN`,
`SA_NOCLDWAIT` is excluded, and no other thread/handler may reap this child. A known incompatible
pre-start disposition returns `PROCESS_START`. The precondition lasts throughout owner lifetime;
the helper does not modify global signal disposition.

Exit 0 means normal script/stop or successful help output; 1 means control/TLS/I/O/deadline or help-output
failure; 2 means invalid arguments. The parent owns the exact child and unique temporary directory,
writes binary control/script data, and reads only numeric report fields. Child side effects are one
loopback listener, numeric `report`/`report.next` replacement and stop-marker polling. Parent stop
uses finite cooperative grace, then may terminate only its owned child and remove exact owned files;
it never scans for foreign processes or recursively removes the caller's root. Forced termination
is not a completed TLS-script result. Controls contain synthetic test data; no plaintext credentials
are passed in argv/environment or diagnostic logs.

Helper-source/API review and CLI/owned-child checks are distinct from production backend tests.
Helper-source/CLI and owned-child checks have separate receipts. Final regression includes the helper;
a helper-only result is not counted as production TLS-runtime evidence.

The current parent writes control-v4; the child also accepts v3 and v2. V3 adds the policy word after
close mode; V4 adds an initial TLS 1.3 ticket count of 0..2, default 0, required 0 for other versions.
V2 selects MODERN. CBC_ONLY/P521_ONLY/SHA1_ONLY belong only to the isolated negative-test server.
TLS 1.0/1.1 allow only MODERN; CBC_ONLY/SHA1_ONLY allow only TLS 1.2. Security level 0 is local to
legacy/SHA-1 test peers; the production client retains its strict TLS 1.2/1.3 profile.
Report-v2 contains 21 numeric words: magic, port, version, bytes read/written, ready, accepted,
handshake, expected-match, close-notify, completed, timeout, failed, SNI-present, SNI-localhost,
cipher suite, group, signature-hash NID, ticket count, signature scheme and signature byte count.
SNI flags contain no name; signature metadata emits no signature contents. Control compatibility
does not imply compatibility with the former 15-word report reader.

## Residual test-infrastructure limitation

In one earlier Windows 64-session run, all runtime checks completed but one test-child retirement
aggregate reported failure. Its cause is unknown; no causal fix is claimed. Added numeric diagnostics
classify future process-retirement outcomes without weakening the assertion. On the final source,
all four regression profiles, a separate 64-session run and a bounded overlap of two 64-session
processes passed without reproduction. This remains a test-infrastructure evidence limitation,
not proof that no defect exists. A recurrence requires investigation of the new numeric result before
further qualification.
