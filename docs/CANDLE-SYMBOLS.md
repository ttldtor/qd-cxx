# Candle symbols

[Русская версия](CANDLE-SYMBOLS.ru.md). English is authoritative.

Include `<qd/events/candle_symbol.hpp>` and link `qdcxx_candle_values`, or use the installed
`qdcxx::feed` target. Build requirements remain C++23. Value operations require no SDK or runtime.

```cpp
using namespace org::ttldtor::qdcxx::events;
auto symbol = CandleSymbol::valueOf(
    u"AAPL&Q", CandlePeriod(15, CandleType::MINUTE), CandlePrice::BID);
auto equivalent = CandleSymbol::valueOf(u"AAPL&Q{=15Minutes,price=BID}");
// symbol.toString() == u"AAPL&Q{=15m,price=bid}"
// symbol == equivalent, with equal hashCode().
```

Getters expose base, exchange and all six keyed attribute values. Text arguments are borrowed
only for construction; results own their text. Copies/moves share immutable storage and preserve
the source. `getSymbol()`/`getBaseSymbol()` borrow nullable views; `toString()` copies.

An existing public subscription accepts the object directly:

```cpp
subscription->setSymbols({symbol, equivalent}); // One canonical desired key.
subscription->removeSymbols(equivalent);        // Removes that same key.
```

The [complete compiled consumer](../tests/conformance/candle_symbol_consumer/main.cpp)
checks these exact examples in pure and ONLY-feed installed/relocated builds. The feed case
uses an inactive supported Quote subscription with a one-symbol limit to check actual desired-map
identity. It starts no network connection. This selector bridge does not add a typed Candle event
or receiving support; the current selected projection and route rules still apply.
Null/non-ASCII or over-limit object text is rejected atomically before subscription mutation.
Existing string selectors keep exact spelling, and empty `setSymbols({})` keeps its old behavior.

Unknown attributes and already-canonical ordering can remain unchanged; do not assume a global
key sort. Explicit periods can preserve a getter that differs from reparsing their text.
Null text is representable: toString returns nullopt, Java-like equals/hashCode have documented
null-domain errors, and C++ operator== compares null values symmetrically.
See the [full contract](../spec/events/CANDLE-SYMBOL.md) for these boundaries, allocation errors,
getter lifetimes and the raw production oracle.
