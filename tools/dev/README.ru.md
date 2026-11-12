# Переносимые инструменты разработки

[English version](README.md). Английская версия нормативная.

[Production oracle расписаний](../upstream/schedule-oracle/README.ru.md) захватывает/проверяет полную закреплённую грамматику конструктора, named/default rules и timezone IDs через Java 8. Без аргументов, EN/RU-справка и Get-Help не требуют входов и не запускают Java. [Консольный пример расписаний](../../examples/schedule_rules/README.ru.md) использует установленный публичный API; без аргументов и с `--help`/`--help-ru` контекст не инициализируется.

[Production oracle Trade](../upstream/trade-oracle/README.ru.md) проверяет composite/regional fields, raw slots, time/flags/defaults и прямые business doubles. Noargs/ENRU/Get-Help не запускают Java; Capture/Verify используют четыре явных пути, finite subprocess limits и новые private run directories.

[Oracle преобразования полей](../upstream/field-conversion-oracle/README.ru.md) захватывает/проверяет
фактическое numeric, physical-slot, nullable String и missing-field поведение. Без аргументов,
EN/RU справка и Get-Help не требуют рабочих входов и не запускают Java.

[Oracle времени событий](../upstream/event-time-oracle/README.ru.md) проверяет необязательные
Sequence/nanoseconds Quote, секунды/миллисекунды business time, значения Profile по умолчанию
и региональные маршруты Quote через production Java delegate. Без аргументов, с
`-Help`/`-HelpRu` и `Get-Help` рабочие пути не требуются, Java не запускается.

[Oracle рыночных символов](../upstream/market-symbol-oracle/README.ru.md) захватывает и сверяет
точные nullable-операции UTF-16 с базой/кодом по закреплённым production-классам Java.
Режимы `-Help`/`-HelpRu` не требуют входов и не начинают работу.

[Oracle атрибутов](../upstream/market-attribute-oracle/README.ru.md) отдельно проверяет
чтение/изменение общих атрибутов, порядок UTF-16, некорректные блоки и null-ключи.
Режимы `-Help`/`-HelpRu` не требуют входов и не начинают работу.

[Явная подготовка зависимостей runtime](../deps/README.ru.md) подготавливает закреплённые Asio/c-ares в выбранном кэше; обычный configure и OFF не выполняют acquisition. По ссылке описаны EN/RU-справка, offline/cache-ошибки и сохранение staging.

Необязательный [пример публичного feed API](../../examples/feed_quote/README.ru.md),
`qdcxx-feed-demo`, использует продвижение библиотекой и типизированные подписки Quote.
Включите `ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` вместе с CLIENT/RUNTIME/RECOVERY;
`--help` (также запуск без аргументов) и `--help-ru` не выполняют SDK/DNS/подключение.
`--run` запускает конечный сеанс обычного TCP. В руководстве описаны аргументы,
вывод, коды выхода, системный DNS и завершение; TLS/HISTORY в этот пример не входят.

[Сборщик HTML/API-справки](../docs/README.ru.md) использует CMake и Doxygen без Python и компилятора.
`cmake -DHELP=ON -P tools/docs/Build.cmake` показывает справку без входных данных и побочных действий.
Обычная сборка создаёт только выбранный каталог документации; параметры, пределы и коды завершения
описаны в руководстве сборщика. Отдельные проверки запускаются через `tools/docs/Tests.cmake`.

Опциональные [проекты проверки реализуемости runtime](../../tests/conformance/transport/runtime-feasibility/README.ru.md)
проверяют закреплённые DNS/TCP/TLS-механизмы, тестовые сертификаты, отмену и границы зависимостей при OFF.
Каждый — отдельный CMake-проект с OFF по умолчанию; production transport target в корневую сборку они не добавляют.

Для отдельного опционального production TCP target корня используйте
[подготовку SDK](../../cmake/runtime-profile/README.ru.md) и
[инструкции сборки/пакета](../../docs/CPP-BUILD.ru.md#опциональный-tcp-runtime).

Опциональный [клиент подписок](../../docs/SUBSCRIPTIONS.ru.md) `qdcxx-connect`
сохраняет исходный контролируемый plain STREAM-режим по умолчанию. Явный
`--mode recovery` добавляет получение STREAM/TICKER/HISTORY, конечные повторы,
уведомления reset и завершение очередей; проверенный TLS и внешние JSON-схемы
имеют отдельные флаги сборки. Оба режима выводят общие записи и подготовленные
Quote/Profile. Встроенная EN/RU-справка работает без входных данных и сети.
См. [формат вывода](../../spec/cli/RECOVERY-OUTPUT.ru.md) и
[контролируемый TLS-профиль процесса](../../spec/cli/TLS-HOST-PROFILE.ru.md).

Самостоятельный [пример подписки Quote](../../docs/SUBSCRIPTION-RECEIVER.ru.md) собирается
с установленным пакетом и использует prepared-получатель и существующий TCP recovery.
Исходный проект — [examples/subscriptions/receiver/CMakeLists.txt](../../examples/subscriptions/receiver/CMakeLists.txt).
`qdcxx-quote-subscription --help` (или отсутствие аргументов) показывает справку без запуска
работы; `--help-ru` выводит русскую справку. Явные настройки примера описывают конечную
нагрузку и политики retry, DNS и drain; TLS в этот пример не входит.

Необязательная [проверка управляющих сообщений сессии](../upstream/live-session-oracle/README.ru.md)
сравнивает нативный вывод протокола и heartbeat с настоящими callbacks закреплённой Java,
включая наследование свойств и скрытие тестовой авторизации. Проверка офлайновая,
с явно подготовленными providers.

Необязательная [проверка нативных подписок парсером](../upstream/subscription-writer-oracle/README.ru.md)
сравнивает захваты composer с настоящими callbacks/time reads закреплённой Java без сокетов.

Необязательный [эталон подписок](../upstream/subscription-oracle/README.ru.md) офлайново захватывает
wire/collector/session поведение закреплённой Java и сравнивает точные индексированные эталоны.
Ему явно нужны JDK 8/CMake/PowerShell; обычные нативные сборки его не запускают.

Нативная утилита `qdcxx-run-utc` и [форматтер на CMake](../format/CPP.ru.md) не требуют Python или
PowerShell в деревьях своих команд. Для работы нужны CMake 3.25+, компилятор C++23 и его нативные
средства сборки; форматирование дополнительно требует Git и явно переданные нативные Uncrustify 0.83.0
и clang-format 20.1.8. Глобальной установки нет. Эти программы для разработки не зависят от
production-библиотек. Корневой `BUILD_TESTING=OFF` не собирает их и не загружает doctest.

[Oracle значений IPF](../upstream/ipf-value-oracle/README.ru.md) захватывает/проверяет поля и типизированные преобразования закреплённого Java; отдельный -Help не начинает Java или I/O работу.

[Oracle локального IPF reader](../upstream/ipf-reader-oracle/README.ru.md) захватывает/проверяет именованные grammar, UTF-8 и complete-prefix outcomes; `-Help`/`-HelpRu` не начинают Java или рабочую I/O.

[Явная подготовка зависимости IPF transfer](../deps/IPF-TRANSFER.ru.md) предоставляет pinned Boost1.92.0 sources, отдельную static URL сборку, EN/RU help helpers, offline/cache checks и sealed переносимый private SDK. Она не добавляет production transfer API и не получает зависимости при ordinary configure либо dependency OFF.

## Первая сборка

Запускать из корня репозитория. Bootstrap — явное исключение логирования первой сборки:
он записывает UTC-границы этапов, длины аргументов, команды, коды завершения и отдельные исходные stdout/stderr,
без построчных отметок времени. Готовая утилита, корневой CTest и doctest ему не нужны.

Windows с Visual Studio 2022, из обычного терминала (PowerShell может быть интерактивной оболочкой):

```text
cmake "-DBINARY=.work/dev" "-DLOG=.work/dev-bootstrap.log" "-DGENERATOR=Visual Studio 17 2022" -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/Release/qdcxx-run-utc.exe --log .work/configure.log -- cmake -S . -B .work/build "-GVisual Studio 17 2022" -A x64
.work/dev/Release/qdcxx-run-utc.exe --log .work/build.log -- cmake --build .work/build --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/test.log -- ctest --test-dir .work/build -C Release --output-on-failure
```

Linux, из POSIX-оболочки с GCC и Ninja:

```sh
cmake -DBINARY=.work/dev -DLOG=.work/dev-bootstrap.log -DGENERATOR=Ninja -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/qdcxx-run-utc --log .work/configure.log -- cmake -S . -B .work/build -G Ninja -DCMAKE_BUILD_TYPE=Release
.work/dev/qdcxx-run-utc --log .work/build.log -- cmake --build .work/build --parallel 6
.work/dev/qdcxx-run-utc --log .work/test.log -- ctest --test-dir .work/build --output-on-failure
```

Имеющиеся проверенные исходники doctest можно передать через `-DFETCHCONTENT_SOURCE_DIR_DOCTEST=<path>`.
Иначе корневая тестовая сборка использует прежнюю закреплённую зависимость FetchContent. Автономный
bootstrap её никогда не загружает. Необязательный `CONFIGURE_ARGS` принимает JSON-массив дополнительных
аргументов CMake. В PowerShell заключать целые аргументы `-DNAME=value` в кавычки, особенно после `--`.
[Необязательные Windows-скрипты](../format/CPP.ru.md) сохраняются для получения инструментов и исторических запусков.

## Контракт журналирования

Вызов: `qdcxx-run-utc --log PATH -- COMMAND [ARG ...]`. Отсутствие аргументов и отдельный `--help`
печатают одинаковый stdout, пустой stderr и возвращают 0 без доступа к файлам. Непустые неверные
аргументы возвращают 2; ошибка вывода help — 1. Исполняемый файл и вектор аргументов передаются без оболочки.
Команда наследует cwd и окружение; только её `TZ` заменяется на `UTC`. Windows использует нативный
Unicode argv независимо от активной кодовой страницы. Окружение родителя и часовой пояс хоста не меняются.

Оба потока читаются одновременно и декодируются независимо инкрементально как UTF-8 с заменой ошибочных последовательностей,
без определения BOM. UTF-8 BOM сохраняется как текст; байты UTF-16 BOM — некорректный UTF-8. CR, LF и
CRLF разделяют строки, в том числе при разрыве между чтениями. Последние незавершённые фрагменты
остаются отдельными. Порядок внутри потока сохраняется; порядок между потоками не определён. Каждая
выведенная строка имеет отметку времени ISO 8601 UTC с миллисекундами и `Z`. Лог и зеркальный stdout
используют UTF-8, LF и не имеют файлового BOM. COMMAND содержит argv/cwd с JSON-экранированием; EXIT
записывает обычный результат дочернего процесса. Это текстовые логи, а не исходные захваты байтов; исходный
вывод инспекторов сохраняется отдельно.

Обычные коды завершения дочерней команды, включая 37, передаются дальше. POSIX-завершение сигналом отображается
в `128 + signal`. Ошибка запуска, назначения вывода, отмены или очистки возвращает 1 с
`UTC LAUNCHER FAILED:` в stderr, если назначение позволяет запись. Владение устанавливается до
исполнения: приостановленный Windows-процесс назначается kill-on-close job до resume; POSIX-потомок
создаёт свой сеанс до exec. Завершение корня отслеживается независимо от EOF потоков; оставшиеся
управляемые потомки завершаются при успехе и ошибке. Очистка имеет один пятисекундный срок от
завершения корня или обнаруженной ошибки вывода; невозможность подтвердить завершение возвращает 1.
Linux принимает осиротевших управляемых потомков и получает статусы их завершения.

На Linux очистка перечисляет исходный сеанс через `/proc`, включая потомков, вызвавших `setpgid`
без выхода из этого сеанса. Статус завершившегося корня не забирается до опустошения сеанса,
что сохраняет его PID/ID сеанса. Сигналы и ожидание потомков используют дескрипторы процессов,
чтобы повторное использование PID не перенаправило очистку. Нужны читаемый `/proc`, корень,
допускающий ожидание (`SIGCHLD` не должен игнорироваться или использовать `SA_NOCLDWAIT`), и нативная
поддержка `pidfd_open`, `pidfd_send_signal`, `waitid(P_PIDFD)` (Linux 5.4 или новее с разрешёнными
возможностями). Отсутствие возможностей приводит к ошибке до исполнения потомка; небезопасного
перехода к числовым PID нет. Интерфейсы ядра описаны в
[pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[pidfd_send_signal(2)](https://man7.org/linux/man-pages/man2/pidfd_send_signal.2.html) и
[waitid(2)](https://man7.org/linux/man-pages/man2/waitid.2.html).

Срок остаётся активным до последнего вывода EXIT и закрытия журнала. Зависшая запись отменяется
через четыре секунды, оставляя последнюю секунду для подтверждённой очистки. Нативная запись журнала
не использует пользовательский буфер и не запрашивает гарантированное сохранение через `fsync`;
POSIX FIFO журнала требует существующего читателя до запуска. Выполнены регрессионные проверки
зависших каналов stdout и зависшего FIFO журнала Linux.
Диагностика ошибок и неверных аргументов в stderr выполняется по возможности: утилита ждёт не более
100 миллисекунд собственный поток нативной записи, затем завершает процесс с определённым кодом,
даже если stderr остаётся заполненным. Поток не использует блокировки stdio; немедленное завершение
процесса прекращает его, если запись всё ещё заблокирована.

SIGINT/SIGTERM и Windows CTRL_C/CTRL_BREAK запрашивают ограниченную очистку. Намеренно вышедшие из
сеанса POSIX потомки, внешнее принудительное завершение утилиты запуска и непрерываемые ошибки ядра/устройств
не входят в гарантию. Закрытие Windows-консоли/принудительное завершение не является обрабатываемой отменой.
Для macOS выполнен только анализ исходников; сборка и работа не заявляются.
Неквалифицированная POSIX-ветка вне Linux сохраняет очистку process group; она не реализует гарантию
Linux для всего сеанса и не является поддержанной/квалифицированной платформой launcher в этой задаче.

## Точные нативные форматтеры для Linux

Переносимый путь — точные исполняемые файлы, переданные вызывающей стороной. Следующий локальный способ
получения архивов использован на Ubuntu 24.04 x86_64. Он применяет `curl`, `sha256sum` и нативную
распаковку CMake; conda, pip, Python и PowerShell не вызываются. Формат упаковки не добавляет
зависимость среды выполнения. Бинарники используют среду выполнения C/C++ хоста; другим дистрибутивам/архитектурам нужны
отдельно проверенные точные бинарники. URL, SHA-256 архивов и исполняемых файлов указаны в
отчёте об инструментах.

```sh
mkdir -p .work/native-format/uncrustify .work/native-format/clang
cd .work/native-format
curl -fL https://api.anaconda.org/download/conda-forge/uncrustify/0.83.0/linux-64/uncrustify-0.83.0-h54a6638_0.conda -o uncrustify.conda
echo '556ea427c375c2b9eb3122b19a1ce0891d535ad51a196f25c73786bb1d7c5156  uncrustify.conda' | sha256sum -c -
cd uncrustify
cmake -E tar xf ../uncrustify.conda
cmake -E tar xf pkg-uncrustify-0.83.0-h54a6638_0.tar.zst
cd ..
curl -fL https://files.pythonhosted.org/packages/a6/77/786aa0fc8a75d8ce94966bb33e44c63fec1964cbf343ee862ed6a5be38c1/clang_format-20.1.8-py2.py3-none-manylinux_2_27_x86_64.manylinux_2_28_x86_64.whl -o clang-format.whl
echo '7c6bcb7e01ba4f05a4c980fda147b330f7e4833c2aea8c92a0c2df9573ae7afe  clang-format.whl' | sha256sum -c -
cd clang
cmake -E tar xf ../clang-format.whl
chmod +x clang_format/data/bin/clang-format
cd ../../..
cmake -DUNCRUSTIFY=.work/native-format/uncrustify/bin/uncrustify -DCLANG_FORMAT=.work/native-format/clang/clang_format/data/bin/clang-format -DMODE=Check -P tools/format/Format-Cpp.cmake
```

Использовать оболочку с проверкой ошибок (`set -e`) либо останавливаться после любой ошибки загрузки,
проверки хеша или распаковки. Получение инструментов отделено от запуска форматтера; драйвер не загружает
инструменты и не подменяет версии.

## Точки входа проверок

Корневой CTest сохраняет прежние наборы тестов и добавляет контракты журналирования, форматирования, покрытия и аудита трассировки.
Обычный тест форматтера использует нативные поддельные инструменты для отрицательных контролей; передать оба
`ORG_TTLDTOR_QDCXX_UNCRUSTIFY` и `ORG_TTLDTOR_QDCXX_CLANG_FORMAT` при configure, чтобы добавить
реальные проверки конвейера. Только один параметр — ошибка. Полные проверочные матрицы передают оба.

`tests/conformance/tooling/matrix.cmake` принимает ROOT, свежие RUN/EVIDENCE, нативные GUARD/LOGGER,
исходники DOCTEST и пути UNCRUSTIFY/CLANG_FORMAT. Он запускает все обязательные профили компиляторов
и восемь сочетаний testing/QTP/TAPE через `--deny-interpreters`. `guard.cmake` отдельно проверяет
реальные пути интерпретаторов, поиск PATH, потомков с проглатыванием ошибки и форматтер, запускающий
интерпретатор; отладчики нельзя вкладывать, поэтому эти контроли guard не запускаются внутри другой
трассировки. Прежний интерфейс guard сохраняет запрет только Python для исторического воспроизведения.
Это доказательство запрета образов процессов в проверяемой цепочке, а не защищённая среда против переименованных или
встроенных интерпретаторов.
`recheck.cmake` пересобирает существующие профили матрицы и повторяет все применимые наборы CTest
после исправлений инструментов, сохраняя исходные доказательства. Он также повторяет автономный
bootstrap, реальное форматирование и точные аудиты трасс; нужны свежие каталоги RUN/EVIDENCE.

[Покрытие](../../tests/conformance/tooling/COVERAGE.ru.md) сопоставляет именованные обязательства;
[валидация](../../tests/conformance/tooling/VALIDATION.ru.md) записывает выполненные команды, результаты
и ограничения. Исходная проверка и самопроверка не закрывают независимую приёмку. Инструменты Java,
оставшиеся ручные Python-проверки и исторические артефакты сохраняют прежние требования.

## Offline-инструменты Java producer

[Production tape corpus](../upstream/real-tape-corpus/README.ru.md) предоставляет отдельно
закреплённые baseline/current producers, детерминированные offline-фикстуры FileWriter/FileReader,
Capture/Verify, ограниченные процессы и отрицательные проверки. Рабочим режимам Windows нужны
PowerShell, JDK 8, Maven и явный нативный UTC runner; справка переносима. Инструмент независим от
нативных configure/build/CTest. Launcher, скрипты регрессии справки и времени жизни процессов
документируют параметры, результаты, коды завершения и побочные действия через `-Help` и
`Get-Help -Full`.

## Приёмка обычных реальных tape

[Компаратор реальных tape](../upstream/real-tape-acceptance/README.ru.md) сравнивает полные нативные
проекции версии 7 и строго проверенное обычное подмножество прежних восьми типов явной версии 8
с закреплённым производственным Java reader. CppMode выбирает нативную проекцию независимо от
политики исходного времени Java NONE/MESSAGE; flags должны равняться 0, removal отсутствует.
Сопровождаемые проверки приёмки и владения процессами описаны рядом с компаратором. Рабочим режимам
Windows нужны явный нативный UTC runner, JDK 8, заранее собранный закреплённый producer и нативный
инспектор; справка не требует рабочих входов. Точные stdout/stderr дочерних процессов сохраняются
отдельными исходными файлами. Названный обычный набор, включая оба реальных производителя
STREAM/MESSAGE, прошёл полную приёмку. Дополнительные payload, flags и collectors вне этого
компаратора; его точная область описана рядом с ним.

## Необязательные контейнеры tape и production oracle

Инструментальный [компонент закрытых контейнеров](../../spec/protocol/TAPE-CONTAINERS.ru.md)
предоставляет ограниченный GZIP/strict ZIP32 pull decoder независимо от core/protocol и replay.
Для сборки включить `ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION=ON` и выбрать tape-inspection target
либо tests. OFF по умолчанию и ON без consumer не получают zlib и не включают C. Необязательный
`ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE` задаёт точный локальный архив zlib 1.3.2; CMake проверяет закреплённые
SHA-256, version и license. Произвольные FetchContent source overrides и system zlib discovery
отвергаются. Target экспортирует требование C++23 для header. Это API компонента;
[явный владеющий профиль просмотра](../../docs/TAPE-INSPECT.ru.md#владеющие-самодостаточные-источники-и-вывод-версии-9)
объединяет операционный выбор файлов, контейнеров и спутников.

[tape-container-oracle](../upstream/tape-container-oracle/README.ru.md) фиксирует фактические
FileWriter/FileReader/StreamCompression observations любого принятого pin без rebuild или записи
upstream sources/classes. В Windows нужны PowerShell 7.4, JDK 8, четырнадцать существующих class roots
и явный native UTC logger. `-Help`, отсутствие аргументов и `Get-Help -Full` не требуют рабочих
входов; README описывает Capture/Verify, параметры, exit codes и сохраняемые evidence.

Help regression выполняется явно из корня репозитория, отдельно от interpreter-free native
configure/build/CTest tree:

```powershell
cmake "-DPWSH=pwsh" "-DSCRIPT=tools/upstream/tape-container-oracle/Invoke-Oracle.ps1" "-DRUN=.work/container-oracle-help" -P tests/conformance/compressed_oracle_help.cmake
```

[Captures](../../tests/fixtures/protocol/tape-containers/CAPTURES.ru.md) и
[валидация](../../tests/fixtures/protocol/tape-containers/VALIDATION.ru.md) отделяют production
observations, native component checks и остающуюся integration acceptance.

## Bounded companion scanner и header admission

Tool-private [LONG/TEXT scanner](../../spec/protocol/SIDECAR-TIME.ru.md) доступен как
`qdcxx_tape_sidecar`, когда выбраны tests либо tape inspection. У него нет optional dependency,
clock и I/O. `TapeExternalTimePolicy::SIDECAR` отдельно допускает точные companion-time headers в
binary-catalog readers; прежние constructor signatures по-прежнему отвергают их. Отдельный владеющий профиль просмотра
объединяет пары источников и хронологический выбор времени; scanner остаётся без I/O.

Native tests `sidecar_scanner`, `sidecar_policy`, `sidecar_allocation`, `sidecar_windows_include`
проверяют конечные parsing/admission, allocation ordering и Windows-header compatibility. При
включённом optional compression component `sidecar_paired_owner` дополнительно проверяет настоящий
container/parser cleanup и cancellation с instrumented close/flush boundaries. Component tests
отделены от полного CLI file ownership, chronological mark application и corpus acceptance.

## Владеющий просмотр конечных tape

Явный [профиль самодостаточных источников](../../docs/TAPE-INSPECT.ru.md#владеющие-самодостаточные-источники-и-вывод-версии-9)
объединяет упорядоченные native-файлы, необязательные контейнеры и явное время NONE/MESSAGE/FIELD/LONG/TEXT.
Включите `ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT`; JSON и replay могут оставаться OFF. При Compression OFF
сохраняется NONE, а явные GZIP/ZIP отклоняются до работы с источниками. Отдельный `--help` прежнего
исполняемого файла перечисляет все селекторы, конечные ограничения/стандартные значения, единицы,
побочные действия и коды завершения без рабочих входных данных.

`source_sequence_cli`, `source_sequence_io` и `source_sequence_large` — native-регрессии CTest.
Большой тест владеет небольшим потоково создаваемым временным источником и проверяет канонические байты,
не удерживая полный вывод. Логические резервы и отдельно измеренный OS RSS остаются разными наблюдениями.


## Oracle локального IPF writer

[Сопровождаемый writer oracle](../upstream/ipf-writer-oracle/README.ru.md) captures/verifies
детерминированные Java writer bytes и reader projections, включая явные lossy/rejected boundaries.
Встроенные `-Help`/`-HelpRu` работают без inputs; optional NativeDirectory читает actual emitted
native writer files pinned Java reader. Сеть, SDK build и новая dependency не запускаются.

- [Oracle источников событий](../upstream/event-source-oracle/README.ru.md): захват/проверка идентичности и отдельная проверка noargs/EN/RU справки.

- [Order source/index oracle](../upstream/order-index-oracle/README.ru.md): production отображения, индексы, source IDs и справка EN/RU без рабочих входов.

- [Oracle возможностей источников](../upstream/order-source-capability-oracle/README.ru.md): publication/full-book и specific-source; EN/RU help без входов, Verify по умолчанию, JDK8/production classes, heap128MiB/60s, код 0/1.
- [Оракул периодов Candle](../upstream/candle-period-oracle/README.ru.md): production-типы агрегации, числовые границы и точный текст периодов; EN/RU-справка без рабочих входов.

- [Oracle option chains](../upstream/option-chains-oracle/README.ru.md): production-группировка профилей, порядок серий/strike и выборка; справка без аргументов/EN/RU без входов, ограниченный JDK8 capture/verify.
- [Oracle атрибутов Candle](../upstream/candle-attributes-oracle/README.ru.md): production-значения и операции с символами; EN/RU справка без входов, Verify по умолчанию, JDK8, heap128MiB/timeout60s, exit0/1.

- [CFI и дополнительные базовые активы oracle](../upstream/glossary-oracle/README.ru.md): Production oracle CFI/AdditionalUnderlyings: справка без входов на EN/RU, JDK8, heap128MiB/60s, Git30s.

- [Oracle ценовых шагов](../upstream/price-increments-oracle/README.ru.md): захват/проверка правил production Java8; `-Help` и `-HelpRu` без рабочих входов.
- [Oracle символов Candle](../upstream/candle-symbol-oracle/README.ru.md): production factories/getters/identity и raw getter bits; noargs/EN/RU help без работы, Verify по умолчанию, JDK8, heap128MiB/timeout60s, exits0/1.

- [Production oracle TradeETH](../upstream/trade-eth-oracle/README.ru.md): закреплённые mapping/delegate,
  composite/regional схемы, business double, переходы flags и ограниченные Capture/Verify.
  Get-Help и справка без аргументов не требуют рабочих путей.

[Oracle IPF compression fixtures](../../tests/fixtures/ipf/compression/README.ru.md) фиксирует конечные JDK8 gzip/ZIP и реальные IPF outcomes. CompressionOracle --help/--help-ru не требует output path и не выполняет file/network/native SDK работу.

- [CollectorOracle](../../tests/fixtures/ipf/collector/README.ru.md): TSV закреплённого JDK8/QD collector; явный --run, справка без входов noargs/EN/RU, без сети/workers.

- [Production oracle Summary](../upstream/summary-oracle/README.ru.md): настоящий delegate, classified corrected long OpenInterest,
  default/tiny/int схемы и regional absence; EN/RU help без входов, bounded Git/Java Capture/Verify.

- [Summary OI long reference](../upstream/summary-oracle/README.ru.md#исправленный-openinterest-long-reference): отдельный Invoke-OpenInterest.ps1, EN/RU help без входов, bounded Capture/Verify с сохранением raw delegate по ADR-0010.

- [ConnectionOracle](../../tests/fixtures/ipf/live/README.ru.md): закреплённый JDK8/QD live/conditional/carryover oracle, справка EN/RU без входов, конечные собственные loopback/server/handler threads.

- [Schedule query и filter oracle](../upstream/schedule-query-oracle/README.ru.md): actual Day/Session/range/DST/hot-update и фильтры, отдельные2026f differences, EN/RU-справка без входов; ограниченный JDK8 захват и сравнение.
- [Пример запросов Schedule](../../examples/schedule_queries/README.ru.md): явная инициализация данных, владеющие Day/Session, --help/--help-ru без входов.

- [CompletionWaitOracle](../../tests/fixtures/ipf/live/README.ru.md): pinned JDK8 first-snapshot wait/interrupt/close observations; EN/RU help без входов, собственный temporary regular file и конечный thread cleanup, без сети.

[Оракул текста событий](../upstream/event-text-oracle/README.ru.md) захватывает/проверяет фактический вывод событий/значений с явным контекстом, числовые изменения JDK по отдельным случаям и три исправления кеша TimeFormat по свежим instances. Без аргументов, EN/RU-справка и Get-Help не требуют рабочих путей и не запускают Java. [UTF-8 консольный пример](../../examples/event_text/README.ru.md) выводит готовую собственную строку через std::cout; входонезависимая справка не создаёт контекст/provider.

[Production oracle selectors](../upstream/selector-oracle/README.ru.md) проверяет pinned Java8 typed identity, source/regional/wildcard delegate demand и signed time bounds. Noargs/ENRU/Get-Help не требуют входов и не запускают Java; четыре operational пути, heap128MiB/60s, fresh evidence и Capture/Verify описаны вместе с инструментом.

[Oracle сериализованных вложений](../upstream/attachment-oracle/README.ru.md) отдельно проверяет тела стандартных Message/Configuration записей и Java Object access. Справка EN/RU без входов не запускает Java; ограниченный Capture/Verify сохраняет точные class/resource inventories и raw wire/tape fixtures.

- [Subscription observer oracle](../upstream/subscription-observer-oracle/README.ru.md): реальные pinned production introspection/change callbacks, reentry/exception traces, ограниченные capture/verify и EN/RU help без рабочих входов.
- [Feed filter oracles](../upstream/feed-filter-oracle/README.ru.md): production-свидетельства QD grammar/classifier, IPF identity/update и source config с конечными ресурсами, неизменяемыми class inventories и EN/RU help без рабочих входов.
- [Oracle версий Configuration](../upstream/configuration-version-oracle/README.ru.md): production collectors/delegate, первое отрицательное default-событие, signed bounds, batch и STREAM; native Feed tests отдельно покрывают
поздний replay cache при подписке. EN/RU/Get-Help без входов, конечный JDK8 Capture/Verify с точными
class/resource inventories.

- [Endpoint configuration oracle](../upstream/endpoint-configuration-oracle/README.ru.md): реальные pinned address/property/lifecycle consumers, собственный loopback, finite Capture/Verify и EN/RU help.

- [Oracle серверной агрегации](../upstream/server-aggregation-oracle/README.ru.md): actual Java public bounds, full/delta/reset/clamp, TimePeriodInfo и реальный reconnect; input-free EN/RU help, finite Capture/Verify.

- [Time-series oracle](../upstream/time-series-oracle/README.ru.md): закреплённые Java8 identity/range/capability, наследование, bounded Capture/Verify и справка EN/RU без входных путей.

- [Oracle HISTORY receiver](../upstream/history-receiver-oracle/README.ru.md): настоящие стандартные TimeAndSale delegate/collector, два диапазона и точный Java wire, конечные Capture/Verify и EN/RU справка без входных путей.

- [Oracle SpreadOrder и AnalyticOrder](../upstream/order-variants-oracle/README.ru.md): варианты, явные источники и независимое время FIELD.
- [Production oracle Order](../upstream/order-oracle/README.ru.md): полные source/index, physical VOID mapping,
  реальные HISTORY parser/delegate values, numeric/FOB/flags/removal границы; EN/RU справка без входов.

- [Oracle исполнения callback](../upstream/callback-execution-oracle/README.ru.md): actual Java inheritance/override, inline и serial delivery на двух workers; конечные Capture/Verify и EN/RU help без входных путей.

- [Oracle живого диапазона HISTORY](../upstream/live-history-range-oracle/README.ru.md): настоящие публичные Java-мутации диапазона, demand и удержанные callbacks, конечные Capture/Verify и EN/RU справка без входных путей.
- [Оракул OptionSale](../upstream/option-sale-oracle/README.ru.md) записывает индексные события HISTORY для схем закреплённой версии Java QD.
