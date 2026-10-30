# Конечная политика бинарных сообщений

[English version](BINARY-MESSAGES.md). Английская версия нормативна.

Дополнительный обязательный аргумент конструктора `FieldTimePolicy::READ` выбирает конечный
[контракт FIELD](TAPE-TIME.ru.md). Прежние сигнатуры сохраняют DISABLED: описанные ниже правила
дескрипторов, привязки, вывода и удаления по умолчанию не включают FIELD скрыто. READ сохраняет
отдельные исходные метаданные и состояние MESSAGE, добавляя конечные фиксированные charges владения
и строительства.

Независимый обязательный последний аргумент `TapeExternalTimePolicy::SIDECAR` принимает только
LONG/long/TEXT/text headers в catalog mode; старые constructors сохраняют DISABLED.
[Companion scanning](SIDECAR-TIME.ru.md), применение marks и whole-pair closure остаются вне protocol.
Admission policy не задаёт external time и не меняет FIELD/MESSAGE state.

`TapeMode::BINARY_CATALOG_V1` явно выбирает эту политику. Прежние LEGACY, RAW_MESSAGE_V1 и
ordinary-v1 discovery сохраняют принятие, defaults и вывод. Конечные доказательства описаны в
[каталоге](BINARY-TAPE-COVERAGE.ru.md) и
[валидации production](../../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.ru.md).
Это generic чтение wire-записей; collectors и бизнес-преобразования исключены.

## Создание, идентичность и владение

Конструктору с готовой схемой нужны проверенная BINARY_FIELDS_V1 и явные конечные лимиты.
Только этот путь принимает подтверждённую вызывающей стороной индексную раскладку. Отдельный
конструктор discovery: `TapeReader(limits, TapeDiscovery::BINARY_CATALOG_V1)`. Он начинает с одной
заранее допущенной identity пустой схемы. Неверное значение discovery или недостаточный начальный
бюджет отклоняются до создания identity. У библиотечных лимитов нет скрытого broad preset: ноль есть ноль.

Discovery создаёт обычные записи с hasTime=false. Имена не определяют исходные slots, индексную
раскладку, бизнес-идентичность или время. Локальные ID и объявленные пары имя/полный тип сохраняются
и ограничиваются после замены отправителя. Возврат прежнего имени с другим полным типом даёт
UNSUPPORTED/TYPE, даже если прежний sender уже не активен. Новые имена расширяют конечный реестр.
Замены активных sparse sender описаны в [INDEXED-SCHEMAS](../schema/INDEXED-SCHEMAS.ru.md).

Каждый успешный непустой пакет описаний получает новую immutable identity, включая равную форму.
Пустой пакет — no-op: без identity, generation, schema-строк и аутентификации headerless.
При ошибке сохраняются опубликованные схема/context/описания и адреса borrowed storage.
Владеющие записи, поля, opaque bytes, символы, removalTime и RecordContext переживают следующие
поколения, уничтожение input/reader/schema и перемещения. Reader не хранит историю snapshots.

Представления `descriptor()` и его текстов прекращают действовать после успешной публикации
дескриптора, перемещения, присваивания или уничтожения. Неуспешный кандидат их сохраняет.
Обновление только descriptor не инвалидирует schema/context и не сбрасывает описания, время,
политику символов и бюджеты. Публикация описаний имеет отдельную границу инвалидирования.
Reader однопоточный; feed возвращает владеющие события, без новых callback/reentrancy-контрактов.

## Dispatch и порядок источника

| Сообщение | Выбранное поведение |
| --- | --- |
| RAW_DATA5, TICKER_DATA10, STREAM_DATA15, HISTORY_DATA20 | Один общий decoder с учётом бюджетов; одна запись на wire-запись, семейство во владеющем frame |
| HEARTBEAT0 | Конечные empty/timestamp-only MESSAGE metadata с timingEvents |
| DESCRIBE_PROTOCOL1 | Атомарный известный full/delta descriptor ниже |
| DESCRIBE_RECORDS2 | Атомарное обновление active sender и immutable generation |
| DESCRIBE_RESERVED3 | Конечное игнорируемое событие: сохранить frame, не выдавать данные, ничего не сбрасывать |
| PART4; subscriptions11/12/16/17/21/22; RMI49–56; TEXT61; ZIP75/GZIP139; другие ID | UNSUPPORTED/MESSAGE |

Семейства используют одинаковые точные поля, typed defaults, шесть известных flag bits и REMOVE_EVENT
из [STREAM-DATA](STREAM-DATA.ru.md). Семейство не дублируется в записи. Прежние трёхаргументные
decodeStreamData/decodeRawData остаются zero-only; явный policy-вход — decodeStreamDataWithEventPolicy.
RAW fanout, ticker/history collector и планировщик не добавлены. Порядок C++ — wire order.
Java pending buffers могут выдать wire15,20,15 как15,15,20; это не контракт generic C++.

Каждый data frame начинает новый symbol/MRU state. Все попытки, пропущенные/неуспешные поля и
владеющий вывод используют накопленные лимиты между семействами. Heartbeat и data-frame attempts
расходуют timingEvents даже для NONE. Принятый timestamp переносится дальше; отсутствие остаётся
отсутствием с sequence0. Descriptor, markers и no-op описания не очищают время.

## Полные дескрипторы и дельты

Читаются DXP3 binary properties/message lists: прежние compact-long counts свойств, projected
compact-int counts/ID списков и ограниченная UTFString grammar. Text tokens не являются descriptor.
Первый явный descriptor требует type=tape, в том числе после допустимого headerless prefix.
Далее известные дельты сохраняют отсутствующие поля, заменяют присутствующие и объединяют sends.
Пустая дельта — конечное no-op событие. Публикация без исключений разрешена только после полного
разбора, проверки политики и admission ресурсов.

| Свойство или расширение | Принятие/отказ |
| --- | --- |
| type | Только tape; обязателен в первом явном descriptor |
| version | Необязательный точный UTF16, включая пустой; диагностика, не тест совместимости/pin |
| time | Отсутствует либо точно NONE/none/MESSAGE/message; FIELD/field только с READ; LONG/long/TEXT/text только с SIDECAR; остальное UNSUPPORTED/TIME |
| opt | Отсутствует, пустой либо точно hs; другие/duplicate/whitespace tokens UNSUPPORTED/OPTIONS |
| Неизвестный или duplicate top key | UNSUPPORTED/DESCRIPTOR после проверенной пары; без произвольной карты свойств |
| Sends | Пусто либо подмножество/порядок точных numeric ID/name четырёх семейств, без properties |
| Duplicate ID, -1 named alias, ID/name mismatch, неизвестные descriptors/properties | UNSUPPORTED/MESSAGE |
| Receive list | Только пустой; остальные объявления UNSUPPORTED/MESSAGE |
| Необязательный EndpointId | Проверить конечный структурный tail, затем UNSUPPORTED/DESCRIPTOR |

Sends — объявления, а не разрешения: известное фактическое семейство может отсутствовать в sends.
Ошибочные counts, обязательные пары, magic и проверенные primitives дают MALFORMED. Unsupported
checkpoint может остановить разбор раньше следующих байтов и не подтверждает непроверенный хвост.
Java принимает некоторые исключённые варианты — это ограничения профиля C++, не «ошибочная Java grammar».

Endpoint tail при ctx=null — compact-long nullable byte-array JVM UID, затем при ненулевом UID
nullable UTFString name и compact-long ID. Пустой UID допустим, -1 означает null; другие отрицательные
длины ошибочны. До skip проверить domain и границу frame, не выделяя/копируя UID; ограничить временное
имя. Truncated/invalid primitive и лишние байты после одного tail — MALFORMED; допустимый null/non-null
tail — unsupported. Не переносить context-reference grammar и не требовать от всех UID длину16.

## Headerless и завершение

Production FileWriter с noHeaderAndFooter создаёт self-described binary: NONE начинается с
descriptions2, MESSAGE может начинаться с heartbeat0. Только явный broad mode принимает этот путь.
Каждый sender данных должен быть описан, даже при готовой локальной схеме. Нет schemeKnown/readAs,
resync/ignore/cycle или числового sender-to-local shortcut.

Finish требует фактический physical EOF, полные frames и принятый descriptor ИЛИ опубликованный
непустой пакет описаний. Пустой файл, только markers/heartbeat/пустые описания не аутентифицируют EOF.
Footer не обязателен. NEED_INPUT и пустой feed не означают EOF. Неполный физический frame ждёт
в feed и завершается ошибкой framing при finish; неполный primitive внутри полного frame сразу
даёт payload error. Готовые записи сохраняются, текущая частичная не выдаётся. Java может удалить
буферизованный prefix сообщения при ошибке; completed-prefix гарантия C++ здесь намеренно сильнее.

Native inspection заимствует потоки. Exit0 подтверждает read/physical EOF/reader.finish/write/final
flush, но не owner close-before-complete. Жизненный цикл owned source остаётся отдельным.
Уже записанные complete-row bytes нельзя отозвать после ошибки flush: нужны и строка, и exit0.
Приёмка Java real-tape отдельно требует instrumented EOF/open/close/error/queue proof и равенство
digest literal FileReader; это не доказательство native owner close.

## Точный логический учёт descriptor

Используется тот же закрытый конечный ledger, что и для schema/session. snapshotUnits ограничивает
одновременно живые metadata, constructionUnits — накопленные admitted parse/copy/construction work.
Прежние режимы сохраняют старые проверки. B — все живые S/D/C owners из INDEXED-SCHEMAS.
Для владеющего descriptor d: `H(d)=1+sum(1+UTF16 length)` по engaged version/options/time;
отсутствующий descriptor стоит0, engaged пустой текст1. Фиксированные scalars/send booleans входят в header1.

Одновременная резервация:
`P=B+H(published)+H(candidate)+T(key)+T(value)+T(messageName)+T(endpointName)`.
Каждый независимый временный текст стоит header1 плюс живые/зарезервированные UTF16 units.
Учитывать все реальные owners; фиксированные duplicate-key bits не требуют истории строк ключей.
Frame bytes, encoded Row, allocator capacity и RSS имеют отдельные ограничения/оговорки.

До разбора admitted work включает весь размер descriptor payload. Полная копия прежнего эффективного
header требует admission H(old), новый пустой кандидат1. Tiny/equal/no-op delta тоже оплачивает копию.
До владеющего UTF decode зарезервировать и charge header1 плюс U:0 для null/empty, иначе
`min(B+1, per-text units, remaining message units)` для допустимой declared body length B.
Проверять представимость/сложение; Java UTFString допускает body до8589934588 bytes. B+1 покрывает
ошибочный supplementary tail до отказа. U передаётся decoder как unit cap до push.
После успеха освобождается лишь неиспользованная live reservation; накопленная admitted work
не возвращается даже после failure/skip. Move переносит учтённое владение; copy требует отдельный admission.

Commit — только после полного успеха. Allocation failure переводится на текущем checkpoint,
сохраняя прежние адреса и накопленную work. UID skip не владеет metadata и покрыт payload work.
Дельты, headers, failed candidates и finish не сбрасывают счётчики. Retired имена остаются в S
и конечных schema bounds. FIELD metadata и companion-header admission требуют отдельных явных policies.
Sidecar I/O, compression и владение цепочкой источников остаются вне protocol; новые зависимости,
сеть и source/callback framework не добавлены.
