# Java oracle текстовых событий

[English version](README.md). Английский текст является нормативным.

`Invoke-Oracle.ps1` захватывает или проверяет TextMessage и TextConfiguration через закреплённые production-делегаты Java, BinaryQTP composer/parser и FileWriter. Нужны готовые production classes, JDK 8 и `qdcxx-run-utc`; сеть и сборка SDK не используются.

```powershell
pwsh -NoProfile -File tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -Full
```

Без аргументов или с `-Help` выводится английская справка, с `-HelpRu` — русская; Java и создание файлов не запускаются. По умолчанию используется Verify. Capture требует новый каталог запуска внутри `.work` и создаёт фиксированный новый каталог fixtures; существующие доказательства не заменяются. Каждый процесс ограничен 128 МиБ и 60 секундами, хеши production source/class/resource проверяются до и после; сохраняются полные UTC-логи команд и receipt. Код 0 означает успех или справку, 1 — ошибку входа, несовпадение, timeout или сбой процесса.

Шесть случаев охватывают null, пустой текст и точное содержимое UTF-16, включая NUL и непарные суррогаты, для обоих типов. Сохранённые FileWriter tapes отдельно проверяются нативным typed mapper. Эти случаи не доказывают совместимость с живым Java endpoint или поведение версий TICKER.
