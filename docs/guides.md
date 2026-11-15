# Practical guides {#practical-guides}

[Русская версия](guides.ru.md) · [Documentation home](index.md)

| Task | Guide | Choose this when |
| --- | --- | --- |
| Build a market-data client | [Quote example](../examples/feed_quote/README.md) | Install `qdcxx_feed`, connect and receive typed events in an independent application |
| Query current and historical data | [Client queries](../examples/client_queries/README.md) | Request an owning Quote, Candle history and an order-book snapshot |
| Work with models and instruments | [Client models](../examples/client_models/README.md) | Offline Candle and order-book models, instrument profiles and option chains |
| Choose event symbols and sources | [Market symbols](MARKET-SYMBOLS.md), [Candle symbols](CANDLE-SYMBOLS.md), [event sources](EVENT-SOURCES.md) | Construct and inspect supported symbol and source values |
| Parse instrument trading hours | [Schedule rules](SCHEDULE-RULES.md) | Complete pinned grammar, named venues and typed defaults in explicit bounded contexts; [console example](../examples/schedule_rules/README.md) |
| Mix market events and select an Order source | [Market console sample](../examples/feed_market/README.md) | Composite and regional Quote, Trade, a source-specific Order, symbol mutation and final statistics |
| Test source and relay integration | [Distribution harness](../examples/feed_distribution/README.md) | Run a source, QTP relay and independent mixed Feed clients |
| Receive typed live events | [Public feed API](FEED-API.md) | Library-owned progress, Quote/Profile subscriptions and plain TCP; [complete example](../examples/feed_quote/README.md) |
| Embed in an existing host | [Runtime bootstrap ownership](../spec/transport/RUNTIME-EMBEDDING.md) | Startup requirements and explicit borrowed bootstrap lifetime |
| Model instrument metadata | [Instrument-profile values](INSTRUMENT-PROFILES.md) | Owned standard/custom fields and typed conversions without I/O |
| Parse regional market symbols | [Market-symbol helpers](MARKET-SYMBOLS.md) | Exact UTF-16 base/exchange composition without subscription routing |
| Read binary tapes | [Tape inspection](TAPE-INSPECT.md) | Input is an existing file; select a supported finite source profile |
| Rewrite a tape offline | [Tape writing](TAPE-WRITE.md) | Convert an existing supported binary tape into a new destination using an explicit schema |
| Replay a historical Quote | [OnDemand loopback example](../examples/on_demand/README.md) | Connect to the local finite fixture for speed, pause and return to live |
| Receive an instrument profile | [Live IPF example](../examples/ipf_live/README.md) | Connect to an IPF source and print owning revisions |
| Build an option chain from a file | [Offline IPF example](../examples/ipf_option_chain/README.md) | Read complete instrument profiles and group calls and puts by underlying and strike |
| Inspect protocol frames | [QTP inspection](QTP-INSPECT.md) | You need framing and record diagnostics offline |
| Inspect Profile events | [Profile inspection](PROFILE-INSPECT.md) | You need the selected Profile mapping |
| Configure records | [Schemas and settings](CONFIGURATION.md) | Built-in Quote/Profile defaults do not describe your input |
| Export a Java schema | [Schema exporter](../tools/schema-export/README.md) | You need an exported schema from locked Java QD |
| Receive controlled STREAM data | [STREAM subscriptions](SUBSCRIPTIONS.md) | A controlled plain-TCP peer and exact selectors meet your needs |
| Compose a STREAM application | [Programmatic example](../examples/subscriptions/README.md) | Your application owns runtime progress and callbacks |
| Run the Kraken gateway | [Kraken Quote gateway](KRAKEN-GATEWAY.md) | Publish the public BTC/USD best bid and offer to QTP Feed clients |

The STREAM program has a finite cutoff and uses plain TCP. The separate TLS runtime has its own
[trust, identity, credentials and lifetime contract](../spec/transport/TLS-RUNTIME.md); selecting TLS
in a stored endpoint does not turn the plain program into a TLS client. Reconnect and a complete
shared subscription facade remain separate capabilities. Check [current support](CURRENT-STATE.md)
before choosing an integration.

- [Indexed and order event sources](EVENT-SOURCES.md): owned ID/name values without subscription routing.

- [CFI and additional underlyings](GLOSSARY-VALUES.md): `qd/glossary/`, optional `qdcxx_ipf`.

- [Instrument price increments](PRICE-INCREMENTS.md): owned ranges, precision and price rounding through `qdcxx_ipf`.

- [Live instrument-profile collector](IPF-COLLECTOR.md): owning revisions, removals and safe listener callbacks.

- [Live instrument-profile connections](IPF-CONNECTION.md): background delivery, explicit worker runtime and safe close.

- [Print event/value text](EVENT-TEXT.md): explicit context, exact UTF-16 and direct UTF-8 output; [console example](../examples/event_text/README.md).


[Endpoint configuration](ENDPOINT-CONFIGURATION.md): explicit addresses/properties, immutable snapshots, effective local values and Java compatibility boundaries.
The OnDemand example uses the qualified local fixture. [Provider access requirements](../spec/transport/ON-DEMAND-ACCESS.md) describe service qualification for a deployed provider.
