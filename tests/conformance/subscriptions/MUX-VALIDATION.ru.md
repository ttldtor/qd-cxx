# Валидация MUX interests

[English version](MUX-VALIDATION.md). Английский документ является основным. Здесь зафиксирована проверка чистого ограниченного компонента желаемых interests. Она не подтверждает сетевой клиент или сквозную доставку подписок.

## Нативная матрица

Итоговая нативная матрица прошла:

| Профиль | Результат |
| --- | --- |
| Windows MSVC 19.42, x64 Release, `CLIENT=ON`, runtime OFF, JSON loader OFF | 44/44 тестов |
| GCC 13.3 Debug с ASan и UBSan, `CLIENT=ON`, runtime OFF, JSON loader OFF | 44/44 тестов; `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` |
| Целевые MUX cases | 16 cases, 537 assertions на каждом профиле компилятора |

Итоговая fixed-point проверка форматирования использовала Uncrustify 0.83, clang-format 20.1.8 и приватные fixed-point проверки; сборка Doxygen 1.18 завершилась без предупреждений, проверены четыре полученные API-страницы. Эти проверки относятся к исходникам и API-справке, но не к публикации Markdown.

## Production-only пакет и потребитель

Отдельные production-конфигурации Windows и Linux использовали `BUILD_TESTING=OFF`, `CLIENT=OFF`, runtime OFF и JSON loader OFF. Каждый установленный пакет содержал 36 файлов; SHA-256 всех 36 файлов совпали после перемещения установки. Независимый потребитель установленного пакета успешно сконфигурирован, собран и запущен на обеих ОС (код завершения 0); он напечатал `MUX union=1; remaining client selected; revision=5`. В production configure traces не было вызовов поиска пакетов и внешнего fetch; trace потребителя содержит только `find_package(qdcxx REQUIRED CONFIG)`. Поиск и включение optional Threads или networking SDK не выполнялись. Обычные compiler и operating-system SDK остаются требованиями toolchain и не входят в утверждение об optional SDK.

## Проверенное поведение

Целевой набор проверяет точное объединение для клиентов A и B: общий ключ сохраняется после удаления interest одного клиента и исчезает после последнего удаления. Для HISTORY проверяются signed minimum raw bound между клиентами и включительная per-client граница выбора.

Дифференциальные случаи сопоставляют фактическое desired-interest состояние core после add, remove, replace, clear, no-op и противоречивых входов, а также HISTORY-bound upserts. Проверки идентичности и валидации охватывают чужое schema provenance, opaque tokens и повторное использование слотов, move, aliases, пустые symbols и нулевые оси, work/byte limits и allocation failure. При отказе команды сохраняют атомарную публикацию.

Java capture `history-raw-bound-change-aggregate-inclusive` фиксирует последовательность aggregate HISTORY lower bound `4294967298 → 4294967297 → 4294967298 → 4294967299 → -1` и парный aggregate REMOVE с record count, равным нулю ([профиль production captures](../../fixtures/subscriptions/CAPTURES.ru.md#производственные-наблюдения)). Отдельно нативные случаи воспроизводят эти границы и сравнивают desired maps с фактическим `core::SubscriptionState`. Это нативное сравнение не является наблюдением native wire или заявлением о parity Java runtime.

## Границы доказательств

`aggregateEntries()` — полный snapshot желаемого спроса, а не отправленный delta, записанные байты, применение или подтверждение peer. `select` — raw interest predicate; это не маршрутизация HISTORY snapshot-control, фактическая fanout callback или изоляция медленного потребителя. Проверенное состояние может меняться при отключённом источнике. Владение сериализовано; публичное concurrency- или TSan-заявление отсутствует. Эти тесты не подтверждают throughput, RSS, latency или ограниченное время уничтожения.
