# Oracle локального несжатого IPF reader

[English version](README.md). Английская версия нормативная.

`pwsh -File tools/upstream/ipf-reader-oracle/Invoke-Oracle.ps1 -Help` показывает встроенную EN-справку;
`-HelpRu` — RU. Справка не требует входов, не создаёт каталог и не запускает Java.

```powershell
pwsh -File tools/upstream/ipf-reader-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/ipf-reader-verification
```

Launcher использует готовые production classes закреплённого QD 3.355 и JDK 8, без сборки SDK,
загрузок, сети, проверки сжатия и новых C++ dependencies. `Mode` по умолчанию Verify;
`Upstream`, `JavaHome`, `Logger`, `RunDirectory` обязательны для работы. Run directory должен
находиться внутри `.work` этого checkout. Для нового capture выбирайте свежий собственный каталог:
launcher заменяет именованные outputs, но не удаляет исторические/orphan files. Java использует
heap 128 MiB и timeout 60 секунд, один процесс одновременно; останавливается только собственное
дерево процессов при timeout.

Capture записывает кандидатные raw inputs/outcomes и provenance в `tests/fixtures/ipf-reader`.
Verify пересоздаёт и сравнивает точные filenames/hashes, хеши отформатированных исходников,
reference revision/source hashes и неизменную инвентаризацию production classes. Проверенные
fixtures не заменяются. Эталон использует default warn-and-return при отсутствующем completion.
Полные UTC logs, raw streams, source/class inventories и receipt сохраняются в RunDirectory.
Exit0 — успех/справка; exit1 — входы, timeout, ошибка процесса либо несовпадение.
После изменения Java/launcher используйте [закреплённое форматирование](../../format/README.ru.md)
и Capture/Verify, не исправляйте старые hashes вручную. См.
[формат/покрытие случаев](../../../tests/fixtures/ipf-reader/README.ru.md) и
[контракт reader](../../../spec/ipf/READER.ru.md).
