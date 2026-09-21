# Штатный oracle для binding

[English version](README.md). Английская версия является основной.

QDCXX-014 добавляет самостоятельно написанный probe над закреплённым BinaryQTPParser QD 3.355.
Исходники upstream не копируются и не исправляются. [Inputs](../../../tests/fixtures/protocol/binding/inputs.tsv)
задают 13 сценариев с новым parser; [покрытие](../../../tests/fixtures/protocol/binding/COVERAGE.md)
различает наблюдаемую доставку, разработанные планы и повторные записи, подтверждённые исходниками.

Invoke-Oracle.ps1 — ограниченная копия description runner; оригинал и его provenance сохранены.
Проверяются origin/commit/tag/version/чистота, native reactor собирается JDK 8 без build cache и с
пропуском upstream tests; сохраняются хеши input/tool/environment/classpath и команды.
Неизменённые record/scheme/observer helpers ProtocolOracle и environment inspector SchemaOracle
компилируются и хешируются; FramingOracle компилируется как существующая зависимость ProtocolOracle.
Диагностического binding walk нет. Java primitive и PentaCodec writers создают descriptions/data;
штатный parser выдаёт delivery/error callbacks. C++ event decoder не добавлен.

Нужны PowerShell 7.4+ и существующая настройка JDK 8/Maven из [protocol-oracle](../protocol-oracle/README.md):

```powershell
pwsh -NoProfile -File tools/upstream/binding-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <locked-checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
# Проверить normalized/binding.tsv до явного переноса в binding/expected.
pwsh -NoProfile -File tools/upstream/binding-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <locked-checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

Сборки общего checkout выполняются последовательно. Capture не обновляет эталоны; Verify сравнивает
точные наборы файлов и хеши, не записывая эталоны. Для negative Verify скопировать binding.tsv во временный
expected directory, заменить `1=101(101)` на `1=999(999)`, передать `-ExpectedDirectory`, потребовать
`VERIFY FAILED: binding.tsv`, затем проверить неизменённый baseline. Два чистых capture и оба результата
Verify сохранены в [валидации](../../../tests/fixtures/protocol/binding/VALIDATION.md).

Optional built-in metadata, external event time, field replacers, XML и optional property overlays
отключены. Искусственные записи R/S/E фиксируют mappings, типы и имена slots; service inventory сохранён
в environment.tsv. Это обычные пробы точного типа 8, не полная provider/metadata/conversion parity.
При нормализации только CRLF превращается в LF; Java diagnostics сохраняются как raw. Время capture — UTC.
