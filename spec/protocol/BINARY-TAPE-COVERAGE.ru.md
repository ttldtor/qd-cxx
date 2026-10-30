# Конечный каталог общего чтения бинарных tape

[English version](BINARY-TAPE-COVERAGE.md). Английская версия нормативная.

Статус: конечный каталог полей реализован для обычного чтения с явным `BINARY_FIELDS_V1`;
расширенная реализация и приёмка остаются ожидающими в строках ниже.
[Решение](../../docs/decisions/ADR-0004-broad-binary-tape-reading.ru.md),
[матрица из 71 строки](../../tests/fixtures/protocol/real-tape/support-matrix.tsv) и
[контракты проверок](../../tests/conformance/protocol/real-tape/CONTRACTS.ru.md) задают одну цель.
Строка, выведенная из исходника, или запланированный тест не означают успешную проверку совместимости.
Статус матрицы `implemented-bounded` относится только к указанной области наблюдения;
он не подтверждает все сочетания контейнера, времени, флагов и схемы.

## Цель и доказательства

Конечный набор эталонов: Java QD **3.355**, commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, и **3.356**, commit
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Первый остаётся закреплённой базой;
выбор второго не меняет `upstream.lock` и не подтверждает совместимость с последующими версиями.
Каждая строка матрицы ссылается на неизменяемые production-исходники обоих commits и отделяет
фактические доказательства от обязательных положительных и отрицательных наблюдений.

[Producer fixtures](../../tests/fixtures/protocol/real-tape/producer/README.ru.md) и
[их проверка](../../tests/conformance/protocol/real-tape/producer/VALIDATION.ru.md)
подтверждают три конкретных положительных случая Java FileWriter/FileReader и производное усечение
для каждой версии. Это compact/Quote, NONE/MESSAGE, нулевые flags и обычный STREAM_DATA.
Они не подтверждают новые типы, индексные схемы, ненулевые flags, файлы без заголовка и другие
семейства ниже. Создающий workflow квалифицирован на Windows; переносимые fixtures не требуют
Windows во время исполнения. Чтение в C++ и его проверки Windows/Linux — отдельные условия готовности.

Общий результат сохраняет sender identity, порядок полей и полный тип, символ, raw-биты скаляров,
точные UTF-16 units либо непрозрачные nullable bytes, flags данных и явно поддерживаемые метаданные
времени. Это каноническая проекция значений, а не сохранение каждого избыточного байтового кодирования.
Исполнение Java objects, бизнес-преобразования, адаптеры событий, ticker/history collectors,
подписки, RMI operations и транспорт вне этой цели. Содержимое частных записей не становится
публичными fixtures.

## Границы профилей

| Существующий выбор | Точная существующая политика полей | Расширенное поведение |
| --- | --- | --- |
| `COMPACT_INT_V1` | 8 | Без изменений |
| `QUOTE_FIELDS_V1` | 2/8/56/120 | Без изменений |
| `EXTENDED_FIELDS_V1` | 2/8/10/40/56/72/88/120; обычные records | Без изменений |
| `TapeMode::LEGACY` | Существующая политика descriptor/STREAM/нулевых flags | Без изменений |
| `TapeMode::RAW_MESSAGE_V1` | Существующая RAW/MESSAGE policy, выбранный local profile, нулевые data flags | Без изменений |
| Обычное wire discovery | Прежние точные ordinary types, добавление IDs, отказ при повторном sender | Без изменений |
| `BINARY_FIELDS_V1` | 19 точных идентичностей только для чтения; обычные и явно индексные локальные записи | Ограниченный набор полей и индексных схем реализован |
| `TapeMode::BINARY_CATALOG_V1` | STREAM/NONE, отсутствие/пустая строка/hs, известные флаги/время удаления и атомарные активные описания | Частичная реализация с явным включением; остальные семейства/опции/флаги ожидаются |

Профиль полей core и отдельный частичный режим протокола выбираются явно.
[Индексный контракт](../schema/INDEXED-SCHEMAS.ru.md) определяет реализованный набор.
Прежние конструкторы и значения по умолчанию не включают широкое обнаружение схемы,
индексную разметку или новую политику контейнера неявно.

Первый расширенный reader может использовать явно переданную проверенную локальную схему.
Новый интерфейс расширенного обнаружения должен быть явным и не восстанавливать `hasTime`
по именам записей. Ядро остаётся независимым от файлового I/O, транспорта, источников
конфигурации и выбора режима протокола.

## Точный каталог полей

Serialization mask равна `~0xf0`, а не `0x0f`; representation selectors задают альтернативы.
Общие serialization bits не разрешают полный identity вне списка. Named aliases, например TIME,
PRICE и MMID, не добавляют identity. Источник:
[SerialFieldType 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L60-L83),
[SerialFieldType 3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L60-L83).

| Полный ID | Имя | Физическое чтение / логическое raw-значение | Прежняя ограниченная поддержка; новая работа |
| --- | --- | --- | --- |
| 0 | VOID | Нет сериализованного значения; явное wire-поле может дать raw zero | Реализовано в скалярной части |
| 1 | BYTE | Знаковые 8 бит | Реализовано в скалярной части |
| 2 | UTF_CHAR | Прежний контракт UTFChar code point | Прежние точные профили |
| 3 | SHORT | Знаковые 16 бит, big endian | Реализовано в скалярной части |
| 4 | INT | Знаковые 32 бита, big endian | Реализовано в скалярной части |
| 8 | COMPACT_INT | Compact long, затем знаковые младшие 32 бита | Прежние точные профили |
| 9 | BYTE_ARRAY | Nullable byte count и непрозрачные bytes | Реализована владеющая полезная нагрузка |
| 10 | UTF_CHAR_ARRAY | Nullable character count и точные UTF-16 units | Прежний extended profile |
| 24 | DECIMAL | Compact long, затем знаковые младшие 32 raw-бита | Реализовано в скалярной части |
| 40 | SHORT_STRING | Compact long, затем знаковые младшие 32 raw-бита | Прежний extended profile |
| 56 | TIME_SECONDS | Compact long, затем знаковые младшие 32 бита секунд | Прежние Quote/extended profiles |
| 72 | SEQUENCE | Compact long, затем знаковые младшие 32 raw-бита | Прежний extended profile |
| 88 | DATE | Compact long, затем знаковые младшие 32 бита day ID | Прежний extended profile |
| 104 | LONG | Compact signed 64 | Реализовано в скалярной части |
| 120 | WIDE_DECIMAL | Compact signed 64 raw-биты | Прежние Quote/extended profiles |
| 137 | STRING | UTFString byte count и точные nullable UTF-16 units | Реализована владеющая полезная нагрузка |
| 152 | TIME_MILLIS | Compact signed 64 milliseconds | Реализовано в скалярной части |
| 168 | TIME_NANOS | Зарезервирован upstream | Не поддерживается; не кандидат реализации |
| 233 | CUSTOM_OBJECT | Nullable непрозрачные bytes | Реализована владеющая полезная нагрузка |
| 249 | SERIAL_OBJECT | Nullable marshalled serialization bytes | Реализована владеющая полезная нагрузка |

Расширенная цель содержит 19 типов-кандидатов. Все прочие полные IDs вне выбранного профиля,
включая комбинации representation/base, которые Java может читать. Отрицательные или превышающие
255 wire descriptor type IDs нарушают production-грамматику descriptor. Допустимый Java input
вне профиля получает unsupported diagnostic; это не утверждение, что Java считает его malformed.

Физическое декодирование следует production-коду
[BinaryRecordDesc 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L434),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L434).
Signed fixed widths, low-32 projection и two-slot long storage — обязательные физические правила.
Преобразования decimal/int/wide/long/shares/time units к другому local type исключены.
Для BYTE/SHORT нет default field-factory instance; production-пробы используют явный requested wire
description и подходящий production consumer, не выдумывают default schema.
Физические VOID tails хранения Java long не становятся дополнительными логическими значениями.
Raw-zero generic value разрешён только для реально присутствующего явного wire VOID-поля.

## Владение payload и грамматика

В конец `FieldValue` добавляется nullable owned byte payload. IDs 9/233/249 требуют raw zero,
отсутствия text units и различают null/empty/content bytes. STRING137 использует прежние nullable
точные UTF-16 units с raw zero и без bytes. У scalar tags нет text/byte payload. Смысл существующих
членов aggregate и все инварианты значений старых профилей сохраняются.

Есть четыре разные грамматики длины:

| Значение | Production-правило длины | Обязательное различие |
| --- | --- | --- |
| BYTE_ARRAY9 / default CUSTOM_OBJECT233 | Compact **long**, проверенный в -1..INT_MAX | Полная длина вне диапазона отвергается до пропорционального выделения памяти |
| SERIAL_OBJECT249 через MarshalledObjField | Compact **int** projection, затем проверка -1..INT_MAX | High/overlong compact forms могут дать иную длину, чем у byte-array |
| STRING137 | UTFString encoded-byte count | Сохранять принимаемые UTF/CESU/overlong forms и точные UTF-16 units |
| UTF_CHAR_ARRAY 10 | Character count, затем BMP UTFChar units | Сохранять различие границ primitive/message |

Правило marshalled length явно задано в
[Marshaller 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/Marshaller.java#L162-L175),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/dxlib/src/main/java/com/devexperts/io/Marshaller.java#L162-L175).
Общий reader сохраняет marshalled bytes и не вызывает Java object deserialization.
Неизвестные class/tag contents остаются непрозрачными; malformed framing/length всё равно ошибка.
Исполнение legacy PlainObjField вне цели. Локальный класс Java object field выбирает consumer decoder,
поэтому wire serialization base недостаточно для утверждения о поддержке object semantics.

Новые конечные лимиты разделяют per-value encoded/payload bytes, накопленные examined encoded bytes,
накопленную admitted payload work и retained bytes завершённых/текущего/временных payload.
Прежние string counters сохраняют смысл. Ноль означает нулевой допуск. Admission предшествует
каждому пропорциональному выделению/копированию; skip, replacement, failure и cancellation
не возвращают уже принятую работу. Опубликованные значения владеют содержимым и переживают
уничтожение source, reader и schema.

## Индексная разметка и поколения описаний

DESCRIBE_RECORDS содержит ID отправителя, имя записи и имена/полные ID полей; бита `hasTime` нет.
Индексная интерпретация требует явных локальных метаданных вызывающего кода либо подтверждённого
источника. Поддерживаются конечные формы: два логических int32 (`PAIR_INT 32`) либо один
логический int64 (`SINGLE_INT 64`) для первых двух целочисленных слотов Java.
[Проверка индексных схем](../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.ru.md)
содержит реальные наблюдения источника, экспортёра и парсера для обеих форм; знакомое имя
записи не определяет разметку. Остальные формы ожидают характеризации. Искусственный хвост
VOID в логические значения не добавляется.

Та же проверка описывает равные и переопределённые sender ID, смену записи/типов/порядка и
фактический reset Java. Новый режим подготавливает полную разреженную таблицу активных
отправителей, локальную схему и контекст, затем публикует их вместе небросающими операциями.
Прежние владеющие результаты сохраняют неизменяемую provenance. Заимствованные представления
схемы, описаний и контекста истекают при успешной публикации, перемещении либо уничтожении
владельца. Ошибка разбора, проверки, допуска ресурсов или выделения сохраняет прежнее полное
поколение. Активное представление содержит ограниченный текущий набор в порядке первого
появления отправителя, без неограниченной истории. Прежнее обычное обнаружение схемы
по-прежнему отклоняет повторный sender ID.

Для независимого закрытого источника явно создаётся новый reader; прежние результаты
остаются действительными. Заголовок и footer не сбрасывают состояние. Сброс существующего
reader остаётся последующей работой при наличии производственной потребности: его семантика
кадров, схемы, дескриптора, heartbeat, терминального состояния и бюджетов требует отдельного
ревью. Замена описания или смена источника не должна сбрасывать накопленную работу в обход
лимитов. Более сильная атомарная публикация намеренно отличается от поэлементной установки Java.

## Символы, опции и флаги

Известные формы символов: кодировки Penta 15/20/30/35 бит, FC UTF-8, FD CESU-8, FE для пустого
символа, FF для повтора, F8 для MRU-флагов и F9 для новых compact-int флагов. Зарезервированные
C0..DF и FA/FB отвергаются. Каждое сообщение данных начинает с чистого состояния символа и
MRU-флагов1; два префикса флагов перед одним символом — ошибка.
[Матрица](../../tests/fixtures/protocol/real-tape/support-matrix.tsv) ссылается на PentaCodec обеих ревизий
и [ограниченную интеграцию флагов](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.ru.md).

`hs` — единственная известная опция протокола. Для отсутствия, пустого и заданного значения нужны
отдельные наблюдения производителя. Java writer маскирует флаги по согласованным опциям и
семейству; reader символов игнорирует набор опций и читает исходные флаги. Новый reader сохраняет
известную маску данных и без `hs` в дескрипторе; ограничение writer не становится выдуманным
ограничением parser. Неизвестные опции явно не поддерживаются. Прежние режимы отклоняют ненулевые флаги.

| Бит | Метаданные данных |
| --- | --- |
| 0x01 | TX_PENDING |
| 0x02 | REMOVE_EVENT |
| 0x04 | SNAPSHOT_BEGIN |
| 0x08 | SNAPSHOT_END |
| 0x10 | SNAPSHOT_SNIP |
| 0x40 | SNAPSHOT_MODE |

Конечная маска данных — **0x5f**: всем 64 подмножествам нужны производственные наблюдения во всех
четырёх семействах, включая комбинации, отбрасываемые обычным writer. Бит0x20 зарезервирован;
REMOVE_SYMBOL0x80 не является флагом writer данных, остальные биты вне профиля. Допуск этих битов
исходным parser фиксируется отдельно от выбранного отказа C++. Collector снимков/транзакций не добавляется.

REMOVE_EVENT меняет грамматику данных: специальные встроенные поля событий, затем один compact-long
времени записи; обычные поля пропущены. Optional `removalTime` в конце владеющей записи сохраняет
это точное время отдельно от FIELD-метаданных события. Явная индексная раскладка проецирует время
в локальные слова; остальные пропущенные значения получают свежие defaults. SymbolValue уже
владеет исходными флагами, дублирующее поле не добавляется. Встроенные FIELD-метаданные ожидают
QDCXX-046 и отвергаются до реализации.

Ограниченная реализация использует `decodeStreamDataWithEventPolicy` и BINARY_CATALOG_V1 с
переданной схемой только для STREAM/NONE. На обеих ревизиях реально наблюдались все 64 подмножества
в каждом семействе Java, включая шесть исходных файлов FileWriter/FileReader с полным TimeAndSale;
это не включает другие семейства контейнеров C++. Опции могут отсутствовать, быть пустыми или
точно равняться hs; неизвестные, повторённые и содержащие пробелы токены отклоняются. Прежние
режимы и функции payload принимают только нулевые флаги. Старые форматы formatter отклоняют
ненулевые флаги либо любой removalTime, включая0, до построения буфера; число байтов явного
нулевого префикса сохраняет прежний вывод.

## Политика messages и descriptors

| IDs | Целевое поведение |
| --- | --- |
| 5 RAW_DATA, 10 TICKER_DATA, 15 STREAM_DATA, 20 HISTORY_DATA | Одна generic wire-record sequence с исходной family identity; 10/20 ожидают 045 |
| 0 HEARTBEAT | Прежняя ограниченная metadata policy; расширения требуют явных доказательств |
| 1 DESCRIBE_PROTOCOL | DXP3 properties, send/receive descriptors и optional endpoint tail; точная расширенная политика ожидает 045 |
| 2 DESCRIBE_RECORDS | Точные field descriptions и проверенная generation policy |
| 3 DESCRIBE_RESERVED | Ожидается bounded ignore policy; без output records |
| 4 PART | Reassembly не поддерживается |
| 11/12/16/17/21/22 | Subscription payload operations не поддерживаются |
| 49/50/51/52/53/54/55/56 | RMI payload operations не поддерживаются |
| 61 TEXT_FORMAT | Nonbinary marker не поддерживается |
| 75 ZIP / 139 GZIP | Container detection markers; ожидают 048, не inner data messages |
| Остальные IDs | Типизированный unknown-message refusal |

Исходная family уже есть в возвращаемом envelope; дублировать type в каждом record не нужно.
Общее charged decoding обслуживает четыре data families, не меняя контрактов существующих прямых
`decodeStreamData`/`decodeRawData`. RAW не разветвляется в три C++ collector callbacks.
Wire order не обещает Java collector delivery order.

Обычный binary descriptor — DXP3, а не text-token representation. QDCXX-045 должна проверить optional
endpoint tails, unknown properties/message descriptors и повторные descriptor deltas до выбора
ограниченного preserve/ignore/refuse поведения. Malformed counts, strings, magic и tails дают ошибку;
поздняя ошибка descriptor не превращает корректный prefix в complete success.

**Self-described binary без заголовка** ожидает реального положительного FileWriter case в 045 и
включается только новым mode. Каждый data sender всё равно описывается до использования. Выбранная
политика не требует heartbeat footer при корректном завершении всех frames. CSV/BLOB bare formats —
отдельные небинарные форматы. `schemeKnown`, `readAs`, эвристическая resynchronization,
ignore/continue/cycle и implicit numeric local IDs вне цели. Допустимые Java alternatives отвергаются
явно; malformed input терминален с документированным completed prefix и точными координатами.

## Будущие gates и приёмка

NONE и прежний MESSAGE subset остаются существующими ограниченными возможностями. FIELD (046),
LONG/TEXT sidecars (047), GZIP/ZIP (048), finite split sequences (049) и расширенный operational
streaming (050) ожидаются. Контракт не добавляет compression dependency, download или callback/source API.
До 048 выбираются dependency/license/platform/OFF acquisition, member/entry и CRC/trailer/inflation
policies. До будущего sink API определяются callback lifetime, stop/exception checkpoints,
публикация completed prefix и layered source/close/EOF completion.

Полная общая поддержка требует для каждой применимой pending-строки production positive/negative
evidence обоих commits, совпадающих C++ canonical observations, точных resource/ownership/error
проверок и независимого final-source review. Прежние ограниченные результаты действуют только
в своих документированных profiles. Достижение EOF структурным обходом не закрывает незавершённые строки.

Для владеющих байтовых, строковых и объектных полей выполнена отдельная [проверка](../../tests/conformance/protocol/field-types/OBJECT-VALIDATION.ru.md).
Точные общие грамматики 249/137 действуют и при отбрасывании значений и намеренно
отличаются от пропуска неизвестных локальных полей Java; непрозрачные байты не доказывают семантику объектов Java.


