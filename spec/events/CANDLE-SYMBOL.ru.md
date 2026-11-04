# Значения символов Candle

[English version](CANDLE-SYMBOL.md). Английская версия нормативная.

Цель C++23 `qdcxx_candle_values` предоставляет
[`candle_symbol.hpp`](../../include/qd/events/candle_symbol.hpp) в
`org::ttldtor::qdcxx::events`; установленная `qdcxx::feed` предоставляет её транзитивно.
[Руководство](../../docs/CANDLE-SYMBOLS.ru.md) и полный установленный consumer используют этот API.
Типизированные события Candle, агрегация, расчёт расписаний и приём Candle не добавляются.

Источник — [upstream.lock](../../upstream.lock), revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, production
`dxfeed-api/src/main/java/com/dxfeed/event/candle/CandleSymbol.java` и семь типов атрибутов.
Сохраняются контракты [периодов](CANDLE-PERIOD.ru.md), [атрибутов](CANDLE-ATTRIBUTES.ru.md)
и [market-symbol](MARKET-SYMBOLS.ru.md).

## Создание и нормализация

`CandleSymbol::valueOf(optional UTF-16)` копирует вход, нормализует его и инициализирует getters
base/exchange/price/session/period/alignment/price-level/data-type. Перегрузка со списком принимает
`CandleAttribute` — замкнутый variant CandleExchange, CandlePrice, CandleSession, CandlePeriod,
CandleAlignment, CandlePriceLevel, CandleDataType. Variadic-перегрузка удобна для тех же типов;
custom-строки CandleDataType остаются открытым набором.

Порядок нормализации: price, session, period, alignment, price level, data type. Exchange остаётся
синтаксическим и не нормализуется. Каждый проход использует прежний market-symbol helper:
изменённый известный атрибут может заменить дубликаты и вставить каноническую запись. Отдельной
глобальной сортировки, проверки/дедупликации неизвестных ключей и escaping нет. Уже канонические
значения, неизвестные и регистрозависимые ключи, malformed-блоки и порядок могут сохраниться.
Например `A{z=1,a=m}` и `A{a=m,z=1}` остаются разными идентичностями. Фактический алгоритм
приоритетнее общего описания сортировки в Java Javadoc.

Потерянный результат удаления default распространяется и на period: `A{=1T}` становится
`A{=t}`, сохраняя явный ключ. Чтение session из символа распознаёт только полное true.
Undefined data type действительно удаляет пустой dt. Недопустимое известное значение может
пережить normalizer, но создание затем отвергнет его при заполнении оставшихся getters.
Неизвестные атрибуты сохраняются.

Factory с атрибутами сначала применяет все изменения по порядку, нормализует текст, затем
сохраняет явные getters и читает из текста лишь остальные. Повтор одного вида, даже равный,
даёт `std::invalid_argument`. Некорректные enum и valueless C++ variant также отвергаются.
Ошибка factory не публикует частичного значения.

Явные getters существенны: период -0 tick выводится как `A{=0t}`, сохраняя -0 в getter;
обычный разбор того же текста даёт +0. Явный бесконечный/NaN period может создать символ,
текст которого без этого явного периода не разбирается. Бесконечный дневной период даёт
`A{=Infinityd}` и бесконечный getter; обычный valueOf этого текста бросает исключение.
Равенство и hash используют канонический текст, а не набор getters. Особенности source
не исправляются предположениями.

## Идентичность, null и владение

`getSymbol()`/`getBaseSymbol()` возвращают заимствованные optional UTF-16 views,
`toString()` — независимую владеющую optional-копию. Null сохраняется, имеет null base и
default getters, пока явные изменения не создадут текст. Пустота и null различаются.

`hashCode()` вычисляет Java String signed32 UTF-16 hash с unsigned wraparound и множителем31.
Для null бросается `std::invalid_argument`, безопасно представляя Java null-domain failure.
`equals(other)` сравнивает точный текст при nonnull слева; при null слева бросает исключение,
кроме сравнения того же C++ объекта с собой. Nonnull слева и null справа дают false.
Это наблюдаемые Java-подобные правила equals/hashCode.

`operator==` — явная C++ convenience adaptation: тотальное симметричное равенство optional-текста;
два null равны. Для null это не объявляется точным паритетом Java equals.
Специализации std::hash и неявного преобразования в строку нет.

Значения разделяют неизменяемое владеющее хранилище. Копирование и move не выделяют память;
после move исходный объект также пригоден. Присваивание целиком заменяет shared-значение,
без частичных обновлений полей. Views/references гарантированы до замены/уничтожения этого
объекта; другая копия может сохранить хранилище дольше. Входные строки и storage атрибутов
не удерживаются. Const-чтение независимых значений допускает конкуренцию; изменение/уничтожение
одной оболочки требует обычной синхронизации. Проверяется factory в global initializer с
прежними constant-initialized атрибутами/периодами. Mutable cache, настройки процесса, SDK и
скрытое продвижение в операциях значений отсутствуют.

Allocation/capacity ошибки проходят как `std::bad_alloc`/`std::length_error`; malformed-диапазоны
market-symbol — как `std::out_of_range`. Работа — шесть проходов нормализации плюс явные
изменения; хранение — владеющие symbol/base и явные строковые значения. Глобального лимита
ввода и неявной сетевой работы нет.

## Связь с идентичностью подписки

Прежняя `Subscription<Event>` принимает один CandleSymbol в addSymbols/removeSymbols и непустые
initializer lists CandleSymbol в addSymbols/setSymbols. Ограниченные перегрузки списков сохраняют
старые вызовы с пустыми скобками: они выбирают строковые перегрузки.

Весь object batch проверяется/копируется до одной прежней mutateSymbols-транзакции.
Канонический текст становится настоящим desired key; равные тексты coalesce, эквивалентный
объект удаляет тот же ключ. Null, непечатный/non-ASCII, leading wildcard и слишком длинный
канонический текст отвергаются прежним путём subscription-symbol. Ошибка batch сохраняет
полный предыдущий desired set. Подготовленные distinct storage ограничены прежними лимитами
числа символов/размера; callback или view не удерживаются.

Bridge применяет существующие event projections и route checks, не выводит тип Candle из текста
и не добавляет приём/агрегацию Candle. Строковые перегрузки сохраняют точное прежнее поведение.

[Production oracle](../../tools/upstream/candle-symbol-oracle/README.ru.md) содержит9822 наблюдения
factory/getters/malformed/duplicate/unknown-key/equality в
[fixture](../../tests/fixtures/events/candle/symbol-oracle.tsv) с
[provenance](../../tests/fixtures/events/candle/symbol-provenance.json); UTC/Honolulu должны совпадать.
Native-тест читает все строки, включая raw getter bits, проверяет владение, null adaptation,
явные getters и startup. Настоящий inactive feed test с лимитом1 доказывает dedup/remove
канонических объектов и атомарный отказ invalid batch в публичной desired map.
