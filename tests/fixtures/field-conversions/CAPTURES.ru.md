# Захваты преобразования полей

[English version](CAPTURES.md). Английский текст является основным.

`oracle.tsv` содержит 50,042 raw production наблюдения с разделителем LF.
[Provenance](provenance.json) связывает SHA-256 полного TSV, закреплённые upstream sources,
фактические JDK executable, harness, launcher и production-class inventory. Не переписывайте
и не нормализуйте набор; повторный захват выполняет
[сопровождаемый helper](../../../tools/upstream/field-conversion-oracle/README.ru.md).

Фактические Quote/Profile поля DXFeedScheme задают имена, properties и default types. Каждая
настроенная пара обращает физический порядок и создаёт настоящие двухслотовые long Java с
VOID tails, затем экспортирует одно логическое поле на полное локальное значение. Записи
обрабатывают generated mappings и фактические delegates MarketFactoryImpl. У каждого сценария
своя плотная пара record IDs; корпус не является receive profile со всеми вариантами одновременно.

| Семейство сценариев | Наблюдения |
| --- | --- |
| numeric2/4/8/24/104/120 | Точные поддерживаемые типы во всех четырёх числовых выборках Quote и десяти Profile; WideDecimal accessor bits и все 14 фактических публичных Java business-double getters |
| raw2/4/8/24/40/56/72/88/104/120/152 | Точный первый slot Sequence/nano/exchange/date/flags/halts, включая различия high/low у long |
| text10/text137 | Null, empty, NUL, paired и unpaired UTF-16; production serialization/read поля создаёт String перед фактическим delegate |
| missing120 | Все необязательные свойства отсутствуют; обязательные числа Quote и Profile Description сохранены |
| required120 | Обязательные числа Quote/Profile Description отсутствуют; фактический отказ factory/delegate |
| bad-numeric40/56/72/88/152/137 | Неподдерживаемое representation либо object storage в numeric selection; наблюдаемый Java отказ или optional missing, при более строгом явном native отказе |
| bad-text9/233/249 | Object storage другого класса; фактический ClassCastException вместо object-to-string coercion |
| factory1/factory3 | Production factory фактически не создаёт локальный BYTE/SHORT DataIntField |

DECIMAL samples охватывают все 128 младших flag patterns при пяти знаковых границах mantissa:
2560 cursor instances в numeric и raw вариантах Quote/Profile. LONG samples охватывают обе границы
signed 64, fitting thresholds signed 56, half ties/carry transitions десятичного сокращения,
границы точности binary64 и округление возле обоих long extremes. WIDE_DECIMAL сохраняет finite,
неканонические специальные и произвольные signed 64 bit patterns. Timezone пары побайтно равны,
class inventories неизменны. Helper ограничивает каждый Java/version/compiler subprocess
heap 128MiB и 60 секундами.

Native тесты сравнивают нужные production accessor results через полные owned и borrowed records.
Отдельно проверяются точные 72/108 preparation alternatives, отказ на единицу ниже, default registry
и явно неподдерживаемые типы. Отдельный public Feed consumer отправляет реальные варианты
DECIMAL/LONG/INT/String и raw TIME_MILLIS-slot через детерминированный TCP, сравнивает owned mixed
Quote/Profile delivery с этими наблюдениями и проверяет точные ADD/REMOVE tuples. Его можно применять
для отдельных installed и physically relocated package checks на обеих ОС.

Native business getters проверяются отдельно от raw decimal полей, включая LONG
36028797018963973 с различием представлений на 1 ULP. Manual/legacy aggregates, copy/move и
fallback при изменении raw — отдельные случаи. Все десять absent Profile business defaults
наблюдаются и сравниваются индивидуально. Snapshot state — inline владеющие числа без новых
heap allocations и borrowed fields; receiving projection byte bounds включают его фактический sizeof.

Source/configuration evidence, native авторские проверки, независимое ревью и package execution
различаются. Сам fixture не доказывает public network service run, готовность всего клиента или
предел памяти процесса. См. [контракт](../../../spec/events/FIELD-CONVERSIONS.ru.md).
