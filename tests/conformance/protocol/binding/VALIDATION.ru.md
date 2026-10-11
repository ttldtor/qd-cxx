# Валидация QDCXX-014

[English version](VALIDATION.md). Английская версия является основной.

Проверки автора выполнены 2026-09-21 UTC. Независимый повтор reviewer не заявляется. Штатные Java
[наблюдения и clean Verify evidence](../../../fixtures/protocol/binding/VALIDATION.md) получены до
реализации. Три новых doctest cases проходят 1357 assertions; четыре CTest targets включают существующие
регрессии compact/UTFString, framing и descriptions. В этих проверках ошибок не обнаружено.

| Профиль | Выполненный результат и evidence |
| --- | --- |
| MSVC 19.42, Debug / Release | Четыре CTest targets проходят: Debug, Release. |
| ClangCL 19.1.5, Debug / Release | Четыре targets проходят: Debug, Release. |
| WSL GCC 13.3.0, Debug / Release | Четыре targets проходят: Debug, Release. |
| GCC ASan+UBSan, Debug | Четыре targets проходят: sanitizers. |
| GCC BUILD_TESTING=OFF | Чистые configure и build; каталог _deps не создан. |
| Negative fixtures | [11 отклонённых мутаций/отсутствующих входов и baseline](negative-proof.json), лог. |
| Закреплённое форматирование | Uncrustify 0.83.0 затем clang-format 20.1.8; google-java-format 1.36.1 на JDK 21. |
| Evidence, ссылки и diff | Аудит, UTC-лог аудита, diff check. |

[Снимок конфигураций](configurations.json) фиксирует фактические cached compiler/generator/flags и
BUILD_TESTING. Build logs находятся рядом с test logs. Существующие build directories использованы
повторно; production-only directory создан заново. Первая sandbox-попытка MSVC не получила доступ к
Windows SDK; повтор с доступом прошёл. Первый GCC run обнаружил сохранённый CR в Windows fixture lines;
reader исправлен, отформатирован, затем полная compiler matrix повторена. Первый сбой GCC сохранён
в логе; отказ SDK записан в переписке задачи,
а успешный повтор MSVC имеет отдельный лог.
Production semantics и expectations не менялись для обхода этих сбоев.

Аудит проверяет хеши всех исторических fixtures и upstream.lock, четыре текущие цепочки Java provenance,
равенство clean captures, 12 проекций описанных сценариев относительно действительно доставленных
значений и покрытие 13 сценариев, включая undescribed error. Проверены относительные Markdown links и
EN/RU evidence references. Source review проверило Doxygen, конечные бюджеты с вычитанием, неизменяемое
локальное владение, независимые упорядоченные планы, отсутствие allocation по sender ID, сохранённых
ссылок на вход и частичного успешного результата. qd_core не зависит от protocol/transport/loading;
doctest используется только в тестах.

Сборки воспроизводятся по [CPP-BUILD](../../../../docs/CPP-BUILD.md), включая все четыре CTest targets.
Запустить `python tests/conformance/check_binding_negative.py <binding_tests> --report <new-report>` и
`python tests/conformance/protocol/binding/audit.py`. Для новых build/test logs использовать tools/run_utc.py.
Fixture override — QD_BINDING_FIXTURES; исторические evidence не изменять.

Не выполнялись/исключены: Windows sanitizers, allocator-failure injection, огромные allocations, 32-bit/macOS,
полный JSON/XML import/validation, conversions, event decoding, built-in metadata/replacers/provider
profiles, dynamic re-description/reset/PART и transport. Промежуточные повторные записи — вывод из исходников;
отсутствующие поля не обещают defaults приложения. API предварительный, performance и полная Java parity
не заявляются. Коммит не создан.

## Дополнение: общий идентификатор типа, 2026-09-21 UTC

Две локальные константы COMPACT_INT=8 заменены задокументированным
[enum SerialFieldType](../../../../include/qd/core/serial_field_type.hpp). Числовые входы и независимые
литеральные oracle expectations не изменены. Это C++ рефакторинг без изменения wire/profile.
Автор повторил все четыре CTest targets на MSVC Debug и
GCC Debug, а также закреплённое форматирование
и evidence/link/diff checks. В этих проверках ошибок не обнаружено. ClangCL, Release, sanitizers,
production-only builds и Java captures для этого дополнения не перезапускались; прежние результаты выше
сохраняют исходный объём. Контракт и результаты EN/RU обновлены.

## Дополнение: общая граница wire type, 2026-09-21 UTC

MAX_SERIAL_TYPE_ID raw parser и MAX_RAW_TYPE planner объединены в задокументированную протокольную
константу MAX_WIRE_SERIAL_TYPE_ID в describe_records.hpp. Структурное принятие и поддержка типов неизменны.
Автор повторил MSVC Debug build,
все четыре CTest targets, закреплённый format check
и evidence/link/diff checks. В этих проверках ошибок не обнаружено. Другие compiler/configuration,
sanitizer и Java checks для этого изменения только константы не повторялись. EN/RU обновлены; коммита нет.
