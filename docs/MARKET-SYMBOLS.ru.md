# Разбор и композиция рыночных символов

[English](MARKET-SYMBOLS.md). Английская версия является нормативной.

Для работы только с символами используйте независимую цель `qdcxx_market_event_symbols`
или линкуйте `qdcxx::feed` при использовании установленного необязательного компонента Feed.
Feed экспортирует помощники транзитивно. Самому помощнику не нужны SDK, сокеты или запуск.

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

Для Java null передавайте `std::nullopt`; `u""` — пустая строка. Заимствованные входы
не сохраняются, возвращаемые строки владеют хранилищем. Выделение памяти может
выбросить `std::bad_alloc`/`std::length_error`. Неявного преобразования UTF-8 нет.
`hasExchangeCode` отличает явный NUL от отсутствия кода, хотя `getExchangeCode`
в обоих случаях возвращает ноль.

Помощники распознают один элемент UTF-16 после `&`; они не проверяют его по
`SUPPORTED_EXCHANGES` и не связывают с площадкой. Список кодов по умолчанию —
`DEFAULT_EXCHANGES`. Некорректный символ преобразуется по
[точным правилам границ](../spec/events/MARKET-SYMBOLS.ru.md), а не отвергается.
Нормализация атрибутов, символы Candle и интеграция региональных селекторов Feed —
отдельные возможности; этот API сам по себе не подписывает на региональные записи.
