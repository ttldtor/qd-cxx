# Oracle ценовых шагов

[English version](README.md). Английский текст является основным.

`Invoke-Oracle.ps1` захватывает или проверяет реальные результаты production Java8 `PriceIncrements` из ревизии `upstream.lock`. Используются существующие производственные классы `dxfeed-api`, `dxlib` и `qd-core`; upstream не компилируется, зависимости не загружаются, сеть и сборка SDK не запускаются.

```powershell
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/price-increment-verify
```

Без аргументов, с `-Help` или `-HelpRu` выводится справка без рабочих входов, создания каталогов или Java-процессов. Параметры описаны в `Get-Help`. `-Mode` по умолчанию равен `Verify`; `Capture` заменяет только `oracle.tsv` и `provenance.json` этого oracle. Для рабочего режима явно задаются все четыре пути. `RunDirectory` должен находиться в `.work` текущего checkout; собственные логи в нём могут заменяться. Глобальные настройки Java не меняются.

Launcher проверяет закреплённую ревизию и отсутствие изменений относящихся к задаче производственных исходников, сохраняет перечень классов до и после выполнения, компилирует harness JDK8 и получает результаты UTC/English и Pacific/Honolulu/French. Байты должны совпасть. Каждый Java-вызов, включая определение версии, ограничен heap128MiB и timeout60s; Git — timeout30s. По таймауту останавливается только собственное дерево процессов. Exit0 означает успех/справку; Exit1 — неверные входы, reference, ошибку/таймаут процесса либо несовпадение наблюдений или происхождения.

Fixture покрывает грамматику фабрик и binary64, точные текст/хеш/диапазоны, точность и границы диапазонов, все режимы округления, пределы направления signed32, проверку step и детерминированные численные окрестности. Ожидаемые результаты непосредственно вызывают production API; harness не вычисляет ожидаемые binary64 самостоятельно. Форматы строк описаны в [описании fixture](../../../tests/fixtures/price_increments/README.ru.md). Полные логи процессов, хеши исходников/классов и `receipt.json` сохраняются в выбранном приватном каталоге. `Verify` не переписывает fixture.
