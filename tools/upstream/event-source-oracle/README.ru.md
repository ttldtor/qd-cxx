# Наблюдения источников событий

[English](README.md). Английская версия является нормативной.

`Invoke-Oracle.ps1 (no arguments), -Help` или `-HelpRu` показывает встроенную справку без входов и работы.
Средство PowerShell 7 использует готовые production-классы закреплённого QD 3.355 и JDK8;
оно не собирает и не загружает SDK. Для форматирования Java нужен отдельный JDK21.

```powershell
pwsh -File tools/upstream/event-source-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-source-check
```

`Mode` по умолчанию Verify. Для работы обязательны все четыре пути. `RunDirectory`
должен находиться в `.work` этого checkout; его собственные логи могут заменяться.
Одновременно работает один ограниченный процесс Java, heap 128MiB, timeout 60 секунд.
При тайм-ауте останавливается только собственное дерево процессов. Вывод UTC и
Pacific/Honolulu должен совпадать; инвентарь production-классов не должен меняться.
Команды, исходные потоки, инвентари и receipt сохраняются в `RunDirectory`.

Capture заменяет только [oracle.tsv](../../../tests/fixtures/event-sources/oracle.tsv)
и [provenance.json](../../../tests/fixtures/event-sources/provenance.json) кандидатами;
проверяйте их вместе. Verify сравнивает повторные наблюдения и хеши исходников,
Java-кода и средства запуска, не заменяя fixtures. Exit 0 означает успех/справку;
exit 1 — неверные входы, несовпадение provenance/наблюдений, тайм-аут или ошибку процесса.
[Покрытие](../../../tests/fixtures/event-sources/README.ru.md) описывает точный объём;
[контракт](../../../spec/events/EVENT-SOURCES.ru.md) задаёт поведение C++.
