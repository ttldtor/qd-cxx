# Рыночные перечисления, исходные флаги и преобразования значений событий

[English version](EVENT-VALUES.md). Английская версия является определяющей.

Устанавливаемая цель `qdcxx_event_values` предоставляет чистые синхронные операции над значениями в
`org::ttldtor::qdcxx::events`. Заголовки: `market_enums.hpp`, `event_flags.hpp`, `time_helpers.hpp`,
`short_string.hpp`, `event_value.hpp` в `qd/events/`. Зависимостей от SDK, транспорта, часового пояса,
расписаний, загрузки схем и свойств процесса нет. Входы передаются по значению либо заимствуются только
на время вызова; ссылки, callback и состояние не сохраняются. Параллельные вызовы независимы.
Декодированная строка владеет данными и имеет обычное время жизни значения. Существующие цели
`qdcxx_trade`, `qdcxx_profile` и совместимая цель пакета `qdcxx::feed` транзитивно предоставляют компонент.
Прежние имена getters Trade/Profile и включения заголовков сохраняются.

## Бизнес-перечисления и неизвестные значения

Все двенадцать production business enums рынка имеют фиксированное представление `std::int32_t`:

| Тип | Явные коды, начиная с нуля | Размер Java-таблицы | Проекция резервного элемента |
| --- | --- | --- | --- |
| Direction | UNDEFINED, DOWN, ZERO_DOWN, ZERO, ZERO_UP, UP | 8 | UNDEFINED |
| TradingStatus | UNDEFINED, HALTED, ACTIVE | 4 | UNDEFINED |
| ShortSaleRestriction | UNDEFINED, ACTIVE, INACTIVE | 4 | UNDEFINED |
| Side | UNDEFINED, BUY, SELL | 4 | UNDEFINED |
| Scope | COMPOSITE, REGIONAL, AGGREGATE, ORDER | 4 | Резервных элементов нет |
| OrderAction | UNDEFINED, NEW, REPLACE, MODIFY, DELETE_ORDER, PARTIAL, EXECUTE, TRADE, BUST | 16 | UNDEFINED |
| IcebergType | UNDEFINED, NATIVE, SYNTHETIC | 4 | UNDEFINED |
| PriceType | REGULAR, INDICATIVE, PRELIMINARY, FINAL | 4 | Резервных элементов нет |
| OtcMarketsPriceType | UNPRICED, ACTUAL, WANTED | 4 | UNPRICED |
| TimeAndSaleType | NEW, CORRECTION, CANCEL | 4 | NEW |
| AuctionType | UNDEFINED, OPENING, CLOSING, REOPENING, OTHER | 8 | UNDEFINED |
| ImbalanceSide | UNDEFINED, BUY, SELL, NO_IMBALANCE, INSUFFICIENT_ORDERS, PAUSED | 8 | UNDEFINED |

`marketEnumFromCode<E>(code)` сохраняет **любой** signed32-код, включая отрицательный, резервный и
выходящий за таблицу. `marketEnumCode(value)` возвращает его точно. `isKnownMarketEnum(value)` проверяет,
обозначает ли исходное значение явный enumerator. Эти операции не выделяют память и не бросают исключения.
Concept `MarketEnum` допускает ровно двенадцать типов выше; прочие enum отвергаются при компиляции.

`marketEnumValue(value)` отдельно воспроизводит Java `valueOf(int)`: именованный код возвращается сам,
резервный элемент таблицы даёт указанный sentinel, отрицательный или выходящий за таблицу код даёт
`std::unexpected(EventValueError::INVALID_ENUM_CODE)`. В Java возникает исключение границ массива;
нативный API использует типизированный проверяемый результат. Sentinel-проекция не сохраняет raw-код.
Для кодирования и изучения неизвестных входов храните исходное значение. Trade/Profile по-прежнему хранят
полные исходные `flags` и возвращают прежние sentinel-проекции, не перезаписывая флаги.

```cpp
#include <qd/events/market_enums.hpp>
namespace events = org::ttldtor::qdcxx::events;
const auto raw = events::marketEnumFromCode<events::Direction>(7);
const auto projected = events::marketEnumValue(raw); // Direction::UNDEFINED
const auto preserved = events::marketEnumCode(raw); // 7
```

Новый native token `OrderAction::DELETE_ORDER` имеет Java-код `DELETE`, равный 4. Написание избегает макроса Windows SDK `DELETE`, не удаляя и не меняя его; бизнес-имя Java остаётся `DELETE`. Installed Windows consumer включает все пять новых заголовков после `Windows.h` и проверяет сохранение макроса и числового кода.

## Битовые поля и indexed-маски

Перегрузки signed32/signed64 `getBits(flags, mask, shift)` и `setBits(flags, mask, shift, bits)`
воспроизводят EventUtil. Извлечение применяет арифметический сдвиг вправо, затем несдвинутую маску.
Замена маскирует новые биты и сохраняет каждый бит вне `mask << shift`, включая неизвестные и знаковый.
Маски допускают произвольные signed-шаблоны. Дистанция использует младшие пять/шесть бит, в том числе
для отрицательных и больших shift, точно как Java. Вышедшие биты отбрасываются. Расчёты используют
определённую unsigned-арифметику, без signed overflow или недопустимых сдвигов C++.

`indexed_event_flags` предоставляет TX_PENDING=0x01, REMOVE_EVENT=0x02, SNAPSHOT_BEGIN=0x04,
SNAPSHOT_END=0x08, SNAPSHOT_SNIP=0x10, SNAPSHOT_MODE=0x40. Это маски транзакций и снимков indexed-событий,
отдельные от бизнес-флагов конкретных событий и метаданных QTP. Константы сами по себе не обрабатывают
транзакции или снимки и не создают mapper indexed-событий.

## Время, упакованный sequence и day ID

`getNanosFromMillisAndNanoPart(millis, part)` возвращает младшие signed64-биты
`millis * 1000000 + part`. Допустимы любые signed64 миллисекунды и signed32 part; части вне 0..999999
сохраняются арифметически. `getMillisFromNanos` и `getNanoPartFromNanos` используют математические
floor division/modulus: -1ns разбивается на -1ms и 999999ns. Композиция оборачивается на границах
signed64; остаток разбиения всегда неотрицателен.

`getSecondsFromTime(millis)` округляет вниз до секунд и насыщает до signed32.
`getMillisFromTime(millis)` независимо возвращает остаток 0..999, даже при насыщении секунд.
Это числовые функции TimeUtil, без его функций часовых поясов и суточного времени.

Упакованное слово time/sequence содержит signed32 секунды в high32, десять бит миллисекунд в битах31..22
low32 и low22 sequence. `getTimeFromTimeSequence` учитывает все десять бит, даже 1000..1023;
`getSequenceFromTimeSequence` возвращает 0..MAX_EVENT_SEQUENCE (4194303). `withTimeSequenceTime` применяет
насыщенные секунды/floor remainder и сохраняет прежний low22 sequence. `withTimeSequenceSequence`
заменяет только low22, сохраняя raw time и резервные шаблоны миллисекунд, либо возвращает
INVALID_SEQUENCE вне 0..4194303. Уникальность и монотонность не гарантируются. Это совместимые с
TradeBase упакованные бизнес-значения, не FIELD event metadata и не индексы заявок.

`getDayIdByYearMonthDay(year, month, day)` и перегрузка signed packed-date воспроизводят DayUtil.
Используется proleptic Gregorian с астрономическим нулевым годом и signed32 year/day. Проверяется только
месяц: 1..12 допустимы; остальные дают INVALID_MONTH. Нулевые, отрицательные и выходящие за обычную
длину месяца дни арифметически нормализуются с правилами високосных лет и столетий. Например,
2000-02-30 равно 2000-03-01. Упакованная форма разбирает
`yearSign * (abs(year)*10000 + month*100 + day)` (+1 для неотрицательного года, включая ноль) через Java signed decimal division и отрицание,
включая переполнение signed minimum. Locale, часовой пояс и торговый календарь не интерпретируются.

`getYearMonthDayByDayId(dayId)` принимает любой signed32 счётчик дней от 1970-01-01 и возвращает signed
packed Gregorian date production-алгоритмом мартовских циклов. Все промежуточные Java int wraps явно
воспроизведены в обоих направлениях. Крайние значения могут переполнить упакованное представление или
смещение эпохи обратного алгоритма; сохраняется совместимое числовое значение, без обещания
round-trip корректной гражданской даты. Произвольное ограничение года не вводится.

## Бизнес-преобразование SHORT_STRING

`encodeShortString(optional<u16string_view>)` принимает null или максимум восемь UTF-16 units,
каждая 0..0xFF. Сначала проверяется **длина**: превышение даёт SHORT_STRING_TOO_LONG, иначе unit выше
0xFF даёт INVALID_SHORT_STRING_CHARACTER. Нулевые units учитываются в длине и пропускаются в
упакованных байтах. Null, пустой и полностью нулевой вход кодируются в ноль. Ненулевые units
упаковываются в исходном порядке, включая старший знаковый бит; память не выделяется.

`decodeShortString(int64)` удаляет все нулевые байты полного raw64 и возвращает null для нуля, иначе
независимо владеющую строку из одной–восьми UTF-16 units со значениями байтов. Это не UTF-8.
Кэша нет; выделение памяти может бросить `std::bad_alloc` или `std::length_error`. Optional view не
сохраняется. Прежняя wire identity SHORT_STRING остаётся 40, а field-level value остаётся signed low32,
как в [FIELD-TYPES](../encoding/FIELD-TYPES.ru.md). Full64 helper не расширяет поле. Sign extension
отрицательного raw32 виден при передаче полному64 decoder; реальные standard-field witnesses сохраняют
это поведение. Нового wire type, неявного преобразования поля и typed TimeAndSale mapper нет.

## Production-доказательства и использование потребителем

[Production oracle](../../tools/upstream/event-values-oracle/README.ru.md) вызывает точно закреплённые
[EventUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/impl/EventUtil.java),
[TimeNanosUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/impl/TimeNanosUtil.java),
[DayUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/DayUtil.java),
[TimeUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java),
все двенадцать enum tables, IndexedEvent constants и Trade time/sequence setters/getters. Также
используются parse/render настоящего SHORT_STRING поля DXFeedScheme TimeAndSale.ExchangeSaleConditions.
[Контракт захвата](../../tests/fixtures/event-values/CAPTURES.ru.md) различает full64 helpers и signed32
стандартное поле. Наблюдения UTC и Pacific/Honolulu должны совпадать побайтно.

[Отдельный installed consumer](../../tests/conformance/event_values_consumer/main.cpp) проверяет публичные
операции и ошибки входов; его CMake-проект отдельно компилирует каждый новый заголовок. Линкуйте
`qdcxx_event_values` либо существующую installed-цель `qdcxx::feed`, когда выбран её компонент.
Приватные заголовки и пути к исходникам не требуются. Native conformance сравнивает каждый результат
с настоящими Java-наблюдениями; результаты платформ, sanitizer, установки и перемещения являются
отдельными выполненными проверками.
