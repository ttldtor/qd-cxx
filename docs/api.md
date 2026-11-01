# API reference {#api-reference}

[Русская версия](api.ru.md) · [Documentation home](index.md)

The generated reference documents declarations in `org::ttldtor::qdcxx`, their parameters, errors
and ownership. Use the namespace/type indexes beside this page to find individual declarations.
[Getting started](getting-started.md) shows package discovery and linking.

| Component | Public headers | Link target | Detailed contract |
| --- | --- | --- | --- |
| Encodings | `qd/encoding/` | `qdcxx_encoding` | [Encoding specifications](../spec/README.md#encoding-contracts) |
| Schemas and record identity | `qd/core/` | `qdcxx_core` | [Schema specifications](../spec/README.md#schema-contracts) |
| Frames, binding and tapes | `qd/protocol/` | `qdcxx_protocol` | [Protocol specifications](../spec/README.md#protocol-contracts) |
| Standard events | `qd/events/` | `qdcxx_quote`, `qdcxx_profile`, `qdcxx_event_mapping` | [Configuration](CONFIGURATION.md) |
| Collectors and multiplexing | `qd/collectors/`, `qd/mux/` | `qdcxx_collectors`, `qdcxx_mux`, `qdcxx_mux_queue` | [Subscription contracts](../spec/subscriptions/CONTRACTS.md) |
| JSON schemas | `qd/schema/json_schema.hpp` | `qdcxx_schema_json` | [Native JSON](../spec/schema/NATIVE-JSON.md); installed with `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` (default ON) |
| Offline replay | `qd/replay/tape_replay.hpp` | `qdcxx_replay` — build tree only | [Replay contract](../spec/protocol/OFFLINE-REPLAY.md); `ORG_TTLDTOR_QDCXX_BUILD_REPLAY=ON` (default OFF), not installed/exported |
| Optional client | `qd/client/` | `qdcxx_client` | [Local STREAM dispatch](../spec/subscriptions/CLIENT-DISPATCH.md) |
| Optional transport | `qd/transport/` | `qdcxx_transport`, `qdcxx_tls` | [TCP](../spec/transport/TCP-RUNTIME.md), [TLS](../spec/transport/TLS-RUNTIME.md) |

Optional targets require their corresponding [build options](CPP-BUILD.md). Contracts distinguish
borrowed views, owning values, finite work credits and local acceptance from peer delivery. A generated
declaration is not a promise that every planned protocol family or configuration is implemented.

The `qd/mux/queue.hpp` header requires `qdcxx_mux_queue`; this optional queue is enabled by `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`.
