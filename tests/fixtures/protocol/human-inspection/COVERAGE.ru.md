# Спроектированные human fixtures

[English version](COVERAGE.md). Английский документ является основным.

[Manifest](designed.json) — спроектированная политика C++-представления, не Java-доказательства human output.
В нём 84 точных случая: 69 ссылок на исторические входы и 15 добавочных Unicode-входов raw QTP.
Прежние Java/TSV файлы используются побайтово неизменными. Имена, метки, пустые значения, небезопасные
escape, offsets кадров и итог сравниваются точно; [полное покрытие](../../../conformance/protocol/human-inspection/COVERAGE.ru.md).
[Генератор](../../../conformance/protocol/human-inspection/design_fixtures.py) пишет только в явно указанный
новый каталог; верификация его не вызывает и не перезаписывает fixtures.
[Валидация](VALIDATION.ru.md) описывает результаты проверок и ограничения.
