# Ограниченный декодер STREAM_DATA

## Дополнительная идентичность локальной записи

[Контракт подготовленного отображения](../events/EVENT-MAPPING.ru.md) добавляет `core::RecordContext`
в каждый `StreamRecordPlan` и в конец `StreamDataRecord`. Известные планы сохраняют непрозрачную
идентичность исходной локальной схемы и локальный ID записи; STREAM и RAW копируют этот контекст
в завершённые записи. Неизвестные планы имеют невалидный контекст и по-прежнему не выдают запись.
Прежние агрегатные инициализаторы остаются допустимыми и создают невалидный контекст; они не
подтверждают принадлежность значений для подготовленного API событий. Идентичность не владеет
объявлениями схемы, исходными именами или состоянием протокола и переживает их уничтожение.
Wire-байты, выходные значения, счётчики работы, позиции ошибок и прежний порядок ошибок не меняются.

## Явный метод для RAW payload

QDCXX-023 добавляет `decodeRawData(payload, context, limits)` для полного payload типа 5 по
[RAW-TAPE](RAW-TAPE.ru.md). Наблюдения production FileReader подтверждают те же точные поля,
новое состояние символов и проекцию значений с нулевыми flags в этом ограниченном подмножестве.
Оба публичных метода используют внутреннюю реализацию payload и сохраняют результат, бюджеты,
позиции и терминальный завершённый префикс. `decodeStreamData` остаётся методом для типа 15.
Ни один API не выбирает произвольные типы сообщений, не применяет время heartbeat и не реализует
Java callbacks ticker/stream/history; tape-слой применяет время один раз для реального RAW frame.

## Явный типизированный профиль

QDCXX-022 добавляет opt-in [контракт полей Quote](../encoding/QUOTE-VALUES.ru.md). Снимок копирует
один ограниченный типизированный raw-zero набор на локальную запись и сохраняет проверенный профиль.
Точный входной тип выбирает UTF_CHAR или compact-long даже для skips. Неверный UTF_CHAR даёт
добавленный INVALID_FIELD на FIELD с началом примитива и фактическим потреблением; прежние enum
ordinals сохранены. Расширенные записи заполняют только typedValues, прежние — только values.
emittedValues считает логические поля один раз. Default context, symbols, flags, приоритет ошибок
и всё прежнее поведение ниже сохраняются; преобразований типов и dynamic session не добавлено.

[English version](STREAM-DATA.md). Английский документ является нормативным.

QDCXX-016 принимает один полный payload type-15 без внешней оболочки. Поддерживаются только обычный
точный COMPACT_INT (8), EMPTY_SET, raw eventFlags=0, без временных индексов, metadata и replacers.
Transport, writer, dynamic session, диспетчеризация описаний, importer и event API не реализованы.

## Доказательства

Добавленный [production oracle](../../tools/upstream/stream-data-oracle/StreamDataOracle.java)
использует закреплённый QD 3.355 BinaryQTPParser с настоящей схемой S/R/E и предшествующими полными
описаниями. [Inputs](../../tests/fixtures/protocol/stream-data/inputs.tsv) — вручную заданные воздействия,
а не ожидаемые результаты. Каждый строгий префикс получает собственный полный кадр type-15. Callbacks
дают локальные ID, UTF-16 hex без потерь, упорядоченные int values, flags, числа batches и ошибки.
P01/P03/P04 и существующие [binding evidence](../../tests/fixtures/protocol/binding/COVERAGE.md) сохранены.

BinaryQTPParser.parseData сбрасывает символы на сообщение, читает символ, затем readCompactInt sender ID.
BinaryRecordDesc.readFields использует readCompactLong для SER_COMPACT_INT, включая skip actions;
FLD_INT приводит к знаковым младшим 32 битам. C++ воспроизводит эту проекцию без signed cast вне диапазона.
Повторные назначения применяются в wire order. Свежие отсутствующие обычные int slots содержат raw zero;
это не прикладное значение по умолчанию. readRecord выбирает readRemoveEventFields для REMOVE_EVENT и
читает compact-long time вместо обычных полей. C++ отклоняет любые ненулевые raw flags до sender ID,
включая принимаемые Java flags; явные нулевые flags допустимы. Composer writeMessageHeader сбрасывает
writer, а writeRecordPayload вызывает BinaryRecordDesc.writeRecord. Источники закреплены в
[SOURCES](SOURCES.md). Первый capture предшествовал C++ реализации; см. парные fixture reports.

## Неизменяемый контекст

StreamDataContext::create принимает проверенную LocalSchema и уникальные raw descriptions, никогда
не принимает mutable RecordBinding plans. SchemaLimits ограничивает записи/поля/lookups/отдельные и
суммарные имена схемы; ContextLimits и bindRecord ограничивают описания, суммарные actions и имена.
Хранилище схемы проверяется, но не копируется: snapshot владеет только планами и числом выходных полей
каждой локальной записи. Указатели и имена не сохраняются, массивы не индексируются sender ID. Пустой
контекст допустим. Отрицательные/повторные sender ID и неверные описания дают INVALID_DESCRIPTION;
не-8 описания дают UNSUPPORTED_PROFILE. Неудачное создание ничего не публикует. Запрет дубликатов —
ограничение API, а не Java replacement semantics. Нет конструктора по умолчанию; перемещение сохраняет
инварианты контекста. Read-only lookup заимствует план только на время жизни контекста.

Порядок валидации: бюджеты схемы в порядке записей, число описаний, затем для каждого описания duplicate
ID, bindRecord с оставшимися суммарными actions/names, учёт проверенных имён/actions, добавление полного
плана. Категории binding errors сохраняются. bad_alloc/length_error дают ALLOCATION_FAILURE.
Новые schema semantics или loader не добавлены.

## Результаты, приоритеты и ресурсы

StreamDataLimits требует конечные payloadBytes, processedRecords (включая skipped/zero-field),
wireFields, emittedRecords, emittedValues, ownedSymbolUnits и SymbolLimits. Ноль допустим.
SymbolLimits.inputBytes означает остаток payload span, а не длину кодировки символа. Логические счётчики
ограничивают элементы, а не allocator overhead/capacity. Суммарные symbol units учитывают каждую выходную
копию, включая repeat; временный текущий символ и состояние reader отдельно ограничены SymbolLimits.

1. Превышение размера payload даёт ошибку PAYLOAD в позиции ноль. Корректность контекста обеспечена созданием.
2. В начале каждой непустой записи учитывается processedRecords на RECORD до чтения символа.
3. Символ читается на SYMBOL с сохранением SymbolStatus. Truncation даёт TRUNCATED_PAYLOAD,
   неверная кодировка/длина — INVALID_SYMBOL; resource/allocation сохраняют свои категории.
4. Ненулевые flags отклоняются на FLAGS после символа, до любых record-specific данных.
5. Compact-int sender ID читается на RECORD_ID. Отрицательный даёт INVALID_ID, отсутствие mapping —
   UNDESCRIBED_ID после полного ID. Обрезанный primitive потребляет доступный префикс.
6. Для emitting plan оставшиеся бюджеты выходных записей/values/symbol проверяются на OUTPUT до выделения
   нулевых локальных values. Unknown records не выделяют выходные values и потребляют все поля.
7. До каждого поля проверяется/учитывается wireFields на FIELD; readCompactLong и назначение младших
   32 бит целевому полю. Skipped fields используют тот же primitive и truncation. Ширины не угадываются.
8. Только полная текущая запись добавляется на OUTPUT; выходные счётчики обновляются после успешного append.
   При успехе stage END и consumed/errorOffset равны payload.size(). Пустой вход успешен.

Первая ошибка возвращает status, stage, payload-relative consumed, errorOffset и завершённые записи.
Primitive errorOffset — начало primitive; проверки validation/resource используют текущий consumed.
Symbol cause заполнен только для чтения символа; stage определяет compact primitive при других truncations.
Перехваченные ошибки выделения на активном checkpoint сохраняют предыдущие записи; частичная текущая
запись не выходит наружу. SymbolReader сообщает committed examination positions. Ошибка выделения vector
указывает checkpoint без дальнейшего чтения байтов. Allocation-failure injection и точная гарантия объёма
памяти allocator не заявляются.

Каждый вызов создаёт новый SymbolReader; нет continuation/recovery state или заимствования между кадрами.
Результаты владеют local ID, точным SymbolValue и int32 values в локальном порядке; переживают input/context
и следующие вызовы. Сохранение completed-prefix в C++ — явно разрешённая задачей политика: Java удаляет batch
текущего повреждённого сообщения (late-failure), но может доставить предыдущие валидные сообщения
(reset-repeat). Java callbacks, внешние позиции кадров и исключения — наблюдения, а не ожидаемые C++
позиции/batching.

## Валидация

[Fixture validation](../../tests/fixtures/protocol/stream-data/VALIDATION.md) и
[conformance validation](../../tests/conformance/protocol/stream-data/VALIDATION.md) фиксируют выполненные
проверки и оставшиеся ограничения. Полная
QTP-совместимость, event semantics, огромные allocations, другие архитектуры и transport не подразумеваются.

<a id="aggregate-accounting"></a>

## Учёт списанной работы

Контракт агрегированного учёта добавляет в StreamDataResult
`processedRecords` и `wireFields` с нулевой инициализацией. Они считают списания бюджета,
а не только завершённые записи/поля: увеличение после успешной проверки в существующей точке
уменьшения, до чтения символа или поля. Учитываются пропуски, повторные цели и попытки, позже
завершившиеся ошибкой. Отказ проверки бюджета не учитывается. Все возвраты, включая ошибки
выделения памяти, сохраняют накопленные списания; каждое ограничено исходным лимитом.
Порядок проверок, wire-семантика, завершённые результаты и consumed/error позиции неизменны.
Вызывающий код может переносить бюджеты файла между сообщениями: передавать min(лимит сообщения,
общий остаток) и вычитать возвращённые фактические списания.

## Явный профиль дополнительных полей

[FIELD-TYPES](../encoding/FIELD-TYPES.ru.md) определяет EXTENDED_FIELDS_V1 с точными 2/8/10/40/56/72/88/120,
владеющим nullable UTF-16, логическими позициями полей независимо от Java slots и накопленными бюджетами строковой работы/хранения.
Оба прежних tape modes сохраняют политики descriptor, flags, времени, терминальных ошибок и append-only updates.
Прежние исключения типов ниже описывают старые профили; только этот явный профиль расширяет их.
Profile event adapter, indexed schema, импортер и неявное обнаружение схемы не добавляются.
