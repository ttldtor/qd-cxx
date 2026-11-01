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

<a id="mux-queue-validation"></a>

## Валидация локальных очередей

Дата: 2026-10-31 UTC. Область: `qdcxx_mux_queue` и [контракт очередей](../../../spec/mux/QUEUES.ru.md).
Исторические результаты чистого interest-компонента QDCXX-066 выше сохраняют исходную область
source/profile и не квалифицируют callback queues. Следующие результаты относятся к окончательным
queue implementation и tests. Native execution и package checks относятся к исходным проверкам реализации;
проверка документации охватывает links, точное извлечение source и language/ownership claims.

### Выполненная native matrix

Обе полные suites использовали `CLIENT=ON`, runtime OFF, JSON loader OFF и CONNECT OFF. Они охватывают
полный настроенный module graph, а не combined application profile с включёнными runtime/TLS/JSON.

| Профиль | Полный CTest | Целевая queue suite |
| --- | --- | --- |
| Windows MSVC 19.42, x64 Release | 47/47; exit 0 | 42 cases, 86 529 assertions; exit 0 |
| Ubuntu 24.04 GCC 13.3, x64 Debug ASan+UBSan | 47/47; exit 0 | 42 cases, 88 035 assertions; exit 0 |

ASan использовал `detect_leaks=0:halt_on_error=1`; UBSan — `halt_on_error=1:print_stacktrace=1`.
LSan исключён. Разные assertion totals объясняются платформенной разницей allocation-failure sweeps.
Isolated TSan прошёл 3 cases/2 851 assertions на той же production implementation до добавления
двух последних test-only reserved-control regressions. Его область — producer handles/controls/results,
immutable leases и настоящее освобождение backing, а не конкурентные queue-owner calls и произвольные
application callbacks. Full-suite TSan не заявлен.

Закреплённый Uncrustify 0.83/clang-format 20.1.8 pipeline достиг fixed point и проверил четыре
выбранных C++ файла, включая installed consumer. Doxygen 1.18 сообщил ноль
warnings; шесть настоящих queue API pages проверены на ownership, serialization, limits, reset и
range evidence. Этот Doxygen result не означает отсутствие warnings во всех builds. GCC Release
package builds сохраняют прежний warning `wide_decimal.cpp` вне queue changes.

### Поведение и ресурсные доказательства

[Queue tests](../mux_queue_tests.cpp) различают принятые команды, authoritative `APPLIED`, derived
source synchronization и callback completion. Они проверяют настоящие STREAM/TICKER/HISTORY collectors,
healthy-client atomic routing, slow/full/paused clients, reserved STOP/RESUME/exact reset ACK,
phased reset, HISTORY mapping/epoch/range guards, stale generations, retained results/batches,
last-handle release и callback/retirement reentrancy. Range-query regression проверяет предварительную
оплату queued-symbol matching, exact/one-short budgets и retry без скрытого увеличения credits.

Детерминированный family workload выполнил 300 source frames на family при source-turn record quota 8,
двух healthy clients и 11 churn operations на run. Настоящие pure-source comparison outputs содержат
900 STREAM, 300 TICKER и 301 HISTORY records. Это конечные наблюдаемые workloads, а не доказательства
более крупных предложенных runs или throughput/latency target.

Resource checks включают STL object/control requests, fixed/old/candidate peak backing, nth-allocation
rollback, work axes и deallocation до снятия charge. Allocation-pause probe проверяет сохранение
charge настоящего record/batch/generation backing до release. Exception-transport/runtime infrastructure,
physical allocator headers, произвольные consumer captures и независимо ограниченное source/MUX/core/
session storage сохраняют отдельные области. Наблюдаемые requested bytes не доказывают RSS.

### Installed и relocated consumers

Свежие production-only Windows/Linux CLIENT ON builds использовали runtime/JSON/CONNECT OFF. Каждый
перемещённый пакет содержал 42 файла, проверенных по installation inventory. Независимый consumer
сконфигурирован, собран и завершился с exit 0 на обеих ОС через relocated imported targets.
[Документированный integration fragment](../../../spec/mux/QUEUES.ru.md#скомпилированный-пример-stream-integration)
дословно извлечён из этого скомпилированного consumer. Настоящая готовая source `Sample`/`ALPHA`
имеет три compact-int32 поля полного type 8, pair layout 0/1 и две одинаковые STREAM rows, time 1/value 10.
Он сохраняет immutable callback backing, запрашивает graceful stop, достигает local terminal, затем
читает и освобождает retained batch в другом потоке после уничтожения queue/callback owner.
Standard eight-field Quote schema и Java HISTORY producer не проверяются.

Свежие pure CLIENT OFF/runtime OFF/JSON OFF/CONNECT OFF production packages также перемещены.
Каждый содержит 37 проверенных файлов. Оба installed consumers завершились с exit 0 после настоящих
`MuxState` create/createClient/add/select/clear. Threads discovery отключён; неиспользуемый bogus
C compiler setting допустим. Queue/client targets и headers отсутствуют; runtime, OpenSSL и
networking SDK dependencies не получались. Четыре перемещённых префикса содержат
42+42+37+37 = 158 проверенных файлов; исходные installation copies повторно не считаются.

В выполненных проверках ошибок не обнаружено. Native constructed delivery не является новым Java
queue/HISTORY wire oracle, полным subscription facade, transport-stop protocol и доказательством
throughput, RSS, latency либо ограниченного wall-clock времени destruction. Final integration review,
приёмка владельцем и release отделены от этой ограниченной записи выполненных проверок.
