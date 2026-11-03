# Public feed API validation

[Русская версия](FEED-VALIDATION.ru.md) · [Application guide](../../../docs/FEED-API.md)

This report covers the optional `qdcxx_feed` public API, receiver registration on an
existing recovery owner, and the standard optional peer EndpointId needed by the
demo source. It does not claim full QD compatibility or high-level TLS/HISTORY support.

## Native profiles

The configured suite enables CLIENT, RUNTIME, RECOVERY, FEED_API, CONNECT and TLS.
JSON OFF and ON are tested separately. TLS remains a lower-level API; enabling it
in this matrix does not add TLS to the public feed API. Dependencies use the locked
project profiles. The public API and its maintained console example use C++23 with extensions disabled.

| Platform | Configuration | Full CTest result |
| --- | --- | --- |
| Windows x64, MSVC 19.42.34444.0 | Release, `/MD`, JSON OFF | 112/112 PASS |
| Windows x64, MSVC 19.42.34444.0 | Release, `/MD`, JSON ON | 113/113 PASS |
| Ubuntu 24.04 / WSL2 x86-64, GCC 13.3.0 | Debug, ASan/UBSan, JSON OFF | 112/112 PASS |
| Ubuntu 24.04 / WSL2 x86-64, GCC 13.3.0 | Debug, ASan/UBSan, JSON ON | 113/113 PASS |

Linux uses `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer and instrumented
third-party SDK internals are not established by these runs.

## Focused checks

| Check | Executed result and scope |
| --- | --- |
| [Public API](../feed_api_tests.cpp) | 14 cases; Windows 131 assertions, Linux 129 assertions, all PASS. Complete owning Quote/Profile values, late registrations, overlapping clients, limits, isolated callback failure, callback mutation/release, wrong-thread and reentrant awaits, concurrent retirement/capture release, independent endpoints and default drain. Platform-specific retained diagnostics account for the assertion difference. |
| [Resolver snapshot](../feed_dns_tests.cpp) and [demo help](../feed_demo_help.cmake) | PASS on both platforms. Bounded ordered IPv4 parsing, duplicates, malformed/unsupported addresses, NUL and size boundaries; equivalent no-argument/English help, Russian help and invalid CLI syntax. Help does not construct the runtime or connect. |
| [Recovery receiver registration](../recovery_receiver_tests.cpp) | 7 cases / 485 assertions PASS on both platforms, plus four selected existing regression tests. TCP registration uses the actual adopted queue/schema and preserves typed refusals. The TLS forwarding wrapper was compiled on both platforms; these new cases do not constitute a new TLS resource qualification. |
| [Live-session parser](../live_session_tests.cpp) | 30 cases / 40,968 assertions PASS on both platforms. Five added cases failed against the old parser before the change. [Synthetic EndpointId witnesses](../../fixtures/subscriptions/endpoint-identity-v1/README.md) come from locked Java QD. Unknown trailing extensions remain unsupported; malformed lengths and insufficient work preserve prior committed descriptor state. Live peer identity bytes are not fixture data. |
| Scoped ThreadSanitizer | GCC 13.3 Debug, `-fsanitize=thread -fno-omit-frame-pointer`, `TSAN_OPTIONS=halt_on_error=1`: seven selected ownership/callback/retirement cases, 53 assertions, all PASS, no TSan report. Actual compile/link flags and `libtsan.so.2` loading were checked. This is not the full suite; c-ares C code was not instrumented. |
| Installed/relocated consumers | Both platforms compiled and linked the maintained console example, the guide snippet and an inactive ownership consumer against only a relocated installed package. Two CTest checks per platform passed; capture release, limits, detached handles and five help/invalid invocations were exercised. No compiler warnings. The snippet was compiled, not externally connected. |
| Optional OFF | Both minimal production builds passed with FEED_API/CLIENT/RUNTIME/RECOVERY/TLS/JSON OFF, Threads discovery disabled and no dependency downloads. Optional targets remained absent. Enabling FEED_API without its explicit prerequisites was rejected. |

The default one-second drain regression was reproduced before the fix:
`drained=false`, `deadlineExpired=true`, `hadGap=false`. A bounded worker turn now
performs at most 64 recovery polls with one recovery-work seed and one submit-work
seed per turn, shared by its polls, for both normal progress and drain. It does not refill credits
inside the turn. The same controlled default-drain case passes after the fix;
this is not a throughput or hard-time guarantee.

## External demo observation

On 2026-11-03 UTC, the [public console example](../../../examples/feed_quote/main.cpp)
connected to `demo.dxfeed.com:7300` through its default system DNS discovery,
subscribed to AAPL Quote and printed exact decimal bid/ask prices and sizes.

| Platform | Receiving interval | Quote rows | Actual local result | Exit |
| --- | --- | --- | --- | --- |
| Windows | 12 seconds | 8 | drained, no deadline expiry, no reported gap; subscription diagnostic NONE | 0 |
| Linux ASan/UBSan | 12 seconds | 7 | drained, no deadline expiry, no reported gap; subscription diagnostic NONE | 0 |

These observations establish actual interoperability for those runs, not future
service availability, fresh exchange data or completeness of unread network input.
Demo data may be delayed.

Local retirement and an error-free transport history are separate results. Windows
retained `socket-read` / native 1236; Linux retained `recovery-peer-end` / native 0.
Both remained visible in the endpoint diagnostic and console stderr. The endpoint
may therefore report FAILED even when the separately inspected local prefix drained.
Neither code is relabelled as expected cancellation. Example exit 0 requires at
least one actual Quote, actual drain without expiry/gap, and no subscription error;
it does not certify a clean transport history. Earlier failed live drain runs were
retained as failure evidence rather than counted as success.

## Reproduce the configured checks

Use the [explicit build prerequisites](../../../docs/CPP-BUILD.md#public-feed-api-build).
With the relevant options enabled:

```text
cmake --build build --target qdcxx_feed_api_tests qdcxx_feed_dns_tests qdcxx_feed_demo qdcxx_recovery_receiver_tests qdcxx_live_session_tests --parallel 2
ctest --test-dir build --output-on-failure -R "^(feed_.*|transport_recovery_receiver|protocol_live_session)$"
ctest --test-dir build --output-on-failure
```

The [complete example guide](../../../examples/feed_quote/README.md) documents the
installed consumer and finite external run. Blocking application callbacks/output
can delay other endpoints and shutdown; finite capacities and drain policy do not
bound process RSS or arbitrary application execution time. Release and user
acceptance remain separate from these engineering checks.
