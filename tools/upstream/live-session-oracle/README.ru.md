# Проверка control-сообщений нативной live session

[English version](README.md). Нормативный текст — EN.

Инструмент разработчика передаёт полный **сформированный нативным кодом** синтетический capture
в неизменённый production `BinaryQTPParser` Java QD. Реальные callbacks `MessageConsumerAdapter`
сразу сериализуются и побайтно сравниваются с TSV, ожидаемым нативным тестом. Preflight проверяет
синтаксис и ресурсы; сам по себе он не доказывает совместимость.

Нужны CMake 3.25+, Git, JDK 8, исходники ревизии из [upstream.lock](../../../upstream.lock),
неизменённые подготовленные классы QD, доверенная квитанция подготовки и результаты нативного
теста. Инструмент не готовит providers и не собирает native tests. См.
[подготовку providers](../../schema-export/README.ru.md). Версия исходников и наблюдаемая строка
версии runtime — разные факты provenance: например, `QDS-UNKNOWN` сохраняется без подмены.

```powershell
cmake -P tools/upstream/live-session-oracle/Verify.cmake
cmake -DHELP=ON -P tools/upstream/live-session-oracle/Verify.cmake
cmake '-DJAVA_HOME=C:/jdk8' '-DUPSTREAM=C:/locked-qd' `
  '-DRECEIPT=C:/prepared/classpath.json' '-DNATIVE=C:/native-controls' `
  '-DRUN=C:/new-control-check' -DMODE=Capture `
  -P tools/upstream/live-session-oracle/Verify.cmake
```

Замените примерные пути существующими локальными входами. `NATIVE` содержит `native-controls.qtp`
и `expected.tsv`, созданные нативным conformance test. `RUN` не должен существовать и должен
находиться вне `UPSTREAM`, `NATIVE`, `EXPECTED`. Обязательные пути не имеют значений по умолчанию.
`MODE` по умолчанию — `Capture`: callbacks **всегда** сравниваются с `NATIVE/expected.tsv`.
`Verify` дополнительно сверяет точные четыре файла артефактов с `EXPECTED`; его default —
`tests/fixtures/live-session/expected`. Для явно выбранного проверенного каталога:

```powershell
cmake '-DJAVA_HOME=C:/jdk8' '-DUPSTREAM=C:/locked-qd' `
  '-DRECEIPT=C:/prepared/classpath.json' '-DNATIVE=C:/native-controls' `
  '-DRUN=C:/new-repeat-check' -DMODE=Verify '-DEXPECTED=C:/reviewed-controls' `
  -P tools/upstream/live-session-oracle/Verify.cmake
```

Проверка вложенности разрешает ближайший существующий родительский путь и сравнивает пути Windows
без учёта регистра. Она обнаруживает aliases, которые CMake разрешает в момент проверки.
Одновременная замена родительских каталогов или reparse points, а также aliases, недоступные
разрешению платформы, остаются вне preflight: это не файловая security sandbox. Используйте
стабильные локальные каталоги входов и результатов.

Без аргументов и с отдельным `-DHELP=ON` выводится одинаковая справка без запуска работы. Код 0:
справка или успешное точное сравнение; 1: неверные настройки, drift, timeout, ошибка Java или
несовпадение. Launcher создаёт только новый `RUN`: classes, частные логи, receipts и `artifacts/`:

| Артефакт | Содержание |
| --- | --- |
| `native-controls.qtp` | Точная копия нативного входа |
| `actual.tsv` | Реальные callbacks production parser |
| `provenance.tsv` | Pin и хеши исходников/helper/входов/providers/runtime, строка версии QD |
| `manifest.tsv` | Размеры и SHA-256 артефактов, кроме самого manifest |

Receipts providers и хеши входов/tool должны совпадать до и после запуска. Сеть, Maven, изменения
upstream, нативная сборка, публикация fixtures и изменения глобальных настроек не выполняются.
Дедлайн `javac` — 60 секунд, разбора — 30 секунд. Heap Java ограничен 256 MiB; это не ограничение
RSS процесса. Неудачный запуск оставляет частичные логи/артефакты и не подтверждает успех.

Создавайте нативный capture из тех же исходников, которые проверяет запуск. Provenance связывает
native test producer, live driver и общие исходники decoding/storage, schema, writer и encoding,
а также compile profile корневого CMake. Нативные build logs подтверждают, из каких исходников
собран executable; wrapper хеширует текущие исходники, не пересобирает их и не выводит эту связь
только из capture-файла. Quote/event-mapping включены, поскольку producer test target также
связан с Quote projection и проверяет её.

Точная сверка артефактов включает хеши providers и Java runtime. Повторный `Verify` предполагает
совпадение подготовленных providers и runtime bytes; другой runtime требует отдельного
проверенного `Capture`. Само несовпадение provenance не определяет ошибку wire-совместимости.

Отдельный Java helper принимает `--input FILE --output NEW_FILE` в любом порядке без рабочих
defaults. Родитель выходного файла должен существовать. Существующий output отклоняется через
`CREATE_NEW`. Без аргументов и с отдельным `--help` справка одинакова, без I/O и инициализации SDK,
даже при отсутствии классов QD в runtime classpath. Коды: 0 — успех/справка; 1 — operational,
parser, resource либо ошибка вывода справки; 3 — неверные, повторные или неполные аргументы.
Диагностика не выводит значения аргументов или исключения parser. Ошибка записи может оставить
частичный файл.

Допускаются только тип 0 (Heartbeat), тип 1 (DescribeProtocol) и пустые envelopes. Весь файл
проходит preflight до SDK parsing. Чтение потока ограничено 1 MiB плюс один байт обнаружения
превышения, поэтому изменение файла не обходит предел. Counts и длины текста проверяются до
выделений SDK-объектов и UTF-буферов:

| Ограничение | Максимум |
| --- | --- |
| Кадры | 4096 |
| Entries в одном списке properties или messages | 256 |
| Сумма property и message entries | 4096 |
| Message ID | от -1 до 65535 |
| Wire bytes одного текста | 16384 |
| UTF-16 units одного текста / всего входа | 4096 / 65536 |
| Строки output (со строкой заголовков) / ASCII bytes | 65536 / 8388608 |

Неизвестные metadata остаются ограниченным непрозрачным текстом. Неверный синтаксис, неполные
кадры, другие типы сообщений, EndpointId trailers, неизвестные heartbeat bits и лишние payload
bytes отклоняются. Непустой `authorization` должен точно равняться синтетическому токену
`Basic c3ludGhldGljOnRva2Vu`: другие непустые значения отклоняются до production parsing без
вывода значения. В callback непустой authorization всегда `REDACTED`; null и пустое значение
различаются. Настоящие credentials использовать нельзя.

`actual.tsv` содержит шесть колонок: `kind`, `index`, `section`, `message_id`, `key`, `value`.
Индекс — общий порядковый номер callback с нуля для обоих видов. Текст: `N` для null, иначе `U`
и четыре uppercase hex-цифры на UTF-16 unit; пустой текст — `U`. Properties сортируются по
порядку UTF-16 Java, null первым; messages — по ID, затем имени. Protocol sections: `property`,
`send`, `receive`, `send-property`, `receive-property`. Message properties включают унаследованные
protocol properties. Property rows содержат key/value; send/receive rows — имя в `key`, `-` в
`value`. У protocol property message ID равен `-`. У heartbeat `mask` ID/key равны `-`, value —
десятичная маска; у `field` ID равен `-`, key — имя (`timeMillis`, `timeMark`, `deltaMark`,
`lagMark`), value — десятичное значение. Выводятся только присутствующие поля.
Пустые envelopes и heartbeat без payload не дают callback; payload с нулевой маской даёт строку
маски. Дескриптор без properties/messages не даёт строк, но увеличивает порядковый номер callback.

Observer не удерживает изменяемые SDK callback objects. Наблюдаются накопленный production
descriptor и унаследованные message properties; выдуманного wire reset marker нет. Новый процесс
helper начинает с нового parser state.

Проверка controls не подтверждает real feeds, EndpointId, PART framing, интеграцию authentication,
native clients, sockets, clocks или доставку subscriptions/data. Только Java-generated входы
характеризуют поведение, но не доказывают native-to-Java interoperability.

Первичные контракты в закреплённом исходнике: `ProtocolDescriptor.composeTo/parseFrom`,
`MessageDescriptor.getProperties/getMessageType`, `HeartbeatPayload.composeTo/parseFrom`,
`BinaryQTPParser.parseDescribeProtocol/parseHeartbeat`, `BufferedInput.readUTFString/readProperties`.
Их хеши сохраняются в provenance. Создание нового файла описано в
[`StandardOpenOption.CREATE_NEW`](https://docs.oracle.com/javase/8/docs/api/java/nio/file/StandardOpenOption.html#CREATE_NEW).
