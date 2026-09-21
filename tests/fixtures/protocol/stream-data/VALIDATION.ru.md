# Валидация Java STREAM_DATA

[English version](VALIDATION.md). Английский документ является нормативным.

Авторские проверки выполнены 2026-09-21 UTC, это не независимое ревью. В этих проверках ошибок не
обнаружено. [Покрытие](COVERAGE.md) ограничивает утверждения; upstream tests пропущены.

Две последовательные чистые native reactor сборки с JDK 8, отключённым build cache и закреплённым
QD 3.355 дали одинаковые нормализованные наблюдения: [capture-a metadata](captures/capture-a/metadata.json),
[capture-b metadata](captures/capture-b/metadata.json). Единственная нормализация — CRLF-to-LF.
Raw stdout/stderr, build logs, environment inventory, точные команды, source/input hashes, classpath
content и dependency hashes сохранены рядом с metadata. Первый capture завершился до C++ реализации.
Сборки shared upstream не выполнялись одновременно; upstream source не изменялся.

Временная мутация expected value 22,44,0 → 22,45,0 отклонена точным Verify:
[mismatch result](captures/mismatch/result.txt), [metadata](captures/mismatch/metadata.json).
Затем неизменённый baseline Verify успешен: [результат](captures/baseline/result.txt),
[metadata](captures/baseline/metadata.json), [proof](captures/proof.json), [UTC log](captures/stream-verify.log).
Байты временного и настоящего expected остались неизменными. Исторические SHA-256
содержат 775 прежних fixture/upstream.lock путей; после работы подтверждена неизменность каждого.

Воспроизведение с PowerShell 7.4+, существующими JDK 8, Maven и закреплённым чистым checkout:

```powershell
pwsh -NoProfile -File tools/upstream/stream-data-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
pwsh -NoProfile -File tools/upstream/stream-data-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

Capture не продвигает goldens; перед продвижением нужно проверить normalized/stream-data.tsv. Для negative
Verify передать временный скопированный/изменённый каталог через -ExpectedDirectory, потребовать VERIFY FAILED,
затем повторить baseline. Runner требует новый каталог вывода и проверяет origin/commit/tag/version и
чистоту исходников до/после сборки. Исторические fixtures/lock не перезаписываются.

Диагностический parser не заменяет production delivery. Helpers ProtocolOracle/FramingOracle/SchemaOracle
сохранены и хешированы. Instrumentation состояния parser отсутствует. Проектные C++ prefix retention,
resource/error positions и неподдерживаемые flags отдельно описаны в
[контракте](../../../../spec/protocol/STREAM-DATA.md) и
[conformance results](../../../conformance/protocol/stream-data/VALIDATION.md). Огромные allocations,
полный upstream test suite, transport, полная flag/event/time или универсальная callback parity не заявлены.

Artifact SHA-256 также фиксирует сохранённый environment output,
файлы версий JDK/Maven, raw diagnostics, logs и metadata всех четырёх запусков.
