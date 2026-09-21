# Исполненная валидация P02

[English version](VALIDATION.md). Английская версия является нормативной.

Исходный этап доказательств выполнен 2026-09-21 UTC до зависимой реализации C++.
Финальное обновление provenance исходников выполнено позже с идентичными наблюдениями. Windows 11 amd64, PowerShell 7.6.6,
oracle Zulu 1.8.0_422-b05, Maven 3.9.16; formatter отдельно использует Microsoft JDK 25.0.4.7
и закреплённый google-java-format 1.36.1. Upstream lock не изменён.

## Результаты и provenance

Две независимые последовательные чистые сборки исходников дали одинаковый raw stdout для всех
десяти случаев, включая 17 подслучаев P02 / 117 расписаний. Все 18 исторических файлов ожиданий
совпали до promotion; добавлены только P02.tsv и P02.semantic.tsv. Capture не записывает ожидания.

[Capture A](captures/capture-a/metadata.json) и [capture B](captures/capture-b/metadata.json)
сохраняют реальные команды, идентичность/чистоту upstream, хеши исходников и скомпилированного
harness, хеши содержимого classpath, перечень зависимостей, environment/providers и build logs.
Raw stdout/stderr находится в raw каждого capture. Ошибочные P02 намеренно дают диагностический
stderr с временем; raw stdout детерминирован. Exact-ожидания нормализуются только CRLF-to-LF.
Диагностический stderr не выдаётся за semantic golden.

Существующий [Verify self-test](../../../../tools/upstream/protocol-oracle/Test-Verify.ps1) намеренно
меняет только временное semantic-значение P04-order с 22 на 999. Затронутый полный protocol workflow
отклоняет именно этот файл, затем проходит все 20 файлов / десять случаев настоящего baseline.
[Negative metadata](captures/mismatch/metadata.json), [baseline metadata](captures/baseline/metadata.json)
и [доказательство](captures/verify-proof.json) сохраняют точные результаты и неизменные хеши обоих
наборов. Это также проверяет прежние случаи с изменёнными dispatcher/launcher. Обнаружение подмены
ожидаемого байта P02 дополнительно выполнено [отрицательными C++ проверками](../../../conformance/protocol/negative-proof.json).

Audit сравнивает все четыре прогона с текущими хешами harness/launcher/cases/inspector,
все десять exact-выходов с независимыми captures, проверяет 117 полных расписаний, закрытие
стандартного adapter и 305 исторических хешей, включая upstream.lock,
все прежние expectations/captures schema/protocol/encoding. Результат audit
успешен. Исторические хеши собраны до правок и не переписаны под новые результаты.

Новые UTC command logs: [capture A](captures/p02-final-a.log), [capture B](captures/p02-final-b.log),
[Verify](captures/p02-verify.log). Maven теперь явно получает `-Duser.timezone=UTC`; дочерняя Java
уже использовала UTC. Предварительная неопубликованная проба p02-a выявила неверный размер тестового
стимула; входы лимитов исправлены до обоих финальных captures. Исторические fixtures не исправлялись.

## Канонические концы строк и сохранённый исходный этап

Исходные до реализации [capture A](captures/pre-implementation/capture-a/metadata.json),
[capture B](captures/pre-implementation/capture-b/metadata.json),
[Verify proof](captures/pre-implementation/verify-proof.json) и их raw logs сохранены отдельно.
Финальный аудит выявил возможность нормализации Git Windows-концов строк harness/inputs после
capture. Задача закрепляет канонический LF для protocol harness/cases и побайтовое сохранение новых
raw evidence. Оба captures и negative/baseline Verify повторены после нормализации; текущие metadata
выше содержат реальные обновлённые хеши, исторические хеши не редактировались. Все наблюдения и P02
ожидания идентичны. Обновление меняет байты provenance, а не поведение Java.

## Воспроизведение

Используйте [существующую настройку launcher](../../../../tools/upstream/protocol-oracle/README.md).
Запускайте Capture с новыми RunDirectory p02-lf-a и p02-lf-b последовательно на одном checkout.
Проверяйте новый P02 до promotion; не перезаписывайте исторические ожидания повторными результатами.
Запустите Test-Verify с новым TestDirectory p02-lf-verify. Полные native command arrays находятся в
metadata, команды UTC wrapper — в указанных logs. Аудит сохранённых доказательств:

```powershell
python tools/run_utc.py --log .work/framing-evidence-audit.log -- python tests/fixtures/protocol/p02/audit.py
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-25.0.4.7-hotspot'
```

Format выполнен до обоих финальных captures; Check проходит все четыре Java-файла. Исходники
schema/encoding harness не менялись, поэтому их отдельные build workflows не перезапускались.
Их oracle-байты включены в аудит неизменности, compact/UTFString проверяются C++ регрессиями.

## Интерпретация и ограничения

[Покрытие](COVERAGE.ru.md) различает наблюдательные hooks и настоящие callbacks, объявленные границы
кадров и after-body hooks, восстановление returning consumer и закрытие стандартного adapter,
потребление диагностикой заголовка и выделение кадров. Различие позиций заголовка объяснено через
BinaryQTPParser.appendBytes до реализации политики C++. Противоречащие наблюдения не удалялись
нормализацией. Final EOF и терминальная ошибка C++ — явные lifecycle-политики, а не эквиваленты
методов Java. Полные комбинации consumer flags и исключённое поведение payload/session/transport
остаются неизмеренными. Эти сборки исходников не запускали upstream tests.
