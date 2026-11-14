# qd-cxx

[Русский](README.ru.md) · English

`qd-cxx` is a C++23 market-data library with live receiving and offline inspection tools implementing a bounded subset of
[devexperts/QD](https://github.com/devexperts/QD). Java QD at the revision in [upstream.lock](upstream.lock)
is the compatibility reference. Supported profiles and limits are explicit; full QD compatibility is not claimed.

## Documentation

[Open the generated documentation](https://ttldtor.github.io/qd-cxx/) for the guides and API reference.

1. [Getting started](docs/getting-started.md) — install the library and run a small consumer.
2. [Practical guides](docs/guides.md) — tapes, schemas, events and controlled STREAM use.
3. [API reference](docs/api.md) — public headers, link targets and ownership contracts.
4. [Architecture and protocols](spec/README.md) — component responsibilities and wire behavior.

## Examples

The [Feed API guide](docs/FEED-API.md) describes connection, delivery and shutdown.

- [Subscribe to Quote](examples/feed_quote/README.md) from an independent client application.
- [Query current and historical data](examples/client_queries/README.md) for the last Quote, Candle history and an order-book snapshot.
- [Receive several event types](examples/feed_market/README.md) with regional symbols and an explicit Order source.
- [Work with instruments](examples/ipf_live/README.md) or [build models offline](examples/client_models/README.md).
- [Run a source, relay and clients](examples/feed_distribution/README.md) on a local QTP network.

[Current capabilities](docs/CURRENT-STATE.md) identify implemented profiles.
The independent [schedule-rules API](docs/SCHEDULE-RULES.md) parses trading hours with explicit calendar contexts.
[Developer documentation](docs/developer.md) contains build options, tools and verification results.
[Build the documentation](tools/docs/README.md) to browse the same reading paths locally.

[Event text](docs/EVENT-TEXT.md) prints supported events and values with an explicit immutable context and direct UTF-8 output.
