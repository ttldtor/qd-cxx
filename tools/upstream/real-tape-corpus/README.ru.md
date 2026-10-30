# Offline-корпус production tape

[English version](README.md). Английский текст является определяющим.

`Invoke-Corpus.ps1` собирает отдельный точно закреплённый Java QD producer, вызывает production
`FileWriter`/`FileReader` и создаёт либо проверяет семь небольших артефактов. Библиотека C++,
wire-контракт и [базовая версия совместимости](../../../upstream.lock) не меняются. Отдельный
[producer lock](producer.lock.json) выбирает baseline 3.355 с commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и current 3.356 с commit
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Последний повторно подтверждён как HEAD официального
репозитория 2026-10-30 UTC; это выбранный точный producer, а не плавающая последняя версия.

## Требования и запуск

Рабочие режимы квалифицированы только для Windows и отклоняют другие платформы до работы с
путями, исходниками и файлами; справка переносима. Нужны PowerShell 7.4+, Git, JDK 8, Maven 3.6+
и явный нативный executable `qdcxx-run-utc`. Соберите существующий developer runner через
[bootstrap](../../dev/README.ru.md#первая-сборка); launcher его не загружает.
Upstream enforcer требует Java `[1.8,1.9)`;
форматтер отдельно требует JDK 21+. Используйте отдельную чистую копию выбранного commit с origin
строго `https://github.com/devexperts/QD.git`. Не запускайте параллельную сборку в той же копии:
launcher выполняет `qds-file -am clean compile`, пропуская upstream-тесты. Не указывайте активную
копию другого oracle. Исходники и lock проверяются до и после сборки. Текстовые хеши lock
нормализуют CRLF в LF; metadata отдельно сохраняет хеши фактических байтов исходников. Настройки
Git, Java, Maven и часового пояса хоста не меняются.

Для справки рабочие входные данные не нужны; сборка и работа с файлами не начинаются:

```powershell
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 -Help
Get-Help ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 -Full
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Test-Help.ps1 -RunDirectory .work/producer-help
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Test-Lifetime.ps1 `
    -Runner $utcRunner -JavaHome $oracleJdk -RunDirectory '.work/producer lifetime'
```

Запуск без аргументов печатает ту же справку launcher. Задайте `$producerCheckout`, `$oracleJdk`,
`$mavenHome`, `$utcRunner` и `$producerCache`: пути отдельной копии, JDK 8, установленного Maven,
нативного UTC runner и локального
кеша Maven. Кеш и новый каталог запуска должны находиться внутри `.work` репозитория, но вне
копии producer. Запускайте из корня репозитория:

```powershell
$common = @{
    Producer = 'current'
    UpstreamRoot = $producerCheckout
    JavaHome = $oracleJdk
    MavenHome = $mavenHome
    Runner = $utcRunner
    MavenRepository = $producerCache
}
& ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 @common `
    -Mode Capture -RunDirectory .work/producer-current-capture
& ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 @common `
    -Mode Verify -RunDirectory .work/producer-current-verify
```

Для 3.355 задайте `Producer = 'baseline'` и отдельную baseline-копию. Maven по умолчанию работает
offline; `-AllowDependencyDownloads` явно разрешает получение зависимостей. Генерация и replay
фикстур никогда не используют сетевой endpoint. Capture сохраняет приватные кандидаты и не
обновляет эталоны; Verify сравнивает точный набор и байты закреплённых артефактов producer. Оба
режима повторяют генерацию и выполняют отрицательные проверки. Exit 0 означает успех всех
запрошенных проверок; ошибки входных данных, сборки, replay, сравнения, timeout и вывода дают
ненулевой код. Прерванные запуски остаются `INCOMPLETE`.

Launcher вызывает штатный Maven classworlds entry point с boot JAR установки и `bin/m2.conf`,
исключая повторный разбор аргументов shell. Он сохраняет фактические версии Maven/JDK, массивы
аргументов, границы UTC, хеш executable runner, хеши исходников и harness, каждого файла classpath,
файлов Maven runtime, кешированных JAR зависимостей и выходных данных. Кеш зависимостей —
инвентаризация сборки, а не утверждение, что каждый JAR загружен процессом корпуса. `raw/` хранит
объединённый UTF-8 stdout runner с timestamps, отдельную диагностику runner, нативный UTC-лог и
нормализованный объединённый payload. Это декодированный текст, а не отдельные сырые потоки байтов
Java. Тесты справки Java отдельно перенаправляют stdout/stderr дочерних процессов сразу в файлы,
проверяют точную справку/пустой stderr и не создают удерживаемые потомками pipe. Полные логи
остаются приватными. [Переносимый provenance](../../../tests/fixtures/protocol/real-tape/producer/README.ru.md)
не содержит путей машины и сырых логов сборки.

## Сценарии, пределы и завершение

Writer явно выбирает BINARY, сжатие NONE, опции EMPTY_SET и STREAM_DATA. `compact-none` записывает
три compact integer (`INT_MIN`, ноль, `INT_MAX`) и cipher/raw символы. `quote-none` и
`quote-message` используют полную стандартную схему Quote и production `QuoteDelegate.putEvent`;
отличается только настройка времени NONE/MESSAGE. Фиксированные значения Quote, символы и время
heartbeat заменяют часы. Урёзывания схемы, переписывания wire-байтов и диагностического decoder
нет. Каждый сценарий содержит три записи; каждый артефакт ограничен 65 536 байтами.

Reader использует ту же полную локальную схему, один буквальный путь файла, MAX_SPEED, без cycle
и resync, с автоматическим определением format/time/compression. Callback cursor заимствуется
только внутри callback; наблюдения сразу копируют имена, символы, flags и каждый integer slot с
полным field type. Проверяются точные исходные значения и event-time sequence, один descriptor,
ноль parser errors, три доставки и равенство обработанных байтов физическому размеру. Канонический
UTF-8 TSV везде использует LF. В положительных логах writer/reader запрещены сообщения error level
и известные сообщения отказа разбора файла. Это необходимо: upstream может записать ошибку
write/read/close в лог и нормально вернуться; одного exit 0 для завершения недостаточно. Также
проверяются flush/close наблюдений. Конечный корпус не является общим валидатором произвольных
записей.

Седьмой артефакт явно получен из `quote-message.qtp` удалением двух последних байтов. Это
отрицательный вход, а не вывод production writer. Production FileReader обязан сообщить ошибку
и доставить только префикс. Его наблюдение содержит `rejected`, никогда `complete`. Ещё пять
проверок отклоняют отсутствие/лишний/пустой файл, изменение wire-байтов и байтов наблюдения.
[Java helper](RealTapeCorpus.java) также предоставляет запуск без аргументов/`--help` и команды
`generate`, `verify`, `negatives`, `help-tests`; для обязательных ограничений процесса вызывайте через launcher.

По умолчанию probe ограничен 15 секундами и heap 128 MiB; настраиваемые диапазоны — 1–120 секунд
и 32–512 MiB. Для Maven отдельно заданы 600 секунд по умолчанию (1–1800) и heap 512 MiB. Каждый
поток runner ограничен 8 MiB; проверки выполняются с конечным интервалом, возможно превышение на
буфер в полёте. Существующий нативный runner владеет Windows kill-on-close job до запуска Java
и завершает оставшихся потомков при завершении Java root. Внешний deadline включает drain
потоков и завершение payload; timeout завершает runner, закрывая принадлежащий ему job, с
конечным ожиданием подтверждения в пять секунд. Ресурсная остановка не становится завершением.
`Test-Lifetime.ps1` воспроизводит завершение root с удерживающим оба pipe потомком и timeout
активного root, проверяя только собственные сохранённые parent/child PID. Проверены рабочие
каталоги Windows с пробелами и Unicode; относительные аргументы harness/output обходят
наблюдаемое ограничение абсолютного Unicode classpath JDK 8. Произвольные Unicode-пути установки
JDK/Maven/исходников не квалифицированы. Небольшие фиксированные число фикстур, размер артефактов и число мутаций
ограничивают сохранённые тестовые данные.

## Доказательства и ограничения

[Валидация](../../../tests/conformance/protocol/real-tape/producer/VALIDATION.ru.md) фиксирует
выполненные проверки и хеши исходников. Контракты видны в выбранных текущих
[FileWriterImpl](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java),
[FileReader](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java)
и [Connect](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-tools/src/main/java/com/devexperts/qd/tools/Connect.java).
Реальный текущий `Tools Help Connect` проверен отдельно без подключения. Сборки из class-файлов
сообщают `QDS-UNKNOWN`; provenance устанавливают точный commit и хеши.

Необязательная live demo-запись здесь **не реализована и не запускалась**. Endpoint, credentials,
доступность подписок, сетевые пределы и заполненное demo не заявлены. Offline production-данные
дают детерминированный заполненный fallback. Контейнеры сжатия, sidecars, split files, другие
схемы событий и равенство значений/порядка C++ остаются отдельной работой. Запуск producer launcher
в Linux/macOS и POSIX-очистка потомков не заявлены. Потребление переносимых фикстур нативными
Linux-тестами — отдельная интеграционная проверка. Общий corpus inventory
и исторические captures не меняются.
