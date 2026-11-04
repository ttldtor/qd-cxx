# Oracle сериализованных вложений

[English](README.md). Нормативна английская версия.

`Invoke-Oracle.ps1` захватывает/проверяет Object-сеттеры Message/Configuration закреплённого QD,
стандартные production-записи и делегаты, callbacks BinaryQTPComposer/BinaryQTPParser и tapes
FileWriter. Нужны готовые production classes точного upstream.lock, Java 8 и `qdcxx-run-utc`.
Нет сети, сборки SDK или получения зависимостей.

```powershell
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/attachment-verify-new
```

Без аргументов/с `-Help` выводится EN; `-HelpRu` выводит RU. Эти режимы и `Get-Help` не требуют
входов, не создают выходных файлов и не запускают Java/QD. Mode по умолчанию Verify. Для работы
нужны все четыре пути. RunDirectory должен быть новым в `.work` текущего checkout, без reparse
ancestors. Прежние доказательства отклоняются. Capture создаёт фиксированный новый каталог
кандидатов `tests/fixtures/attachments/capture-v1` и provenance; существующий capture отклоняется.
Verify читает fixtures без изменения и сравнивает каждый артефакт и provenance.

Launcher отдельно учитывает все classes/resources в 14 явных корнях classpath и проверяет
неизменность production source/class/resource hashes до и после. Каждый подпроцесс ограничен
heap 128 MiB и timeout 60 секунд; завершается только собственное просроченное дерево процессов.
UTF-8 observations используют LF. Артефакты UTC и Pacific/Honolulu обязаны совпадать. Новый run
сохраняет полные timestamped argv/output/exit logs, stdout/stderr logger, inventories и receipt.
Exit 0 — успех/справка; exit 1 — неверный вход/reference, mismatch, сбой или timeout.

14 event cases проверяют null, String, пустой/непустой Java byte[], безвредный Serializable,
engaged-empty raw и повреждённые raw для обоих событий. Две строки подтверждают только поле
Text 10. Полные Text API, TICKER-фильтрация Configuration и нативный реестр кодеков не квалифицируются.
См. [fixtures](../../../tests/fixtures/attachments/README.ru.md) и
[контракт](../../../spec/events/ATTACHMENTS.ru.md).
