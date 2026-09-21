# Conformance статического binding

[English version](COVERAGE.md). Английская версия является основной.

[Контракт](../../../../spec/protocol/STATIC-BINDING.md), [тесты](../../binding_tests.cpp),
[Java observations](../../../fixtures/protocol/binding/COVERAGE.md), [валидация](VALIDATION.md).

Три doctest cases выполняют 1357 assertions. Все 12 описанных Java cases / 15 descriptions обязательны;
строго проверяются имена/порядок, payload hex и грамматика планов. Пустое, отсутствующее или повреждённое
покрытие не проходит. Java values независимо подтверждают разработанные ID/order/skip решения;
промежуточные duplicate assignments, подтверждённые только исходниками, явно отмечены в Java coverage.

Тесты политики API покрывают все 256 raw types для известных и неизвестных записей (проходит только 8),
отрицательные/внедиапазонные типы, пустые имена/отрицательные sender ID, пустую схему/записи, sparse INT_MAX ID,
повторные цели, точные ключи без придуманных префиксов, duplicate/ambiguous keys, некорректные ссылки,
непокрытые ключами поля, unsupported local type/time shapes, отклонение некорректного локального UTF-16 и
unknown lookup для raw isolated UTF-16. Каждый schema/planner budget проверен при нуле, ниже, на и выше
малой успешной границы. size_t maximum budgets конечны, огромной allocation нет. Схема сохраняется после
уничтожения входа; план — после уничтожения схемы и описания. Вызовы не сохраняют session state.

[Negative runner](../../check_binding_negative.py) обнаруживает 11 временных повреждений: local target ID,
order, skip, intermediate duplicate target, missing case, empty coverage/file, malformed hex, unknown
case, trailing row и missing file. Baseline и временные файлы неизменны. Существующие compact, UTFString,
framing и descriptions targets выполняются как регрессии. C++ event decoder, production library dependency,
importer и transport не добавлены. Allocation-failure injection, огромные allocations, 32-bit/macOS и
полные schema/protocol profiles не выполнялись.
