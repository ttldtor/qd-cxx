# Авторская проверка oracle точных типов полей

[English version](VALIDATION.md). Английский текст является основным.

Начальный HEAD: `a9299317c1d4e7f3202ed8136916fd3ee94b9f3f`. Выполнено 2026-09-22 UTC с закреплённым
QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 и закреплённым upstream build path.
Форматирование использовало OpenJDK 23.0.2 и google-java-format 1.36.1. Это авторские проверки,
не независимая приёмка. В перечисленных успешных проверках ошибок не обнаружено.

| Выполненная проверка | Результат |
| --- | --- |
| Java Format затем Check | PASS, 16 обнаруженных исходников |
| Чистый захват A (`.work/field-types-clean-a`) | PASS, чистая сборка upstream, Verify неизменённого прежнего tape baseline, 82 проверенных новых артефакта |
| Чистый захват B (`.work/field-types-clean-b`) | PASS, та же чистая процедура; все 82 артефакта побайтово равны A |
| Test-Negatives (`.work/field-types-negatives-final`) | Все 17 мутаций отклонены, затем неизменённый baseline PASS |
| Полный negative Verify (`.work/field-types-negative-verify`) | Ожидаемый exit 1: `artifact mismatch: mixed-stream.tsv` относительно отдельной копии expectations с изменённой unit |
| Полный baseline Verify (`.work/field-types-baseline-verify`) | PASS относительно неизменённых expectations репозитория после отрицательного запуска |

Фактические команды и происхождение связаны в [CAPTURES](CAPTURES.ru.md). Команды повторения —
в [README](../../../../tools/upstream/field-types-oracle/README.ru.md). Полная отрицательная команда
добавляет `-ExpectedDirectory .work/field-types-negatives-final/unit`; затем идёт обычный неизменённый
baseline, который не пересоздаёт expectations. Отрицательный exit — намеренный отказ comparator,
не падение Java. У Java-процесса heap 128 MiB и timeout 30 секунд.

В раннем рабочем запуске composer не добавлял автоматические descriptions и закономерно получил
unknown-record errors; harness исправлен на production automatic descriptions до всех сохранённых
чистых захватов. Этот неуспешный рабочий запуск не представлен как доказательство приёмки.

Измерены все неизменённые стандартные поля Profile, null/empty/content, полные prefix-пробы
примитивов, production skips/defaults/repeats/mismatches и два существующих режима контейнера.
Наблюдения исходников и прямых примитивов отделены от production file delivery. Поведение
conversion/recovery Java не расширяет exact-match/terminal контракт C++.

Отчёт не заявляет результаты C++ compiler/sanitizer/CLI, независимую приёмку, полноту частного
файла, дополнительные типы, индексированные схемы или отображение бизнес-события Profile.
Устойчивое различие IOUtil/BufferedInputPart закреплено в EN/RU контракте полей и покрытии;
дублирующий ce-compound learning для этого этапа не создан. Staging и коммит не выполнялись.
