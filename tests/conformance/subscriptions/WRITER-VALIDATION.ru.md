# Проверка writer подписок

[English version](WRITER-VALIDATION.md). Английская версия нормативная.

Выполнено 2026-10-31 для [живого composer](../../../spec/subscriptions/WRITER.ru.md).
Windows MSVC 19.42.34444 x64 Release и Ubuntu 24.04 GCC 13.3 Debug ASan+UBSan прошли затронутый
минимальный CTest-профиль по **39/39**. Каждый целевой writer executable прошёл
**15 cases / 4906 assertions**. LeakSanitizer отключён по прежнему ограничению окружения;
результат LSan или ThreadSanitizer не выводится.

Проверены шесть точных message identities, symbol reset/repeats/UTF-16, raw HISTORY bounds,
indexed requirement обеих history форм, sparse identities/layouts, limits/work/no-refund,
allocation/late-work rollback, удержанный полностью consumed output, move/destruction/credits,
каждая позиция хвоста, compact body/symbol-count 63/64 и жизнь исходной схемы.
Огромные representational overflow ветки проверены чтением кода, без фиктивных invalid spans.

Оба компилятора выдали одинаковый 317-байтовый поток. Закреплённый производственный Java 3.355
parser вернул 42 tuples и 7 реальных HISTORY_ADD time reads; оба TSV точно совпали с ожиданиями
нативных входов. Совпали оба пятифайловых дерева (4844 байта), включая provenance.
Java выполнялась на Windows JDK 8 Zulu 1.8.0_422; выполнение Java на Linux не заявляется.
Pin и границы фикстуры — в [реестре захватов](../../fixtures/subscriptions/writer/README.ru.md).
Произвольные преобразования data fields/business этим не квалифицированы.

Изменение ожидаемой raw bound вызвало отказ wrapper на несовпадении настоящего time read.
Итоговый Java helper после повторной компиляции отдельно прошёл 15 CLI/production controls:
ожидаемые результаты 3×0/10×1/2×3. Его Java→Java характеризация использует другую 214-байтовую
фикстуру с 30 tuples/5 time reads и local IDs; она не заменяет нативный sparse remapping.
Noargs/help совпали без QD providers; отказ stdout справки и существующий выход были отвергнуты.

Полный пример C++23 скомпилирован/слинкован/выполнен отдельными Windows/Linux consumers
с BUILD_TESTING/JSON OFF без скачанного dependency tree. C++ source/header прошли
Uncrustify 0.83.0, затем clang-format 20.1.8 Format/Check; Java — закреплённый
google-java-format 1.36.1 Format/Check для 41 sources. Doxygen 1.18.0 создал справку без предупреждений;
просмотрены composer/output/work/limits страницы и текст параметров/результатов.

Полные 39-test профили отключают JSON/replay/inspector/writer tools/compression; в них есть
неизменённая recording tape-writer регрессия. Не заявлены all-enabled acceptance, sockets/TLS/DNS,
live negotiation/epochs/collectors, cross-thread output release, macOS или process RSS cap.
[Инструмент проверки](../../../tools/upstream/subscription-writer-oracle/README.ru.md) описывает
воспроизведение, фиксированный receiver, пределы, правила частичного выхода и точное сравнение.
