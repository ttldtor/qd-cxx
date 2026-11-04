# Production oracle цепочек опционов

[English](README.md). Английская версия является основной.

`OptionChainsOracle.java` читает проверенные UTF-16 hex-поля и вызывает закреплённые
production `OptionChainsBuilder.build`, `OptionChain.getSeries`,
`OptionSeries.getCalls/getPuts/getStrikes`, `getNStrikesAround`. Скопированного алгоритма
группировки/сравнения/выборки нет. Для детерминированного вывода сортируется только
внешняя карта цепочек, порядок которой Java не гарантирует.

```powershell
pwsh -NoProfile -File tools/upstream/option-chains-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/option-chains-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/options-verify
```

Verify — режим по умолчанию, сравнивает точные fixture/provenance. Capture заменяет
только `tests/fixtures/option-chains/oracle.tsv` и `provenance.json` кандидатами.
Нужны существующие production classes закреплённого QD (`dxfeed-api`, `dxlib`, `qd-core`),
JDK8, UTC logger и собственный RunDirectory внутри `.work` текущего checkout.
Сеть, загрузки и сборка SDK не запускаются. Каждый Java/javac/version-процесс запускается
отдельно с heap128MiB и timeout60s; при timeout останавливается только его дерево.

Результат: полные команды/streams/exits, фактическая Java version, хеши исходников/бинарников,
неизменные inventories production classes, receipt.json. Собственные run-файлы могут
заменяться. Публичный provenance содержит относительные пути исходников и хеши;
приватные пути окружения остаются в run-логах. Exit0 — успех/справка; exit1 — неверные
входы/reference, timeout, ошибка процесса или несовпадение. Без аргументов и с -Help
выводится одинаковая английская справка; -HelpRu — русская. Справка не требует входов,
не создаёт каталогов и не запускает Java. `Get-Help -Full` описывает каждый параметр.

[Формат fixtures и покрытие](../../../tests/fixtures/option-chains/README.ru.md).
