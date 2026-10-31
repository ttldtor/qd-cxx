# Runtime feasibility probe

[Русская версия](README.ru.md).

This isolated technical probe measures a proposed caller-driven standalone Asio/OpenSSL
profile. It is not the production transport adapter and does not establish the future
production adapter's dependency-OFF behavior.

The local fixture uses only ephemeral IPv4 loopback sockets and synthetic test certificates.
It performs no DNS lookup and contacts no external endpoint. DNS cancellation is a separate
feasibility requirement. Keep the test keys local; they are deliberately unencrypted and have
no use outside this fixture. Certificates expire after two days; regenerate before rerunning.

## Pinned dependencies and SDK build

`dependencies.lock.json` records source URLs, SHA-256 values, peeled tag commits and licenses.
Asio's [official source page](https://think-async.com/Asio/Repository.html) identifies its GitHub
repository. OpenSSL's [official downloads](https://www.openssl-library.org/source/) identifies
the release archive and checksum. The measured OpenSSL archive hash matches that published
checksum; the Asio archive is pinned by its measured hash and release tag commit. PGP signature
verification was not performed. Preserve upstream license files when redistributing sources
or SDKs: Asio uses BSL-1.0 and OpenSSL uses Apache-2.0.

Download the two exact URLs in the lock file into an owned directory, verify their SHA-256
values before extraction, and retain both upstream licenses. For example, use `curl --fail
--location --output <archive> <locked-url>` and PowerShell `Get-FileHash -Algorithm SHA256`.
No download or dependency acquisition happens during the default OFF configure.

Configure OpenSSL out of source with `no-shared no-tests no-docs no-autoload-config`, an explicit
owned `--prefix`, and an explicit owned `--openssldir`. Windows x64 uses `VC-WIN64A`, MSVC
14.42, Perl and NASM, followed by `nmake` and `nmake install_sw`; invoke these commands from
the VS developer environment. Ubuntu x86_64 uses `linux-x86_64`, GCC 13.3 and Perl, followed
by `make -j4` and `make install_sw`. Assembly is enabled on both measured platforms. The
Windows static SDK uses `/MT /Zl`. The pinned upstream configuration deliberately omits
default CRT library directives with `/Zl`; this allows the consumer's CRT selection.
Both `/MD` (`MultiThreadedDLL`, the repository default) and an explicitly selected `/MT`
(`MultiThreaded`) consumer passed this probe against the same unchanged static SDK, without
linker warning suppression or default-library overrides. The normal route therefore keeps
the repository's `/MD` default; no project-wide CRT change is needed. `/MD` executables require
the matching Microsoft C++ runtime deployment. This does not prove arbitrary CRT versions
or foreign DLL allocation boundaries; OpenSSL objects still use their matching OpenSSL
creation/free APIs. The test-only SDK is not a FIPS profile, and OpenSSL's own test suite is
intentionally outside this proof. See the pinned upstream
[VC configuration](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/Configurations/10-main.conf#L1579).

`no-autoload-config` exports `OPENSSL_NO_AUTOLOAD_CONFIG`: it disables automatic system
`openssl.cnf` loading without changing process-wide initialization flags at runtime. A normal
system OpenSSL build without that option is outside this measured SDK profile. The probe
rejects mismatched OpenSSL/Asio versions and missing configuration macro at compile time.

## Build and run

The following PowerShell variables are caller-selected absolute paths. `Source` names this
directory, `AsioInclude` names the extracted standalone Asio `include`, and `OpenSslSdk`
names the private installed OpenSSL SDK. CMake and Ninja must already be available.

```powershell
cmake -S "$Source" -B "$Build" -G Ninja `
  -DORG_TTLDTOR_QDCXX_TLS_PROBE=ON `
  "-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=$AsioInclude" `
  "-DOPENSSL_ROOT_DIR=$OpenSslSdk" `
  "-DORG_TTLDTOR_QDCXX_TLS_FIXTURES=$Fixtures" `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build "$Build"
cmake "-DOPENSSL=$OpenSslSdk/bin/openssl.exe" "-DFIXTURES=$Fixtures" `
  -P "$Source/GenerateFixtures.cmake"
ctest --test-dir "$Build" --output-on-failure
```

On Linux use the same CMake cache settings with Linux paths, omit the MSVC runtime setting,
and use `<sdk>/bin/openssl`. `ORG_TTLDTOR_QDCXX_SANITIZE=ON` instruments the probe with
ASan/UBSan for GCC/Clang; it does not instrument the separately built OpenSSL SDK.

The default OFF route can be checked without either SDK:

```powershell
cmake -S "$Source" -B "$OffBuild" -G Ninja `
  -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON `
  -DCMAKE_DISABLE_FIND_PACKAGE_Threads=ON `
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON `
  -DOPENSSL_ROOT_DIR=missing-sdk -DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=missing-sdk
cmake --build "$OffBuild"
```

The executable's no-argument invocation and `--help` print identical usage text and do not
start the suite. `--run --fixtures DIRECTORY` explicitly enables it. Exit codes are 0 for
help/success, 1 for runtime/proof/output failure and 2 for invalid arguments. The run prints
actual library/provider versions, the selected backend and per-case completion/error data.

## What the cases establish

One caller thread owns and pumps every `io_context` and every user completion. Windows IOCP
uses an internal timer-wait helper when the first Asio timer queue is created. That helper
waits on a kernel waitable timer and posts an IOCP wake; it invokes no user callback and
reads no external configuration. Host shutdown sets its shutdown flag, arms the waitable
timer to wake it, drains owned operations and joins the helper. The host therefore outlives
quiescence and is destroyed outside its callbacks. A socket-provider fallback without
ConnectEx may create an additional select-reactor helper; that fallback is outside the
measured modern Windows TCP profile. Neither helper constitutes a second owner domain.
Pending operations retain their buffers/SSL objects until all completion handlers return.
Cancellation closes only owned sockets, and the host continues pumping until `run()` drains.
`io_context::stop()` is not used as evidence of quiescence. A completion may request stop by
closing the run's sockets; it never destroys or joins its execution host.

Read cancellation uses an idle connected peer. Write cancellation uses an idle peer that
never reads, small send/receive buffers, and a bounded 32 MiB payload; the timer asserts the
write has not completed before cancellation. A backlog fixture opens at most 512 local
connections and reports how many are unfinished before cancellation. A separate immediate
connect/cancel race explicitly reports `pending-proof=0`. If the platform backlog accepts
every connection, that case supplies no pending-connect evidence and must not be reported
as a pending proof merely because cancellation was requested.
The suite fails if its bounded backlog fixture produces no pending cancelled connect.

Each TLS client uses one explicit CA PEM file, mandatory peer verification, TLS 1.2 minimum
and TLS 1.3 maximum. DNS identity uses `SSL_set1_host`, SNI, `NO_PARTIAL_WILDCARDS` and
`NEVER_CHECK_SUBJECT`; IP identity uses `X509_VERIFY_PARAM_set1_ip_asc` and no DNS SNI. The
server certificate has a deliberately incorrect CN and correct DNS/IP SAN values. Cases
cover trusted DNS/IP, TLS 1.2 negotiation, an untrusted issuer, wrong hostname, expiration,
a raw TCP peer that stalls TLS handshake, and a peer that stalls TLS shutdown. A stalled
shutdown is aborted by its 100 ms policy; it is not reported as a successful graceful close.
There is no plaintext fallback, implicit OS trust, default trust-path loading or verification
bypass.

The context limits certificate-list messages to 64 KiB and verification depth to four
intermediate CA certificates, disables record compression, certificate compression in both
directions and renegotiation, and uses explicit 17 KiB Asio input/output record buffers.
The paired OpenSSL BIO rings and OpenSSL's own record/certificate state are additional.
These controls do not establish a total per-endpoint SDK heap or
CPU budget. OpenSSL allocates certificate/algorithm/session state outside the application's
buffers, and offers no per-context allocation quota or resumable CPU-work limit for a single
handshake call. Global allocator-hook replacement is not part of this profile. A future
adapter must count/restrict live contexts and SDK allowances separately, bound input and
pending application buffers, and document its measured conservative SDK memory allowance;
it must not claim that application payload bytes alone satisfy the resource contract.

`OSSL_LIB_CTX` and an explicitly loaded built-in default provider outlive every SSL context,
stream and pending operation. `SSL_CTX_new_ex` binds each SSL context to that library context;
Asio's native-handle context constructor takes ownership and frees `SSL_CTX`. Contexts and
streams are destroyed after handler drain, then the provider and library context are freed.
The probe does not invoke `OPENSSL_cleanup`, set a global default library context, load global
configuration, or change global trust/provider properties. Normal OpenSSL internal automatic
initialization still occurs on first real TLS use.

The measured profile also assumes the application has not manually loaded a process-wide
OpenSSL `system_default` SSL configuration. `SSL_CTX_new_ex` consults that registry even for
an explicit library context; `no-autoload-config` suppresses automatic file loading, not a
host's explicit global configuration call. Such mixed global configuration is outside this
proof and needs an explicit integration policy before the production TLS adapter.

Per-operation timers are 100 ms for deliberate cancellation and three seconds for case
failure escalation. CTest has a 30-second outer process deadline for the suite. Timers cannot
preempt arbitrary blocking C++ callback code; the fixture uses only prompt owned callbacks.
The proof covers Windows x64/MSVC IOCP and Ubuntu x86_64/GCC epoll only. Other architectures,
macOS, io_uring, system OpenSSL packages, OS-native trust, a background execution host,
FIPS, revocation/OCSP/CRL checks, encrypted key handling, proxies, real credentials and production transport integration
remain unproven or outside this profile.

Primary contracts: [Asio native SSL context](https://think-async.com/Asio/asio-1.38.2/doc/asio/reference/ssl__context/context.html),
[Asio stop](https://think-async.com/Asio/asio-1.38.2/doc/asio/reference/io_context/stop.html),
[OpenSSL SSL_CTX_new_ex](https://docs.openssl.org/3.5/man3/SSL_CTX_new/),
[library contexts](https://docs.openssl.org/3.5/man3/OSSL_LIB_CTX/),
[hostname verification](https://docs.openssl.org/3.5/man3/SSL_set1_host/),
and upstream `INSTALL.md` for `no-autoload-config`.
