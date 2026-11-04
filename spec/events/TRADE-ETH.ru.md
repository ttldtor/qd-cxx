# Проекция TradeETH

[English version](TRADE-ETH.md). Английский текст нормативный.

TradeETH — отдельный владеющий агрегат последней сделки расширенной сессии и её дневных итогов.
Subscription<TradeETH> и RegisteredEvent доставляют его независимо от обычного Trade. Summary пока
не поддерживается как typed event. Основание — закреплённые production Java QD3.355 TradeETH,
TradeBase, TradeETHMapping, TradeETHDelegate и [корпус](../../tests/fixtures/trade-eth/CAPTURES.ru.md).

Все скаляры инициализированы; пять decimal-полей начинают с raw0 (NaN). Встроенный optional snapshot
из пяти элементов владеет business double и wide-битами без отдельной heap allocation. Символ
владеющий; копии не зависят от cursor/schema/callback. Как у [Trade](TRADE.ru.md), getter-ы getPrice,
getSizeAsDouble, getChange, getDayVolumeAsDouble, getDayTurnover сохраняют прямой Java business
результат до округления LONG→wide. Изменение raw выбирает актуальный raw view только этого поля.
Ручной/default агрегат не имеет snapshot. getSize/getDayVolume используют Java усечение, насыщение
и NaN→0. API и aggregate layout обычного Trade сохранены.

TimeSequence объединяет signed32 секунды и все32бита Sequence. getTime использует unsigned bits22..31
как миллисекунды, включая1000..1023; getSequence возвращает low22. getTimeNanos сохраняет Java long
wrapping и исходный signed nano part. EventTime — независимые FIELD metadata в миллисекундах,
по умолчанию0. Часовой пояс и календарная фильтрация не добавляются.

Flags bit0 берётся из данных, может стать false при начале регулярной сессии с сохранением ETH цены,
размера, изменения и итогов. Тип события не заставляет этот бит быть true. Direction задают bits1..3
существующего enum;6/7 означают UNDEFINED. У ETH delegate **нет Last.Tick fallback**. Обязательное
ETHLast.Flags копируется без синтеза. Oracle transition/extra-tick проверяет обе особенности.

| Физический ключ / property | Обязателен | Типы | При отсутствии |
|---|---|---|---|
| ETHLast.Time / Time | нет | raw-int | секунды0 |
| ETHLast.Sequence / Sequence | нет | raw-int | low word0 |
| **Last.TimeNanoPart** / TimeNanoPart | нет | raw-int |0 |
| ETHLast.Exchange / ExchangeCode | нет | raw-int | exchange записи, composite0 |
| TradeId | нет |104,120,152 | raw64 zero |
| ETHLast.Price / Price | да | numeric | ошибка подготовки |
| ETHLast.Size / Size | да | numeric | ошибка подготовки |
| ETHLast.Change / Change | нет | numeric | wide0/business NaN |
| DayId | нет | raw-int | signed day0 |
| ETHVolume / DayVolume | нет | numeric | wide0/business NaN |
| ETHDayTurnover / DayTurnover | нет | numeric | wide0/business NaN |
| ETHLast.Flags / Flags | да | raw-int | ошибка подготовки |

Numeric full IDs:2,4,8,24,104,120; raw-int:2,4,8,24,40,56,72,88,104,120,152.
[Преобразования](FIELD-CONVERSIONS.ru.md) используют high physical word двухслотового raw int;
exchange сужается до low UTF-16 unit. TradeId сохраняет оба слова104/120/152. Java однослотовый
TradeId может читать соседний чужой слот: корпус фиксирует факт, typed adapter явно отказывает
INCOMPATIBLE_TYPE. Отсутствующий TradeId равен0. Полная shape/context проверка включает лишние поля.

Nano key именно Last.TimeNanoPart, без ETH. Только ETHLast.TimeNanoPart без настоящего ключа/alias
даёт default0. Подготовка требует12selections/22lookup alternatives/99type alternatives:99 проходит,
98 атомарно отказывает RESOURCE_LIMIT. DefaultTradeETHSchema — удобная native схема12полей с nano,
wide numerics и LONG TradeId; она не обещает включения всех optional production полей.

Builtin сохраняет Quote0/Profile1/Trade2 и добавляет TradeETH3:4записи/49полей. Явный registry с
настоящими TradeETH/TradeETH&Q связывает selector AAPL&Q с physical(TradeETH&Q,AAPL) и владеющим AAPL&Q.
Суффикс общий; Q — fixture. Без региональной записи composite сохраняет selector. Quote/Trade/TradeETH
выбирают fallback независимо; Profile сохраняет точный символ. Непредставимые selectors отклоняются
до публикации demand по [контракту Trade](TRADE.ru.md).

Все15 непустых наборов имеют immutable plans с максимум16индексами. Существующий sizeof-учёт покрывает
15планов и временные копии. Checked atomic grant зависит от настроенных клиентов и реальных записей.
Public defaults остаются8subscriptions/16records/128fields. All-mask fixture явно задаёт17subscriptions
(15mixed, typed ETH, thrower),2символа,6записей/72поля. Exact/one-below на реальном MuxState доказывает
прогресс без повышения public caps.

Callback span живёт только во время вызова; копии переживают retirement. Сохраняются атомарность
ошибок, fault isolation и quiescent close. Installed/moved consumers проверяют настоящий composite/
regional wire,15масок, negative routes, attributes, remove, throw и reconnect. No-data marker ограждает
последнее desired replacement; ACTIVE этого не обещает. Live probe ограждает первую отмену. Финальная
отмена требует точное множество восьми tuples по полным frames. Ordinary close остаётся drained,
explicit reconnect сохраняет gap-семантику. Локальные проверки не являются публичным live-service run.

Pinned primary sources: [TradeETH](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/TradeETH.java), [TradeETHMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/TradeETHMapping.java), [TradeETHDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/TradeETHDelegate.java).
