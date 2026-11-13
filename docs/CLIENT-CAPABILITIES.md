# Client capabilities and wire profiles {#client-capabilities}

[Русская версия](CLIENT-CAPABILITIES.ru.md). English is authoritative.

Use this reference to choose a receiving contract, physical record and source before creating a typed subscription. Each row gives the documented scope and exact CTest identifiers for deterministic or pinned Java fixture checks. Build and run the tests on the configuration you are qualifying. A listed test identifies a verification path. Its result depends on the source revision and build configuration.

## Typed receiving routes

`TICKER` retains current values, `STREAM` preserves admitted record order and `HISTORY` preserves bounded indexed transactions and range coverage. A physical record name in this matrix identifies the negotiated record family. Numeric record IDs belong to the selected schema. The exact defaults, field types and error behavior belong to each event's mapping contract.

| Event | Contract and boundary | Deterministic checks |
| --- | --- | --- |
| `Quote` | TICKER and STREAM. Composite `Quote`, regional `Quote&Q` with logical `AAPL&Q` and physical `AAPL` when the registry contains that route. Owning bid/ask values and source exchange fields. HISTORY admission is outside this route. | `feed_api`, `event_time_feed`, `event_time_routing`, `quote_java_values`, `quote_java_tape` |
| `Profile` | TICKER and STREAM `Profile`. Explicit registry can bind a reversed caller field layout and aliases. Logical regional Profile can retain its suffix on a composite record. | `feed_api`, `event_registry_feed`, `profile_mapping_contract` |
| `Trade` | TICKER and STREAM `Trade`, with `Trade&Q` through an explicit compatible registry. Preserves signed packed time, flags and business conversion of LONG values. | `trade_feed`, `trade_java`, `event_field_conversions_feed` |
| `TradeETH` | TICKER and STREAM `TradeETH`, with `TradeETH&Q` through an explicit registry. Nano comes from `Last.TimeNanoPart`. The ETH flag can clear on the transition to regular hours. | `trade_eth_feed`, `trade_eth_java` |
| `Summary` | TICKER and STREAM `Summary`. Explicit `Summary&Q` may omit open interest. The composite adapter keeps signed day IDs and full-width open interest. | `summary_feed`, `summary_java` |
| `TextMessage` | STREAM `TextMessage`, with UTF-16 ownership and absent versus empty text. TICKER registration is refused. | `text_events_feed`, `text_events_java_oracle` |
| `TextConfiguration` | TICKER and STREAM `TextConfiguration`. A mapped version governs TICKER admission and late replay. STREAM keeps admitted records in order. | `text_events_feed`, `text_events_java_oracle` |
| `Message` | STREAM `Message`, full field identity 249 (`SERIAL_OBJECT`). Null and engaged empty body remain distinct. Native decoding requires an explicitly registered codec. | `misc_feed`, `attachment_serialized_contract`, `attachment_codec_contract` |
| `Configuration` | TICKER and STREAM `Configuration` with serialized bytes and optional Version. First negative version yields the Java default-zero TICKER view. Lower versions leave the retained value intact. | `misc_feed`, `misc_event_projections`, `attachment_serialized_contract` |
| `Candle` | TICKER and HISTORY through `Candle` and `TradeHistory`, including an explicit fixed bid-minute route. STREAM is refused. The Feed receives candles from compatible records and supplies no missing periods. | `candle_feed`, `candle_routes`, `candle_payload` |
| `Greeks` | TICKER and ranged HISTORY `Greeks`, DEFAULT source. Explicit tiny-decimal schema and independent FIELD timestamp are covered by the event path. | `greeks_feed` |
| `Underlying` | TICKER and ranged HISTORY `Underlying`, DEFAULT source. The available call/put values govern option volume and NaN handling. | `underlying_feed` |
| `TheoPrice` | TICKER and ranged HISTORY `TheoPrice`, DEFAULT source. HISTORY needs physical Time and Sequence and replays the range on reconnect. | `theo_price_feed` |
| `TimeAndSale` | STREAM and HISTORY `TimeAndSale` and built-in regional routes. HISTORY preserves correction, cancel, removal, sequence and snapshot coverage across reconnect. | `time_and_sale_feed`, `sale_lifecycle_feed` |
| `OptionSale` | HISTORY `OptionSale`. Signed index, business time and FIELD time are independent. Removal and transactions retain their own semantics. | `option_sale_feed`, `sale_lifecycle_feed` |
| `Series` | HISTORY `Series`, including literal `AAPL&Q` and DEFAULT source. Preserves signed index and snapshot/transaction flags. | `series_feed`, `indexed_schema_java_current` |
| `NuamTrade` | Composite `NuamTrade` on TICKER and STREAM. Exchange statistics update independently of common Trade values. | `nuam_trade_feed`, `nuam_trade` |
| `NuamTimeAndSale` | Composite `NuamTimeAndSale` on STREAM and HISTORY, with `matchId` and HISTORY transaction/removal flags. Regional delivery requires explicit registration. | `nuam_time_and_sale_feed` |
| `Order` | HISTORY native `Order` (DEFAULT and explicit `Order#...`, including the exact `#NTV` case). The record suffix determines source identity. TICKER Quote-derived and HISTORY MarketMaker-derived orders are separate projections. | `order_feed`, `order_route`, `derived_order_feed`, `derived_order_oracle` |
| `MarketMaker` | HISTORY `MarketMaker` with native fields and a separately selected aggregate Order projection. Its aggregate coordinates and flags belong to that projection. | `derived_order_feed`, `derived_order_oracle` |
| `SpreadOrder` | HISTORY `SpreadOrder`. Retains the underlying symbol and a separate nullable `spreadSymbol`. Ordinary TICKER/STREAM registration is refused. | `order_variants_feed` |
| `AnalyticOrder` | HISTORY `AnalyticOrder`. Complete raw iceberg flags are independent of indexed flags. Explicit `AnalyticOrder#NTV` is eligible for mapping under the configured schema even when that source's publication capability is absent. | `order_variants_feed` |
| `OtcMarketsOrder` | HISTORY `OtcMarketsOrder` and `OtcMarketsOrder#pink`. Preserves signed quote access payment, packed source/index and independent OTC flags. | `auction_order_feed` |
| `OrderImbalance` | TICKER `OrderImbalance`, `#NTV`, `#ARCA`, `#NUAM`. The same symbol can have one value per source. HISTORY/STREAM registration is refused. | `auction_order_feed` |
| `NuamOrder` | HISTORY `NuamOrder`, `NuamOrder#NUAM`, `NuamOrder#nuam`. The two non-default sources keep case-sensitive identities and exact indexed values. | `nuam_order_feed` |

## Symbols, sources and values

| Capability | Contract and boundary | Deterministic checks |
| --- | --- | --- |
| Market symbol and exchange | A market symbol value preserves the logical exchange suffix `&Q`. `MarketEventSymbols` parses and changes an exchange or attribute without normalizing unrelated text. The actual regional record decides whether the physical symbol loses that suffix. | `market_event_symbols`, `event_time_routing` |
| CandleSymbol and period | Parse and render period, price, alignment, session, exchange and price-level attributes. Canonical defaults, invalid attributes and the tick `TradeHistory` choice have separate witnesses. | `candle_symbol_values`, `candle_attribute_values`, `candle_period_values`, `candle_routes` |
| Indexed sources | `OrderSource` and `IndexedEventSource` retain identity and case. DEFAULT, explicit `#NTV`, synthetic sources and source publication flags have separate meanings. An unknown or incompatible physical route can refuse admission. | `event_sources`, `order_source_capabilities`, `order_route` |
| Decorated selectors and filters | Exact, decorated and wildcard logical selectors have explicit profile/type rules. Physical registry context governs the route, and disabled or conflicting admission leaves the accepted intent intact. | `subscription_selectors`, `selector_profile`, `selector_feed`, `feed_filter`, `feed_filter_config` |
| Event time and raw values | Independent FIELD time, unspecified event time, packed business time, signed days, SHORT_STRING, full raw flags and nullable fields retain distinct semantics. Exact wire type and bounded copies are checked before projection. | `event_values_java`, `event_time_java`, `event_time_feed`, `event_field_conversions_java`, `configured_java_events`, `configured_event_fixture_negatives` |
| Event text | Java-compatible event text uses an explicit time context. Rounding, allocation refusal and Summary's width difference have dedicated checks. The display formatter is separate from canonical CandleSymbol rendering. | `event_text`, `event_text_summary`, `event_text_rounding`, `event_text_allocations` |
| Immutable event registry | Custom immutable schema, exact record ID, field aliases and type identity. Incompatible duplicate routes, context mismatch, malformed shape and over-limit receiving profiles refuse before native activation. | `event_registry`, `event_registry_feed`, `selector_profile` |

## Queries, subscriptions and models

| Capability | Contract and boundary | Deterministic checks |
| --- | --- | --- |
| TICKER retained read | `getLastEvent` and `getLastEventIfSubscribed` read owning current values and distinguish no exact demand, defaults and cached data. The passive path works on eligible local TICKER tape, including without an application listener. | `feed_api`, `tape_feed` |
| TICKER single request | `requestLastEvent` can add a temporary exact TCP interest. Cancellation, deadline, clear, reconnect and close settle the future and retire private demand. Tape requests report `UNSUPPORTED_PROFILE`. | `feed_api`, `tape_feed` |
| TICKER batch request | `requestLastEvents` preserves duplicate-symbol positions and successful cache hits when a shared miss cannot be admitted. Clear or reconnect affects only unfinished children. Pending tape requests settle before file activation. | `feed_api`, `tape_feed` |
| HISTORY subscription and observation | `getHistorySnapshot()` returns owning status and coverage metadata for the committed range. Event payloads arrive separately through ordinary HISTORY subscription delivery, including compatible tape. Incomplete and truncated coverage remain explicit. | `history_feed`, `tape_feed` |
| HISTORY indexed snapshot | `requestIndexedEvents` and `getIndexedEventsIfSubscribed` require one exact native route by type, symbol and source. Synthetic and ambiguous routes are outside this query profile. TCP results distinguish `NOT_SUBSCRIBED`, `INCOMPLETE`, proved empty and truncated snapshots. Both methods refuse tape with `UNSUPPORTED_PROFILE`. | `feed_api`, `tape_feed` |
| HISTORY time-series snapshot | `requestTimeSeries` and `getTimeSeriesIfSubscribed` use inclusive business milliseconds, including nonzero upper-bound sequences. Results own events in business-time/index order. Reversed intervals, exhausted limits and insufficient coverage have explicit outcomes. | `time_and_sale_feed`, `time_series_query_policy`, `feed_api` |
| Concurrent HISTORY operations | Independent requests can share exact wire demand within the endpoint's slot limit. Cancellation settles only its own future. Retiring registrations remain charged. Automatic recovery fences unfinished snapshots from the old source epoch. | `feed_api`, `time_and_sale_feed` |
| Independent subscriptions | A typed subscription may attach to several feeds. Detach affects one attachment generation. Detach-and-clear schedules one ordered clear after its admitted delivery and preserves the other feed. | `feed_api`, `callback_execution` |
| Lifecycle and diagnostics | State waits, listener removal, callback exception isolation, callback-aware close, bounded drain and statistics use existing runtime ownership. An endpoint or subscription snapshot does not keep native transport alive. | `feed_api`, `callback_execution`, `endpoint_configuration` |
| Indexed event model | Owning indexed snapshots track source/index, removal and transaction/snapshot completion with finite storage. | `indexed_event_model` |
| Time-series event model | Owning time-series snapshots retain exact coordinates across range changes and apply qualified HISTORY replay after reconnect. | `time_series_event_model`, `sale_lifecycle_feed` |
| Order-book model | Applies qualified indexed updates to owning bid/ask snapshots under explicit model limits. Scope is local application-side book reconstruction. | `order_book_model`, `client_models_values` |
| Instrument and IPF values | Portable instrument profiles, local parsing/writing and collector snapshots preserve exact fields, typed values and owned revisions. Parsing and malformed input outcomes are exercised independently. | `ipf_values`, `ipf_reader`, `ipf_writer`, `ipf_collector` |
| Instrument helpers | Glossary, price increments and option chains use bounded value APIs, with production Java fixtures where linked by the corresponding tests. | `glossary`, `price_increments`, `option_chains` |
| Schedules | Calendar/rule values and session queries use the optional pinned project time-zone data. Unknown zones and unsupported queries use the documented error outcomes. | `schedule_rules`, `schedule_queries` |

## Source and package profiles

| Source or build profile | Contract and boundary | Deterministic checks |
| --- | --- | --- |
| Plain QTP TCP | Endpoint receives negotiated, schema-bound TICKER, STREAM and HISTORY frames under an explicitly selected contract. Independent FIELD timestamps come from frames that carry them. Source/callback limits and recovery fences apply. | `protocol_live_session`, `event_time_feed`, `feed_api`, `history_feed` |
| Verified QTP TLS | Optional controlled static SDK and explicit trust/peer identity. Verified Quote passes. Untrusted chain, wrong SAN, insufficient resource allowance, local tape and plaintext OnDemand are refused. TLS-disabled builds refuse TLS settings with `UNSUPPORTED_PROFILE`. | `feed_tls`, `transport_tls_recovery`, `transport_tls_resources` |
| QTP Basic over verified TLS | Provider output can be adopted only for the verified current challenge and connection revision. Application DATA is admitted after acknowledgement. Reconnect can use rotated credentials. A second challenge after authorization is terminal (`tls-authentication-denied`). | `feed_tls`, `protocol_live_session` |
| Local binary Feed tape | One regular binary file, compatible header or self-described headerless input and inline NONE, MESSAGE or FIELD time. Contract-specific DATA and RAW_DATA have separate routing. Pause, pacing, restart and stop preserve epoch fences. Feed refuses text/blob, compression and companion time in this source profile. | `tape_feed`, `protocol_binary_tape`, `raw_tape_java_conformance` |
| OnDemand loopback replay | Explicit IPv4 loopback, one finite uncompressed composite Quote response, Java QD v2 request envelope. Covers initial catch-up, pause/rate, EOF, explicit clear and handover to ordinary QTP. Invalid framing or absent start coverage admits no historical epoch. | `on_demand_feed` |
| External live OnDemand provider | Excluded from this qualification scope. Loopback HTTP and Java envelope evidence concerns the deterministic fixture. | Approved scope exclusion |
| IPF transfer and connection | Local regular file and bounded HTTP, with optional controlled HTTPS. Transport, gzip and IPF completion are separate. The connection publishes owning revisions and preserves a completed prefix when later native retirement fails. | `ipf_transfer_control`, `ipf_transfer`, `ipf_compression`, `ipf_connection`, `ipf_connection_control` |
| Independent install | Installed `qdcxx_feed` and separately built examples exercise exported targets on Windows MSVC and Linux GCC. A relocated consumer and a TLS-enabled or disabled dependency graph require their own package checks. | [Package build](CPP-BUILD.md) |
| Client samples | Offline fixture paths cover the market Feed, queries and local models. Example help runs without opening a connection. | `feed_demo_help`, `feed_market_help`, `feed_market`, `client_queries_help`, `client_queries_loopback`, `client_models_help`, `client_models_values` |

## Qualification limits

Each event row describes the routes exercised by its listed evidence. Cross-products of every custom schema, source, transport and event type are outside those results. A missing fixture or unexecuted configuration leaves the corresponding qualification open. Complete compatibility with arbitrary QD peers, general historical providers or binary tape containers is outside the profiles documented here.

For concrete API calls, see the [Feed API](FEED-API.md). The [receiving registry](../spec/events/EVENT-REGISTRY.md) describes schema identity and route admission. The [binary tape coverage reference](../spec/protocol/BINARY-TAPE-COVERAGE.md) describes the separate offline decoder and inspector.
