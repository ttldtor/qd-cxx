# Покрытие conformance parser descriptions

[English version](COVERAGE.md). Английский документ является основным.

[Контракт](../../../../spec/protocol/DESCRIBE-RECORDS.md), [тесты](../../description_tests.cpp),
[Java-доказательства](../../../fixtures/protocol/p08/expected/descriptions.tsv), [валидация](VALIDATION.md).

Все 81 наблюдение обязательны, исключений нет. Прямое сравнение использует явно диагностическую сырую
проекцию Java: полные ID типов, точные UTF-16 имена/порядок, завершённый префикс, категорию, причину primitive,
consumed, stage/errorOffset. Столбцы constructed-map Java остаются отдельными, проверяются как грамматика
fixture и не принимаются за сырые descriptors. C++ parser не генерирует expectations.
EOF соответствует TRUNCATED_PAYLOAD, UTFDataFormatException — INVALID_ENCODING, IOException выбранного
случая длины — INVALID_LENGTH; ошибки заголовков не имеют primitive cause. Это ограниченная проекция,
а не равенство текста Java exceptions или локальных таблиц.

После каждого созданного Java frame идёт независимый heartbeat sentinel. FrameReader и payload parser
запускаются целиком, на каждом двухчастном разбиении с пустыми концами и побайтно. Тесты сравнивают
результаты до и после последующих feeds/reset, проверяют, что повреждённый payload не поглощает следующий
frame, и явно продолжают без задания session recovery. Эти слои компонуются только в тестах.

Малые тесты покрывают каждый бюджет при нуле, ниже, на и выше успешной границы, точные resource
positions/stages/causes, исчерпание общего бюджета имён в поздних записях, сохранение предыдущих descriptions
при ресурсной ошибке, пустой payload/ноль полей, атомарный учёт пары UTF units, приоритет логического
UTF body overrun, приоритет повреждённого заголовка, sparse sender ID INT_MAX, отклонение count INT_MAX
до allocation и арифметические пределы size_t без огромных буферов. Уничтожение временного входа
проверяет владение результатом. Дополнительный structural property check сохраняет все сырые типы
0..255; это не заявление поддержки serialization.

Три случая doctest выполняют 50 841 assertions. Регрессии compact/UTFString и framing обязательны в CTest.
[Отрицательный runner](../../check_description_negative.py) отклоняет изменённый ожидаемый тип,
неверный hex, неизвестные row/status, пропущенный case, пустые coverage/file и лишнюю строку;
временные и исходные expectations неизменны. Fixture parser требует точного непустого покрытия и известной грамматики.

Нет инъекций allocation failure, огромных allocations, запуска 32-bit/macOS, заявлений concurrency/performance,
привязки к local schema, декодирования событий, transport, frame writing и полной совместимости протокола.
