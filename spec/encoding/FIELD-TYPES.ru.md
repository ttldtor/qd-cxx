# Ограниченная поддержка дополнительных точных типов полей

[English version](FIELD-TYPES.md). Английская версия является нормативной.

Первоначальный контракт QDCXX-024 определён по захватам примитивов и production
FileWriter/FileReader закреплённого Java. Расширение двоичных скаляров ниже добавляет
отдельно подтверждённые наблюдения обоих pins. [Oracle](../../tools/upstream/field-types-oracle/FieldTypesOracle.java)
и [отчёт о захватах](../../tests/fixtures/encoding/field-types/CAPTURES.md) разделяют наблюдения исполнения,
диагностические входы и проверки воспроизводимости. Lock неизменён. Это поддержка полей,
не адаптер события Profile и не доказательство полной совместимости частного tape.

## Явный профиль двоичных скаляров

`LocalProfile::BINARY_FIELDS_V1` — явно выбранное обычное подмножество скаляров
[конечного двоичного каталога](../protocol/BINARY-TAPE-COVERAGE.ru.md), доступное
только для чтения. Оно принимает ровно 0/1/2/3/4/8/10/24/40/56/72/88/104/120/152.
Прежние локальные профили сохраняют точные наборы ниже. Член для object/byte payload,
индексная схема, преобразование, новый tape mode или профиль инспектора не добавлены.
Более широкий каталог остаётся незавершённым.

| Wire identity | Полная identity C++ | Физическое чтение | Канонический `raw` |
| --- | --- | --- | --- |
| 0 VOID | `VOID_FIELD` | Ноль байтов | 0 |
| 1 BYTE | `BYTE` | Один знаковый байт | -128..127 |
| 3 SHORT | `SHORT` | Два big-endian байта | -32768..32767 |
| 4 INT | `INT` | Четыре big-endian байта | INT32_MIN..INT32_MAX |
| 24 DECIMAL | `DECIMAL` | Compact long | Знаковые младшие 32 raw-бита |
| 104 LONG | `LONG` | Compact long | Все знаковые 64 бита |
| 152 TIME_MILLIS | `TIME_MILLIS` | Compact long | Все знаковые 64 raw-бита миллисекунд |

`SerialType::VOID_FIELD` и `SerialFieldType::VOID_FIELD` избегают конфликта с Windows
macro `VOID`; числовая wire identity остаётся нулевой. У всех скаляров `units`
отсутствуют. DECIMAL не получает числовой интерпретации; LONG/TIME_MILLIS сохраняют
старшие биты, как прежний WIDE_DECIMAL. Общие serialization bits не разрешают aliases,
reserved168, неизвестные старшие биты, отрицательные ID и неподдерживаемые комбинации.

Явно объявленный локальный VOID — плотный локальный raw-zero default, включая
отсутствие поля в source description. Source binding существует только при явном
wire-объявлении. VOID не читает байты значения, но оплачивает одну попытку wire-field;
пропуски и повторные bindings подчиняются тому же правилу. Production Java удаляет
VOID из операций чтения/записи descriptor. Независимый cursor slot VoidIntField
остаётся нулевым; это не доказывает доставку логического Java-поля или его запись
FileWriter. Физические tails LONG/TIME_MILLIS/WIDE_DECIMAL никогда не превращаются
в выдуманные source declarations или логические значения.

Общий charged scalar decoder обслуживает полные payload STREAM и RAW. Усечение
фиксированной ширины потребляет все доступные байты prefix и сообщает
TRUNCATED_PAYLOAD на FIELD с offset начала примитива; частичная текущая запись
не публикуется. Compact-формы потребляют всю физическую ширину перед low32 projection.
Скаляры не выделяют value payload и не расходуют строковые бюджеты. Прежние ограничения
payload, записей, полей, символов и вывода остаются конечными, включая ноль; admission
defaults выполняется до построения. Завершённые записи владеют данными после разрушения
input, schema, context или reader и сохраняются при последующей ошибке поля.

Переданная вызывающей стороной новая схема может использовать прежние грамматики
контейнеров LEGACY/NONE/STREAM или RAW_MESSAGE_V1/MESSAGE. Их политики descriptor,
message-family, flags, timing и EOF не меняются; обычное автоматическое discovery
сохраняет EXTENDED_FIELDS_V1. `OutgoingLayouts::create` отвергает любую схему
BINARY_FIELDS_V1 типизированным UNSUPPORTED на `profile`, включая пустые layouts
и layouts только с прежними типами, до обхода, копирования и выделения памяти.
Outgoing codec не добавлен. [Scalar validation](../../tests/conformance/protocol/field-types/SCALAR-VALIDATION.ru.md)
разделяет факты исходников, requested composer frames, FileWriter captures и реальные
parser/FileReader callbacks на обеих закреплённых ревизиях.

## Идентичность, хранение и binding

`LocalProfile::EXTENDED_FIELDS_V1` принимает ровно полные идентификаторы 2/8/10/40/56/72/88/120.
COMPACT_INT_V1 сохраняет ровно 8; QUOTE_FIELDS_V1 — ровно 2/8/56/120. Биты сериализации
не разрешают другие полные идентификаторы. Поддерживаются только обычные локальные records
с `hasTime=false`. Логические позиции полей независимы от Java int/object индексов и физических VOID tails.
Binding сохраняет упорядоченные повторные присваивания, пропускает поддерживаемые неизвестные поля/records,
отвергает несовпадение полных типов известных полей и владеет планами независимо от имён и схем.
Более широкие преобразования и восстановление Java не расширяют эту политику.

`FieldValue` добавляет `optional<u16string> units` после `type` и `raw`. Тип 10 требует raw zero:
отсутствующие units означают null, присутствующая пустая строка — empty; остальные units сохраняются точно,
включая NUL и непарные суррогаты. Скалярные теги требуют отсутствующих units. Типы 8/40/56/72/88 требуют
raw int32 с расширением знака, тип 2 допускает 0..0x10ffff, тип 120 сохраняет все signed64 биты.
Неизвестные теги и неканонические вручную созданные значения недопустимы. Equality сравнивает все три
компонента. Публичная проверка значения, Quote mapper и typed formatter обеспечивают эти инварианты.
Прежние скалярные aggregate-выражения допустимы. Свежие отсутствующие поля имеют свой тег и raw zero/null;
последующие совпавшие присваивания заменяют предыдущие значения, включая замену на null/empty.

Типы 40 (SHORT_STRING), 72 (SEQUENCE) и 88 (DATE) читают compact-long с последующей знаковой
проекцией младших 32 битов. Избыточные/расширенные кодировки принимаются. Преобразование ShortString
в текст, проверка календаря и преобразование единиц даты отсутствуют. SEQUENCE поля независим от
`TapeTiming::eventTimeSequence` heartbeat. Прежние скалярные контракты: [QUOTE-VALUES](QUOTE-VALUES.ru.md).

### Компоненты сериализации и представления

Полный числовой идентификатор — побитовое OR базового типа сериализации и одного селектора
представления. C++ enum `SerialType` и `FieldRepresentation` именуют реализованные компоненты;
`SerialFieldType` сохраняет прежние полные идентификаторы, теперь заданные через `composeSerialTypeId`.
Например, `WIDE_DECIMAL` — это `COMPACT_INT | WIDE_DECIMAL` (`8 | 0x70 = 120`), не новая базовая кодировка.
Селекторы обозначают альтернативы, а не независимые флаги, которые можно объединять друг с другом.

`REPRESENTATION_MASK = 0xf0` и `SERIAL_TYPE_MASK = ~REPRESENTATION_MASK` соответствуют
[закреплённому SerialFieldType.Bits](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L330).
`serialTypeBits` и `representationBits` возвращают числовые компоненты без проверки допустимости.
В частности, маска сериализации сохраняет неизвестные старшие биты; это не `0x0f`.
Составление ID не проверяет допустимость пары и не разрешает декодирование. Неизвестные селекторы
представления остаются числами, а неподдерживаемые полные ID по-прежнему отвергаются неизменной
проверкой профиля по точному ID. Общая база сериализации не означает одинаковую логическую ширину:
WIDE_DECIMAL сохраняет все 64 бита.

Перечисления намеренно неполные. Рефакторинг ради читаемости не добавляет поддерживаемых ID,
преобразований, wire-поведения или новых ожидаемых результатов fixtures. Построение схемы и binding
продолжают использовать единую политику `supportsSerialType`; тесты сохраняют независимые числа.
См. [проверку рефакторинга](../../tests/conformance/protocol/field-types/TYPE-MODEL-REFACTOR.ru.md).

## Примитив массива символов и граница сообщения

Тип 10 содержит число символов compact-long, затем столько BMP-значений, закодированных UTFChar.
Допустимые counts: -1..INT32_MAX; -1 означает null. Канонический Java writer считает UTF-16 units
и записывает каждый суррогат отдельно. Reader принимает измеренные избыточные формы, включая
четырёхбайтовую кодировку BMP units. Supplementary scalar отвергается после полной кодировки;
CESU surrogate pair — две допустимые units. Полный хвост UTFChar читается до проверки continuation bits.
Нормализация, замена символов и framing UTFString с числом байтов отсутствуют.

Ограниченный reader предоставляет `CharArrayBoundary::PRIMITIVE` (по умолчанию) и `MESSAGE`.
PRIMITIVE соответствует IOUtil/StringField над ByteArrayInput или DataInput; неполное содержимое
потребляет исследованные байты. MESSAGE дополнительно применяет проверку BufferedInputPart production
BinaryQTPParser: после префикса положительное число символов, превышающее все оставшиеся байты сообщения,
даёт INVALID_LENGTH без чтения тела. Payload decoder всегда выбирает MESSAGE. Эти интерфейсы различаются
для повреждённых counts; наблюдение прямого примитива само по себе не доказывает позиции production-ошибок.
Ни один режим не обрезает тело по числу символов как по байтам: следующее поле начинается в позиции,
достигнутой после ровно объявленного числа символов.

`readCharArray` заимствует span, возвращает владеющее optional UTF-16 значение, consumed bytes,
charged units и явный status; состояние продолжения не сохраняется. Ошибка не возвращает частичную строку.
Ошибки allocation/length преобразуются в ALLOCATION_FAILURE с последней исследованной позицией и списаниями.

## Конечные лимиты, списания и приоритеты

`CharArrayLimits` ограничивает полное число encoded bytes (с префиксом) и code units одного значения.
Новые поля в конце `StreamDataLimits` с нулевой инициализацией: `fieldString`, `fieldBytes`,
`fieldUnits`, `ownedFieldUnits`. Они ограничивают одну строку, суммарные исследованные строковые байты,
суммарные зарезервированные единицы работы декодирования и хранимые строковые units соответственно.
Ноль — нулевой допуск: нулевой encoded-byte бюджет запрещает даже префикс null; нулевые units разрешают
null и empty. Скалярные поля не списывают эти строковые бюджеты. Symbols и текст descriptor/name
сохраняют отдельные бюджеты.

| Позиция | Проверка/списание | Ошибка и приоритет |
| --- | --- | --- |
| До поля | Прежняя попытка wireFields | Прежняя FIELD ошибка; отвергнутая попытка не списывается. |
| Исследование prefix/body | Читать не более min(encodedBytes одного значения, остаток fieldBytes) | Каждый consumed byte списывается даже при ошибке. Неполнота на искусственной границе байтов даёт RESOURCE_LIMIT; реальный конец ввода — INSUFFICIENT_INPUT. |
| Полный count | Проверить -1..INT32_MAX, затем остаток байтов MESSAGE | INVALID_LENGTH до unit/resource проверок. Null успешен с нулевыми units. |
| До пропорционального allocation | Проверить count против min(codeUnits одного значения, остаток fieldUnits, доступные owned units) | RESOURCE_LIMIT после префикса; отвергнутая резервация units не списывается. |
| Допущенный count | Зарезервировать весь count как работу декодирования | chargedUnits сохраняется при повреждённом суффиксе, allocation failure, skip или замене; возврата бюджета нет. |
| Каждый символ | Полный хвост, continuation validation, BMP check, затем append | Частичного значения нет; исследованные байты остаются списанными. |
| Присваивание | Переместить decoded value, затем освободить units заменённого поля текущей записи | Временная замена сосуществует со старым значением и должна помещаться до декодирования. |
| Полная запись | Атомарно опубликовать, перенести текущий owned count в завершённый вывод | Ошибочная текущая запись отбрасывается; прежние завершённые записи сохраняют владение. |

Доступные owned units — остаток совокупного допуска вывода минус units текущей записи.
Это ограничивает завершённый вывод вместе с текущей записью и одной временной строкой, включая skipped
strings. Замены/skip освобождают временное хранилище, но не списания декодирования. Ограничение считает
логические UTF-16 элементы, не capacity контейнеров, metadata аллокатора или точные heap bytes.
`StreamDataResult` предоставляет `fieldBytes`, `fieldUnits`, `ownedFieldUnits`; последнее считает только
завершённый вывод. Tape переносит оба списания работы и завершённые owned units между всеми сообщениями,
включая ошибочные попытки, и предоставляет накопленные счётчики. Работа не вычисляется из конечных значений.
Учёт байтов использует вычитание до сложения; недоверенный count не вызывает непроверенный reserve.

Прежние приоритеты record/output/flags и errorOffset начала примитива неизменны. STREAM и RAW
используют общий field decoder. Append-only обновления описаний, владеющий завершённый префикс,
терминальные ошибки и атомарные heartbeat updates следуют [TAPE-READER](../protocol/TAPE-READER.ru.md)
и [RAW-TAPE](../protocol/RAW-TAPE.ru.md). Ошибочная строка не публикует частичную запись или изменения времени.

## Явная диагностическая схема и версия вывода 4

Локальный для утилиты адаптер `oracle-field-types-v1` задаёт record ID 0, имя `FieldTypes`, hasTime=false
со следующими логическими полями и точными lookup names. Схема не выводится из tape.

| Позиция | Имя | Полный тип | Java slot |
| --- | --- | --- | --- |
| 0 | Text | 10 | object 0 |
| 1 | Short | 40 | int 0 |
| 2 | Sequence | 72 | int 1 |
| 3 | Date | 88 | int 2 |
| 4 | Count | 8 | int 3 |
| 5 | Char | 2 | int 4 |
| 6 | Seconds | 56 | int 5 |
| 7 | Wide | 120 | int 6, physical VOID 7 |
| 8 | Tail | 10 | object 1 |

Профиль поддерживает оба существующих container modes и использует версию вывода 4 в обоих.
TSV header: `format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\t<mode>\n`.
Строки frame/descriptor/heartbeat/timing/markers/complete сохраняют прежнюю грамматику выбранного режима.
Строка данных: `field-record`, frame index, emitted index, local ID, name hex, symbol hex, values;
семь колонок разделены табуляцией. Values — разделённые запятыми `type:raw:payload` в логическом порядке.
Скалярный payload — `-`; payload типа 10 — `null`, `empty` или четырёхзначные UTF-16 units в lowercase hex.
Null и empty имеют raw 0 и различаются. Десятичные числа — канонические знаковые целые;
empty sentinel имён/symbols сохраняет `-`. Вывод — LF без BOM.

Human header: `qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=<mode> identities=local`.
Строка данных: `Fields <index>: frame=<frame> local=<id> name=<quoted> symbol=<quoted> fields={...}`.
Элементы: `field[i]=(type:T raw:R)` для скаляров или `field[i]=(type:10 raw:0 units:null|<quoted>)`.
Quoted values используют прежние безопасные UTF-8/escape правила без потерь, включая экранирование
управляющих символов и одиночных суррогатов. Event mapper и business/calendar/text interpretation не вызываются.

Каждая полная строка проверяется против остатка encoded output бюджета до allocation и первой записи.
Короткие записи или обнаруженные sink failures возвращают ненулевой exit и не выводят последующий completion.
Финальный flush может завершиться ошибкой после доставки completion row: только exit 0 подтверждает
успешный вывод. Прежние профили/версии вывода/диагностики неизменны, кроме текста help.

## Доказательства стандартной записи и ограничения

Неизменённый default Profile имеет 16 логических полей: десять WIDE_DECIMAL, ExdDiv.Date (88),
Halt.StartTime/Halt.EndTime (56), Flags (8), Description/StatusReason (10). Его 24 Java int slots включают
десять физических VOID tails; два object slots отдельны. Library tests передают явную локальную
проекцию, сохраняющую все логические поля и измеренные property-name lookup. Значения Java FileWriter/FileReader,
исходные описания и raw timing доказывают доставку полей, не Profile event mapping.
Примеры SHORT_STRING/SEQUENCE здесь синтетические; более широкие стандартные схемы могут требовать исключённые
LONG/indexed semantics. Импортер, indexed schema, compression, flags, дополнительные типы и транспорт
не разрешены. Частный tape не читается и не является входом приёмки.
