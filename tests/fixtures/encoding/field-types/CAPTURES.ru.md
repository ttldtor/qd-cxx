# Захваты точных типов полей

[English version](CAPTURES.md). Английский текст является основным.

[82 ожидаемых артефакта](expected/) скопированы без изменений из чистого захвата A и побайтово
воспроизведены чистым захватом B. Оба запуска сначала пересобрали закреплённый upstream reactor
и проверили неизменённый прежний tape baseline. Времена/пути/байты не нормализовались,
upstream.lock не изменялся. Полностью сохранены четыре исходных FileWriter-файла и один
BinaryQTPComposer-файл; 35 отдельно названных диагностических tapes проверяют production-разбор.

- [Metadata захвата A](evidence/a/field-types-metadata.json), [чистая сборка/environment A](evidence/a/metadata.json).
- [Metadata захвата B](evidence/b/field-types-metadata.json), [чистая сборка/environment B](evidence/b/metadata.json).
- [Точное сравнение захватов](evidence/compare.txt).
- [17 мутаций строгого verifier](evidence/negative-results.json), [неизменённый baseline](evidence/negative-baseline.txt).
- [Metadata полного negative Verify](evidence/negative/field-types-metadata.json), [отказ](evidence/negative/field-types-verify.stderr).
- [Metadata полного baseline Verify](evidence/verify/field-types-metadata.json), [результат](evidence/verify/field-types-result.txt).

Соседние logs сохраняют фактические UTC времена, пути запусков, хеши source/tools/harness,
хеши артефактов, команды Java/Maven, содержимое classpath, наблюдения providers, dependencies
и identity прежнего инспектора. Production corruption diagnostics в stderr — намеренные
отрицательные наблюдения. Рабочие захваты использовали уже собранные закреплённые классы,
чтобы установить поведение до контракта полей и C++; они не считаются чистыми повторениями.

Исходный стандартный Profile содержит все 16 логических полей; schema.tsv дополнительно
сохраняет десять физических VOID tails и local/property names. Новая диагностическая запись
явно показывает независимые Java int/object indices. По три одиночные записи занимают три
кадра данных в каждом producer-файле; harness проверяет число кадров. RAW callbacks 10/15/20
сохранены отдельно; STREAM callback задаёт проекцию сравнения одной wire-записи.

Частный tape не читался. Захваты доказывают измеренные ограниченные случаи; полные event adapters,
произвольные схемы и полная совместимость частного файла не заявляются.
