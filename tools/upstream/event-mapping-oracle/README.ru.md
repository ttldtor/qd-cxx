# Production oracle настраиваемых событий

[English version](README.md). Английская версия нормативна.

Oracle измеряет production delegates Quote/Profile и FileWriter/FileReader закреплённой Java
в 21 сценарии с отдельными JVM. См. [покрытие](../../../tests/fixtures/events/mapping/COVERAGE.ru.md),
[captures](../../../tests/fixtures/events/mapping/CAPTURES.ru.md) и
[проверки](../../../tests/fixtures/events/mapping/VALIDATION.ru.md).
Package harness позволяет наблюдать package-private Flags Profile и packed Sequence Quote;
методы upstream не подменяются. Частный tape не используется.

Каждый запуск требует нового run directory, JDK 8, Maven и существующего исходного инспектора.
Для одного upstream checkout запуски выполняются последовательно: clean build заменяет его classes.
Сначала выполняются неизменённый tape oracle Verify и clean upstream reactor build с сохранением
provenance исходников/classpath/providers/dependencies. Каждый новый сценарий имеет heap 256 MiB,
таймаут процесса30 секунд и лимит 64 callback records. Таймаут — ошибка, а не доказательство EOF.
Java форматируется отдельным закреплённым formatter на JDK 21+.

```powershell
$args025 = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -RunDirectory .work/events-a
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -RunDirectory .work/events-b
& tools/upstream/event-mapping-oracle/Compare-Events.ps1 -Actual .work/events-a/events -Expected .work/events-b/events
& tools/upstream/event-mapping-oracle/Test-Negatives.ps1 -RunDirectory .work/events-negatives
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -Mode Verify -RunDirectory .work/events-verify
```

`Capture` создаёт наблюдения и проверяет форму строк и согласованность событий put/read.
Repository expectations не переписываются. `Verify` дополнительно сравнивает каждый байт артефактов
с явно выбранным expected directory. Comparator проверяет точный состав сценариев/файлов, известную
арность строк, ровно один финальный completion marker и три put/read события для каждого события/режима.
Все mapped properties сравниваются между фактическим delegate output и FileReader output с учётом
отдельного eventTime. Итоговое точное сравнение байтов отвергает неизвестные, отсутствующие,
дублированные и хвостовые данные.

Каждый сценарий содержит observations.tsv и, кроме rejected/xml-unsupported, четыре исходных
production files и два отдельно построенных wire-omission файла. Schema rows сохраняют Java slot,
полный тип, local name и property name (UTF-16 hex). Для reorder/minimal/numeric-int используется
phase `variant`, иначе `initial`; type0 строки — физические tails. Mutation дополнительно записывает
снимки singleton-after/endpoint-after, но его tapes используют initial. Event rows используют binary64
hex и точный UTF-16; null отличается от пустого (`-`). Phase `direct-sequence-boundary` проверяет
только прямое отображение; wire-omitted файлы проходят production parser с отдельно созданными
пустыми descriptions.

Столбцы Quote: row, phase, index, symbol, bidTime, bidExchange, bidPriceBits, bidSizeBits,
askTime, askExchange, askPriceBits, askSizeBits, time, sequence, timeNanoPart, eventTime, packedSequence.
Столбцы Profile: row, phase, index, symbol, betaBits, earningsPerShareBits, dividendFrequencyBits,
exDividendAmountBits, exDividendDayId, high52WeekPriceBits, low52WeekPriceBits, sharesBits,
freeFloatBits, highLimitPriceBits, lowLimitPriceBits, haltStartTime, haltEndTime, rawFlags,
Description, StatusReason, tradingStatusCode, shortSaleRestrictionCode, eventTime.


