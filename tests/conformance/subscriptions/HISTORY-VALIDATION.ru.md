# Валидация HISTORY-коллектора

[English version](HISTORY-VALIDATION.md). Английский документ является нормативным.

Здесь зафиксирована квалификация ограниченного HISTORY в объединённой ревизии с принятым чистым компонентом MUX. Она не подтверждает полный клиент подписок или широкую совместимость QD.

## Java reference и границы сравнения

Опубликованный [профиль захватов подписок](../../fixtures/subscriptions/CAPTURES.ru.md) содержит 87 cases и 10 051 строку наблюдений, включая 33 collector cases и 6 983 наблюдения. Пятнадцать timed HISTORY delivery cases сопоставляют фактический post-decoder input, локальные команды, projected outputs/counts и выбранные controls. Фазы aggregate demand `*-add`/`*-remove`, cipher/raw-symbol encoding и дополнительное draining sink через helper в это сравнение не входят. Два wildcard/untimed случая являются отдельными native refusal checks; это не утверждение о parity Java exceptions или API.

Финальное source-bound cross-platform сравнение замороженного набора из четырёх файлов охватывает 2 082 координаты в этих 15 timed cases и 61 output phase; все 165 388 байт native output совпали между Windows и Linux. Это не подтверждает parity для всех 17 HISTORY cases.

Коллектор принимает целые события type 20 только после их допуска соответствующей [LiveSession](../../../spec/subscriptions/LIVE-SESSION.ru.md). Descriptors/control сессии и transport относятся к отдельным protocol-доказательствам и не считаются HISTORY delivery cases. Скомпилированный пример ниже показывает эту границу вызывающей стороны на контролируемом native frame, а не Java-produced HISTORY Quote/Profile stream.

## Нативная квалификация

| Профиль | Результат |
| --- | --- |
| Windows MSVC 19.42 x64 Release, CLIENT ON, runtime/JSON/CONNECT OFF | Полный CTest: 45/45 пройдены за 75,53 с; целевые HISTORY-тесты: 30/30 cases и 27 576/27 576 assertions |
| GCC 13.3 Debug ASan+UBSan, CLIENT ON, runtime/JSON/CONNECT OFF | Полный CTest: 45/45 пройдены за 106,12 с; целевые HISTORY-тесты: 30/30 cases и 27 576/27 576 assertions. `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`; `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; LSan выключен. |
| Отдельный token TSan case | 1 case, 33 assertions; четыре worker-потока каждый копировали и освобождали 10 000 detached token handles при уничтожении owner. Проверялась только жизнь token-marker, а не передача collector/protocol lease или параллельный доступ к owner. |

Production-only пакеты Windows и Linux собраны с выключенными `BUILD_TESTING`, CLIENT, runtime и JSON. Каждый manifest содержит 37 установленных файлов в `lib64` и public includes. Consumer сконфигурирован относительно каждого перемещённого install и завершился с кодом 0; чистый пакет не обнаруживал Threads. Это подтверждает замыкание установленного заголовка/библиотеки `qdcxx_collectors` для этих двух профилей, но не для произвольных toolchain или всех платформ.

Закреплённые Uncrustify 0.83.0 и clang-format 20.1.8 прошли Format и Check; четыре C++-файла достигли побайтно стабильной точки в приватных копиях. Doxygen 1.18.0 завершился без предупреждений, проверены пять сгенерированных API-страниц. Результат Doxygen относится к справке по исходному API, не к публикации сгенерированного Markdown.

## Проверенное поведение и границы доказательств

Нативные случаи проверяют per-view индексированные проекции, явную полноту запрошенного диапазона и фактические допущенные события сессии. Диапазон одного view может быть полным, пока отдельный source snapshot сессии остаётся `RECEIVING` для более низкой границы. Shared admission и отказы локальных views проверяются отдельно: старое и входящее состояние учитываются одновременно до освобождения, поздний shared отказ не публикует ранний отказ кандидатного view, а отказы views применяются атомарно с сохранением изоляции здоровых views. При Work/allocation failure прежние предложения и владение caller input сохраняются. `forgetUnobserved` явно освобождает допустимые пустые метаданные при churn; clock-free native policy не заявляет Java opportunistic expiry через 60 секунд.

Отказ для обычной индексированной точки MAX и виртуальная транзакционная граница MAX REMOVE — native policies, подтверждённые исходником и boundary-тестами, а не исполненная Java MAX-record parity. Обычные untimed Quote/Profile схемы отвергаются для HISTORY. Отдельные caller-indexed native envelopes дополняют полные стандартные схемы временными индексами: проверяются все 8 Quote или 16 Profile materialized fields, включая defaults отсутствующих полей; существующие prepared mappings проходят. Это не заявление о Java HISTORY Quote/Profile producer. Общие native-проверки охватывают null и empty, точные NUL и непарный UTF-16, float и raw scalar values, opaque byte/custom/marshalled payloads после уничтожения source; произвольная business/custom deserialization не входит в профиль.

Эта валидация не подтверждает callbacks, декодирование source/business ID, live transport, сквозную выдачу событий между коллектором и MUX, throughput/RSS/latency или ограниченное время уничтожения. Requested logical bytes не являются RSS процесса. Результаты относятся к указанным объединённым профилям сборки и установленных пакетов; чистое объединение желаемых интересов имеет отдельную [квалификацию MUX](MUX-VALIDATION.ru.md).
