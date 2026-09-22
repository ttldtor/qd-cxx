# Production-пробы tape reader

[English version](README.md). Английская версия является основной.

[Контракт tape](../../../spec/protocol/TAPE-READER.ru.md), [покрытие](../../../tests/fixtures/protocol/tape-reader/COVERAGE.ru.md).
Java-harness создаёт ограниченные диагностические входы и передаёт их настоящему неизменённому FileReader:
явный BINARY, сжатие NONE, MAX_SPEED, cycle=false и локальная схема S/R/E. Он не воспроизводит парсер
дескриптора, не наследует production parser, не внедряет привязки, не использует reflection и не заявляет
совместимость CLI/connector. Созданные байты — стимулы, не результаты FileWriter; original.qtp —
неизменённый constrained-артефакт FileWriter из QDCXX-019.

```powershell
$args021 = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/apache-maven/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/tape-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -RunDirectory .work/probes-a
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -RunDirectory .work/probes-b
./tools/upstream/tape-reader-oracle/Compare-Probes.ps1 -Actual .work/probes-a/probes -Expected .work/probes-b/probes
./tools/upstream/tape-reader-oracle/Test-Negatives.ps1 -Expected tests/fixtures/protocol/tape-reader/expected -RunDirectory .work/probes-negative
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -Mode Verify -ExpectedDirectory .work/probes-negative/value -RunDirectory .work/probes-negative-verify
# Предыдущая команда ОБЯЗАНА завершиться ошибкой. Затем проверяется неизменённая база:
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -Mode Verify -RunDirectory .work/probes-verify
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

Все каталоги запуска должны быть новыми. Сначала launcher выполняет неизменённый tape-oracle: чистую сборку
upstream и полный Verify 22 артефактов. Это даёт provenance зафиксированного исходника/чистого дерева/JDK8/
Maven/окружения/classpath/зависимостей. Затем компилируется новый Java-harness, записываются хеши исходника/
launcher/классов, вызов/время, пробы запускаются с лимитом процесса 30 секунд и проверяется точный набор
из 102 файлов. Запуски на одном upstream последовательны. Новых Python и зависимостей нет; upstream-тесты
пропущены. Metadata содержит фактическое UTC-время, нормализации нет. Таймаут — ошибка, не доказательство EOF.

Compare проверяет вид/число ячеек каждой строки, ограниченный размер наблюдений, последовательность/ID/
область значений записей, итоговые счётчики и ровно одно завершение; сравнивает хеши байтов всех артефактов.
Размер исходного файла независимо сверяется со строкой case. Capture/Verify никогда не обновляют expected.
Десять мутаций отдельных копий обязаны отклоняться, включая пустое покрытие и изменения значений/типов/ID/
позиций. Мутация позиции меняет наблюдаемый размер входа, а не выдуманную позицию Java-примитива.
Production callbacks не предоставляют позиции примитивов; C++-проекции позиций проверяются отдельно.

Java может продолжать после ошибок; observer сохраняет фактические доставки/ошибки/исключения и отдельное
завершение эксперимента. Строка `complete` не означает успешного разбора повреждённого входа.
Исходники грамматики: ProtocolDescriptor.parseFrom/parseMessageListFrom, MessageDescriptor.parseFrom,
BufferedInput.readProperties и BinaryQTPParser.parseDescribeProtocol/parseDescribeRecords из upstream.lock.
Переносимость и общая tape за пределами профиля не заявляются. Итоговые хеши и авторские проверки —
в [валидации](../../../tests/fixtures/protocol/tape-reader/VALIDATION.ru.md).
