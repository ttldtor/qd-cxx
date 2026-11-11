# Символы Candle

[English version](CANDLE-SYMBOLS.md). Английская версия нормативная.

Подключите `<qd/events/candle_symbol.hpp>` и `qdcxx_candle_values` либо установленную
`qdcxx::feed`. Требуется прежний C++23; операциям значений не нужны SDK/runtime.
[Mapper полей Candle](../spec/events/CANDLE-PAYLOAD.ru.md) принимает разрешённый CandleSymbol.

```cpp
using namespace org::ttldtor::qdcxx::events;
auto symbol = CandleSymbol::valueOf(
    u"AAPL&Q", CandlePeriod(15, CandleType::MINUTE), CandlePrice::BID);
auto equivalent = CandleSymbol::valueOf(u"AAPL&Q{=15Minutes,price=BID}");
// symbol.toString() == u"AAPL&Q{=15m,price=bid}"
// symbol == equivalent, with equal hashCode().
```

Getters предоставляют base/exchange и шесть keyed-атрибутов. Текст заимствуется лишь при создании;
результат владеет копией. Copy/move разделяют immutable storage и сохраняют исходный объект.
`getSymbol()`/`getBaseSymbol()` заимствуют nullable views, `toString()` копирует.

Прежняя публичная подписка принимает объект прямо:

```cpp
subscription->setSymbols({symbol, equivalent}); // One canonical desired key.
subscription->removeSymbols(equivalent);        // Removes that same key.
```

[Полный компилируемый consumer](../tests/conformance/candle_symbol_consumer/main.cpp) проверяет
эти примеры в pure и ONLY-feed installed/relocated сборках. Feed-случай использует inactive
поддерживаемую Quote-подписку с лимитом1 для реальной проверки desired-map identity, без
сетевого соединения. Selector bridge не добавляет типизированный Candle event или его приём;
действуют прежние выбранные projection и route rules. Null/non-ASCII/over-limit text отвергается
атомарно до изменения подписки. Строковые selectors сохраняют точное написание; пустой
`setSymbols({})` сохраняет прежнее поведение.

Неизвестные атрибуты и уже канонический порядок могут сохраняться; глобальная сортировка
не гарантируется. Явный period может сохранить getter, отличающийся от повторного разбора текста.
Null представим: toString даёт nullopt, Java-подобные equals/hashCode имеют описанные null-domain
ошибки, а C++ operator== сравнивает null симметрично. См.
[полный контракт](../spec/events/CANDLE-SYMBOL.ru.md): границы, allocation errors, lifetime getters
и raw production oracle.
