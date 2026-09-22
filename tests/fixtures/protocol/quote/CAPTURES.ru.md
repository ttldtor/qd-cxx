# Захваты стандартного Quote

[English version](CAPTURES.md). Английская версия является основной.

[86 ожидаемых артефактов](expected/) скопированы без изменений из чистого захвата A после того,
как чистый захват B воспроизвёл их побайтно. Нормализации и изменения upstream.lock не было.
Исходный production [default-quote.qtp](expected/default-quote.qtp) занимает 326 байт и содержит
полный результат FileWriter для пяти реальных объектов Quote в неизменённой composite-записи.
Остальные 40 QTP-файлов — точечные диагностические входы, прочитанные production FileReader.

- [Захват A](evidence/a/quote-metadata.json), [сборка/окружение upstream A](evidence/a/metadata.json).
- [Захват B](evidence/b/quote-metadata.json), [сборка/окружение upstream B](evidence/b/metadata.json).
- [Точное сравнение захватов](evidence/compare.txt).
- [Отрицательный Verify](evidence/negative/quote-metadata.json), [отклонение](evidence/negative-rejection.txt).
- [Baseline Verify](evidence/verify/quote-metadata.json), [результат](evidence/verify/quote-result.txt).
- [Девять мутаций verifier](evidence/negatives/results.json), [неизменённый baseline](evidence/negatives/baseline.log).

Каждый файл метаданных сохраняет фактическое время, хеши исходника/tools/harness и артефактов.
Связанные baseline-метаданные содержат закреплённый upstream, реальные команды Java/Maven,
содержимое classpath/service observations, хеши dependency JAR и прежнего inspector.
Логи захватов лежат рядом. Raw stdout/stderr содержат пути конкретных запусков и UTC-время;
это provenance, а не нормализованные эталоны. Хеши исходников и прежних доказательств не переписаны.

Первое пробное наблюдение использовало уже собранные закреплённые классы и предшествовало
контракту и реализации C++; затем чистые A/B пересобрали upstream reactor с нуля и воспроизвели
его Quote-артефакты. Финальный verifier дополнительно требует точного равенства всех raw и mapped
строк до/после production-сериализации файла. Финальный Verify сохраняет фактические текущие
хеши tools; предыдущие захваты сохраняют исходные метаданные.

Инвентаризация схемы снята во время выполнения и не является подменной схемой. Quote содержит
8 логических полей в 12 локальных int-слотах; хвосты `VOID` сохранены в описании и никогда не
выдаются как дополнительные логические значения. `inputs.tsv` отдельно фиксирует входные случаи
с потерями и фактический результат Java. Имена, UTF-16 units символов, типы/числовые значения,
IEEE bits и идентификаторы сценариев не переводятся.
См. [покрытие](COVERAGE.ru.md), [проверки](VALIDATION.ru.md) и
[команды воспроизведения](../../../../tools/upstream/quote-oracle/README.ru.md).
