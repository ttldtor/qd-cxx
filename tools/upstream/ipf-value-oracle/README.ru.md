# Наблюдения значений instrument profile

[English version](README.md). Английская версия нормативная.

`pwsh -File tools/upstream/ipf-value-oracle/Invoke-Oracle.ps1 -Help` показывает встроенную справку EN, `-HelpRu` — RU,
без входов, создания каталогов и Java/network работы. Windows PowerShell 7 launcher использует
готовый JDK 8, точный закреплённый checkout Java QD и его production classes; JDK форматтера отдельный.
Он не собирает и не загружает SDK. Oracle не входит в production-зависимости C++.

```powershell
pwsh -File tools/upstream/ipf-value-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/ipf-verification
```

`Mode` по умолчанию Verify. Для работы обязательны `Upstream`, `JavaHome`, `Logger`, `RunDirectory`.
Выходной каталог должен находиться внутри `.work` текущего checkout; существующие собственные логи
могут заменяться. Java process использует heap 128MiB и timeout 60 секунд. Одновременно работает один
процесс. Наблюдения UTC и Pacific/Honolulu должны совпадать. Инвентаризации production classes
до/после должны совпадать. Полные UTC command logs, raw streams, class inventories и receipt остаются
в выходном каталоге. Exit 0 — успех/справка; exit 1 — неправильный вход, несовпадение
reference/provenance/наблюдения, timeout или ошибка процесса. Launcher останавливает только
собственное дерево процессов при timeout.

Capture заменяет только `tests/fixtures/ipf/value-oracle.tsv` и `provenance.json` кандидатами;
проверяйте их вместе. Verify пересоздаёт наблюдения и проверяет fixture, source/harness/launcher
hashes и class inventory без замены проверенных fixtures. После изменения Java/launcher выполните
[закреплённое форматирование](../../format/README.ru.md) и Capture/Verify; не исправляйте hashes вручную.
[Формат fixture и покрытие](../../../tests/fixtures/ipf/README.ru.md) и
[контракт значения](../../../spec/ipf/PROFILE.ru.md) определяют границы доказательств.
