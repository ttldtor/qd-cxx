# Production oracle HISTORY receiver

[English version](README.md).

`Invoke-Oracle.ps1` захватывает или проверяет закреплённый production HISTORY collector Java8 с
настоящими стандартными TimeAndSale schema и delegate. Его 242 TSV-строка содержит полную физическую
схему, преобразование миллисекунд в сырое record time, два независимых диапазона, partial/covered
доставку, END/removal, transaction, SNIP и cached replay после переподключения view. Семь QTP fixtures
создаются непосредственно `BinaryQTPComposer`, без самописного бизнес-энкодера. Business output
сопровождается полными сырыми значениями non-VOID integer/long/wide-decimal полей. Физические VOID
хвосты не становятся логическими native-полями.

Это доказательство raw QDAgent/delegate. Оно не подтверждает публичную типизированную native
TimeAndSale, полноту DXFeed callback, ACK peer и будущую сетевую свежесть. Native test подаёт именно
эти bytes общим production HISTORY queue/receiver и observation helpers.

```powershell
pwsh -NoProfile -File tools/upstream/history-receiver-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/history-receiver-check-01
```

Без аргументов, с `-Help`, `-HelpRu` и через `Get-Help ./Invoke-Oracle.ps1 -Full` пути не нужны,
основная работа не запускается. Verify выбран по умолчанию. Capture заменяет только
`tests/fixtures/history-receiver/oracle.tsv`, `provenance.json` и семь `wire-step-N.qtp` файлов.
Все четыре рабочих пути обязательны. Новый каталог запуска должен находиться внутри `.work`
checkout, без перенаправляющих предков; существующие доказательства не перезаписываются. Готовые
production классы/ресурсы перечисляются до и после, upstream не собирается и не изменяется.
Хеши исходников, JDK, harness, launcher и fixtures связывают наблюдения с входами. Javac/Java
последовательны, heap 128MiB; срок каждого процесса 60s. Останавливаются только собственные дочерние
процессы с истёкшим сроком. UTC и Pacific/Honolulu должны дать побайтно одинаковые TSV и каждый
wire-файл. Exit0 означает успех/справку; Exit1 — неверный ввод, ошибку процесса/таймаут,
несовпадение reference/provenance или наблюдений. Полная диагностика остаётся в каталоге запуска.
Сеть и подготовка SDK не используются.

См. [native-контракт](../../../spec/subscriptions/HISTORY-RECEIVING.ru.md).
