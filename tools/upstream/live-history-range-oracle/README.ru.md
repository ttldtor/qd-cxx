# Oracle подключённой подписки с живым диапазоном HISTORY

[English version](README.md).

`Invoke-Oracle.ps1` наблюдает настоящий LOCAL_HUB endpoint зафиксированной Java8, публичную
`DXFeedTimeSeriesSubscription<TimeAndSale>`, demand настоящего distributor QDHistory и публичные
event callbacks. Конечный управляемый executor удерживает старую доставку при изменении fromTime.
57 строк TSV охватывают начальный MAX, сужение/расширение, отключение данных, быстрые изменения,
свойство пустого набора, два пересекающихся диапазона, удаление символа и свойство после close.
Изменение только границы передаётся как ADD с последней сырой границей, без REMOVE; MAX остаётся
физическим sentinel demand. Наблюдения UTC и Pacific/Honolulu должны совпадать побайтно.

Это наблюдения подключённого production Java API, а не native public TimeAndSale mapper, Java TCP
recovery test или подтверждение peer. Native приём использует общий путь HISTORY и отдельно
захваченные [wire fixtures стандартной записи](../history-receiver-oracle/README.ru.md).
См. [контракт живого диапазона](../../../spec/subscriptions/LIVE-HISTORY-RANGES.ru.md).

```powershell
pwsh -NoProfile -File tools/upstream/live-history-range-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/live-history-range-check-01
```

Режим по умолчанию — Verify. Без аргументов, с `-Help`, `-HelpRu` и через
`Get-Help ./Invoke-Oracle.ps1 -Full` справка доступна без входных путей и не запускает работу.
Все четыре рабочих пути обязательны. RunDirectory должен быть новым внутри `.work` этого checkout,
без перенаправляющих предков; прежние доказательства не перезаписываются. Capture заменяет только
`tests/fixtures/live-history-range/oracle.tsv` и `provenance.json`. Существующие зафиксированные
production classes/resources инвентаризуются до и после. Хэши source, JDK, harness, launcher и fixture
связывают наблюдения. Последовательные subprocess имеют deadline 60 секунд; javac/Java используют
heap128MiB. Останавливаются только собственные children с timeout. Полные UTC commands/exits,
raw streams, inventories и receipt сохраняются в run directory. Нет сборки/записи upstream, сети
или provisioning SDK. Exit0 означает успех/справку; Exit1 — неверные inputs, ошибку процесса,
timeout или несовпадение provenance/наблюдений.

Первичные исходники зафиксированной версии:
[DXFeedTimeSeriesSubscription](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedTimeSeriesSubscription.java),
[DXFeedImpl](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedImpl.java),
[History](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/History.java).
