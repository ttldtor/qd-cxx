# Атрибуты символа Candle

[English version](CANDLE-ATTRIBUTES.md). Английская версия нормативная.

Цель C++23 `qdcxx_candle_values` предоставляет
[`candle_attributes.hpp`](../../include/qd/events/candle_attributes.hpp) в
`org::ttldtor::qdcxx::events`. Установленная `qdcxx::feed` предоставляет API транзитивно.
Чистая цель зависит от операций market-symbol и приватного числового архива; SDK, IPF, runtime,
сеть, расчёт расписаний и настройки процесса не нужны. Композиция CandleSymbol, типизированные
события Candle, агрегация, фильтры расписаний, Java serialization и идентичность/hash Java-объектов
не входят в контракт значений.

Источник — [upstream.lock](../../upstream.lock), revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, production-классы CandlePrice, CandleSession,
CandleAlignment, CandlePriceLevel, CandleDataType, CandleExchange в
`dxfeed-api/src/main/java/com/dxfeed/event/candle/`. Все шесть присутствуют.

## Значения и ключи

| Тип | Кодирование/ключ | По умолчанию | Допустимые значения |
| --- | --- | --- | --- |
| CandlePrice | price | LAST: last | LAST last, BID bid, ASK ask, MARK mark, SETTLEMENT s |
| CandleSession | tho | ANY: false | ANY false, REGULAR true |
| CandleAlignment | a | MIDNIGHT: m | MIDNIGHT m, SESSION s |
| CandlePriceLevel | pl | NaN | Неотрицательный конечный binary64, включая +0; любой NaN канонизируется |
| CandleDataType | dt | UNDEFINED: пусто | Любой точный UTF-16; константы SOURCE source, THO tho, ALL all, RTH rth, ETH eth, UNDEFINED пусто |
| CandleExchange | Региональный суффикс, без ключа | COMPOSITE: нулевая кодовая единица | Любая кодовая единица UTF-16, без проверки площадки |

Парсеры price/session принимают непустой префикс канонического текста без учёта регистра;
alignment — только точные m/s без учёта регистра. Поэтому settlement недопустимо, s допустимо;
regular/any недопустимы, t/f допустимы. Сопоставление Java учитывает dotted/dotless I, long S и
Kelvin sign при сравнении с ASCII. Пробелы и Unicode не нормализуются. Null, пустой и неизвестный
текст перечислений дают `std::invalid_argument`; недопустимые C++ enum также отвергаются до
изменения символа.

Чтение session из символа отличается от самостоятельного парсера: лишь полное true без учёта
регистра означает REGULAR; отсутствие, пустота, префиксы, пробелы и прочие строки означают ANY.
Price/alignment разбирают присутствующее значение строго; default используется только при
отсутствии атрибута. Null-символ не содержит атрибутов.

Price level отвергает -0, отрицательные числа и обе бесконечности. Парсер использует общий Java
double-синтаксис: обрезание ASCII control/space до U+0020, decimal/hex-литералы, числовые суффиксы,
регистрозависимые NaN/Infinity, переполнение и underflow. Null/ошибка дают
`std::invalid_argument`; underflow в -0 также отвергается. Конструктор без аргументов и DEFAULT
означают NaN. Все NaN равны; payload канонизируется, как Java valueOf. Форматирование использует
integer-first политику и Java8 digits [периодов Candle](CANDLE-PERIOD.ru.md):
+2^63 превращается в 9223372036854775807, NaN — в NaN, +0 — в 0.

CandleDataType копирует вход. Null и пустота означают undefined; неизвестные значения, пробелы,
разделители, регистр и некорректный UTF-16 сохраняются. SOURCE и source различаются.
`getValue()` заимствует текст до изменения/уничтожения объекта; `toString()` возвращает копию.
CandleExchange выводит COMPOSITE для нуля, иначе одну точную кодовую единицу; парсера текста нет.
Явный региональный NUL читается как composite.

## Операции с символами и нормализация

Свободные функции перечислений: `parseCandlePrice`, `candlePriceText`, `getCandlePrice`,
`changeCandlePrice`, `normalizeCandlePrice`; для Session/Alignment используются аналогичные имена.
Три класса значений предоставляют `getAttributeForSymbol` и `changeAttributeForSymbol`;
PriceLevel/DataType также предоставляют `parse` и `normalizeAttributeForSymbol`.
Exchange создаётся из кодовой единицы.

```cpp
#include <qd/events/candle_attributes.hpp>
using namespace org::ttldtor::qdcxx::events;
auto symbol = changeCandlePrice(u"AAPL{=m}", CandlePrice::BID);
auto interval = CandlePriceLevel::parse(u"0.25");
auto grouped = interval.changeAttributeForSymbol(*symbol);
auto custom = CandleDataType(u"source-specific");
```

Смена price/session/alignment на defaults удаляет ключи. NaN price level и undefined data type
также удаляют ключи; composite exchange удаляет региональный суффикс. Остальные значения
вставляются/заменяются по существующему
[контракту market-symbol](MARKET-SYMBOLS.ru.md), включая дубликаты, malformed-вход и отсутствие
escaping. Null представлен `std::nullopt`; запись nondefault в null создаёт символ только с
атрибутами, удаление default сохраняет null. Возвращаемые символы владеют независимыми копиями.

Нормализация сохраняет особенности production-кода. Price/alignment/price-level вызывают
удаление default, но теряют результат: канонический default остаётся, псевдоним канонизируется
без удаления ключа. price=LAST становится price=last, a=M — a=m, pl=+NaN — pl=NaN.
Недопустимые значения сохраняются. Session оставляет всё, кроме полного true; TRUE становится
true. Это отличается от изменения атрибута на default. DataType действительно удаляет пустой
dt; все непустые значения остаются точными. Нормализатора Exchange нет.

## Владение, ошибки и проверка

Вход заимствуется только на время вызова. Значения и результаты не сохраняют входные views,
callbacks, runtime или отложенную работу. Текст enum — неизменяемые статические views.
Const-операции независимы; конкурентное изменение одного объекта требует внешней синхронизации.
Работа/хранение линейны по тексту, скрытого глобального лимита нет; числовой engine ограничен
binary64. Ошибки allocation/length проходят наружу, не превращаясь в синтаксическую ошибку или
игнорируемую ошибку normalizer. Некорректный диапазон подстроки даёт `std::out_of_range`.

[Production Java8 oracle](../../tools/upstream/candle-attributes-oracle/README.ru.md) проверяет все
одиночные кодовые единицы трёх парсеров и exchange, null/malformed-символы, defaults, псевдонимы,
недопустимый ввод, числовые границы и детерминированные binary64. Native-тест читает все
[265722 наблюдения](../../tests/fixtures/events/candle/attributes-oracle.tsv) с
[provenance](../../tests/fixtures/events/candle/attributes-provenance.json), отдельно проверяет
владение и invalid enums. Прежний fixture периода остаётся регрессией общего rendering.
UTC и Pacific/Honolulu должны совпадать. Рабочие build/package receipts не означают приём Candle.

Канонические константы DataType и NaN default PriceLevel имеют constant initialization: их можно безопасно читать из динамического initializer другого translation unit. Входные custom-строки по-прежнему копируются; возвращаемый текст независим от исходного ввода.
