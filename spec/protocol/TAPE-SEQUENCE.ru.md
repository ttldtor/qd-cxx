# Конечные последовательности самодостаточных tape-файлов

[English version](TAPE-SEQUENCE.md)

## Выбор и независимость

`SELF_CONTAINED_SEGMENTS_V1` — явный конечный список вхождений в порядке вызывающей стороны.
Повторный путь означает повторное воспроизведение с повторной полной резервацией. Маски, сортировка,
дедупликация, поиск файлов, follow, cycle, часы, scheduler и retention не подразумеваются. Отсутствующий
выбранный файл и физически пустой источник данных завершают обработку ошибкой. Отсутствие записей
в корректном, независимо описанном сегменте допустимо.

Каждое вхождение получает новые parser, sender mapping, generation схемы, container, scanner и timing.
Последовательность выбирает одну однородную политику `NONE`, `MESSAGE`, `FIELD`, `LONG` или `TEXT`.
Начальный descriptor каждого сегмента должен ей соответствовать, а sender declarations должны
предшествовать использованию. Только явный выбор вызывающей стороной `NONE` разрешает отсутствие time
property в новом начальном descriptor, как у producer по умолчанию. Остальные политики требуют явного
совпадающего объявления. Headerless/dependent-сегменты, смешанные или отсутствующие обязательные time
declarations отклоняются до публикации данных этого вхождения. Совместимые изменения схемы остаются
в пределах существующей политики parser.

`NONE` запрещает timestamp-bearing MESSAGE heartbeat. `MESSAGE` требует такой heartbeat до первой
публикации data-family frame, включая пустой data frame; явный ноль является присваиванием.
`FIELD` включает явную политику READ, требует raw
FIELD metadata у каждой опубликованной записи и запрещает MESSAGE overlays. `LONG` и `TEXT` требуют
явного companion и применения первой отметки до первой публикации data-family frame, включая пустой
data frame; MESSAGE overlays запрещены.
Первая отметка может следовать после объявлений без данных. Отсутствие присваивания нельзя выводить
из упакованного нуля. Относительный синтаксис и обращение к окружающим часам не поддерживаются.

Tool-owned интерфейс находится в
[`tape_sequence.hpp`](../../tools/tape-inspect/tape_sequence.hpp). Core и protocol не открывают файлы,
не разрешают имена, не выводят companion и не получают окружающие часы. Контракты отдельных источников
описаны в [TAPE-CONTAINERS](TAPE-CONTAINERS.ru.md) и [SIDECAR-TIME](SIDECAR-TIME.ru.md).
Владение FIELD и raw metadata соответствуют [TAPE-TIME](TAPE-TIME.ru.md).

## Admission и бюджеты

`admitSequence` синхронно проверяет все спецификации вызывающей стороны до копирования путей,
выделения памяти, открытия или работы с потоками. Он не сохраняет caller span, путь, схему, callback
или динамический текст. Фиксированная диагностика содержит ошибку, нумеруемое с нуля вхождение и
статическую метку компонента. Admission подтверждает только конфигурацию, но не существование,
содержимое, целостность или завершённость источника.

Все per-source limits уже полностью разрешены. Admission не вызывает `prepareLimits`, не выводит
default, не увеличивает ёмкость и не приравнивает независимые parser limits. Он проверяет существующее
требование frame: pending capacity вмещает maximum body плюс девятибайтовый compact prefix;
положительные представимые IO chunks, совместимость container scratch, retained scanner scratch и
ненулевые construction/snapshot capacities для начальной discovery identity. Остальные нулевые
ёмкости остаются буквальными конечными отказами при попытке соответствующей работы.

`SequenceReservations` содержит покомпонентные суммы полных настроенных ёмкостей каждого вхождения.
Вызывающая сторона задаёт явные общие allowances; per-source defaults автоматически не умножаются.
Неиспользованные ёмкости не возвращаются после close, ошибки или отмены; одно вхождение не заимствует
ёмкость другого. Возможен консервативный отказ, даже когда фактические данные поместились бы.
Резервация не является измерением байтов, работы или RSS.

Учитывается каждое вложенное поле `TapeLimits`: frame storage; accepted input и envelopes; descriptor
properties, messages, encoded text и UTF-16 units; schema records, fields, lookups и names; context
descriptions и binding actions/names; construction и snapshot units; processed/emitted records,
values, symbols, wire fields, per-value string/payload limits, cumulative examined bytes/work,
temporary/owned payload/string storage, FIELD metadata owners и timing attempts. Per-value и retained
peak capacities также суммируются.

Data и выбранный companion резервируют отдельные полные векторы `ContainerLimits`: compressed и
inflated bytes, ratio/slack, GZIP members, ZIP entries, single/cumulative header bytes, work, inflate
calls, retained bytes, inflater allocation bytes и chunk capacity. Scanner выбранного companion
резервирует все input, line-size, line-count, mark-count, work и retained capacities. Feed buffers,
encoded source output и event-copy units суммируются отдельно.

Каждый container, включая `NONE`, резервирует один physical excess-probe byte и один отбрасываемый
inflated excess-probe byte для каждого повторного вхождения и выбранного companion. Эти явные общие
allowances не увеличивают admitted data. Опубликованные inflated bytes остаются внутри обычных
лимитов; диагностические наблюдения учитываются отдельно. Обычные work/inflate-call gates действуют
и для probes; нулевая работа не разрешает работу.

Имена учитываются в native code units без преобразования, нормализации или filesystem access.
Пустые имена и встроенный native NUL недопустимы. Defaults разрешают 256 вхождений, 4096 native units
на путь и 1 Mi native name units суммарно. Сумма включает повторные data/companion names. Безопасное
диагностическое кодирование резервирует два hexadecimal characters на native byte. Copied storage
резервирует объекты вхождений и все native characters/terminators с проверкой произведений и сумм.
Owner держит не более одной активной пары, то есть двух физических handles; меньший настроенный
лимит должен вмещать выбранное число ролей. Хранилище четырёх фиксированных diagnostics (primary,
data close, companion close и flush/reporting) тоже проходит admission. Это логический учёт storage,
а не измерение allocator overhead или RSS. Wall-clock deadline не обещан.

Один общий output allowance покрывает source boundaries, data, diagnostics и final summary.
Terminal reservation недоступна для публикации источников. Сумма per-occurrence output capacities
должна вмещаться в остаток. Owner отделяет фактические output counters от capacity reservations
и отклоняет непредставимые суммы до allocation/open. Защищённая terminal capacity по умолчанию равна
65536 encoded bytes; проверяемый минимум учитывает фиксированные 92 aggregate resource rows, одну
summary и не более четырёх diagnostic rows. Per-source layer resource rows расходуют отдельную
publication capacity источника; overhead грамматики source identity проходит отдельный admission.

## Identity, lifetime и завершение

Identity сохраняет source occurrence ordinal, segment-local generation, sender/local IDs и segment
wire record ordinal. Проверяемый aggregate emitted ordinal учитывается отдельно. Raw FIELD
sourceOrdinal не переписывается в якобы глобальный ID. Совпадающие local IDs/generations разных
сегментов не делают схемы взаимозаменяемыми. Records владеют payload и FIELD bytes; schema views
потребляются синхронно до освобождения при новой generation или segment. Retaining test sinks
копируют ограниченный schema snapshot, а не сохраняют reader pointers.

Owning operation сохраняет обнаруженную container или parser failure до fallible rendering завершённого
префикса. Только доступный produced prefix обрабатывается через bounded feed/resubmit/render checkpoints.
Если дальнейшее понимание префикса требует ещё не прочитанного companion time, обработка останавливается
на безопасном checkpoint с исходной причиной; новый companion read после неё запрещён.
Source refusal, cancellation или sink stop запрещают
последующие reads, opens и transitions. Cleanup не дочитывает input. Logical accepted/frame/payload
offsets, compressed physical read/consumed positions, sidecar positions и проверяемые aggregate
counters остаются раздельными; обратное compressed mapping не выдумывается.

Parser finish, EOF/integrity каждого выбранного container, scanner END/strict tail/position finish и
physical close — разные требования. Adapters освобождаются до закрытия обоих открытых файлов.
Обе physical close attempts выполняются и после первого отказа. Первая causal error остаётся
неизменной; ограниченная secondary cleanup diagnostic её не заменяет. Сохраняются только secondary
slots для data-close, companion-close и flush/reporting; более поздние semantic/sink/cancel causes могут
не попасть в эту фиксированную историю, при этом исходная причина остаётся primary. Первая cleanup error становится
primary при отсутствии предыдущей причины. Повторные terminal checks не выполняют IO или close.

Aggregate COMPLETE разрешён только после закрытия последнего вхождения и всех его слоёв, записи
final summary и успешного final sink flush. Ошибка summary/flush оставляет последовательность
незавершённой, даже если все записи опубликованы. Прежний per-file inspector нельзя использовать
как black-box sequence operation: его per-file COMPLETE/flush не доказывают это условие.

## Граница совместимости

Java FileReader может сохранять sender mappings и heartbeat time между файлами одного формата,
пропускать отсутствующие/повреждённые файлы и только записывать некоторые поздние integrity/close
ошибки в журнал. Этот явный native profile требует независимых сегментов и завершает их обработку
такими ошибками. Это finite selection policy, а не parity с Java wildcard selection или общий
session-continuation API. Исследование закреплённых production FileWriter/FileReader и native
validation являются раздельными обязательствами по доказательствам. Первичные baseline-источники:
[FileWriterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java)
и [FileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java);
compatibility baseline закреплена в [upstream.lock](../../upstream.lock).

## Формат вывода 9 с идентичностью источника

Явный режим последовательности использует отдельную грамматику без потери значений. Форматы 1–8
и их отказ для метаданных FIELD не меняются. Этот вывод не является форматом обратной сериализации QTP.
Точный заголовок TSV:

```text
format<TAB>qdcxx-tape-inspect<TAB>9<TAB>binary-catalog-v1<TAB>self-contained-segments-v1
```

До открытия источников каждое вхождение получает строку `source` из пяти ячеек: метка, нулевой номер
вхождения, native-кодировка (`utf16le` в Windows или `bytes` в POSIX), lowercase hex native-байтов пути
данных и такой же hex пути companion либо `-`. Кодовые единицы UTF-16 кодируются байтами little-endian.
Пути заимствуются только на время форматирования; преобразования кодировки, нормализации и вывода
сырого текста пути нет. `sourceIdentityBytes` вычисляет точный размер строки без выделения памяти
и IO с проверкой переполнений.

Каждый сегмент начинается со строки `source-begin<TAB>ordinal`, сразу после которой следует
четырёхъячеечная строка `source-policy<TAB>POLICY<TAB>DATA_CONTAINER<TAB>TIME_CONTAINER_OR_-`.
Имена политик и контейнеров пишутся строчными буквами. Политика относится к текущему вхождению;
временная политика однородна, но явно выбранные контейнеры разных вхождений могут различаться.
Существующие строки frame, descriptor-v8, schema, source-schema, schema-end, heartbeat и timing
относятся к текущему вхождению. Поколения схем и sender/local ID записей локальны для сегмента.

Форматтер записи использует заимствованную неизменяемую схему до смены поколения или источника.
Он не сохраняет схему, запись, входной span, путь или callback и не копирует payload ради удаления FIELD.
Проверка полной схемы с точными типами, provenance, формы значений и явного назначения времени
предшествует подсчёту и выделению памяти для строки. Отдельный владелец источников проверяет порядок
объявлений и однородную временную политику до публикации записей.

`wire-data-v9` содержит ровно 27 ячеек, включая метку:

```text
label,sourceOrdinal,frameIndex,segmentRecordIndex,aggregateRecordIndex,generation,senderId,localId,
recordNameHex,symbolHex,typedValues,externalTimestampMs,externalPacked,externalEventTimeMs,flags,
removalTime,fieldPresent,fieldRawSeconds,fieldRawSequence,fieldSourceOrdinal,fieldPacked,
fieldEventTimeMs,timePolicy,externalOrigin,effectiveOrigin,effectivePacked,effectiveEventTimeMs
```

Typed values, flags, removal time, UTF-16 имён и символов сохраняют точную грамматику output8, все
девятнадцать полных идентичностей полей и различия null/empty/absent. `fieldPresent` равен 0 или 1.
При отсутствии FIELD следующие пять ячеек равны `-`; при наличии исходные секунды, optional sequence
и локальный номер попытки исходной записи сохраняются без изменений. Packed-проекция FIELD сохраняет
переполнение младших слов и миллисекундные подполя 1000–1023. Отсутствующая raw sequence отличается
от явно записанного нуля.

Внешнее назначение сохраняет точный optional timestamp и отдельный packed cursor, включая назначенный
ноль. Origin равен `none|message|sidecar`. Политика NONE выбирает effective packed zero; иначе сначала
выбирается ненулевой external packed, затем присутствующий FIELD, затем ноль. Effective origin равен
`zero|external|field`. Обычные поля Time никогда не становятся метаданными. Назначенный внешний ноль
сохраняет timestamp и origin, даже когда effective time берётся из FIELD.

После успешного завершения всех слоёв и закрытия физических источников `source-end` содержит девять
ячеек: метку, ordinal, принятые логические байты данных, frames, emitted records, processed records,
wire fields, фактически полученные физические байты данных и фактически потреблённые байты
контейнера данных. Resource-строки содержат шесть ячеек: `resource`, статический scope
(`sequence|data|time|scanner|parser`), ordinal либо `-`, статическое имя счётчика, зарезервированную
ёмкость и фактически наблюдавшийся счётчик либо `-`. Резервы не выдаются за измерения.

Единственная итоговая строка `complete-v9` содержит семь ячеек: метку, успешно закрытые источники,
принятые логические байты данных, frames, emitted records, processed records и wire fields. Она
допускается лишь после всех source-end и предварительного flush данных. Байты маркера предварительны
до успешного итогового flush и успешного результата либо exit zero; поздняя ошибка flush означает
FAILED, даже если байты маркера уже видны.

Диагностика содержит ровно тринадцать ячеек: `incomplete-v3` (либо вторичная `cleanup-v3`), ненулевой
exit code, статический stage, статический reason, optional ordinal источника, layer
(`data|time|parser|sequence|sink`), физические байты fetched, физические байты consumed, логические байты
produced, байты parser accepted, логическое начало frame, логическое начало payload и sidecar line.
Неизвестные координаты равны `-`; известный ноль остаётся нулём. Координаты локальны для сегмента
и не подразумевают обратного отображения compressed-to-logical. Stage, reason и имена ресурсов
ограничены 64 безопасными ASCII-буквами, цифрами или дефисами; вызывающая сторона передаёт только
фиксированный документированный набор, а не входной текст.

Owner использует фиксированные stage tokens:
`admission`, `aggregate`, `allocation`, `close`, `configuration`, `container`, `flush`, `io`,
`open`, `parse`, `profile`, `record`, `schema`, `sidecar`, `stop`, `write`.
Его ограниченный набор reason, включая внутренние передачи ошибки:

```text
allocation-failure, cancelled, checksum, compressed-limit, configuration-refused,
container-failure, container-finish, counter-overflow, data-open, diagnostic-io,
diagnostic-output-limit, empty-segment, header-limit, identity-output-limit, identity-overflow,
inflated-limit, invalid-argument, invalid-container-state, invalid-data, invalid-directory,
invalid-header, invalid-presentation, length-limit, member-entry-limit, message-time-overlay,
missing-field-time, missing-source-header, missing-source-plan, missing-time-assignment,
missing-time-declaration, mixed-time-declaration, no-progress, none, not-finished,
ordinal-overflow, output-limit, parser-failure, parser-finish, ratio-limit,
recorded-source-failure, scanner-failure, scanner-finish, sink-io, source-close, source-io,
stream-failure, time-open, truncated, unsupported-container, work-limit
```

Все строки используют общий подсчёт до выделения памяти с проверками через вычитание. Нулевой
бюджет не разрешает непустой строки. `terminalReserveWorstCase` не выделяет память, учитывает
максимальную ширину complete, resource и primary/cleanup diagnostics и отвергает переполнение
сумм и произведений. Владелец резервирует этот объём до открытия источников и учитывает все identity
и все опубликованные байты строк в конечных бюджетах.
Метод `sourceWireDescription`, используемый только в output9, сохраняет каждое физическое поле
источника и его порядок, включая виртуальный префикс FIELD. `wire-record` и `wire-field` сохраняют
прежнюю грамматику ячеек; только target-ID фактически распознанного виртуального metadata action
равен `-`. Распознавание требует ненулевого authoritative prefix count плана, точно совпадающего
с ведущим `EventTime`/full56 и необязательным непосредственно следующим `EventSequence`/full72.
У этих actions должны отсутствовать local targets, а точные типы должны совпадать. Все остальные
actions требуют присутствующего допустимого local target и совпадающего полного типа. Похожие имена,
неверные полные идентичности, неведущие Event-поля и обычные несвязанные actions не становятся
виртуальными. Обычный явно связанный EventTime остаётся обычным полем. Проверка предшествует
подсчёту и выделению памяти для строки; schema/context и описание заимствуются только на время
вызова. Прежний метод `wireDescription` сохраняет отказ при отсутствии action target.
