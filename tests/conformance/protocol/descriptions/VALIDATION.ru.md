# Валидация QDCXX-013

[English version](VALIDATION.md). Английский документ является основным.

Выполнено 2026-09-21 UTC. [Java-доказательства](../../../fixtures/protocol/p08/VALIDATION.md) получены до C++.
[Покрытие](COVERAGE.md), [отрицательные проверки](negative-proof.json), аудит, UTC-логи.

MSVC 19.42 и ClangCL 19.1.5 Debug/Release, WSL Ubuntu 24.04 GCC 13.3 Debug/Release и GCC
ASan+UBSan проходят три CTest targets: compact_java_vectors, protocol_java_frames и
protocol_java_descriptions. Новый target: 3 случая / 50 841 assertions / 81 Java-наблюдение.
Регрессии compact/UTFString сохраняют 832+314 наблюдений; framing сохраняет прежнее выбранное покрытие.
Первоначальный sandbox-доступ к Windows SDK и WSL был запрещён; разрешённые elevated-запуски прошли.
В выполненных проверках ошибок компиляции или семантики не обнаружено. BUILD_TESTING=OFF конфигурирует/собирает обе production
libraries в новом WSL-каталоге без _deps; FetchContent не достигается, doctest не приобретается.
Новой production dependency нет.

Закреплённые Uncrustify 0.83.0, затем clang-format 20.1.8 Format/Check и Java google-java-format 1.36.1
Format/Check проходят. Восемь отрицательных fixture checks обнаруживают мутации и неверное/пустое
покрытие, затем неизменный baseline проходит. Source review проверил Doxygen, владение, разделение
диапазонов и операций, арифметику через остатки бюджетов и отсутствие reserve/allocation по counts или sender ID.
Финальный evidence audit проверяет старые хеши fixtures/upstream.lock, равенство независимых captures,
текущие хеши probes/inputs, происхождение исторических payload, все строгие префиксы, EN/RU ссылки
на evidence и относительные Markdown links. git diff --check проходит. Коммит не создавался.

Повторение через существующие [команды сборки](../../../../docs/CPP-BUILD.md), включающие новый target.
QD_DESCRIPTION_FIXTURES используйте лишь для временного отрицательного выбора либо запускайте
`description_tests tests/fixtures/protocol/p08/expected/descriptions.tsv` напрямую. Запустите
`python tests/conformance/check_description_negative.py <executable> --report <new-report>` и
`python tests/conformance/protocol/descriptions/audit.py` для ограниченных аудитов. Команды build/test
логируйте через `python tools/run_utc.py --log <path> -- <command>`.

Выполненные sanitizers — только GCC ASan+UBSan. Windows sanitizer configurations, инъекция allocation
failure, огромные allocations, 32-bit/macOS, другие сочетания Java buffers/profiles, локальные binding,
события и transport не запускались или не реализованы. API предварительный; полная совместимость QTP
не заявляется. Исторические доказательства неизменны, новые результаты добавлены отдельно.

## Последующее ревью, 2026-09-21

Ревью охватило compact, UTFString, framing и сырые descriptions, тесты и Java description harness.
Повторные сборки MSVC Debug/Release и все три CTest targets прошли. Аудиты encoding и P08,
восемь отрицательных description checks и неизменный baseline прошли.
Аудит P08 проверил 534 исторических файла. Запись его отчёта отключена для сохранения прежнего audit.json.
Эти запуски отражены в сессии ревью; они не заменяют сохранённые выше UTC-логи.
ClangCL, GCC/WSL, sanitizers и Java captures при этом ревью не перезапускались. Блокирующих дефектов
не найдено; это не доказательство отсутствия дефектов.

Сырые промежуточные результаты/позиции получены source-checked diagnostic walk через Java primitives,
а не raw callbacks штатного parser. Совпадение только с этим обходом не является независимым
end-to-end доказательством. QDCXX-014 требует доказательств binding
через наблюдаемые результаты штатного parser; diagnostic и internal inspection evidence отмечаются отдельно.
