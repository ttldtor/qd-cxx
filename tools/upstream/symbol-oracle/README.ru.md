# Oracle символов

[English](README.md). Английская версия является нормативной.

Независимо написанный SymbolOracle вызывает закреплённые PentaCodec Reader/Writer непосредственно,
с EMPTY_SET и без resolver. Входы ограничены и заданы hex UTF-16 либо wire hex, без платформенного
преобразования Unicode. Байты writer, дополнительные формы reader, строгие префиксы, наблюдаемое
состояние после ошибок, reset и следующий контрольный символ представлены отдельными строками.
Пробы после ошибки используют отдельные ByteArrayInput: это состояние примитива, не восстановление
production parser.

Интеграция использует BinaryQTPComposer и BinaryQTPParser с обычной пустой записью R и нулевыми
флагами. Опции composer — EMPTY_SET; production parser внутри вызывает reset с SUPPORTED_SET
(PentaCodec игнорирует опции reader). Семь доставленных символов в двух сообщениях проверяют повторы
и сброс на границе сообщений. Диагностический декодер не подменяет эти callbacks.

Invoke-Oracle.ps1 — отдельная адаптация binding launcher. Проверяет origin/commit/tag/version
чистого закреплённого upstream, выполняет последовательную чистую сборку native reactor JDK 8/Maven
с отключёнными upstream tests и build cache, хеширует инструменты/входы/классы/зависимости, сохраняет
raw stdout/stderr, окружение и точные команды. Существующие вспомогательные исходники не меняются.

```powershell
pwsh -NoProfile -File tools/upstream/symbol-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
pwsh -NoProfile -File tools/upstream/symbol-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
```

Capture не обновляет эталоны. Перед копированием в
[expected](../../../tests/fixtures/encoding/symbols/expected/symbols.tsv) следует проверить
normalized/symbols.tsv. Verify сравнивает точные наборы файлов и хеши; временно изменённый эталон должен
дать отказ, затем неизменённый baseline должен пройти. См. парные
[покрытие](../../../tests/fixtures/encoding/symbols/COVERAGE.md) и
[валидацию](../../../tests/fixtures/encoding/symbols/VALIDATION.md). Огромные аллокации, кеши resolver,
другие буферы, negotiation, семантика событий, транспорт и запуск upstream test suite исключены.
