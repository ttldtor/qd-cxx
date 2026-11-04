# Production oracle TradeETH

[English version](README.md).

TradeETHOracle.java вызывает actual generated TradeETHMapping, TradeETHDelegate и TradeBase из ревизии
upstream.lock. Полные composite/regional physical variants строятся из production DXFeedScheme,
с двухслотовыми полями и VOID tails. Формулы numeric/time/flags/symbol не подменяются. Чтение
соседнего physical slot у однослотового TradeId сохраняется как основание явного native отказа.

Справка: Get-Help ./Invoke-Oracle.ps1 -Full; вызов без аргументов, с -Help или -HelpRu.
Для неё не нужны рабочие пути, Java, SDK или сетевое подключение.

```powershell
pwsh -File tools/upstream/trade-eth-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/trade-eth-verify
```

Mode по умолчанию Verify: сравнение raw bytes/provenance без замены fixtures. Capture заменяет
только tests/fixtures/trade-eth/oracle.tsv и provenance.json. Обязательны четыре рабочих пути:
точный locked checkout с уже собранными production classes, JDK8, UTC logger и **новый** каталог
доказательств внутри .work этого checkout. Redirecting ancestors и повторное использование каталога
запрещены. Upstream source и SDK не пересобираются и не меняются.

Процессы последовательны, Java heap не более128MiB, timeout60 секунд. Останавливается только
собственное дерево процесса при его timeout. Каталог сохраняет полные команды, exits, UTC intervals,
source/class inventories и ошибки. Java version берётся из настоящей bounded JDK8 version line,
а не служебной строки logger. Фиксированные входы включают dxscheme.nanoTime=true и TradeETH exchanges
Q/z, часовые пояса UTC/Pacific-Honolulu. Q/z — fixture inputs, не константы реализации. Байты двух
timezone outputs должны совпасть.

Exit0 — успешная справка/Capture/Verify; exit1 — неверные входы, отсутствующий/другой reference,
ошибка/timeout subprocess, несовпадение наблюдений или I/O. Capture пишет candidate fixture/provenance
и run artifacts, Verify — только новые run artifacts. Подключение к публичному feed не выполняется.
Для повтора используйте новый каталог, сохраняя неудачные попытки.

Public provenance содержит revision, source/harness/launcher/JDK hashes, actual Java version,
production-class inventory digest и raw fixture digest без локальных путей. TSV хранится без Git text
conversion. Перед Capture отформатируйте owned harness закреплённым Java formatter; после final source
freeze запустите Verify. См. [TradeETH](../../../spec/events/TRADE-ETH.ru.md) и
[формат захвата](../../../tests/fixtures/trade-eth/CAPTURES.ru.md).
