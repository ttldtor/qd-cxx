# Проекция Trade

[English version](TRADE.md). Английская версия является определяющей.

Контракт охватывает владеющий `events::Trade`, `PreparedTrade`, зарегистрированные composite и
региональные записи Trade, типизированную `Subscription<Trade>` и mixed-доставку `RegisteredEvent`.
[TradeETH](TRADE-ETH.ru.md) поддержан отдельным типом; Summary пока не поддерживается. Generic-записи
доступны независимо.

Эталон — Java QD3.355 из `upstream.lock`: TradeBase, Trade, generated TradeMapping/TradeDelegate,
MarketEventMapping, MarketEventDelegateSet и Direction. [Описание захвата](../../tests/fixtures/trade/CAPTURES.ru.md)
связывает actual production oracle с raw dataset `oracle.tsv`; это наблюдения настоящих методов,
а не независимая копия формул преобразования.

## Публичные значения и владение

Trade владеет UTF-16 символом и всеми скалярами. После map не сохраняются ссылки на cursor, поля,
схему или callback. Все члены имеют определённую default initialization. Пять decimal-членов
инициализированы raw0 (NaN); eventTime, packed time, nano, exchange, TradeId, day и flags — нулём.
Это агрегат с завершающим optional inline snapshot из пяти элементов, без отдельной heap allocation.
У значения нет транспортного базового класса или изменяемой глобальной конфигурации.

Пять WideDecimal-членов представляют generated wide-accessor view. Java-подобные getPrice,
getSizeAsDouble, getChange, getDayVolumeAsDouble и getDayTurnover возвращают business-double view,
полученный до округления LONG→wide. Например, LONG36028797018963973 даёт business binary64 на1ULP
выше wide-to-double. Snapshot содержит соответствующие wide raw bits. Изменение одного decimal-члена
выбирает его текущее raw-представление при несовпадении; остальные source business values сохраняются.
Сброс snapshot выбирает raw view всех членов; восстановление исходных raw bits снова делает применимым
соответствующее source value. У вручную созданного агрегата snapshot отсутствует. WideDecimal::toDouble
не меняется. Копии и результаты перемещения владеют состоянием независимо от источника.

getSize/getDayVolume применяют Java binary64→long: усечение к нулю, NaN→0, насыщение signed64 для
бесконечности и выхода за диапазон. Native cast вне диапазона не выполняется. Для NaN гарантирована
классификация, а не sign/payload результата арифметики.

## Точные ключи, типы и defaults

Ключи регистрозависимы, задаются явными local lookups. Extra fields/records сохраняются. Перед
проекцией проверяются полный context и canonical shape всех значений, включая невыбранные extra fields.
Нет string/number coercion или автоматического выбора mapper по имени записи.

| Physical key / property alternative | Обязателен | Полные local types | При отсутствии |
|---|---|---|---|
| Last.Time / Time | нет | raw-int set | seconds0 |
| Last.Sequence / Sequence | нет | raw-int set | packed low word0 |
| Last.TimeNanoPart / TimeNanoPart | нет | raw-int set |0 |
| Last.Exchange / ExchangeCode | нет | raw-int set | exchange из suffix записи, composite0 |
| TradeId | нет |104 LONG,120 WIDE_DECIMAL,152 TIME_MILLIS |0 |
| Last.Price / Price | да | numeric set | ошибка preparation |
| Last.Size / Size | да | numeric set | ошибка preparation |
| Last.Tick / Tick | нет | raw-int set |0 |
| Last.Change / Change | нет | numeric set | wide raw0, business NaN |
| DayId | нет | raw-int set |0 |
| Volume / DayVolume | нет | numeric set | wide raw0, business NaN |
| DayTurnover | нет | numeric set | wide raw0, business NaN |
| Last.Flags / Flags | нет | raw-int set |0 |

Numeric set — точно full IDs **2,4,8,24,104,120**. Raw-int set — точно
**2,4,8,24,40,56,72,88,104,120,152**; их представления описаны в
[FIELD-CONVERSIONS](FIELD-CONVERSIONS.ru.md). Raw-int getter двухслотового поля читает high32 physical
slot. Exchange берёт младшую UTF-16 unit без нормализации. DayId — signed days с1970-01-01, без
календарной интерпретации.

TradeId использует отдельный raw long accessor, сохраняющий оба слова без decimal/time conversion.
Java также может прочитать однослотовое поле вместе с соседним physical slot, даже принадлежащим
другому полю; oracle сохраняет это наблюдение. Native typed adapter явно отказывает для такого
TradeId с INCOMPATIBLE_TYPE: logical FieldValue не заимствует биты соседнего значения. Это граница
поддерживаемого профиля, а не универсальная Java-паритетность для такой нестандартной схемы.
Отсутствующий TradeId даёт actual Java default0; generic decoding не меняется.

Preparation объявляет13 selections,23 lookup alternatives и110 type alternatives. Полный маршрут
вмещается в существующие128 alternatives EventRegistration; явные109 дают атомарный RESOURCE_LIMIT.
Receiving profile сохраняет16 records/128 logical fields. Built-in registry оставляет Quote0/Profile1
и добавляет composite Trade2. defaultTradeSchema — явная полная native convenience-схема из13 полей,
включая optional nano, wide numeric и LONG TradeId; она не утверждает, что все optional fields
включены в Java без конфигурации.

## Время и флаги

Delegate помещает raw Last.Time как signed32 seconds в high word timeSequence и все32 бита raw
Last.Sequence в low word. В отличие от Quote, generated Trade time accessor читает physical int
даже у local type152. getTime() равен signed_high32×1000 плюс unsigned bits22..31; значения packed
milliseconds1000..1023 сохраняются. getSequence() возвращает low22 без обещания монотонности/уникальности.
getTimeNanos() объединяет миллисекунды с неизменённым signed nano part через Java long arithmetic;
native unsigned arithmetic обеспечивает определённое wrapping. Отрицательный nano и значения вне
0..999999 не ограничиваются. eventTime — независимые FIELD metadata milliseconds; business time,
indexed identity и source ordinal не подставляются вместо него.

Direction занимает flags bits1..3: UNDEFINED0, DOWN1, ZERO_DOWN2, ZERO3, ZERO_UP4, UP5; reserved6/7
читаются как UNDEFINED. Только для UNDEFINED production delegate смотрит Last.Tick:1 заменяет
direction на ZERO_UP,2 — на ZERO_DOWN, остальные значения оставляют flags без изменений. Замена
сохраняет остальные биты, включая extended-hours bit0. Trade может сообщать extended hours;
отдельный [TradeETH](TRADE-ETH.ru.md) использует собственную запись без Tick fallback.

## Региональный demand и доставка

Регистрация явно выбирает record ID и EventType::TRADE. При экспортированных Trade и Trade&Q
логический AAPL&Q выбирает настоящий regional route и отправляет physical tuple(Trade&Q,AAPL).
Результат владеет `AAPL&Q`; отсутствующий Last.Exchange получает Q из suffix записи. Реализация
обрабатывает suffix в общем виде, без константы Q. Распознанные trailing attributes сохраняются.
При отсутствии matching regional record composite route сохраняет логический selector.

Quote и Trade выбирают regional fallback независимо; Profile использует обычный exact symbol.
Пятнадцать непустых наборов Quote/Profile/Trade/TradeETH имеют конечные immutable plans максимум по16 уникальным
bindings, включая непоследовательные сочетания типов. Fixed index arrays и временная stack-копия
16 bindings включены в retained/work accounting. Инициализация, чтение/запись metadata и обе проверки
индексов/копирования оплачиваются до копирования; one-below отказ не меняет output или credits.
Queue-owned command copies оплачиваются отдельно.

Worker также оплачивает целый atomic STREAM fanout, чтобы повтор на fresh turn мог завершиться.
При C client slots, D actual schema records, E interest entries, A aggregate entries и U maximum
symbol units actual MuxState select preflight требует V=32×(2+C+D+E+A)×(2+E) visits и V×U text units
на query. Routing chunk выполняет до routeRecords×C queries; проверенные visits/text credits
добавляются к прежнему конечному interest-work seed. Operations, bytes, quantum и record caps
не меняются. Overflow даёт прежний actionable RESOURCE_LIMIT до native activation. Boundary test
оплачивает настоящие MuxState selects точными whole-chunk credits и значением на одну text unit ниже,
без копии selector. Work units — logical credits, не CPU time или память.

Selector, не представимый без потерь региональным преобразованием, например AAPL&N&Q или attributes
перед заменяемым suffix, синхронно получает native INVALID_ARGUMENT на
subscription-symbol-representation, атомарно для desired batch. Нет молчаливой canonicalization или
исправления неправильно направленного запроса после доставки. Logical membership остаётся точным.

Receiver и callback vector используют actual sizeof event/variant в конечных профилях copy/preparation.
Callback spans не переживают возврат; скопированные события сохраняются после remove/reconnect,
retirement endpoint/runtime. Исключения listener используют прежнюю границу отказа одной logical
subscription. Новый polling loop или lifecycle mechanism не нужен.

Это конечный явно зарегистрированный regional subset, а не полная поддержка catalog/selectors.
Каталог более16 records требует существующего integration decision. Этот leaf не расширяет cap и
не заявляет готовность полного клиента.

Primary pinned sources: [TradeBase](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/TradeBase.java), [TradeMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/TradeMapping.java), [TradeDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/TradeDelegate.java).
