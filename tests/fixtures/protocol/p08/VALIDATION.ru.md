# Валидация Java-доказательств P08

[English version](VALIDATION.md). Английский документ является основным.

Выполнено 2026-09-21 UTC до C++ реализации: два независимых чистых capture и отрицательный/положительный
Verify. [Metadata capture A](captures/capture-a/metadata.json), [metadata capture B](captures/capture-b/metadata.json),
[отрицательный результат](captures/mismatch/result.txt), [baseline](captures/baseline/result.txt),
[доказательство](captures/proof.json), [лог capture](captures/description-capture-a.log),
[лог последовательных проверок](captures/description-evidence.log).

Оба normalized descriptions.tsv совпадают с отдельно перенесённым [эталоном](expected/descriptions.tsv).
Отрицательный запуск использовал только временную копию с изменением типа raw descriptor с 8 на 9;
получено точно `VERIFY FAILED: descriptions.tsv`. Хеши временной копии и baseline не изменились.
Каждый запуск чисто пересобрал закреплённый upstream с JDK 8/Maven, без upstream tests, patches или
замены binary. Нормализация только CRLF в LF. Сборки общего checkout последовательны.
Точные команды, исходники, входы, providers, class outputs и хеши зависимостей сохранены в metadata;
raw stdout/stderr и build logs находятся рядом.

Отдельные driver/launcher добавлены без изменения старых хешированных Java tools.
Исторические хеши покрывают все прежние tracked fixtures и upstream.lock;
финальный аудит проверяет их и provenance текущих probes.
[Документация harness](../../../../tools/upstream/description-oracle/README.md) объясняет диагностические и реальные
parser hooks и ограничения профиля. Правила исходника, инструментированная сырая проекция и реальные
наблюдения установки/повреждения Java не взаимозаменяемы.

Закреплённые Java Format/Check прошли. Последующая C++ [валидация](../../../conformance/protocol/descriptions/VALIDATION.md)
отдельная. Полный upstream suite, повтор local factories, огромные ресурсы, события и transport не заявлены.

После подготовки документации/tools также выполнен сохранённый Test-Verify.ps1; отдельные
[доказательство финального self-test](captures/final-verify/proof.json) и [UTC-лог](captures/final-verify/run.log)
сохраняют новые отрицательную/положительную сборки с полными текущими metadata каталога tools.

Git attributes сохраняют точные захваченные байты probe scripts/documents/inputs и raw artifacts;
Java source имеет канонический LF formatter. Сохранённые хеши исходников не переписывались ради line endings.
