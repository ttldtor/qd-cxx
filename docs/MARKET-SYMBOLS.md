# Parse and compose market symbols

[Русская версия](MARKET-SYMBOLS.ru.md). English is authoritative.

Use the independent `qdcxx_market_event_symbols` target for symbol-only code,
or link `qdcxx::feed` when using the optional installed Feed component. Feed
exports the helper transitively. The helper itself needs no SDK, socket or startup.

```cmake
find_package(qdcxx CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE qdcxx_market_event_symbols)
```

```cpp
#include <qd/events/market_event_symbols.hpp>

namespace symbols = org::ttldtor::qdcxx::events::market_symbols;
auto regional = symbols::changeExchangeCode(u"SPY", u'Q'); // owns SPY&Q
auto base = symbols::getBaseSymbol(u"SPY&Q{price=bid}"); // owns SPY
auto changed = symbols::changeBaseSymbol(u"SPY&Q{price=bid}", u"IBM");
// changed owns IBM&Q{price=bid}; attribute text is preserved literally.
```

Pass `std::nullopt` for Java null; `u""` is an empty string. Borrowed inputs are
not retained and returned strings own their storage. Allocations can throw
`std::bad_alloc`/`std::length_error`. There is no implicit UTF-8 conversion.
`hasExchangeCode` distinguishes an explicit NUL code from absence even though
`getExchangeCode` returns zero in both cases.

The helpers recognize one UTF-16 unit after `&`; they do not validate that unit
against `SUPPORTED_EXCHANGES` or map it to a venue. The default code list is
`DEFAULT_EXCHANGES`. A malformed symbol is transformed according to the
[exact boundary rules](../spec/events/MARKET-SYMBOLS.md), not rejected.
Attribute normalization, Candle symbols and regional Feed selector integration
are separate capabilities; this API alone does not subscribe to regional records.
