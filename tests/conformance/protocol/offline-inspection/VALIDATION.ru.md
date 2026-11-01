# Финальная валидация офлайн-инспекции

[English version](VALIDATION.md). Английская версия является нормативной.

исходные проверки выполнены 2026-09-21 UTC; это не независимое ревью. В перечисленных финальных
проверках ошибок не обнаружено. [Покрытие](COVERAGE.ru.md) определяет проверенное и исключения.

| Профиль | Доказательства |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 8 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 8 suites | Debug, Release |
| WSL Ubuntu GCC 13.3.0 x86_64 Debug / Release, 8 suites | Debug, Release |
| GCC Debug ASan+UBSan, 8 suites | Тесты, сборка |
| BUILD_TESTING=OFF, инструмент ON / OFF | ON configure, ON build, OFF configure, OFF build |
| Uncrustify 0.83.0 → clang-format 20.1.8, 27 файлов | Format, Check |
| google-java-format 1.36.1, JDK 21, 9 файлов | Check |
| 17 отклонённых мутаций и неизменённый baseline | Лог, [proof](negative-proof.json) |

Каждый тестовый профиль включает все шесть прежних suites, process-level CLI и инъекции I/O.
Финальные сборки/тесты повторены после последних изменений диагностики неполного конверта,
тестов и C++ комментариев/форматирования.
Все build/test логи используют tools/run_utc.py и UTC; парные build/configuration логи —
в logs. [Профили сборок](build-profile.json) фиксируют compiler identities, flags
и отсутствие `_deps` в обоих production-каталогах. BUILD_TESTING=OFF обходит FetchContent
и поиск Python. Предупреждение о неиспользуемом FETCHCONTENT_FULLY_DISCONNECTED в новых production
сборках отражает этот обход; CMake deprecation warning закреплённого doctest не является ошибкой теста.

Начальные попытки в sandbox не могли найти Windows SDK или запустить выбранный CTest Python:
лог SDK, лог Python.
Повторные запуски успешны; неудачные попытки не считаются прошедшими проверками.

[Java validation](../../../fixtures/protocol/offline-inspection/VALIDATION.ru.md) фиксирует два
независимых clean captures composer/parser и negative/baseline Verify до реализации CLI.
Ни один байт исторических fixtures или upstream.lock не изменён.
[Хеши исходников](source-sha256.json) идентифицируют финальные production/test/build входы;
скрипт аудита, результат и UTC-лог покрывают
хеши исходников/captures, исторические байты, EN/RU ссылки на доказательства и whitespace.
Хеши новых fixtures — в [fixture-sha256.json](fixture-sha256.json).

EN/RU контракты, usage, покрытие, результаты и состояние синхронизированы. API-GATE принят:
processedRecords/wireFields считают списания бюджета, включая ошибочные попытки, с неизменной
wire-семантикой/порядком проверок/позициями. Независимое ревью ожидается; коммит не создавался.

Инструмент поддерживает только сырые офлайн начальные описания и обычные type-8 данные с нулевыми
flags и одним неизменяемым контекстом. Нет инъекции отказа allocator, огромных ресурсов, 32-bit
окружения, importer, transport, dynamic session, timing или заявления о полной совместимости.
Физическая ошибка вывода может оставить частичные байты; маркер complete недостаточен без exit 0.
