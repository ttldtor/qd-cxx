# Валидация символов Java

[English](VALIDATION.md). Английская версия является нормативной.

Выполнено автором реализации 2026-09-21 UTC; это не отчёт независимого ревью.
QD 3.355 / 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f, native reactor JDK 8, upstream tests пропущены,
Maven build cache отключён, внешний исходник не изменён. Сборки общего checkout шли последовательно.

- [Capture A](captures/capture-a/metadata.json) и [capture B](captures/capture-b/metadata.json) —
  независимые чистые сборки с одинаковым normalized symbols.tsv; A предшествовал реализации C++.
- [Negative Verify](captures/mismatch/result.txt) отверг изменённые decoded units ABC; временные
  эталоны не переписаны. [Baseline Verify](captures/baseline/result.txt) прошёл без изменений.
- [Proof](captures/proof.json) и [UTC run log](captures/evidence.log) сохраняют порядок выполнения.
- Исторический SHA-256 manifest фиксирует байты прежних fixtures/lock,
  сверенные с HEAD через Git clean-filter hashes; изменений нет.

Каждый запуск сохраняет raw/normalized output, версии Java/Maven, build и javac stdout/stderr,
окружение, команды, хеши tools/inputs, скомпилированных harness/classpath и dependency jars.
Бинарные файлы и build directories не копируются в доказательства. Исследовательский capture
.work/symbol-a предшествовал финальной паре и не стал эталоном; добавление README изменило только provenance.
Существующие helper sources и метаданные прежних oracle не изменены.

Команды воспроизведения и нормализация — в [README runner](../../../../tools/upstream/symbol-oracle/README.ru.md).
[Покрытие](COVERAGE.ru.md) разделяет direct API, состояние после ошибок и production callbacks.
Ожидания C++ намеренно применяют terminal errors вместо продолжения Java primitive.
В этих ограниченных проверках ошибок не обнаружено; полная совместимость протокола/upstream tests не заявляется.
