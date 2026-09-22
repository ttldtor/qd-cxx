# Production oracle стандартного Quote

[English version](README.md). Английская версия является основной.

QDCXX-022 использует неизменённый закреплённый QD 3.355 и полную стандартную `DXFeedScheme`.
Harness отклоняет любые переопределения свойств схемы, сохраняет региональные записи и service
providers, выбирает настоящий `QuoteDelegate` через `MarketFactoryImpl.createStreamOnlyDelegates`
и передаёт реальные объекты `Quote` через `putEvent`, production `FileWriter`, production
`FileReader` и `createEvent`. `EventDelegate.createEvent(cursor)` устанавливает символ и вызывает
`QuoteDelegate.getEvent`. Это прямое наблюдение production-файла; endpoint публикации и connector
здесь не проверяются.

Writer явно настроен на BINARY, compression NONE, time NONE, EMPTY_SET и STREAM_DATA. Полный
исходный `default-quote.qtp`, включая дескриптор, фактическое описание записи и footer, сохранён.
Заголовки не удаляются, схема не переписывается. `inputs.tsv` содержит входные значения API,
`put.tsv` — результат delegate до сериализации, `default-quote.tsv` — независимо снятые raw-поля
и отображённые события после чтения. `schema.tsv` перечисляет все 331 записи и все локальные
слоты, полные идентификаторы типов, локальные/property-имена и восемь логических полей Quote.
`primitives.tsv` содержит точечные проверки полей и utility API. Остальные QTP-файлы — явно
диагностические входы, построенные harness и прочитанные неизменённым production FileReader.

Нужны PowerShell 7.4+, JDK 8, Maven и прежний QTP inspector. Сначала запускается исходный tape
oracle: проверяется lock/чистота upstream, выполняется чистая сборка reactor, сохраняются реальные
classpath/provider/tool hashes, все прежние tape-артефакты сравниваются с неизменёнными эталонами.
Python не добавлен. Каждый каталог запуска должен быть новым; повторяя команды, выбирайте свежие
каталоги.

```powershell
$quoteArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -RunDirectory .work/quote-clean-a
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -RunDirectory .work/quote-clean-b
& tools/upstream/quote-oracle/Compare-Quote.ps1 -Actual .work/quote-clean-a/quote -Expected .work/quote-clean-b/quote
& tools/upstream/quote-oracle/Test-Negatives.ps1 -RunDirectory .work/quote-negatives
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -Mode Verify -RunDirectory .work/quote-verify
```

`Compare-Quote.ps1` требует точный конечный набор артефактов, арность типизированных строк,
канонические целые, грамматику UTF-16/байтов/IEEE bits, полные идентификаторы полей, порядок
записей и полей, форму mapping/метаданных, завершение и счётчики записей; затем сравнивает все
артефакты побайтно. Мутации типа, значения, порядка, идентичности записи, структуры, decimal bits
и бинарных данных должны отклоняться. `Capture` проверяет собственную структуру; только `Verify`
либо сравнение двух захватов устанавливает равенство другому набору. Нормализации наблюдений и
исходных байтов нет. Время и пути в логах/метаданных относятся к конкретному запуску; их равенство
не ожидается.

См. [покрытие](../../../tests/fixtures/protocol/quote/COVERAGE.ru.md),
[захваты](../../../tests/fixtures/protocol/quote/CAPTURES.ru.md) и
[выполненные проверки](../../../tests/fixtures/protocol/quote/VALIDATION.ru.md).
RAW_DATA, время MESSAGE, сжатый tape, общие преобразования типов, mapping региональных событий
и личный tape владельца находятся вне профиля приёмки этого oracle.
