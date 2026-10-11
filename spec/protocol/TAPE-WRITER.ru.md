# Ограниченная запись tape, RAW/MESSAGE версии 1

[English](TAPE-WRITER.md). Английский текст является нормативным.

Контракт определяет исходящий поднабор RAW_MESSAGE_V1: BINARY без сжатия, RAW_DATA (5), время
MESSAGE, нулевые event flags и обычные точные полные типы 2/8/10/40/56/72/88/120. Этот ограниченный
API реализует writer в памяти; файловый ввод-вывод и публикация не предоставляются.
[RAW-TAPE](RAW-TAPE.ru.md), [FIELD-TYPES](../encoding/FIELD-TYPES.ru.md),
[STATIC-BINDING](STATIC-BINDING.ru.md) и прежние режимы reader сохраняются. Поля VOID на проводе,
новые представления, индексные записи, преобразования, адаптеры событий, запись STREAM/NONE/FIELD,
сжатие, транспорт и поведение сессий не допускаются.

## Основания и доказательства

Закреплённая ревизия — 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f в [upstream.lock](../../upstream.lock).
Одиннадцать производственных примеров и точный набор 43 артефактов описаны в
[captures](../../tests/fixtures/protocol/tape-write/CAPTURES.md),
[coverage](../../tests/fixtures/protocol/tape-write/COVERAGE.md) и
[validation](../../tests/conformance/protocol/tape-write/VALIDATION.md).
Они подтверждают конечные примеры Java FileWriter → FileReader, а не этот C++ API,
произвольную строгую перезапись или долговечность файловой публикации.

| Факт | Первичный источник и сохранённое наблюдение |
| --- | --- |
| Исходящие имена — имена свойств, не первые ключи поиска; хвосты VOID опускаются. | [BinaryRecordDesc:182–204](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L182); [проводное описание](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message-wire.tsv). |
| ID записи отправителя и порядок описаний полей задаются явно. | [BinaryQTPComposer:209](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L209); [Quote](../../tests/fixtures/protocol/tape-write/expected/extras/quote-raw-wire.tsv), [Profile](../../tests/fixtures/protocol/tape-write/expected/extras/profile-raw-wire.tsv). |
| Тип10 считает элементы UTF-16 и записывает отдельные UTFChar. | [IOUtil:339–361](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/IOUtil.java#L339); [значения](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message.tsv). |
| Строковый symbol использует FD и массив символов; повтор — FF; состояние сбрасывается на сообщение. | [PentaCodec:421–477](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java#L421). |
| Заголовок записи — symbol, затем compact sender ID; поля следуют описанию. | [BinaryQTPComposer:90](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L90), [BinaryRecordDesc:455](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L455). |
| Группировка MESSAGE для группы и одиночной записи различается; NONE теряет время, FIELD добавляет поля. | [группа](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-message-wire.tsv), [одиночные](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-singletons-wire.tsv), [FIELD](../../tests/fixtures/protocol/tape-write/expected/extras/extras-raw-field-time-wire.tsv). |

Форма API, строгое взаимно однозначное соответствие, нормализация symbols только строковым способом,
конечные бюджеты и атомарные терминальные операции ниже — сознательная ограниченная политика.
Паритет исключений Java и побайтное совпадение полного файла FileWriter не обещаются.

Сводка реализации и интеграционных доказательств приведена в
[валидации writer](../../tests/conformance/protocol/tape-write/WRITER-VALIDATION.ru.md).
Сохранённые входы/выходы native writer, наблюдения Java parser и журналы платформ индексированы
в архиве доказательств writer.

## Неизменяемые layouts и имена

OutgoingField владеет wireName:u16string, serialTypeId:int32_t и localFieldId:int32_t.
OutgoingRecord владеет senderId:int32_t, localRecordId:int32_t, wireName:u16string и упорядоченными
fields. OutgoingLayouts::create(const LocalSchema&,span<const OutgoingRecord>,LayoutLimits)
возвращает expected<OutgoingLayouts,WriterDiagnostic>; входы заимствуются на вызов, результат
владеет ограниченными копиями схемы и layouts, предоставляет только неизменяемые представления.

IDs отправителя находятся в0..INT32_MAX-1 и отличаются от локальных IDs. INT32_MAX отклоняется
как INVALID_LAYOUT при проверке ID записи. Закреплённый production-парсер вычисляет `id + 1`
в знаковом Java int в [BinaryQTPParser:501–515](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java#L501);
INT32_MAX вызывает переполнение, после которого доступ к таблице завершается ошибкой.
Меньшие большие IDs всё ещё требуют плотного Java-массива и зависят от ограничений длины
массива, heap и работы данной JVM; числовой диапазон не обещает универсальной совместимости
по ресурсам Java. Выбирайте IDs в пределах бюджетов принимающей реализации. Диапазон прежнего
reader не меняется; строгое переписывание отклоняет непредставимый исходящий ID, не перенумеровывает его.
Выделение памяти в нативном writer не индексируется
числовым ID отправителя. Локальный ID указывает существующую запись схемы; имя записи точно равно её имени.
IDs отправителя, локальные IDs и имена записей уникальны во всём наборе. Несколько входных sender IDs для
одной local записи при строгой перезаписи сознательно отвергаются вместо объединения.

Каждая исходящая запись покрывает каждое логическое поле схемы ровно один раз, без пропусков,
повторов и отсутствующих целей. Порядок может быть любой перестановкой. Полный тип совпадает
с локальным типом и поддерживается явным локальным профилем. Имя на проводе — переданный точный ключ поиска
для данного поля. Имена полей уникальны внутри записи, непустые и корректно закодированы UTF-16.
Нет нормализации, смены регистра, добавления префиксов, выбора псевдонимов или вывода имени свойства.
Пустые схемы/layouts и записи без полей допустимы; неиспользованные записи схемы могут не иметь
layout. hasTime=true, физические слоты Java и хвосты VOID непредставимы.

Порядок проверки: количество layouts/бюджеты схемы; записи по входному порядку — диапазоны IDs
и повторные идентичности, корректность/бюджет/совпадение имени, количество/бюджет полей; поля по проводному
порядку — диапазон цели, повтор цели, корректность/бюджет/поиск имени, точный тип; затем полное
покрытие целей. Ошибочное создание ничего не публикует. Исключения памяти/длины дают ALLOCATION_FAILURE.

## Точный API и владение

Пространство имён org::ttldtor::qdcxx::protocol; заголовки qd/protocol/tape_writer.hpp и позднее
qd/protocol/tape_admission.hpp. Компоновка принадлежит qdcxx_protocol, примитивы — qdcxx_encoding.
Публичные результаты используют типы проекта/STL. API однопоточный, без повторного входа,
поддерживает перемещение, не копирование; сброс, изменение параметров, обработчики и внешние настройки отсутствуют.

| Тип или метод | Контракт |
| --- | --- |
| OutgoingDescriptor | Владеет optional<u16string> version/options, точным u16string time и bool explicitRawSend; прежних логических флагов и неизвестных свойств нет. |
| OutgoingRecordView | Заимствованные senderId, ненулевой symbol:u16string_view, span<const FieldValue> values в локальном логическом порядке и eventFlags=0; действуют только на вызов. |
| WriterChunk | Владеет vector<byte>; одна успешная операция — один целый конверт, finish — пустой vector. За удерживаемые фрагменты отвечает вызывающая сторона. |
| TapeWriter::create(layouts,descriptor,WriterLimits) | Возвращает expected<TapeWriter,WriterDiagnostic>; полностью проверяет/копирует конфигурацию, READY без вывода байтов. |
| begin() | READY→ACTIVE, единственный дескриптор типа1. Повтор или неправильный порядок терминально ошибочен. |
| describe(span<const int32_t> senderIds) | ACTIVE, одна группа типа2 в заданном порядке из ещё не объявленных неизменяемых layouts. Пустая группа допустима. Повтор, уже объявленный или неизвестный ID отвергается. Объявления фиксируются лишь после полной успешной операции. |
| heartbeat(const TapeHeartbeat&) | ACTIVE, один кадр типа0; только пары content/timestamp (отсутствие,отсутствие), (0,отсутствие), (1,присутствующий signed64). |
| data(span<const OutgoingRecordView>) | ACTIVE, тип5 даже для пустой группы. Каждый ID уже объявлен. Порядок записей сохраняется; локальные values сериализуются через явные цели полей. |
| marker() | ACTIVE, compact-конверт нулевой длины, не подтверждающий EOF. |
| finish() | ACTIVE→FINISHED без неявного marker/heartbeat/description/байтов. Повтор FINISHED идемпотентен без расходов; READY finish ошибочен. |
| state()/diagnostic()/counters() | Неизменяемые READY/ACTIVE/FAILED/FINISHED, первая диагностика и логические счётчики; заимствование до уничтожения/перемещения. |

Операции возвращают expected<WriterChunk,WriterDiagnostic>. Каждая ошибка созданного writer делает
FAILED. Позднейшие вызовы возвращают ту же первую диагностику без чтения входа и расходов. После
FINISHED любая операция кроме повторного finish даёт INVALID_STATE и переводит в FAILED. Ошибка
операции не отдаёт байтов и не фиксирует объявления, время или счётчики вывода. Прежние фрагменты
независимы и могут составлять допустимый префикс, но не успешно завершённый tape. Уничтожение writer
не завершает его и не выполняет I/O.

Writer владеет неизменяемыми layouts/дескриптором и флагами объявлений. Между data-операциями symbol
не сохраняется; внутри кадра хранится не более одного ограниченного предыдущего symbol. Входы не
должны перекрываться с внутренней памятью. Исключения памяти/длины операций перехватываются как
терминальная ошибка. Гарантия внедрённого отказа выделения памяти без проверки реальной границы владения
не заявляется.

## Descriptor, frames и metadata

Дескриптор DXP3 содержит точный type=tape, необязательный непрозрачный ненулевой version,
необязательный opt (отсутствие, пустая строка, точный hs), обязательный time MESSAGE/message.
Null version/options/time и другой текст отвергаются; написание сохраняется. explicitRawSend=false
даёт пустой список отправки, true — ID5/name RAW_DATA без свойств; список приёма пустой. Канонический порядок свойств:
type, optional version, optional opt, time. Исходный порядок пар и ширина примитивов нормализуются.
Информация дескриптора, отсутствующая в API, требует отказа допуска, а не удаления. Конвертация
TapeDescriptor отвергает explicitTimeNone/explicitStreamSend, противоречия и будущие непредставимые поля.

Длина конверта — канонический compact-long размер compact-int message ID и полезной нагрузки. Type2 содержит
упорядоченные sender ID/name/field-count/name/full-type по прежней грамматике описания; диапазоны
предшествуют бюджету. Пустой type0 отличается от content0; content1 несёт точные знаковые compact-long
миллисекунды. Timestamp/cursor соответствуют RAW-TAPE, включая отсутствие, zero, отрицательные,
повторные, убывающие и насыщенные значения. Последовательные и конечные heartbeats/markers явные.
Data не синтезирует время и не делит группу при его изменении: необходимые границы heartbeat задаёт вызывающая сторона.

## Raw values и каноническое кодирование

FieldValue проверяется точно по FIELD-TYPES: тип10 имеет raw0 и допускает null либо точные элементы UTF-16;
скаляры не имеют units;8/40/56/72/88 — sign-extended int32;120 — любой int64;2 —0..0x10ffff включая
surrogate code points. Тип и локальный порядок соответствуют схеме. Null symbol непредставим;
пустой symbol, NUL и отдельные суррогаты разрешены в пределах бюджета. Ненулевые flags отвергаются
после бюджета попытки записи до чтения symbol/ID/values. Cipher и длина кодирования префикса flags
не входят в API или обещание семантической сохранности.

| Полный тип | Канонический writer |
| --- | --- |
| 2 | Прежний writeUtfChar(raw); кодовая точка вне BMP — один скаляр поля. |
| 8/40/56/72/88 | Прежний writeCompactInt(sign-extended raw), совместимый с Java compact-long/low32. |
| 120 | Прежний writeCompactLong(raw), без преобразования десятичного представления. |
| 10 | Новый writeCharArray(optional<u16string_view>,span<byte>,CharArrayLimits): compact-int count -1/null,0/empty или число элементов UTF-16≤INT32_MAX, затем прежний writeUtfChar каждого элемента. Пара суррогатов остаётся двумя элементами. |

Повторно используются атомарные compact/UTFChar/UTFString writers, без параллельных кодировщиков.
writeCharArray заранее проверяет count, кодированный размер, units и ёмкость с проверенной арифметикой;
ошибка пишет ноль и не меняет выход. Предел кодированных байтов включает префикс. Статусы: INVALID_LENGTH
для count, RESOURCE_LIMIT для бюджета, INSUFFICIENT_OUTPUT для ёмкости. Память не выделяет, вход не хранит.

Новая ограниченная компоновка symbol пишет FD и ненулевой writeCharArray для первого/отличающегося
symbol; FF — при точном равенстве units с предыдущим в этом кадре. Первый даже пустой symbol не FF.
F8/F9 и penta/cipher не выводятся. Это сознательная нормализация, включая текст wildcard `*`.
До приёмки реализации производственный Java должен подтвердить точные прочитанные элементы UTF-16 для пустого,
wildcard, исходно cipher, длинного, строкового и содержащего отдельные суррогаты symbol. Кратчайшие
байты Java cipher не обещаются; хранение и сравнение предыдущего symbol ограничены и учитываются.

## Конечные бюджеты и первая ошибка

Все бюджеты библиотеки — обязательные конечные size_t без неограниченного маркера. Ноль — реальный предел,
равенство допускается. Бюджеты layouts/schema проверяются до пропорционального выделения памяти;
точный размер кучи/ёмкости контейнеров не обещается.

| Бюджеты | Единицы и учёт |
| --- | --- |
| LayoutLimits {schema,records,fields,nameUnits,totalNameUnits} | Прежний SchemaLimits, копируемые layouts/поля, отдельные и суммарные имена записей/полей; работа копирования включает схему. |
| WriterLimits {frameBytes,outputBytes,frames,records,fields,text,textUnits,fieldString,fieldBytes,fieldUnits,symbolUnits,totalSymbolUnits,scratchBytes,scratchUnits,workUnits} | Байты тела/целого файла, конверты включая markers, попытки записей/полей, отдельный UTFString и общие элементы текста дескриптора/имён, отдельный тип10 и общие байты/элементы, элементы symbol и обращения к ним, пиковые временные байты/элементы, накопленная работа. |
| workUnits | Каждый просмотренный элемент схемы/layout, ID describe, поле/элемент имени, попытка записи/поля, элемент при проверке/измерении/кодировании/сравнении. Каждый проход учитывается отдельно, повторы и ошибки не возвращают расходы. |

scratchBytes покрывает одновременно владеющие буферы кадра/chunk до передачи; предпочтительно
измерить и выделить один конверт, без дублирования полезной нагрузки. scratchUnits покрывает временные строки
и предыдущий symbol; неизменяемые схема/layout/дескриптор ограничены отдельно. Входы и фрагменты вызывающей стороны
исключены. Попытки — все элементы, не только выведенные. Повторная обработка strings/symbols учитывается;
вывод — по каноническому размеру. Успех передаёт фрагмент и освобождает временное состояние. Накопленная
работа сохраняется на ошибке; счётчики подтверждённого вывода/кадров меняются только целой операцией.
Диагностика различает эти счётчики.

Счётчики используют subtraction-before-addition, длины проверяют представимость. Перед обходом или
выделением памяти проверяется оставшийся бюджет. Порядок: состояние; бюджет операции/группы/записи; структурные
и профильные проверки по входу; корректность текста/значений и отдельные бюджеты; накопленная работа;
измерение и бюджеты кадра/временной памяти/вывода; выделение памяти; кодирование; фиксация. Для записи: бюджет попытки, flags0,
symbol, sender ID/объявление, точное количество local values, поля по local order (tag/raw/units),
затем wire-порядок сериализации. Арифметика — RESOURCE_LIMIT; значение/layout — INVALID_VALUE/
INVALID_LAYOUT; неподдерживаемые profile/flags/type/descriptor — UNSUPPORTED; жизненный цикл —
INVALID_STATE; память — ALLOCATION_FAILURE. Ошибка создания отличается от ошибки операции writer.

Diagnostic владеет кодами и числовыми точками операции/записи/поля, использует статический stage,
не имена входа. Для невышедших байтов смещение файла не выдумывается. Ошибка I/O вызывающей стороны принадлежит ей;
writer не может подтвердить успешность внешнего вывода.

## Строгий допуск и проверки реализации

[TAPE-WRITE](../../docs/TAPE-WRITE.ru.md) определяет preflight, полное покрытие дескриптора, строгую
привязку и публикацию; [OFFLINE-REPLAY](OFFLINE-REPLAY.ru.md) — отдельное владение часами/отменой.
Одна декодированная запись не подтверждает допуск.

Обязательны независимые экземпляры/владение результатом, все типы и пределы raw, перестановки имён/
явные IDs, null/empty/суррогаты type10, все формы heartbeat/descriptor, отсутствие неявного footer,
ошибки порядка/неизвестных IDs/повторов, каждый бюджет zero/exact/excess, обработка allocation там,
где проверяется реальная граница. Java должен разобрать C++ chunks; сравнивается одна wire-проекция
со STREAM_DATA callbacks, не тройная RAW-доставка. Примитивы могут требовать exact bytes; целые tapes —
семантическое равенство действий/имён/IDs/типов/значений/symbols/raw time/cursor. Прежние fixtures,
режимы и версии вывода сохраняются.
