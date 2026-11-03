# Преобразования полей событий по локальной схеме

[English version](FIELD-CONVERSIONS.md). Английский текст является основным.

Подготовленные mapping Quote и Profile преобразуют полные проверенные локальные значения.
Локальная схема сохраняет каждый точный полный serial type, дополнительное поле и alias поиска.
Преобразование не меняет generic `FieldValue`, идентичность его схемы, nullable payload или raw bits.
Фиксированный восьмиполевой `mapQuote` сохраняет исходный контракт точных типов.

## Поддерживаемые accessor

| Выборка события | Допустимые точные локальные типы | Проекция |
| --- | --- | --- |
| Quote Bid/Ask Price и Size; десять числовых свойств Profile | UTF_CHAR (2), INT (4), COMPACT_INT (8), DECIMAL (24), LONG (104), WIDE_DECIMAL (120) | Существующие decimal поля следуют `get…WideDecimal`; business getters — прямому production `getAsDouble` |
| Quote Sequence, TimeNanoPart и Bid/Ask Exchange; Profile ExdDiv.Date, Flags и Halt.StartTime/EndTime | 2,4,8,24,40,56,72,88,104,120,152 | Первый физический int slot Java, без преобразования representation |
| Quote Bid/Ask Time | TIME_SECONDS (56), TIME_MILLIS (152) | Секунды умножаются на 1000 либо миллисекунды сохраняются |
| Profile Description и StatusReason | UTF_CHAR_ARRAY (10), STRING (137) | Nullable точная UTF-16 String; без numeric или object-to-string coercion |

Таблицы полностью перечисляют варианты typed adapters. Generic `EventMapping::prepare` принимает
явные альтернативы типов из существующего конечного каталога BINARY_FIELDS_V1; сам он не
преобразует значение и не разрешает все типы каталога для typed event. Неподдерживаемая
присутствующая выборка даёт `INCOMPATIBLE_TYPE`. Неизвестные идентичности запроса дают
`INVALID_INPUT`. Биты representation не отбрасываются ради видимости совместимости.

Закреплённая production `SerialFieldType.createDefaultIntInstance` не создаёт локальные поля
BYTE (1) и SHORT (3). Эти идентичности остаются поддерживаемыми generic binary values и явно
отвергаются данными typed adapters. Граница не запрещает преобразование wire BYTE/SHORT в
поддерживаемую локальную схему и не исключает стандартные события из направления принимающего
клиента. VOID (0) не является typed scalar selection или искусственным локальным хвостом long.

## Числовые значения

UTF_CHAR, INT и COMPACT_INT превращаются в целочисленные WideDecimal. LONG использует production
контракт целочисленной композиции: знаковый 56-битный significand и rank, десятичное сокращение с
округлением половин к положительной бесконечности, затем production правила нормализации.
Обе границы signed 64 обрабатываются без знакового overflow и промежуточного `double`.

DECIMAL использует production путь `Decimal.tinyToWide`. Стандартная и дополнительная точность,
доли1/128, зарезервированные кодировки, NaN, положительная/отрицательная бесконечность и
неканонические нули следуют raw bits входа. WIDE_DECIMAL сохраняет каждый входной бит, включая
неканонические специальные значения. Helper не выделяет память и не содержит неограниченных
циклов; допустимому входу нужны максимум три десятичных сокращения и восемь шагов нормализации.

Публичные decimal поля сохраняют результат generated WideDecimal accessor. Их
`WideDecimal::toDouble` преобразует это десятичное значение и сохраняет существующий raw-decimal
view. Java-подобные business getters берут фактическое значение события прямо из исходного scalar:
Quote `getBidPrice`, `getBidSizeAsDouble`, `getAskPrice`, `getAskSizeAsDouble`; Profile `getBeta`,
`getEarningsPerShare`, `getDividendFrequency`, `getExDividendAmount`, `getHigh52WeekPrice`,
`getLow52WeekPrice`, `getShares`, `getFreeFloat`, `getHighLimitPrice`, `getLowLimitPrice`.

Различие существенно для LONG 36028797018963973: production wide getter возвращает raw
922337203685477759, чьи decimal-to-double bits равны 4854880398305394688. Фактический Java business
value имеет bits 4854880398305394689. Typed business getter сохраняет второе значение;
существующее decimal поле — первое. DECIMAL следует прямому tiny-decimal getter path, включая
дополнительную точность и зарезервированные кодировки; WIDE_DECIMAL использует production numeric
view. Для finite, infinite и signed-zero business results сравниваются точные binary64 bits;
NaN классифицируется без обещания сохранить payload/sign результата арифметики.

Необязательный inline массив четырёх/десяти `{wideRaw, directValue}` снимков добавлен после всех
существующих полей Quote/Profile. Он владеет только числами, не выделяет heap memory и не
заимствует record references. Старые aggregate initializers и legacy `mapQuote` оставляют его
отсутствующим. Getter использует source snapshot только пока текущие decimal raw bits данного
свойства совпадают. Manual event, отсутствие snapshot или изменение raw поля используют текущий
decimal view; изменение одного поля не аннулирует неизменённое соседнее. Copy/move сохраняют
владеющее inline state. Это value object с прежними обычными требованиями синхронизации
конкурентных изменений публичных полей. `WideDecimal::toDouble` и legacy mapQuote не меняются.
Обычный [пример Feed](../../examples/feed_quote/README.ru.md) использует business getters.
Общий API numeric-to-string, string-to-number и десериализации объектов не вводится.

## Физические slots и отсутствие

Long Java занимает два физических int slots. Raw `getInt` по индексу поля читает знаковые
старшие 32 бита; numeric LONG и WIDE_DECIMAL читают все 64 бита. Native локальное значение
остаётся одним логическим scalar; typed raw selections явно воспроизводят первый slot.
Halt поля Profile всегда умножают этот raw signed 32 slot на 1000, даже при точном локальном
типе TIME_MILLIS или WIDE_DECIMAL. Конвертер времени Quote здесь не применяется.

| Отсутствующая локальная выборка | Native проекция |
| --- | --- |
| Обязательное числовое поле Quote | Подготовка завершается MISSING_REQUIRED |
| Обязательное Profile Description | Подготовка завершается MISSING_REQUIRED |
| Необязательное числовое поле Profile | Raw WideDecimal 0 и business NaN; каждый из десяти публичных Java double getters наблюдается отдельно |
| Необязательные Quote time, Sequence, TimeNanoPart | Ноль |
| Необязательный Quote Exchange | Синтаксический exchange региональной записи либо ноль |
| Необязательные Profile date, flags, halt | Ноль |
| Необязательный Profile StatusReason | Null |

Отсутствие означает, что lookup не выбрал поле. Присутствующее поле неверного типа отвергается
даже там, где Java optional lookup считает другую storage class отсутствующей. Null отличается
от engaged empty String. Текст копируется только после проверки поштучной и суммарной границы
выхода; borrowed проекция не сохраняет source storage. Проверки полного shape, canonical payload
и context по-прежнему предшествуют проекции, включая не выбранные событием поля.

## Конечная подготовка

Полная таблица Quote объявляет 72 точные альтернативы, Profile — 108, включая необязательные
выборки даже при отсутствии. `EventRegistration.mappingLimits.typeAlternatives` по умолчанию 128;
явные меньшие лимиты сохраняют смысл. Точные 72/108 проходят; на единицу меньше дают
RESOURCE_LIMIT без частичного mapping. Comparison work отдельно оплачивает все фактические
сравнения имён и типов. Default comparison allowance консервативно включает новый конечный
бюджет типов; registry preparation по-прежнему суммирует объявленные allowances проверяемой
арифметикой и расходует существующие caller credits. Receive caps record 16 и field 128 не
меняются. Counts ограничивают логическую подготовку, а не измеренную память процесса.

Result-byte coordinate получателя и high-level preparation используют
`sizeof(client::SubscriptionProjection)`, поэтому inline snapshot автоматически увеличивает
конечные allowances. Финальный callback vector по-прежнему ограничен принятым числом записей
и фактическим sizeof события. Queue frames содержат generic FieldValue, а не typed snapshots;
их byte representation не растёт. Selection visits по-прежнему один раз читают numeric source,
вычисляя оба view из одного scalar. Receiver tests с точным result-byte бюджетом и на один байт
ниже проверяют обновлённую владеющую проекцию. Это логические copy-work/output bounds,
а не пределы allocator/RSS.

## Production доказательства и проверки

[Oracle helper](../../tools/upstream/field-conversion-oracle/README.ru.md) использует закреплённую
ревизию QD из [upstream.lock](../../upstream.lock), фактические `DXFeedScheme`, `MarketFactoryImpl`,
generated QuoteMapping/ProfileMapping, production RecordCursor и delegate/value getters.
Настраиваемые записи сохраняют production имена полей, меняют физический порядок на обратный,
создают настоящие long VOID tails и экспортируют полные логические поля. В
[fixture](../../tests/fixtures/field-conversions/CAPTURES.ru.md) фиксируются sign/rank/tie/carry/extreme
входы, все 128 младших tiny-decimal flag patterns при пяти знаковых границах mantissa, raw-slot
варианты, nullable строки, отсутствие обязательных/необязательных полей и неподдерживаемые
representation.

Первичные исходники преобразований закреплены в
[RecordMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordMapping.java),
[WideDecimal](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/WideDecimal.java)
и [Decimal](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/util/Decimal.java).

Native conformance сравнивает production WideDecimal bits, первый slot, business time, текст и
defaults через owned и borrowed mapping entry points. Точные границы подготовки, default
registry, provenance/complete-shape validation и конечное владение текстом сохраняют обязательность.
Детерминированная wire delivery, installed/relocated package consumers и platform results —
отдельные выполненные проверки; одна исходная таблица их не доказывает.
