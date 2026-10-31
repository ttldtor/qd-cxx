# Валидация параметров endpoint

[English version](OPTIONS-VALIDATION.md). Английская версия нормативная.

## Область и поддерживаемое поведение

Нативный [`EndpointOptions` API](../../../include/qd/client/endpoint_options.hpp)
предоставляет неизменяемые владеющие снимки схемы/настроек отдельных endpoint в
`qdcxx_client`. Он один раз копирует явные входы, сохраняет происхождение схемы
и не разрешает JSON, environment, файлы, Java properties или defaults процесса.
Исполняемое приложение остаётся STREAM по числовому TCP без credentials. Хранение
TLS/authentication, retry и DNS-host handoff-значений не включает эти возможности.
Единицы, момент чтения, владение и ограничения выбранного driver описаны в
[нативном руководстве](../../../docs/SUBSCRIPTIONS.ru.md#endpoint-options) и
[справке по конфигурации](../../../docs/CONFIGURATION.ru.md).

Допустимость общего снимка отделена от достаточных work guards текущего driver.
Driver до собственного I/O отклоняет выбранную запись, если кредиты полей dispatch
меньше удвоенного числа её локальных полей. Только фактический WORK_LIMIT диспетчера
из первого вызова свежего хода до любого успешного шага становится видимым отказом
и abort с теми же кредитами. Успешный шаг с yield на остатке хода, native YIELD/IDLE
и обычное ожидание сохраняют смысл. При stop, drain, failure или cancellation
заданные кредиты не увеличиваются; прежний overload сохраняет поведение. Это
не универсальный минимум `TcpRuntime` или общих partial-progress/handoff settings.

## Выполненные проверки

Дата квалификации: 2026-10-31.

| Профиль | Windows | Linux |
| --- | --- | --- |
| Квалифицированная сборка | MSVC 19.42.34444 x64 Release | GCC 13.3 Debug ASan+UBSan |
| Runtime/client/connect ON, JSON OFF | 52/52 CTest прошли | 52/52 CTest прошли |
| Runtime/client/connect ON, кэшированный JSON ON | 53/53 CTest прошли | 53/53 CTest прошли |
| Focused immutable options | 4 cases / 55 assertions прошли | 4 cases / 55 assertions прошли |
| Сценарии snapshot driver | 6 cases / 176 assertions прошли | 6 cases / 176 assertions прошли |
| Отдельный threaded snapshot test | Настоящий многопоточный случай включён выше | TSan: 1 case / 9 assertions прошёл |

Linux ASan+UBSan использовал `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` и
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer был отключён;
эти результаты не подтверждают его покрытие. TSan-case копировал, читал и освобождал
разные handles снимка в четырёх настоящих потоках.

[Тесты параметров](../endpoint_options_tests.cpp) охватывают независимые endpoint,
происхождение скопированной схемы, lifetime входов/снимка, shared ownership provider
без вызовов credentials callback, статические ошибки без секретов, форму TLS identity
и явные применимые границы, неверные enum/единицы, переполнение арифметики и честный
отказ handoff-профилей. Синтетический PEM-текст допустимой формы не является проверкой криптографического доверия.

[Driver/CLI tests](../connect_tests.cpp) проверяют оба направления минимума и фактические
effective operation caps 1, 2 и 3; продолжение после успешного шага и residual yield;
preflight выбранной записи; pristine WORK_LIMIT целого batch с последующим отказом,
abort и discard; graceful stop/throw из snapshot callback; отмену queued-команды из
16 входов; завершение caller/callback; и настроенную native read inactivity.
Финальные случаи вернулись с local/native terminal evidence и завершением своего peer.
Проверены равенство CLI/native effective values, все пять новых однократных flags,
числовое переполнение, неподдерживаемый TLS и отсутствие synthetic secret в ошибках.
[Help/process checks](../connect_cli.cmake) сохраняют равенство noargs/отдельной справки,
stdout/stderr/exit behavior и отклонение неверных входов; process runner также
проверяет interruption и отказ вывода. Внешний provider endpoint не требуется.

## Production packages и замыкание зависимостей

При `BUILD_TESTING=OFF` каждая платформа прошла configure/build/install,
релокацию и независимый consumer для следующих профилей:

| Профиль пакета | Файлов в установленном/перенесённом пакете | Результат |
| --- | --- | --- |
| CLIENT ON; runtime/connect/JSON OFF | 40 | Relocated options consumer прошёл |
| CLIENT/runtime/connect/tools/JSON OFF | 37 | Relocated pure Quote/Profile consumer прошёл |
| Runtime/client/connect ON; JSON OFF | 45 | Relocated generic/Quote/Profile TCP consumer прошёл |

Наборы файлов и SHA-256 исходного/перенесённого пакетов совпали во всех шести случаях.
Финальные options headers совпали с квалифицированным публичным исходником. Фактические
configure traces и графы успешных сборок показали отсутствие discovery, fetch или
link closure необязательных transport/TLS/JSON SDK в OFF-профилях; оба OFF-профиля
не включали C. Пакеты с runtime не добавили discovery/export OpenSSL или JSON.
[Options consumer](../endpoint_options_consumer/main.cpp) содержит полные заголовки
и `main`; его [CMake project](../endpoint_options_consumer/CMakeLists.txt) связывает
`qdcxx_client qdcxx_quote` из установленного пакета без runtime/JSON. Он не запускал
сеть и напечатал:

```text
Independent endpoint snapshots: 7301, 7302
```

Прежний [subscription consumer](../connect_consumer/CMakeLists.txt) дополнительно
проверил установленные generic и полные Quote/Profile applications с контролируемыми
TCP peers. Это явные package/runtime проверки, без утверждения приёмки публичного peer.

## Стиль, документация и ограничения

Pinned pipeline Uncrustify 0.83.0 → clang-format 20.1.8 в режимах Format/Check и
сравнение fixed point прошли для десяти изменённых C++ файлов, включая полный consumer.
Doxygen 1.18.0 создал новые API-страницы без предупреждений; проверены параметры/результаты,
provider lifetime/thread rules и документация enumerator `CALLER_DRIVEN_TCP`.
Финальное перемещение комментария enumerator сохранило все C++ tokens.

Здесь не квалифицируются TLS handshake/trust/authentication, reconnect или интеграция
DNS-driver. Локальная структурная проверка TLS не подтверждает SAN/chain validity;
получение credentials относится к адаптеру проверенного TLS, а CONFIG никогда его
не вызывает. Произвольное состояние/captures/resources provider и работа callback/
деструктора — исключения, принадлежащие вызывающему коду. Нет утверждений о secure erasure,
process RSS или жёстком времени. Общие properties loading/catalogue/live updates и
приёмка полных подписок остаются отдельной работой.
[Руководство](../../../docs/SUBSCRIPTIONS.ru.md) описывает вывод, exit codes и
сохранённые native close evidence.
