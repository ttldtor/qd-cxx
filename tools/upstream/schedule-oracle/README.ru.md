# Production oracle расписаний

[English version](README.md). Английский текст является основным.

`Invoke-Oracle.ps1` захватывает или проверяет фактические закреплённые Java-фабрики `Schedule`,
перечисление площадок и построенные локальные правила. Reflection читает модели настоящего
конструктора; harness не переписывает parser и не вызывает будущие C++-запросы Day/Session.

Нужен PowerShell 7.4 или новее. Без аргументов, с `-Help`, `-HelpRu` и `Get-Help -Full` рабочие пути
не требуются, run directory не создаётся и Java не запускается. Рабочие режимы требуют готовые
production classes точной [upstream revision](../../../upstream.lock), reference JDK8/TZDB из
[манифеста identity](../../../resources/schedule/manifest.json) и готовый нативный logger
`qdcxx-run-utc`. JDK форматтера выбирается отдельно.

```powershell
pwsh -NoProfile -File tools/upstream/schedule-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/schedule-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/schedule-check-new
```

`Verify` — default: сравнивает tracked-наблюдения/provenance без замены. `Capture` записывает
проверяемые кандидаты `tests/fixtures/schedule/oracle.tsv` и `provenance.json` только после совпадения
двух timezone/locale запусков и неизменности source/class inventories. `RunDirectory` должен быть
новым внутри `.work` checkout; старые логи отклоняются, а не перезаписываются. Сохраняются revision,
точные хеши source/runtime/tool/input/output и inventories классов. Reference не собирается и не
изменяется; приобретение SDK и сеть не используются.

Одновременно работает один собственный JVM с `-Xmx128m` и timeout 120 секунд. По timeout завершается
только собственное дерево процессов. Java harness ограничивает вход 4 096 строками/16 MiB, выход —
16 MiB. Вход — UTF-8 TSV с шестью cells: ID, режим (`D` default/`N` named/`V` venues/`U` typed update),
hex UTF-16 площадки, hex UTF-16 trading hours, типизированная версия, упорядоченные typed entries.
Обновления проходят настоящий Java-загрузчик defaults через его `TimeFormat.GMT`; это не означает,
что типизированная C++-граница реализует raw-byte загрузчик.

Результаты — реальные модели успешного построения, классы семантических исключений или списки
площадок. Ошибка инструмента/reflection завершает захват и не превращается в semantic fixture.
Наблюдения UTC/English и Pacific-Honolulu/French должны совпасть. Корпус включает все 631 reference ID,
100 именованных площадок, полные семьи грамматики, malformed inputs и изменения typed defaults.
Fixture доказывает эти наблюдения, а не каждую возможную строку или будущий запрос по дате.

Exit 0 — успех/справка. Exit 1 — отсутствующие/несовпавшие входы/provenance, существующий run directory,
timeout, ошибка процесса или наблюдений. Справка не имеет workload-побочных действий; рабочие режимы
пишут только выбранный run directory и, в Capture, два файла кандидатов.

Внутренний entry point `ScheduleOracle` также предоставляет справку без аргументов/`--help`/`--help-ru`,
использует ту же конечную схему и пишет явный output. Для runtime/provenance/process limits запускайте
его через launcher. Первичные исходники —
[Schedule.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/schedule/Schedule.java)
и [TimeUtil.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java).
