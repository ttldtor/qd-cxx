# Architecture and protocol contracts {#architecture-protocols}

[Русская версия](README.ru.md) · [Documentation home](../docs/index.md)

Read [Architecture](../docs/ARCHITECTURE.md) for component responsibilities and
[Compatibility](../docs/COMPATIBILITY.md) for the Java QD behavioral boundary. Specifications describe
observable contracts; [current capabilities](../docs/CURRENT-STATE.md) identify implemented profiles.

## Encoding contracts {#encoding-contracts}

See the [wire encoding specification](encoding/PRIMITIVES.md), including compact integers, text and symbols.

## Schema contracts {#schema-contracts}

[Configuration](../docs/CONFIGURATION.md) describes explicit caller schemas and source precedence.
The [schema model](schema/NORMALIZED-MODEL.md), [native JSON](schema/NATIVE-JSON.md) and
[ordinary wire discovery](schema/WIRE-INVENTORY.md) specify the corresponding boundaries.

[Schema-bound event mappings](events/EVENT-MAPPING.md) and the
[immutable receiving event registry](events/EVENT-REGISTRY.md) define explicit
typed projections over complete caller schemas.
[Mixed registered subscriptions](events/MIXED-SUBSCRIPTIONS.md) select several
supported types through one owning variant-valued logical subscription.

- [Candle period values](events/CANDLE-PERIOD.md): pure aggregation types and period text, independent of Candle receiving.

## Protocol contracts {#protocol-contracts}

- [QTP framing](protocol/FRAMING.md) and [tape reading](protocol/TAPE-READER.md)
- [Subscription contracts](subscriptions/CONTRACTS.md), [live session](subscriptions/LIVE-SESSION.md)
  and [local STREAM dispatch](subscriptions/CLIENT-DISPATCH.md)
- [TCP runtime](transport/TCP-RUNTIME.md), [TLS runtime](transport/TLS-RUNTIME.md)
  and [TLS resource assumptions](transport/TLS-RESOURCES.md)

For rationale and unresolved QD/native design questions, use the [knowledge base](../docs/knowledge/README.md).
Exact conformance results belong to the [developer reading path](../docs/developer.md).

[Option-chain grouping](ipf/OPTION-CHAINS.md) defines profile-based series/strike ordering, replacement and finite-resource admission.
