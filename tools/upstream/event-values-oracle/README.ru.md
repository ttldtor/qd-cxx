# Production oracle значений событий

[English version](README.md). Английская версия является определяющей.

`EventValuesOracle.java` наблюдает production utilities, двенадцать market enum tables, IndexedEvent
masks и методы времени/sequence Trade из [upstream.lock](../../../upstream.lock). Вызываются настоящие
EventUtil, TimeNanosUtil, DayUtil и TimeUtil, включая отрицательное время, переполнение, нормализацию
произвольных Gregorian day, неверные входы и неизвестные enum-коды. Стандартная запись TimeAndSale
получается из DXFeedScheme; наблюдаются parse/render настоящего SHORT_STRING поля ExchangeSaleConditions.
Диагностическая реализация не подставляет ожидаемые преобразования. Endpoint не открывается, SDK не собирается.

```powershell
pwsh -File tools/upstream/event-values-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-values-verify
```

`Mode` по умолчанию Verify: сравнивает байты fixture и provenance. Capture заменяет только
`tests/fixtures/event-values/oracle.tsv` и `provenance.json`. Все четыре рабочих пути обязательны:
точный закреплённый checkout с готовыми production classes, JDK8, native UTC logger и **новый** каталог
доказательств внутри `.work` этого checkout. Существующие каталоги и перенаправляющие предки запрещены.
Source hashes, неизменный inventory production classes, версия Java/хеши инструментов, source revision,
хеши harness/launcher и digest наблюдений связывают захват. Verify не перезаписывает fixtures.

Дочерние процессы последовательны; каждый имеет Java heap максимум 128MiB и timeout60 секунд.
Останавливается только собственное дочернее дерево launcher при timeout. RunDirectory сохраняет полные
команды, raw streams, UTC phase bounds, class/source inventories, наблюдения и receipt. Запуски UTC и
Pacific/Honolulu должны совпадать побайтно. Exit0 означает успех/справку; exit1 — неверные входы,
недоступный/неверный reference, process/timeout, несовпадение наблюдений/provenance либо I/O failure.

Без аргументов, `-Help`, `-HelpRu` и `Get-Help ./Invoke-Oracle.ps1 -Full` показывают справку без рабочих
входов, создания каталогов, Java/SDK/network. Справка без аргументов совпадает с английской. CTest
регрессия также проверяет перевод, игнорирование неверных рабочих путей в режиме справки и отказ
неполного рабочего ввода. Форматируйте harness закреплённым JDK21 formatter перед Capture; после
финальной фиксации исходников повторите Verify. Глобальные настройки Java, Git, toolchain и SDK не меняются.

См. [контракты значений](../../../spec/events/EVENT-VALUES.ru.md) и
[структуру захвата](../../../tests/fixtures/event-values/CAPTURES.ru.md). Native compiler/sanitizer,
installed/relocated consumers и независимое ревью проверяются отдельно; oracle не подтверждает будущие
typed event mappers, календарные расписания и полноту принимающего клиента.
