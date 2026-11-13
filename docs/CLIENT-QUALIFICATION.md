# Receiving client qualification {#receiving-client-qualification}

[Русская версия](CLIENT-QUALIFICATION.ru.md). English is authoritative.

Start with the [installed Feed sample](../examples/feed_quote/README.md) and select the receiving route from the [capability and wire matrix](CLIENT-CAPABILITIES.md). The matrix links the individual public contracts to deterministic and pinned Java fixture checks.

| Area | Observable result | Verification path |
| --- | --- | --- |
| Feed and lifecycle | Typed Quote delivery, state and statistics observations, terminal retirement and capture release | [Feed API](FEED-API.md), `feed_api`, `client_dispatch`, `mux_queue` |
| Market and historical examples | Market event output, bounded history and last-event queries, detached model results | [Market sample](../examples/feed_market/README.md), [query sample](../examples/client_queries/README.md), [model sample](../examples/client_models/README.md) |
| Source controls | Local binary tape and finite OnDemand replay, replay speed and pause, clear or explicit live transition | `tape_feed`, `on_demand_feed`, [OnDemand profile](../spec/transport/ON-DEMAND-ACCESS.md) |
| Secure package | Verified TLS and configured QTP Basic, rejection of wrong trust or name, disabled TLS without OpenSSL | [Build and install](CPP-BUILD.md), `feed_tls`, [standalone secure consumer](../tests/conformance/feed_tls_consumer/CMakeLists.txt) |

Build with MSVC Release on Windows or GCC Debug with ASan and UBSan on Linux. The Linux qualification profile has leak detection disabled. Use the [build profiles](CPP-BUILD.md) to install the SDK, then configure an independent consumer against the installed prefix and again after moving that prefix. Verify the sample output and confirm that the old installation path is no longer present. On Windows, use both SDK header include orders in the installed header test.

The OnDemand qualification uses the bounded loopback profile and pinned fixtures. Access and interoperability with an external replay provider require separate qualification.
