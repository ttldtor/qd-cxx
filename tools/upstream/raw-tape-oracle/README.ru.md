# Производственный оракул RAW/MESSAGE

[English version](README.md). Английская версия является нормативной.

Harness QDCXX-023 наблюдает закреплённый неизменённый QD через `FileWriter/FileReader` и
реальный `DXEndpoint → TapeConnector → FileWriterHandler`. Default Quote использует неизменённые
`DXFeedScheme` и `QuoteDelegate`, включая дробные размеры. S/R/E остаётся целевой схемой точного
типа 8. У производственных артефактов не удаляются заголовки и не заменяются ID сообщений.
Отдельные случаи `diagnostic-wire` строят/меняют конверты и читают их производственным FileReader.

Запускать из корня репозитория с PowerShell 7.4+, JDK 8, Maven и существующим QTP inspector.
Каждый запуск требует нового каталога. Базовый tape launcher проверяет чистый закреплённый
checkout, выполняет clean reactor build upstream, сохраняет хеши classpath/providers и сверяет
все старые tape artifacts. Лимит нового harness — 30 секунд; timeout — ошибка, не доказательство EOF.

```powershell
$rawArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -RunDirectory .work/raw-a
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -RunDirectory .work/raw-b
& tools/upstream/raw-tape-oracle/Compare-RawTape.ps1 -Actual .work/raw-a/raw-tape -Expected .work/raw-b/raw-tape
& tools/upstream/raw-tape-oracle/Test-Negatives.ps1 -RunDirectory .work/raw-negatives
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -Mode Verify -RunDirectory .work/raw-verify
```

`Compare-RawTape.ps1` — строгое ядро Verify: точный конечный набор файлов, арности типизированных
строк, канонические signed integers, грамматики UTF-16 и IEEE bits, полные типы и порядок
полей/записей/callback, завершение/счётчики и побайтовое равенство каждого артефакта. Capture
проверяет структуру относительно себя; cross-capture и Verify устанавливают равенство независимо
сохранённым expectations. Негативные проверки меняют байты, значения/типы, время, порядок, арность,
набор файлов и completion; затем проверяется неизменённая база. Verify не переписывает expectations.

`*.qtp` — входные байты, `*.tsv` — реальные наблюдения parser/delegate, `*-frames.tsv` —
дополнительное сканирование конвертов. `settings.tsv` сохраняет действующие заданные настройки,
неизменённую форму схемы и исходный Quote API input; `packing.tsv` — независимые значения
TimeSequenceUtil. Времена/пути metadata/logs остаются специфичными для запуска. Нормализации нет.

Java доставляет RAW_DATA callback 10,15,20; C++ comparator использует только STREAM_DATA (15)
как проекцию одной wire-записи. См. [контракт](../../../spec/protocol/RAW-TAPE.ru.md),
[покрытие](../../../tests/fixtures/protocol/raw-tape/COVERAGE.ru.md),
[захваты](../../../tests/fixtures/protocol/raw-tape/CAPTURES.ru.md) и
[проверки](../../../tests/fixtures/protocol/raw-tape/VALIDATION.ru.md). Разрешительное поведение
Java для дескриптора, options и heartbeat не расширяет ограниченную политику C++.

Отдельный путь `RawTapeInventory.java` / `Check-Inventory.ps1` инвентаризирует непрозрачные data
конверты и структурно допустимые описания; это не производственный acceptance oracle. Хеши его
исходников исключены из manifest оракула и записываются его запусками. Частный вход не копируется
в эти fixtures; generated acceptance не означает совместимость частного файла.
