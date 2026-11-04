# API reference {#api-reference}

[Русская версия](api.ru.md) · [Documentation home](index.md)

The generated reference documents declarations in `org::ttldtor::qdcxx`, their parameters, errors
and ownership. Use the namespace/type indexes beside this page to find individual declarations.
[Getting started](getting-started.md) shows package discovery and linking.

| Component | Public headers | Link target | Detailed contract |
| --- | --- | --- | --- |
| Schedule rules | `qd/schedule/schedule.hpp` | `qdcxx_schedule` | [Owned schedule identities and typed calendar generations](SCHEDULE-RULES.md); `ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON` (default OFF); no Day/Session queries or filters |
| Public typed feed | `qd/api/feed.hpp`, `qd/api/event_registry.hpp` | `qdcxx_feed` | [Endpoint, Feed and typed subscriptions](FEED-API.md), [immutable registry](../spec/events/EVENT-REGISTRY.md), [mixed subscriptions](../spec/events/MIXED-SUBSCRIPTIONS.md); `ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` (default OFF), requires CLIENT/RUNTIME/RECOVERY |
| Encodings | `qd/encoding/` | `qdcxx_encoding` | [Encoding specifications](../spec/README.md#encoding-contracts) |
| Schemas and record identity | `qd/core/` | `qdcxx_core` | [Schema specifications](../spec/README.md#schema-contracts) |
| Frames, binding and tapes | `qd/protocol/` | `qdcxx_protocol` | [Protocol specifications](../spec/README.md#protocol-contracts) |
| Standard events | `qd/events/` | `qdcxx_quote`, `qdcxx_profile`, `qdcxx_trade`, `qdcxx_trade_eth`, `qdcxx_summary`, `qdcxx_event_mapping` | [Configuration](CONFIGURATION.md), [Trade](../spec/events/TRADE.md), [TradeETH](../spec/events/TRADE-ETH.md), [Summary](../spec/events/SUMMARY.md) |
| Candle periods | `qd/events/candle_period.hpp` | `qdcxx_candle_values` | [Types, text and interval estimates](../spec/events/CANDLE-PERIOD.md); no Candle receiving |
| Candle attributes | `qd/events/candle_attributes.hpp` | `qdcxx_candle_values` | [Values and symbol operations](../spec/events/CANDLE-ATTRIBUTES.md); no aggregation |
| Candle symbols | `qd/events/candle_symbol.hpp` | `qdcxx_candle_values` | [Owning values and object selectors](CANDLE-SYMBOLS.md); no Candle receiving |
| Collectors and multiplexing | `qd/collectors/`, `qd/mux/` | `qdcxx_collectors`, `qdcxx_mux`, `qdcxx_mux_queue` | [Subscription contracts](../spec/subscriptions/CONTRACTS.md) |
| JSON schemas | `qd/schema/json_schema.hpp` | `qdcxx_schema_json` | [Native JSON](../spec/schema/NATIVE-JSON.md); installed with `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` (default ON) |
| Offline replay | `qd/replay/tape_replay.hpp` | `qdcxx_replay` — build tree only | [Replay contract](../spec/protocol/OFFLINE-REPLAY.md); `ORG_TTLDTOR_QDCXX_BUILD_REPLAY=ON` (default OFF), not installed/exported |
| Optional client | `qd/client/` | `qdcxx_client` | [Local STREAM dispatch](../spec/subscriptions/CLIENT-DISPATCH.md) |
| Optional transport | `qd/transport/tcp_runtime.hpp`, `qd/transport/tls_runtime.hpp`, `qd/transport/tls_resources.hpp` | `qdcxx_transport`, `qdcxx_tls` | [TCP](../spec/transport/TCP-RUNTIME.md), [TLS](../spec/transport/TLS-RUNTIME.md) |
| Connection recovery | `qd/transport/recovery.hpp`, `qd/transport/tcp_recovery.hpp`, `qd/transport/tls_recovery.hpp` | `qdcxx_recovery`, `qdcxx_tls_recovery` | [Recovery contract and prerequisites](../spec/transport/RECOVERY.md); `ORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON` (default OFF) |

Optional targets require their corresponding [build options](CPP-BUILD.md). Contracts distinguish
borrowed views, owning values, finite work credits and local acceptance from peer delivery. A generated
declaration is not a promise that every planned protocol family or configuration is implemented.

The `qd/mux/queue.hpp` header requires `qdcxx_mux_queue`; this optional queue is enabled by `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`.

[Option chains](OPTION-CHAINS.md): `qd/ipf/option_chains.hpp`, optional `qdcxx_ipf` with `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`.

- [CFI and additional underlyings](GLOSSARY-VALUES.md): `qd/glossary/`, optional `qdcxx_ipf`.

- [Instrument price increments](PRICE-INCREMENTS.md): owned ranges, precision and price rounding through `qdcxx_ipf`.
