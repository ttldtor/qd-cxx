# Developer documentation {#developer-documentation}

[Русская версия](developer.ru.md) · [Documentation home](index.md)

Use this section when building optional components, changing the library or checking compatibility.
Application setup and usage start in [Getting started](getting-started.md) and [Practical guides](guides.md).

## Build and tooling

- [Detailed build options and component reference](CPP-BUILD.md)
- [Contribution requirements](../CONTRIBUTING.md), [C++ conventions](CPP-STYLE.md) and [naming](NAMING.md)
- [Developer-tool catalog](../tools/dev/README.md): UTC logging, formatting and offline compatibility tools
- [Build this documentation](../tools/docs/README.md)
- [Upstream reference](UPSTREAM.md) and [compatibility policy](COMPATIBILITY.md)

## Verification results and limits

Results describe exact inputs and toolchain profiles. Source analysis, native execution and independent
reruns are different evidence; none is a general security certification or an RSS/hard-time guarantee.

- [TLS runtime/resource qualification](../tests/conformance/transport/TLS-VALIDATION.md)
- [TCP runtime qualification](../tests/conformance/transport/TCP-VALIDATION.md)
- [Recovery-owned HISTORY range observation](../tests/conformance/transport/HISTORY-STATUS-VALIDATION.md)
- [Tape reader and supported inputs](../spec/protocol/TAPE-READER.md)
- [Tape writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md)
- [Schema export validation](../tests/conformance/schema-export/VALIDATION.md)
- [Subscription compatibility](../tests/conformance/subscriptions/VALIDATION.md)
- [Composed receiving profiles and CLI qualification](../tests/conformance/subscriptions/DELIVERY-VALIDATION.md)
- [Subscription receiver and installed console example](../tests/conformance/subscriptions/RECEIVER-VALIDATION.md)
- [Public feed API, installed package and live Quote](../tests/conformance/subscriptions/FEED-VALIDATION.md)
- [Knowledge base and decision trace](knowledge/README.md)

[Current capabilities](CURRENT-STATE.md) distinguish delivered components from the
[remaining product direction](NEXT-STEPS.md). Conformance captures and technical specifications remain
part of the source distribution; the navigation above does not turn proposals into supported behavior.

- [Text-event Java oracle](../tools/upstream/text-events-oracle/README.md): finite standard text delegate and wire observations.
- [Event-source oracle](../tools/upstream/event-source-oracle/README.md): capture/verify pinned Java source identities, with help and bounded processes.
- [Order-source capability oracle](../tools/upstream/order-source-capability-oracle/README.md): six event families, full-book support and specific-source values.

- [CFI and additional underlyings oracle](../tools/upstream/glossary-oracle/README.md): Production glossary oracle: CFI, AdditionalUnderlyings, noargs/EN/RU help, JDK8, heap128MiB/60s, Git30s.

- [Instrument price increments](PRICE-INCREMENTS.md): owned ranges, precision and price rounding through `qdcxx_ipf`.

- [IPF collector contract](../spec/ipf/COLLECTOR.md): SDK-free thread-safe model, finite retention/work and callback cutoff; [bounded Java oracle](../tests/fixtures/ipf/collector/README.md).

## Event value utilities

The [event-value oracle](../tools/upstream/event-values-oracle/README.md) captures and verifies exact pinned Java conversions without an SDK rebuild or endpoint. See its built-in EN/RU help and [public contracts](../spec/events/EVENT-VALUES.md). The [installed consumer](../tests/conformance/event_values_consumer/CMakeLists.txt) compiles every new public header separately and exercises raw preservation, conversions and input errors; it can link the SDK-free value target or the compatible feed target.

- [Live IPF connection contract](../spec/ipf/CONNECTION.md): explicit joined runtime, private incremental sink, finite cumulative grants; [bounded actual Java oracle](../tests/fixtures/ipf/live/README.md).

- [Event text validation](../tests/conformance/EVENT-TEXT-VALIDATION.md): production Java profiles, exact reference differences, numeric rounding, finite ownership and installed consumers.
- [Live IPF public sample](../examples/ipf_live/README.md): installed independent consumer, input-free EN/RU help, bounded duration/revision capture and creator shutdown; [shutdown oracle](../tests/fixtures/ipf/live/README.md#shutdown-oracle) uses pinned JDK8 with finite owned cleanup.

- [Message/Configuration receiving and codecs](MISC-EVENTS.md): owning payloads, explicit decode and Java-compatible Configuration version admission; [version oracle](../tools/upstream/configuration-version-oracle/README.md).

- [Indexed Order reception](ORDER.md): explicit HISTORY, complete owning fields and overlapping source selectors.
- [MarketMaker/derived Order oracle](../tools/upstream/market-maker-order-oracle/README.md): full types, physical time, delegate sources and real binary parser observations.
- [SpreadOrder and AnalyticOrder oracle](../tools/upstream/order-variants-oracle/README.md): source-specific records, physical FIELD time and actual QTP bytes.
- The [TimeAndSale oracle](../tools/upstream/time-and-sale-oracle/README.md) captures composite and regional STREAM and HISTORY records in four schema profiles.
- [Order parser/delegate oracle](../tools/upstream/order-oracle/README.md): original physical slots and bounded standard-schema observations.
- [Series oracle](../tools/upstream/series-oracle/README.md): pinned HISTORY binary round trip, schema variants and indexed option values.
- The [Candle HISTORY oracle](../tools/upstream/candle-lifecycle-oracle/README.md) captures exact Java snapshots, local replay, same-index corrections and source QTP.
- The [OptionSale oracle](../tools/upstream/option-sale-oracle/README.md) captures indexed HISTORY events for pinned Java QD schema variants.
- The [Sale HISTORY lifecycle oracle](../tools/upstream/sale-lifecycle-oracle/README.md) captures Java corrections, cancellations, indexed removals, snapshots and fresh collector streams.

- [NuamTrade oracle](../tools/upstream/nuam-trade-oracle/README.md) captures pinned Java mapper and delegate values, regional symbols and required-field refusals.
