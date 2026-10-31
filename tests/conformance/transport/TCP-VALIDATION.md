# TCP runtime validation

[Русская версия](TCP-VALIDATION.ru.md). English is authoritative.

This matrix qualifies the [caller-driven TCP contract](../../../spec/transport/TCP-RUNTIME.md),
[DNS envelope](../../../spec/transport/DNS-RESOURCES.md) and
[qualified SDK source set](../../../cmake/runtime-profile/README.md).
It does not qualify TLS, collectors, application callback delivery or reconnect.

## Behavioral coverage

- Bootstrap covers a real native readiness sample and a supplied per-call FALSE
  result before SDK ownership. Windows verifies that a foreign SDK reference
  survives refusal and no extra reference remains; Linux's always-success public
  getter is not used as a reference counter, so that ownership ordering is source
  evidence. Neither case reproduces an actual OS entropy failure or tests quality.
- Factory work/arena failures preserve the input schema/composer; endpoint activation,
  network I/O and DNS query admission have not started when preparation fails.
  SDK context/channel preparation may already have occurred and must retire safely.
- Original full standard Quote descriptions/data pass through byte fragments into
  the prepared event mapping. Retained immutable owners survive close.
- One-byte writes preserve ordering and the whole output owner; a busy slot rejects
  another composition without mutation. Socket byte credits yield and resume.
- Pending read, genuinely pending connect and genuinely pending write retire
  through abort/deadline. The harness pumps the owner before checking that the
  operation remains pending; an initial WOULD_BLOCK alone is not this proof.
- Windows cancellation preserves reported-write lower bounds and acceptance
  uncertainty. A local pending suffix is not declared definitely unsent.
- Read prefix/error, EOF/incomplete framing and abort/completion boundaries retain
  the first observed non-cancellation cause and a bounded distinct secondary cause.
- Wrong-thread actions reject without mutation; explicit close has allocation-free
  admission, repeated fresh runs do not share operation state or callbacks.
- Explicit DNS A/AAAA, same-response CNAME/case folding, TCP fallback and small
  read fragments, deduplication, unsupported names, unrelated answers, CNAME-only
  replies, loops and endpoint overflow have controlled local peers.
- UDP WOULD_BLOCK resumes through the owned registry write hint. A caller budget
  above 128 cannot enlarge the independent per-entry native-attempt cap.
- Silent DNS cancellation/deadline retires the channel. DNS/session-output overlap
  remains in the historical resource peak after close; peak never decreases.

Tests use their own finite peers, threads and sockets, with harness timeouts and
cleanup. No public endpoint is needed. Internal CTest entries are `transport_tcp`
and `transport_cares_ownership`; the latter uses valid objects and a supplied
queue-admission error status, not an actual malloc-failure reproduction.

## Required configurations and evidence limits

Run final-source Windows x64 MSVC Release and Linux x86-64 GCC Debug with ASan/UBSan.
Instrument both the adapter and c-ares in the Linux profile; retain the explicit
LeakSanitizer exclusion for the main suite. A separate small ownership-unit LSan
run is recorded independently and does not change that suite's scope.

Check runtime OFF without valid SDK roots: no SDK read/find/fetch/include/link or
extra dependency compiler is required. Check enabled production builds with
`BUILD_TESTING=OFF`, installed/relocated consumers and explicit native static link
closure. Wrong source hashes/file sets, missing or incompatible SDKs and unqualified
target architectures must fail clearly. Preserve original source/preparation data.

Source review and source manifests establish the conditional SDK storage/work
derivation. Execution verifies adapter enforcement and owned-operation lifetime;
neither converts logical credits to measured CPU latency or requested capacities
to physical RSS. Frozen source, checks and independent review must identify the
same artifact before integration.

## Executed reference matrix

| Profile or check | Result and scope |
| --- | --- |
| Windows x64, MSVC 19.42, Release, runtime ON | Final-source CTest 42/42. |
| Linux x86-64, GCC 13.3, Debug ASan/UBSan, runtime ON | Final-source CTest 42/42; adapter and c-ares instrumented, main-suite LSan OFF. |
| Runtime OFF, both platforms | No runtime SDK find/file operations or C language enablement; relocated production consumers pass. Windows platform RC setup remains normal CMake behavior. |
| Runtime ON, both platforms | Production-only build and relocated consumer configure/link/run pass; 30 installed files, custom `lib64` and `public include`, explicit `qdcxx_DIR`. No SDK headers or C compiler needed by the consumer. |
| Source qualification | 637 Asio and 532 c-ares source hashes match. Extra/modified Asio header, missing license and old c-ares revision are rejected. Synthetic ARM64 target metadata is rejected; this does not qualify an actual ARM64 build. |
| Independent execution | Five critical Windows regressions pass, 1,245 assertions; four valid-object SDK ownership cases pass. |
| Existing protocol compatibility | Native controls from Windows/GCC match actual locked Java parser callbacks; 344-byte input and 4,778-byte callback artifact remain unchanged. Java execution used the Windows JDK for both native captures. |
| Style and API reference | Nine C++ files pass the pinned format/check chain; Doxygen 1.18.0 emits no warnings, generated API content inspected. The SDK C ownership unit retains upstream C style. |

Windows discovery through prefix alone did not find the custom `lib64` location;
explicit `qdcxx_DIR` resolves that documented package directory.
