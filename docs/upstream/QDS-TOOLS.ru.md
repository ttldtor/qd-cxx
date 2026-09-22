# Утилиты закреплённого qds-tools и приоритеты реализации

[English](QDS-TOOLS.md). Английская версия является основной.

QDCXX-020; изучены исходники QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
Сведения о работе программ ниже выведены из исходников, не измерены заново. [Проверки](QDCXX-020-VALIDATION.ru.md).

## Перечисление, псевдонимы и упаковка

Отслеживаемый production-реестр содержит **20** провайдеров AbstractTool; таблица учитывает
все 20. Воспроизведение: перечислить отслеживаемые `*META-INF/services/*AbstractTool*`, изучить
каждого провайдера и подклассы AbstractTool/`@ServiceProvider`, затем сверить два ресурса
`META-INF/qds/qdshelp.txt` и упаковочные POM. Тестовые реестры не являются production-командами.
Единственный отслеживаемый реестр AbstractTool находится в qd-tools. qd-nio добавляет справку
соединителей, не новую команду.

[Реестр](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/resources/META-INF/services/com.devexperts.qd.tools.AbstractTool),
[Tools](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Tools.java),
[Help](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Help.java),
[статьи справки](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/resources/META-INF/qds/qdshelp.txt),
[POM qds-tools](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-tools/pom.xml).

Tools сопоставляет простое имя класса без учёта регистра; отдельного реестра псевдонимов нет.
Поэтому `connect` — написание Connect, не другая утилита. Правило относится ко всем 20 строкам.
Методы `main` классов вызывают Tools. `+` разделяет команды в одной JVM; Batch — статья,
не провайдер. Tools/Contents/All, Tape, File, Address, Scheme, Filters, Stripers, Time format,
Text format и статьи соединителей не являются дополнительными утилитами.
У одиннадцати провайдеров есть отдельные статьи: Help, Connect, Compare, Dump, Feed, Multiplexor,
Post, SchemeDump, Time, NetTest, Instruments. Остальные используют ToolSummary через reflection.

qds-tools — shaded JAR: Main-Class Tools, объединённые service-ресурсы/справка; включены qd-logger,
qd-nio, qd-stripe, qd-tools, dxfeed-tools. Manifest Class-Path перечисляет qds.jar, qds-file.jar,
qds-monitoring.jar, dxlink-websocket.jar, config.jar. Это не доказывает установку этих файлов.
Shell/batch launcher qds-tools в отслеживаемых путях не найден; исходники поддерживают вход
`java -jar qds-tools.jar <command> ...` с зависимостями либо Tools с полным classpath.
Упакованный JAR здесь не собирался и не запускался.

Instruments делегирует преобразование IPF, разбор OCC и сервер dxfeed-tools/dxfeed-ipf;
он уже учтён и не является дополнительным publisher. Ещё один публичный main,
[MICBuilder](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-tools/src/main/java/com/dxfeed/ipf/tools/MICBuilder.java),
— незарегистрированный служебный вход: каталог версий ISO MIC CSV → текст базы в stdout для
обновления исходника MIC; нет схемы QD/опций, синхронные IO/ошибки использования.
Предлагается отложить как обслуживание базы upstream (N), без аналога qdcxx.
Остальные исходники dxfeed-tools — библиотеки, не зарегистрированные команды.
TestThroughput из qd-sample и fileparser/connect/UI launcher из dxfeed-samples — отдельные примеры,
не псевдонимы Tools; dxfeed-viewer/plotter — отдельные продукты вне границы этого перечня.

## Общие обозначения

D = эффективная стандартная/endpoint DataScheme QD, выбранная через QDFactory или QDEndpoint
и провайдеры/свойства; это не означает обязательную установку SampleScheme или полной DXFeedScheme.
Ни одна строка не обещает единую схему независимо от окружения. Отсутствующая опция не выбрана,
если не указан default. Не перечисленные подробные транзитивные свойства/краевые ошибки остаются
**неизвестными/неизмеренными**; ссылки указывают точные getOptions/execute/lifecycle каждой строки.
Таблица перечисляет доступные опции, не задаёт нормативную грамматику.

Общие опции там, где перечислены: --log/-l, --stat/-s (период, по умолчанию секунды),
--name/-n; management = --html-management/-h и --rmi-management/-r.
--fields фильтрует суффиксы, --file принимает [filter@]name для начального хранилища,
--write задаёт период записи хранилища. Они не являются псевдонимами Connect --tape.
Доступность опций может зависеть от EndpointOption.isSupportedEndpointOption. См.
[Options](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Options.java) и
[исходники опций](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools).
-Dname=value меняет свойства JVM во время разбора; все команды разбираются до Services.startup,
затем выполняются, ожидают соединения/потоки и закрывают объявленные ресурсы. Обычный запуск
заканчивается System.exit(0), перехваченная ошибка — 1; поглощённые/записанные в лог ошибки
не обязательно дают неуспешный код. Пустой Tools печатает usage в stderr и возвращается;
команда без аргументов может выполнять работу. Это поведение не переносится на справку qdcxx.

Общая грамматика адресов реализована в
[MessageConnectors](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageConnectors.java):
[filter@]address[properties], группы (address1)(address2), устаревшие варианты через слеш;
группы свойств содержат key=value. Client/server sockets, file/tape, NIO и прочие установленные
фабрики разрешают конкретные формы; IPF URL и HOCON имеют отдельные грамматики.
Файловые форматы/defaults/опции отдельно исследованы в [TAPE-FILES](../../spec/protocol/TAPE-FILES.ru.md).
Справка расходится с исходниками: --alias/1/2 статьи Compare устарели относительно --names и
compare1/compare2; позиционная схема в статье Scheme и пример SampleScheme у NetTest не
соответствуют текущему разбору аргументов. Старые defaults sidecar в статье Tape не главнее
FileWriterParams/TimestampsType. Это расхождения исходников, не новые наблюдения выполнения.

## Полное сопоставление обнаруженных команд

Приоритет P0 — ближайшее условие офлайн-работы; P1 — первые потребители/publisher реальных событий;
P2 — раздача после подписок/core/транспорта; P3 — последующие диагностика/IPF/нагрузка.
Порядок предложен, не разрешён к реализации. W — сохранить выбранную семантику протокола/данных
после получения доказательств; F — выбранную семантику входных данных/файлов;
C — предложение собственного CLI/конфигурации qdcxx; N — прямой перенос не нужен.
**Каждая строка W/F также предлагает собственные имена/CLI/вывод/ошибки (C)**,
без обещания совпадения аргументов, адресов, свойств, кодов завершения и байтов вывода Java.

| Команда / закреплённый исходник входа | Входы | Назначение, опции/defaults, схема/зависимости, жизненный цикл | Предлагаемый аналог / приоритет / совместимость |
| --- | --- | --- | --- |
| [Compare](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Compare.java) | `address records symbols [date-time]` | Два источника → статистика совпадений/задержек/потерь; ticker; --collector/-c, --other-address/-A, --other-collector/-C, --other-record/-R (минимум одна), обязательная --stat; --fields, --names (compare1/compare2), --log, management. D; ожидание соединений + закрытие endpoint. | P3 qdcxx-compare; W |
| [Connect](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Connect.java) | `address records symbols [date-time] OR address subscription-address` | Приём записей → по умолчанию текст на экране, опционально tape. ticker; --collector/-c, --stripe, --sticky, --tape/-t, --fields/-f, --quiet/-q, --stamp/-S, --top/-T, --name (connect), --stat, --log, management. D; ожидание соединений + закрытие processor/endpoint. | P1 qdcxx-connect; W |
| [Dump](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Dump.java) | `address [subscription]` | Все полученные сообщения данных И подписок → текст/tape; нет опции collector. --quiet, --tape, --stamp, --name (dump), --stat, --log, management. D; ожидание соединений + закрытие endpoint/writer. | P1 qdcxx-dump; W |
| [Feed](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Feed.java) | `feed-address agent-address` | Wildcard-поток → нижестоящие агенты; collector all. --collector, --stripe, --sticky, --file/-f, --write/-w, --raw/-R, --subscribe/-S, парные --delay/-d и --buffer/-b, --name (feed), --stat, --log, management. D; ожидает соединения. subscribe на raw-uplink может неограниченно увеличивать подписку. | P2 qdcxx-feed; W |
| [Forward](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Forward.java) | `address; required --forward/-F service-names address` | Пересылка RMI-запросов, не публикация рыночных записей. --name (forward), --log. Endpoint D; RMI server/client, ожидание соединений. | Deferred RMI / отложено RMI; W |
| [FileAnalysis](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/analysis/FileAnalysis.java) | `file-or-url` | Binary tape → отчёт о структуре/символах/сжатии, stdout без --out/-o. --symbols/-s, --group/-g файл свойств, --compression/-c glob, --type-override/-t. FileReader на максимальной скорости, только BINARY; диагностическая схема parser; синхронное завершение. | P0 bounded qdcxx-tape-inspect, P3 analysis / ограниченный qdcxx-tape-inspect, затем анализ; F |
| [GCTimeTransformer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/GCTimeTransformer.java) | `file` | GC-лог → текст с абсолютным временем; --time/-t начало, --output/-o (gctimes.txt). Нет схемы записей; синхронная обработка файла. | Not applicable JVM diagnostic / неприменимо: диагностика JVM; N |
| [Help](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Help.java) | `[article]` | Статьи/справка через reflection → stdout; --width/-w по умолчанию 120, минимум 10. Схема данных по назначению не нужна; создание всё равно через AbstractTool. Синхронно. | P0 integrated --help / встроенная --help; C |
| [Instruments](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Instruments.java) | `option-driven; no positional args` | IPF/OCC FIXML/live-источники → преобразование/IPF/server. --bizdate/-b, --osi/-o, --generate/-g (100000), --read/-r, --products/-p, --transform/-t, --merge/-m, --exclude/-e, --check/-c, --sort/-s, --write/-w, --script/-i, --performance/-n, --log. Схема IPF, не записей QD. Server ':port[opts]' подразумевает merge и ограничивает опции; ожидание себя + закрытие server; файловый режим возвращается. Свойство writeEmptyResults по умолчанию false. | P3 qdcxx-instruments, separate IPF / отдельный IPF; F |
| [Invoke](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Invoke.java) | `address service[@id] method[:return-type] [arg[:type] ...]` | RMI-вызов → результат/исключение в логе; автоматические типы аргументов, тип результата по умолчанию void, нет специальных опций команды. RMI-схема/marshalling; блокирующий запрос. Отсутствие target или перехваченный RMIException могут дать обычный возврат и успех диспетчера. | Deferred RMI / отложено RMI; W |
| [Multiplexor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Multiplexor.java) | `distributor-address agent-address` | Входящие данные/нижестоящие подписки ↔ маршрутизация collector. all; --collector, --stripe, --sticky, --file, --write, --delay, --drop/-P (1..100), --name (multiplexor), --forward/-F, --route/-R, --stat, --log, management; multiplexor.qd.subscribe.*. D; ожидание соединений + закрытие ресурсов. | P2 qdcxx-multiplexor; W |
| [NetTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTest.java) | `p\|c address [address2 ...]` | Нагрузка producer/consumer → статистика пропускной способности. ticker, один экземпляр, 100000 символов, статистика 10s; --collector, --stripe, --sticky, --symbols/-S, --connections/-C, --wildcard/-w, --name, --stat, --log, последовательность частот (-r/--rate-limit). D + синтетическая нагрузка; запускает потоки стороны, сам NetTest не переопределяет lifecycle hooks: конечность запуска не предполагается. | P3 qdcxx-net-test; W |
| [Post](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Post.java) | `address; stdin text records` | Публикация входных записей в uplink; --collector/-c по умолчанию raw (альтернативы ticker/stream/history), --name (post), --log. D, TextQTPParser/очередь; join PostingThread. Не детерминированный генератор объектов событий. | P1 qdcxx-post; W |
| [Services](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Services.java) | `address` | Объявления RMI-сервисов → лог. Доступна только --log (поле name отсутствует в getOptions). Endpoint D; ожидание соединений. | Deferred RMI / отложено RMI; W |
| [SchemeDump](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SchemeDump.java) | `[records]` | Стандартная схема/фильтр → текст записей/полей/типов/классов в stdout; нет специальных опций, по умолчанию '*'. D; синхронно. Без аргументов Java выполняет работу. | P1 qdcxx-scheme-dump; F |
| [SubscriptionDumpParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SubscriptionDumpParser.java) | `files` | Бинарные дампы управления collector → по умолчанию subscription.txt. --columns/-c по умолчанию sRa, --sort/-s отсутствует, --group/-g подразумевает sort/columns, --output/-o. Метаданные дампа + поиск записей D; синхронно. Это не tape parser. | P3 qdcxx-subscription-dump; F |
| [SubscriptionDumpRepeater](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SubscriptionDumpRepeater.java) | `address files` | Дампы управления → воспроизведение подписок/нагрузки, опционально полученный текст/tape. all; --collector, --tape, --fields, --quiet, --stamp, --top, --name, --stat, --log, management, --filter/-F regex агента. D + dump parser; ожидание соединений + закрытие ресурсов. | P3 qdcxx-subscription-replay; W |
| [TDP](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/TDP.java) | `files` | Полные дампы потоков JVM → profile.txt; готовый TDP profile → сокращённый file + "~". IOExceptions печатаются и поглощаются. --locks/-l, --topdepth/-t 50, --methoddepth/-m 50, --threshold/-h 5%, --count/-c, --state/-s, --name/-n, --output/-o; нет схемы записей; синхронно. | Not applicable JVM diagnostic / неприменимо: диагностика JVM; N |
| [Time](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Time.java) | `no positional args; --request or --stat required` | Разницы времени JVM → логи; --request/-R, --verbose/-v, --stat, --log, management. TimeSyncTracker, нет схемы записей; self-join (долгоживущий процесс). Не преобразователь времён tape. | Deferred clock observability / отложено наблюдение часов; W |
| [Launcher](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/launcher/Launcher.java) | `config` | Экспериментальный HOCON modules/endpointProperties/launcherConfig → управляемые сервисы; --check/-c; фабрики модулей, Typesafe Config, monitoring/JMX/MARS. Схема принадлежит module/endpoint. Join watcher + close; по исходникам startTimeout/pollPeriod 1000ms, moduleEventLogLimit 100000. | Deferred qdcxx service composition / отложена композиция сервисов qdcxx; C |

## Общие требования и решения владельца

- Схемы/mapping: точные serial identities, проверенная локальная модель независимо от загрузчика,
  запрошенные структуры, разрешение имён и преобразование событий. Импорт XML/JSON отдельно.
- Подписки/core: добавление/удаление, wildcard, ticker conflation в отличие от упорядоченного stream,
  history indices/flags, агрегация и изоляция клиентов до совместимости Multiplexor/Feed.
- Протокол/транспорт: согласование и формирование сообщений подписок/данных, reconnect/re-description,
  частичные чтение/запись, отмена, сроки ожидания, владение и детерминированное завершение.
- Буферы: конечные клиентские/общие бюджеты, обратное давление, политика медленного потребителя/потерь,
  опустошение и счётчики; опция задержки Java не является полным контрактом ресурсов.
- Эксплуатация: явный приоритет конфигурации, сокрытие учётных данных, диагностика, метрики байтов/
  записей/очередей/reconnect, надёжные коды завершения и сигналы. MARS/JMX, HOCON, Java RMI и
  общие свойства JVM — зависимости для оценки, не автоматически переносимые API.

Нужно отдельно одобрить предложенную политику CLI/конфигурации/ошибок/вывода каждой ограниченной
утилиты, а намеренные отклонения протокола/данных — через процедуру совместимости. Прямая замена
без изменений не заявляется. Все qdcxx executables: без аргументов = standalone --help, stdout,
пустой stderr, exit 0 при успешном выводе, без работы/файлового IO; ошибки вывода сохраняются.
Точные форматы TSV/human существующего inspector не меняются.
Исследование не выбирает сетевую библиотеку, модель потоков или runtime-зависимость.

Первый результат — ограниченный qdcxx-tape-inspect (часть FileAnalysis/Connect-file), затем
инспекция полного стандартного Quote, ограниченные Connect/Post с записью/воспроизведением,
затем Feed/Multiplexor с настоящей маршрутизацией подписок. В Java воспроизведение файлов —
возможность соединителя, не выдуманная зарегистрированная утилита Replay.
Приоритет не означает реализацию всех upstream-опций.
[Порядок зависимостей и одна следующая задача](../NEXT-STEPS.ru.md),
[реальное событие](../REAL-EVENT-BASELINE.ru.md), [удаление Python](../PYTHON-MIGRATION.ru.md).

## Ограничения покрытия

В изученном checkout нет полного runtime/classpath утилит (Tools.class отсутствует).
Формирование справки/списка может создавать провайдеры, а запуск вызывает Services.startup;
произвольный classpath не собирался и сервисы ради справки не запускались. Поэтому runtime-справка
диспетчера и отдельных команд не захватывалась. Изучены статьи/аннотации/опции исходников;
видимость справки упакованного JAR и внешние необязательные провайдеры не измерены.
Полнота ограничена закреплённым отслеживаемым реестром, публичными входами shaded-модулей и
путями справки/упаковки выше, а не всеми устанавливаемыми плагинами. Подробные транзитивные
defaults соединителей, все команды скриптов Instruments и сочетания конфигурации/ошибок
требуют отдельных задач перед заявлением совместимости.
