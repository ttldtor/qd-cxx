# Проекция Summary

[English version](SUMMARY.md). Английский текст нормативный.

Владеющий Summary и PreparedSummary доставляются через typed и mixed Feed. Эталон — закреплённые
production Java QD 3.355 Summary, SummaryMapping, SummaryDelegate. Это lasting snapshot торговой
сессии, отдельный от Trade/TradeETH, без time-series свойств. [Корпус](../../tests/fixtures/summary/CAPTURES.ru.md).

Плоский агрегат владеет символом и всеми скалярами. Day IDs, OpenInterest, flags, eventTime начинают
с 0; шесть WideDecimal — с raw 0 (NaN). Необязательный inline snapshot из 6 элементов владеет прямыми business double
и исходными wide-битами без отдельной allocation. Ссылки на cursor/schema/callback/transport не
сохраняются; копии переживают retirement. Getter-ы getDayOpenPrice/getDayHighPrice/getDayLowPrice/
getDayClosePrice/getPrevDayClosePrice/getPrevDayVolume сохраняют прямой Java результат до округления
LONG→wide. Для LONG 36028797018963973 wide-to-double отличается на 1 ULP. Изменённый raw выбирает текущий
wide view только этого поля; остальные snapshots действуют. Ручной/default агрегат snapshot не
имеет; reset выбирает все текущие wide views. WideDecimal::toDouble не меняется.

OpenInterest использует настоящий **long-accessor mapping**, намеренно исправляя конечную потерю
данных pinneddelegate по [ADR-0010](../../docs/decisions/ADR-0010-summary-open-interest-width.ru.md).
DefaultWIDE120 точно хранит3000000000, но Java delegate возвращает-1294967296;4294967296 превращается
в0. Publiclong контракт такого ограничения не документирует. Nativeint64 сохраняет actual
getOpenInterestLong/getAsLong: это явно принятое отличие от Java delegate.

| Source representation | Преобразование OpenInterest |
|---|---|
| INT full2,4,8 | signed physicalint с расширением доint64 |
| LONG104 | полное signed64 значение |
| DECIMAL24 | actualtiny→double, Java усечение/насыщение/NaN→0 вlong |
| WIDE120 | точный WideDecimal.toLong без последующего narrowing вint |

Wide toLong округляет к ближайшему, половины к плюс бесконечности, насыщает до signed64 и задаёт
NaN→0. Tiny double-to-long вместо этого усекает. LONG/WIDE не используют double intermediate.
LegacyINT8 остаётся физически32-битным; missing/regionalOI остаётся0. Тип/layout, допущенные numeric
форматы и receivinglimits не меняются. Сырые Java delegate captures сохраняются; отдельный полный
классифицированный actual long reference задаёт только исправленный expectedOIcell.
| Физический ключ / property | Обязателен | Типы | При отсутствии |
|---|---|---|---|
| DayId | да | raw-int11 | ошибка подготовки |
| DayOpen.Price / DayOpenPrice | да | numeric6 | ошибка подготовки |
| DayHigh.Price / DayHighPrice | да | numeric6 | ошибка подготовки |
| DayLow.Price / DayLowPrice | да | numeric6 | ошибка подготовки |
| DayClose.Price / DayClosePrice | нет | numeric6 | wide0/business NaN |
| PrevDayId | да | raw-int11 | ошибка подготовки |
| PrevDayClose.Price / PrevDayClosePrice | да | numeric6 | ошибка подготовки |
| PrevDayVolume | нет | numeric6 | wide0/business NaN |
| OpenInterest | нет | numeric6/corrected business-long |0 |
| Flags | нет | raw-int11 |0 |

Numeric6:2,4,8,24,104,120; raw-int11:2,4,8,24,40,56,72,88,104,120,152.
[Преобразования](FIELD-CONVERSIONS.ru.md) сохраняют принятую factory/profile границу local BYTE1/SHORT3.
Present неподдерживаемые типы отклоняются INCOMPATIBLE_TYPE, не маскируются как missing. Java optional
lookup может сам считать non-int поле отсутствующим; native profile от этого не расширяется.
Полная context/count/type/shape проверка включает непроецируемые поля до выдачи результата.

DayId/PrevDayId/Flags используют первый physical int, без getAsInt: у двухслотовых LONG/WIDE/TIME_MILLIS
это signed high32. Day IDs сохраняют signed дни с1970-01-01 без календарной валидации и timezone.
Flags — business flags, отдельно от indexed transaction flags. PriceType: bits2..3 day close,
bits0..1 previous close; REGULAR0/INDICATIVE1/PRELIMINARY2/FINAL3. High bits сохраняются; missing
flags означает REGULAR/REGULAR. Используются уже существующие общие PriceType и bit helper.

Production default Summary имеет 10 logical fields; Summary&X — 9, без OpenInterest. Optional close/
previous volume/flags включены. Wide=true по умолчанию: все семь numeric selections, включая OI,
имеют full120. При wide=false цены/volume используют DECIMAL24, OpenInterest — COMPACT_INT8. Per-field
price/volume selectors не относятся к OpenInterest. DefaultSummarySchema — явная native ten-field
wide convenience schema.

Business Time/Sequence/Nano у Summary нет. Factory time-slot names null и hasTime=false не создают
nullable timestamp. Независимые FIELD metadata задают eventTime в миллисекундах, при отсутствии 0.
Day IDs не порождают eventTime. Общие day/time utilities остаются отдельными API.

Подготовка: 10 selections / 15 keys / 75 type alternatives; 75 проходит, 74 атомарно отказывает RESOURCE_LIMIT.
Default 128 alternatives и caps 16 records / 128 fields / 256 lookups сохраняются. Ошибки сохраняют исходные
причины, частичный event не выдаётся.

Builtin сохраняет Quote0/Profile1/Trade2/TradeETH3 и добавляет Summary4:5records/59fields. Явные
Summary/Summary&Q routes связывают AAPL&Q с physical(Summary&Q,AAPL) и владеющим AAPL&Q. Суффикс общий,
Q — fixture. Quote/Trade/TradeETH/Summary выбирают regional fallback независимо; Profile сохраняет
точный символ. Действуют прежние attribute-preserving helpers и отказ непредставимых selectors.

Все 31 непустых наборов имеют immutable plans максимум 16 индексов. Checked sizeof-учёт включает 31 план
и временные копии. All-mask fixture явно задаёт 33 subscriptions (31 mixed, typed Summary, thrower),
2 symbols / 8 records / 91 fields; public default 8 не меняется. Exact/one-below на реальном MuxState проверяет
whole atomic grant и свежий turn. Work credits не являются RSS/CPU измерением; overflow отклоняется
RESOURCE до native activation.

Span живёт только в callback; копии владеют значениями. Исключение listener изолировано. Ordinary
clearSymbols cases требуют DRAINED; reconnect сохраняет gap. Закрытие подписки из callback — явный
local ABORT. Queue хранит lifetime aborted-registration history, поэтому только этот case ожидает
non-drained/noGap/noExpiry, доказав typed CLOSED, endpoint CONNECTED/healthy и причинную business
доставку всем 31 другим подписчикам после close. Native cancellation diagnostic фиксируется отдельно,
не является причиной классификации. Пустая source withdrawal TICKER завершается до graceful closing по
[контракту очереди](../mux/QUEUES.ru.md).

Installed/moved consumers проверяют реальные composite/regional demand и значения, 31 memberships,
negative routes, attributes, remove/throw/callbackclose/reconnect. No-data ADD marker ограждает
последний desired replacement; ACTIVE не является backend fence. Live DATA probe ограждает удаление.
Финальная отмена требует точное множество десяти tuples по полным frames, включая отказ при
пропуске/дубликате/лишней записи. Детерминированный локальный TCP не является public live-service run.
