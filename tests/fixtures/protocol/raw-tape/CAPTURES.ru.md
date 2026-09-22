# Захваты RAW tape

[English version](CAPTURES.md). Английская версия является нормативной.

[284 expected artifacts](expected/) скопированы неизменёнными из захвата A; затем clean-захват B
воспроизвёл их побайтово. Нормализации времени/путей/байтов и изменения upstream.lock не было.
Шесть исходных producer-файлов и 88 diagnostic wire-файлов сохраняют полные заголовки и тела.
Производственные `raw-quote.qtp` и `connector-quote.qtp` содержат по 296 байтов; файл со
стандартными options `connector-default.qtp` — 298 байтов с hs. Действующая схема сохраняет
331 запись; выбранный Quote имеет 12 integer slots, ноль object fields и 8 логических полей типов 56/2/120.

- [Metadata захвата A](evidence/a/raw-metadata.json), [clean build/environment A](evidence/a/metadata.json).
- [Metadata захвата B](evidence/b/raw-metadata.json), [clean build/environment B](evidence/b/metadata.json).
- [Точное сравнение захватов](evidence/compare.txt).
- [Десять мутаций строгого verifier и неизменённая база](evidence/negative-results.tsv).
- [Metadata полного отрицательного Verify](evidence/negative/raw-metadata.json), [отказ](evidence/negative/raw-verify.stderr).
- [Metadata полного базового Verify](evidence/verify/raw-metadata.json), [результат](evidence/verify/raw-result.txt).

Соседние logs и metadata сохраняют реальные UTC-времена, пути запуска, SHA-256 source/tool/harness,
хеши артефактов, команды Java/Maven, содержимое classpath, наблюдения providers, хеши зависимостей
и идентичность старого inspector. Сохранённый stderr намеренно содержит production corruption
diagnostics: это измеренные негативные исходы, а не утверждение о допуске всех входов. Прежние
захваты и хеши остаются историческими, они не выдаются за финальную проверку.

Первый development-запуск использовал уже собранные закреплённые классы и установил поведение
до [контракта](../../../../spec/protocol/RAW-TAPE.ru.md) и реализации C++. Clean-сборки воспроизвели
финальные наблюдения. Независимое ревью C++ проводится отдельно. Частного owner sample в этих
доказательствах нет; его структурная инвентаризация и ограничения описаны отдельно.

