# Проекция события Profile

[English version](PROFILE.md). Английская версия нормативна.

QDCXX-025: проекция следует новым [production Java доказательствам](../../tests/fixtures/events/mapping/COVERAGE.ru.md)
и [границе настраиваемой схемы](../schema/CONFIGURED-EVENTS.ru.md). Отдельный адаптер события принимает
явно подготовленное отображение схемы; загрузка источников и доставка tape остаются вне него.

## Поля и значения при отсутствии

Локально обязательно только Description. Неизменённый default имеет 16 логических полей,
24 Java integer slots (включая десять VOID tails), два object slots и hasTime=false.
Логический порядок соответствует таблице. Все числовые поля ниже в default имеют полный120,
а в numeric-int дополнительно измерены с точным полным8.
Таблица описывает эти исходные generated witnesses. Исчерпывающие текущие numeric, raw-slot
и текстовые альтернативы заданы в [FIELD-CONVERSIONS](FIELD-CONVERSIONS.ru.md).

| Локальный lookup | Property lookup | Проекция события | Типы исходного witness | При отсутствии локально |
| --- | --- | --- | --- | --- |
| Beta | Beta | beta | 120 или 8 | NaN |
| Eps | Eps | earningsPerShare | 120 или 8 | NaN |
| DivFreq | DivFreq | dividendFrequency | 120 или 8 | NaN |
| ExdDiv.Amount | ExdDivAmount | exDividendAmount | 120 или 8 | NaN |
| ExdDiv.Date | ExdDivDate | exDividendDayId | 88 | 0 |
| 52High.Price | HighPrice52 | high52WeekPrice | 120 или 8 | NaN |
| 52Low.Price | LowPrice52 | low52WeekPrice | 120 или 8 | NaN |
| Shares | Shares | shares | 120 или 8 | NaN |
| FreeFloat | FreeFloat | freeFloat | 120 или 8 | NaN |
| HighLimitPrice | HighLimitPrice | highLimitPrice | 120 или 8 | NaN |
| LowLimitPrice | LowLimitPrice | lowLimitPrice | 120 или 8 | NaN |
| Halt.StartTime | HaltStartTime | haltStartTimeMs в миллисекундах | 56 | 0 |
| Halt.EndTime | HaltEndTime | haltEndTimeMs в миллисекундах | 56 | 0 |
| Flags | Flags | точные знаковые business flags | 8 | 0 |
| Description | Description | владеющий nullable UTF-16 | 10 | обязательно; отсутствие локально отвергается |
| StatusReason | StatusReason | владеющий nullable UTF-16 | 10 | null |

Дата сохраняет знаковый исходный day ID без календарного преобразования. Halt times умножают
знаковые int32 секунды на 1000 в int64. Полный120 сохраняет точные raw decimal bits и существующую
binary64 проекцию; полный8 — точное знаковое числовое целое. Отсутствующие локальные decimals
и raw zero полного120 дают NaN, а существующее wire-omitted поле полного8 даёт ноль.
Событие владеет символом и nullable strings, сохраняя пустые значения, NUL, парные и одиночные
суррогаты. Подготовка и копирование применяют конечные лимиты до выделения памяти; ошибка
проекции не публикует частичное событие. До проекции обязательны проверка каноничности FieldValue
и проверка контекста схемы/записи.

Десять Java-подобных business getters возвращают прямые source binary64 значения и сохраняют
наблюдаемый missing default каждого публичного Java getter. Существующие WideDecimal поля
сохраняют generated wide accessor view; для LONG эти представления могут различаться.
Необязательные владеющие inline snapshots добавлены в конец aggregate; после изменения raw поля
или при отсутствии snapshot getter использует текущий raw view. См.
[числовое состояние и точный LONG пример](FIELD-CONVERSIONS.ru.md).
Raw date/flag/halt альтернативы Profile читают первый физический int slot; long halt поля поэтому
используют старшие 32 бита как секунды. Пределы symbol/text и complete-shape/context checks сохранены.

## Подготовленный API и бюджеты владения

`defaultProfileSchema(limits)` создаёт локальную запись ID 0/Profile с EXTENDED_FIELDS_V1, всей
таблицей выше и явными local, full-name и property-ключами. Это удобная схема по умолчанию с тем же
путём проверки LocalSchema, что у схем вызывающего кода. Она не подтверждает поддержку произвольных
записей с именем Profile и не выполняет загрузку, разрешение параметров или wire-конверсии.

`PreparedProfile::prepare(schema, localId, limits)` разрешает явные local/property-ключи таблицы
через общий [контракт EventMapping](EVENT-MAPPING.ru.md). Перестановка, ненулевые локальные ID,
отсутствие необязательных полей, явные псевдонимы и дополнительные поддержанные поля используют
один путь. Разные найденные цели отклоняются как неоднозначные; присутствующие поля должны иметь
один из точных полных типов таблицы. Неизменяемый план владеет контекстом, полной формой типов
и выбранными позициями; имена/контейнеры источника не сохраняются. `context()` возвращает заимствованную
ссылку до перемещения, присваивания или уничтожения отображения; копия контекста независимо сохраняет
время жизни непрозрачной идентичности. Дополнительные/custom поля остаются неизменными и доступными.

`map(symbol, values, EventValueLimits, eventTime=0)` принимает RecordValueView с контекстом. Сначала
проверяется контекст, затем все теги и каноничность значений, включая невыбранные поля, до копирования.
Результат владеет символом, Description и StatusReason; nullable UTF-16 строки отличаются от пустых.
EventValueLimits независимо ограничивает symbolUnits, stringUnits на каждую копируемую строку
и totalUnits всех трёх значений. Null и пустая строка занимают ноль единиц. Совокупные проверки
используют вычитание до копирования, предотвращая переполнение размера. Ноль — действительный бюджет.
Ошибки выделения/длины дают MappingError::ALLOCATION_FAILURE, превышение бюджета — RESOURCE_LIMIT;
частичное событие не публикуется. Проекция не хранит счётчиков: вызывающий код, сохраняющий несколько
событий, отвечает за совокупный бюджет. Инспектор дополнительно ограничивает накопленные скопированные
единицы событий, работу декодера и закодированный вывод.

Десять числовых членов используют WideDecimal. Full120 сохраняет raw-биты; full8 проецирует точное
знаковое32 целое в decimal с коэффициентом, равным этому целому, и рангом 128
(`raw = value * 256 + 128`). Полученное представление события сохраняет binary64-семантику целого,
не меняет full8-тег/raw исходной обобщённой записи и не добавляет общий API создания decimal.
Отсутствующие локальные числовые члены остаются NaN. EventTime — явные metadata вызывающего кода,
по умолчанию ноль; адаптер не читает часы tape. `tradingStatus()` и `shortSaleRestriction()` возвращают
именованные enum, описанные ниже; `isTradingHalted()` / `isShortSaleRestricted()` предоставляют
их измеренные Boolean-представления.

## Flags и время

Flags остаются знаковыми32 данными записи, включая старшие reserved bits; это не QTP event flags.
Trading status — bits 0..1: 0 undefined, 1 halted, 2 active, 3 отображается в undefined.
Short-sale restriction — bits 2..3: 0 undefined, 1 active, 2 inactive, 3 отображается в undefined.
Следовательно, isTradingHalted — status==1; isShortSaleRestricted — restriction==1. Raw flags
сохраняются независимо от проекций. Захваченный 0x76543216 даёт active/active; -1 даёт undefined/
undefined, не превращаясь в raw zero. QTP event flags нулевые во всех принимаемых tapes.

EventTime отделён от halt times, business times Quote и Sequence. STREAM/NONE replay имеет
eventTime=0. RAW/MESSAGE replay переносит заданные 1600000000123 миллисекунд по существующему
контракту времени. Business flags Profile и halt fields не управляют временем контейнера.

## Связанные наблюдения Quote

Подготовленный Quote требует Bid.Price/Bid.Size/Ask.Price/Ask.Size. Bid.Time/Ask.Time и exchange
поля optional с нулевыми defaults; Sequence(полный72) и TimeNanoPart(полный8) — независимые
optional поля с нулевыми defaults. Sequence сохраняет знаковые32 упакованные bits: младшие22 —
public sequence, беззнаковые старшие10 — миллисекунды. Время Quote для секундных полей равно
max(bidTimeMs,askTimeMs)+unsignedHigh10. Отрицательные packed values остаются значимыми bits,
а не неверными sequence counts. Прямые граничные пробы: INT_MIN/-1/0/1/4194303/4194304/INT_MAX;
TimeNanoPart сохраняет знаковое raw значение даже вне обычных диапазонов приложения.
Прежний восьмиполевой Quote сохраняет свой контракт и вывод.
