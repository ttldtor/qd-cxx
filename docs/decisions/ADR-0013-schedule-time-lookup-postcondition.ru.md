# ADR0013: Проверять containing-day postcondition

[English version](ADR-0013-schedule-time-lookup-postcondition.md) является нормативной.

Статус: принятое решение реализации.

## Основание

Javadoc закреплённого Java `Schedule.getDayByTime` обещает Day, содержащий переданный момент.
`Day.containsTime` проверяет `[start,end)`. Для `(tz=UTC;td=1234567;0=d00000000)` и time0 actual
production Java возвращает пустой Day1 со start=end86400000, не содержащий0. Два отдельных
backward/forward цикла не перепроверяют первое условие после продвижения через пустой Day.
Это подтверждённое нарушение public contract независимо от timezone dataset.
[Actual witnesses и source identity](../../tests/fixtures/schedule-query/containing-day-correction.json)
сохраняют Java output и совпадавший native результат до исправления.

## Решение

Сохранить source lookup и полную грамматику. Перед возвратом выбранного по времени Day проверить
`containsTime(time)`. При false вернуть typed `ScheduleError::NO_SESSION`, как при отсутствии
содержащей сессии. Не добавлять поиск, не отвергать определение parser и не придумывать resource failure.
Прямые id/YMD-фабрики продолжают создавать пустые Day и сохраняют session records.
Schedule session-by-time распространяет типизированный отказ.

## Следствия и проверки

Day time query намеренно отличается от сохранённого Java fixture для этой подтверждённой ошибки;
соответствующий session query уже отвергает момент. Два случая классифицированы явно, отдельно
от36 TZDB2026f data/reference differences. Тесты сохраняют actual Java bytes и проверяют заданное
исправление C++. Обычные positive/boundary запросы сохраняют алгоритм, method-specific ranges и
конечные construction/lifetime contracts. Решение не разрешает автоматически нормализовать или
отвергать другие допустимые source quirks.

## Альтернативы

Возврат не содержащего момент Java Day нарушает документированный контракт. Новый nearest-day поиск добавляет иную семантику и работу. Отказ parser для пустых определений сужает допустимую грамматику. Postcondition gate избегает этих трёх изменений.
