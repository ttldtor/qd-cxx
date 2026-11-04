# Oracle версий Configuration

[English version](README.md). Английская версия определяет контракт.

`Invoke-Oracle.ps1` захватывает или сверяет481 детерминированное наблюдение закреплённого QD3.355,
revision `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Java helper использует production collectors,
`VersionedRecord`, `ConfigurationDelegate` и публичные getters события. Сеть, endpoint, сборка и загрузка
SDK не запускаются. Готовый production classpath должен существовать.

```powershell
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help ./tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/configuration-version-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/configuration-version-check
```

Без аргументов, с `-Help` и `-HelpRu` рабочие входы не требуются, работа не начинается. `Mode` по умолчанию
равен `Verify`. Для работы обязательны `Upstream`, oracle `JavaHome` (JDK8), существующий UTC `Logger`
и новый `RunDirectory` внутри `.work` этого checkout. Перенаправляющие ancestors отклоняются. Capture
создаёт поддерживаемые fixture/provenance кандидаты и отказывает при существующем fixture-каталоге.
Verify сверяет точные наблюдения, хеши исходников и provenance, не заменяя fixtures.

Одновременно работает один Java-процесс, heap128MiB, timeout60с. По timeout завершается только его
дерево процессов. `RunDirectory` хранит полные UTC command/exit logs, raw stdout/stderr, точные source
и class/resource inventories, два timezone outputs и receipt. Exit0 — успех или справка; exit1 — ошибка
входа, reference/provenance, timeout, процесса или наблюдений. JDK форматтера задаётся отдельно от JDK8 oracle.

Сценарии проверяют standard, absent-Version и wide-Version mapping в TICKER/STREAM: первые отрицательные,
нулевые и граничные версии, меньшие/равные обновления и несколько версий одного batch. Native Feed
tests отдельно покрывают поздний replay cache при подписке в `misc_feed_tests.cpp`.
Absent использует production `VersionedRecord` без поля и проверяет отключённый mapping, без отдельной
квалификации environment-property source. Default-событие проверяется целиком: symbol, eventTime, version,
attachment. Writer проверяет ошибки flush и close до успешного выхода. UTC и Pacific/Honolulu должны
совпадать побайтно. См. [fixtures](../../../tests/fixtures/configuration-version/README.ru.md) и
[нативный контракт](../../../docs/MISC-EVENTS.ru.md).
