# Candle physical routes

[Русская версия](CANDLE-ROUTES.ru.md). English is authoritative.

`PreparedCandleRoutes` takes an immutable schema and explicit record IDs for
one receiving contract. Preparation verifies the full set of names, payload
mappings, resource bounds and unique Java delegate descriptors.

| Record name | Selection |
| --- | --- |
| `TradeHistory` | Tick period with the TradeHistory payload |
| `Trade.<period>` | A fixed period with the Candle payload |
| `Candle` | Generic fallback with the Candle payload |
| `Candle{...}` | A fixed period or a minute-price specialization |

Minute subscriptions select their price specialization, then the ordinary
minute route, then the generic route. Other periods select the fixed period
before the generic route. A fixed attribute is carried by the record identity
and removed from the physical symbol. Exchange, session and other remaining
attributes stay on that symbol. `restore()` applies the fixed attributes to
produce an owning canonical CandleSymbol for a received event.

Missing routes, invalid descriptors and exhausted bounds return structured
errors before a physical demand value exists. The caller supplies only indexed
candidates for HISTORY. The [Java route oracle](../../tools/upstream/candle-routes-oracle/README.md)
checks both directions in TICKER and HISTORY.
