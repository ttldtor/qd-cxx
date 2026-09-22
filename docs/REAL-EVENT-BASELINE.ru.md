# Первый пример с реальным событием: стандартный составной Quote

[English](REAL-EVENT-BASELINE.md). Английская версия является основной.

Исследование QDCXX-020; QD 3.355, `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
Все выводы о событии здесь основаны **только на исходниках**. Новые генерация событий, выгрузка
схемы, публикация и воспроизведение не выполнялись. Предыдущие [наблюдения tape](../spec/protocol/TAPE-FILES.ru.md)
используют синтетические записи S/R/E и не подтверждают совместимость Quote.

## Выбор и цепочка исходников

Для первого полезного примера чтения tape и отображения события выбран составной `Quote`.
Он проверяет реальные цены, дробные размеры, символы бирж и время bid/ask без модели
транзакций/снимков индексируемых событий. Trade дополнительно требует TradeId, sequence, дату,
объём, оборот и флаги; короткое название не означает более простой профиль.

Закреплённые исходники:

- [DXFeedScheme](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedScheme.java): снимок системных свойств, зарегистрированные фабрики событий и PentaCodec.
- [MarketFactoryImpl.buildScheme/createDelegates/createStreamOnlyDelegates](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java): поля, значения по умолчанию и контракты делегатов.
- [SchemeProperties](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeProperties.java): `isWide`, `selectDecimal`, `selectTime`, `isEventPropertyEnabled`, `getExchanges`.
- [SchemeBuilder](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeBuilder.java): порядок добавления, необязательные поля, два слота long и `hasTime`.
- [SerialFieldType](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java): ID, именованные типы и классы локальных полей.
- [QuoteMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/QuoteMapping.java) и [QuoteDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/QuoteDelegate.java): поиск полей и оба направления преобразования событий.
- [Quote](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/Quote.java), [EventDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegate.java), [MarketEventDelegateImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketEventDelegateImpl.java): символ, время события и время/sequence API.

## Точная неизменённая запись по умолчанию

Используется встроенная DXFeedScheme закреплённой версии без переопределений схемы/свойств.
`dxscheme.wide` по умолчанию **true**: цены И размеры выбирают WIDE_DECIMAL, несмотря на
резервные аргументы DECIMAL/COMPACT_INT фабрики. Нельзя задавать `dxscheme.wide=false` или
заменять типы на 8 ради текущего декодера. `dxscheme.price`, `dxscheme.size`, `dxscheme.bat`
и включение свойств события задают другие профили; при использовании их нужно фиксировать.

Ниже локальные имена полей схемы; полные имена имеют префикс `Quote.`. Индексы —
выведенная из исходников проекция целочисленных слотов Java, не ID записей в протоколе
и не требование к хранению C++.

| Поле | Полный ID типа / примитив протокола | Слоты Java | Преобразование Quote API |
| --- | --- | --- | --- |
| Bid.Time | TIME_SECONDS 56 / compact int | 0 | BidTime в миллисекундах, хранится в секундах |
| Bid.Exchange | UTF_CHAR 2 / UTF-символ | 1 | BidExchangeCode (`char`); явный нестандартный псевдоним свойства |
| Bid.Price | WIDE_DECIMAL 120 / compact long | 2–3 | BidPrice через decimal-преобразование |
| Bid.Size | WIDE_DECIMAL 120 / compact long | 4–5 | BidSizeAsDouble; сохранять дробный размер |
| Ask.Time | TIME_SECONDS 56 / compact int | 6 | AskTime в миллисекундах, хранится в секундах |
| Ask.Exchange | UTF_CHAR 2 / UTF-символ | 7 | AskExchangeCode (`char`); явный нестандартный псевдоним свойства |
| Ask.Price | WIDE_DECIMAL 120 / compact long | 8–9 | AskPrice через decimal-преобразование |
| Ask.Size | WIDE_DECIMAL 120 / compact long | 10–11 | AskSizeAsDouble; сохранять дробный размер |

Эти локальные имена с точками отличаются от исходящих имён свойств. Стандартный binary writer
использует `BidTime`, `BidExchangeCode`, `BidPrice`, `BidSize`, `AskTime`, `AskExchangeCode`, `AskPrice`,
`AskSize` в указанном порядке. [AbstractDataField](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/AbstractDataField.java)
удаляет точки в стандартных именах свойств; [DefaultRecord](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java)
применяет псевдонимы бирж из QuoteMapping. [BinaryRecordDesc](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java)
использует getPropertyName и пропускает void-хвосты. [BinaryQTPComposer.isWideDecimalSupported](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java)
возвращает true для этого прямого файлового composer независимо от EMPTY_SET; переопределения
сетевого composer нужно изучать отдельно. Захватывать реальные descriptions наряду с локальными именами полей.

Восемь логических сериализуемых полей, двенадцать локальных int-слотов, нет object-слотов,
`hasTime=false`. Второй слот long — локальный void-хвост, а не ещё одно значение в протоколе.
`Sequence` (SEQUENCE 72) и `TimeNanoPart` (COMPACT_INT 8) необязательны и выключены по умолчанию;
mapping читает ноль и игнорирует запись. Время и биржа bid/ask необязательны при построении
mapping, но включены в стандартной записи; цены/размеры обязательны. Нельзя пропускать
включённые поля только потому, что mapping допускает их отсутствие.

`Quote.getTime()` использует большую секунду bid/ask и миллисекунды из старших десяти бит
`timeMillisSequence`; младшие 22 бита — sequence, диапазон 0..4194303. При отсутствии
стандартного поля Sequence миллисекунды/sequence теряются при круговом преобразовании.
Первые положительные значения должны иметь целые секунды, нулевые sequence и nano part;
отдельный случай с потерей точности должен показывать результат Java, а не ожидать точного
сохранения. Quote не является индексируемым событием: слоты индекса времени history и флаги
индексируемых событий не нужны. Raw-флаги курсора и необязательные метаданные
EventTime/EventSequence отличаются от бизнес-времени Quote. Первый файловый профиль требует
нулевые raw-флаги, EMPTY_SET, time NONE и нулевое event time.

Полная стандартная схема также строит `Quote&A`–`Quote&Z` (см.
[MarketEventSymbols.DEFAULT_EXCHANGES](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java)).
Региональные записи не содержат оба поля Exchange, выводят биржу из суффикса записи и имеют
шесть логических полей / десять int-слотов. Значения по умолчанию Sequence/nano прежние.
Первый тест выбирает только составные символы; он не удаляет региональные записи из схемы
Java и не заявляет их поддержку. До утверждений о слотах/ID нужно захватить идентичность
реальной полной схемы и выбранную структуру записи.

## Предлагаемый детерминированный тест через production-путь

1. Собрать закреплённую Java двумя чистыми запусками; записать JDK, classpath/провайдеры,
   все эффективные свойства схемы и хеши. Перечислить реальную стандартную запись и mapping.
   При отличии полей/типов от таблицы завершить проверку ошибкой и исследовать, не упрощать схему.
2. Создать настоящие объекты `Quote` для `IBM`, `MSFT`, затем снова `IBM`. Задать время bid/ask
   целыми секундами около `1700000000000`, коды бирж `N`/`Q`, цены `100.25`/`100.5`, размеры
   `10.5`/`12.25`, затем изменённую котировку. Event time, sequence и nano part равны нулю.
   Нет входа от часов, случайного генератора или внешнего потока.
3. Вызвать настоящий QuoteDelegate `putEvent`, выбранный среди stream-only делегатов стандартной
   схемы, затем production FileWriter: BINARY, compression NONE, time NONE, EMPTY_SET, STREAM_DATA.
   Сохранить descriptor, автоматические descriptions и footer; закрыть файл до воспроизведения.
   Это тест «событие → production-файл», а не наблюдение DXPublisher endpoint/сети.
4. Прочитать неизменённый файл production FileReader в ту же полную стандартную схему и вызвать
   настоящий QuoteDelegate `getEvent`. Независимо сохранить байты, биты полей, кодовые единицы
   символов, порядок записей и значения API. Сравнить два чистых захвата; группировка callbacks
   не является контрактом. Затем сравнить будущий ограниченный C++ reader и представление
   события с этими наблюдениями Java.
5. Добавить конечные положительные/ошибочные случаи: пустые/усечённые файлы, поздние descriptions,
   неизвестные/переставленные поля, границы UTF-символа, finite/NaN/infinity/negative-zero wide decimal,
   дробные размеры, отсутствующие/переопределённые descriptions и исчерпание бюджетов. Ожидания —
   production-наблюдения Java и отдельно заданная политика ресурсов/ошибок C++. Изменение одного
   типа, decimal-значения или порядка полей должно отвергаться verifier; затем снова проверить baseline.

Это предложение, а не выполненный захват. Позднее endpoint-тест должен публиковать через
STREAM_PUBLISHER и воспроизводить через STREAM_FEED, чтобы избежать ticker conflation, с явным
опустошением очередей/завершением и конечными тайм-аутами. Перед ним нужно явно обработать
RAW_DATA/стандартные опции TapeConnector; это не ограниченный профиль прямого FileWriter выше.
Внешний endpoint не требуется.

## Пробелы C++ и порядок реализации

Имеющиеся compact-long и UTFString не реализуют семантику WIDE_DECIMAL или UTF_CHAR.
Валидация локальной схемы core, binding и STREAM_DATA допускают только обычный тип 8;
нужны точные 56/2/120, владение/пропуск long-значений, decimal-преобразования и mapping/представление
Quote. Полная идентичность типа сохраняется даже при общем compact-примитиве. Общих преобразований,
загрузчика схем, транспорта, writer и ticker collector нет.

[Рекомендуемая следующая реализация](NEXT-STEPS.ru.md) — узкий tape reader QDCXX-019:
descriptor, добавление bindings и EOF позволяют читать неизменённый production-файл на имеющихся
фикстурах S/R/E. Это предварительное условие, **не** этап приёмки реального события. Сразу после
него — ограниченное расширение типов/mapping Quote и тест выше; компоненты можно разделить,
если этого требует проверка API/доказательств. Первый type-8 tape reader не означает общую поддержку dxFeed.

Загрузка внешних XML/JSON — отдельный адаптер, поставляющий проверенную неизменяемую локальную схему.
Wire descriptions содержат входящие ID записей, имена и serial types; они не загружают XML и не
восстанавливают полную локальную политику mapping/default/provider. Первый профиль Quote может
использовать явный проверенный встроенный локальный адаптер, сохраняя стандартные типы Java
и публикуя ограничения. Общий importer не является предпосылкой одного события; адаптер и
публичный API события требуют одобрения владельца.
