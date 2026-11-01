# Проверка RAW tape oracle

[English version](VALIDATION.md). Английская версия является нормативной.

Начальный HEAD репозитория: `c1654f2dbb5163917b848d1b84b17dfb9d0999fc`. Выполнено 2026-09-22 UTC
с закреплённым QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 и закреплённым путём
сборки upstream. Formatter использовал отдельный OpenJDK 23.0.2 и google-java-format 1.36.1.
В успешных проверках ниже ошибок не обнаружено. Это исходные запуски, не независимая приёмка.

## Выполненные проверки

| Проверка | Фактический результат |
| --- | --- |
| Java Format затем Check | PASS, 15 найденных исходников; финальный inventory source включён |
| Clean capture A (.work/raw-clean-a2) | PASS, Verify старой tape-базы плюс 94 новых случая / 284 структурно проверенных артефакта |
| Clean capture B (.work/raw-clean-b) | PASS, такая же clean-сборка и захват; байты всех 284 артефактов совпали с A |
| Test-Negatives (.work/raw-negatives-final) | Отклонены все десять мутаций byte/value/time/order/arity/missing/extra/completion/noncanonical/full-type; неизменённая база прошла |
| Полный отрицательный Verify (.work/raw-negative-verify) | Ожидаемый exit 1, `artifact mismatch: raw-quote.tsv`, относительно отдельной копии expectations с изменённым timestamp |
| Полный базовый Verify (.work/raw-baseline-verify) | PASS относительно неизменённых repository expectations после негативного запуска |

Фактические аргументы clean-запусков находятся в metadata по ссылкам [CAPTURES](CAPTURES.ru.md).
Команды воспроизведения и явные пути — в [README](../../../../tools/upstream/raw-tape-oracle/README.ru.md).
Для полного отрицательного запуска обычная команда Verify дополнительно использовала
`-ExpectedDirectory .work/raw-negatives-final/time`; остальные настройки не менялись. Ошибка —
ожидаемый отказ verifier, а не падение Java. Финальная база не генерировала и не заменяла
expected files. Исходные expectations скопированы из A и независимо сверены B.

Первая development-попытка launcher столкнулась с уже существующим каталогом логов `raw`
базового launcher и завершилась до нового захвата. Каталог исправлен на `raw-tape`, затем вся
clean-процедура повторена как A2. Неуспешный запуск не выдан за успешный захват.

Производственный путь выполнен для явных пустых и стандартных hs options коннектора, реального
default Quote и целевых синтетических записей. Runtime-строки descriptor/record/heartbeat/error,
исходные байты и типизированная строгая проверка обязательны; одного диагностического сканирования
конвертов недостаточно. Java допускает более широкие options/time/heartbeat, чем ограниченная
политика реализации, и может продолжить после malformed complete messages. Наблюдения сохранены,
они не превращены в неявное заявление о поддержке C++.

Отчёт не заявляет результаты C++ compiler/sanitizer/CLI, независимую приёмку, полноту частного
файла, общую поддержку tape, scheduling, ненулевые flags или новые типы. Это отдельные вопросы
доказательств/оставшихся границ. Семантические выводы закреплены в EN/RU RAW-TAPE и coverage.
