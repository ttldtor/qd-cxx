# Финальная валидация STREAM_DATA

[English version](VALIDATION.md). Английский документ является нормативным.

Авторские проверки 2026-09-21 UTC после финальных C++ изменений; это не независимое ревью. В этих
проверках ошибок не обнаружено. Все шесть suites успешны в каждом test profile, включая пять прежних.
Четыре новых cases выполняют 85 549 assertions. [Покрытие](COVERAGE.md) задаёт ограничения.

| Выполненный профиль | Успешные доказательства |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release | Debug, Release |
| WSL GCC 13.3.0 x86_64 Debug / Release | Debug, Release |
| GCC Debug ASan+UBSan | Tests, build |
| BUILD_TESTING=OFF | Configure, build |
| Uncrustify 0.83.0 → clang-format 20.1.8 | Check: 23 files |
| google-java-format 1.36.1 / JDK21 | Check: 8 files |
| Negative/baseline | Log, [proof](negative-proof.json) |

Все build/test команды выполняются через tools/run_utc.py. Соответствующие build logs сохранены в logs.
[Build configuration](build-profile.json) фиксирует компиляторы, sanitizer flags и production no-_deps
check. Production configure явно использует BUILD_TESTING=OFF и FETCHCONTENT_FULLY_DISCONNECTED=ON;
FetchContent не вызывается, dependency не загружается. Начальное sandboxed обнаружение MSVC SDK получило
access-denied; разрешённая сборка успешна. Эта ошибка среды не считается успешной проверкой.

[Java evidence](../../../fixtures/protocol/stream-data/VALIDATION.md) фиксирует два независимых чистых
capture и negative/baseline Verify. Прежние goldens и upstream.lock сохранены. Пятнадцать отрицательных
вариантов fixtures отклонены, неизменный baseline успешен. Закреплённые C++ Format затем Check и Java
Format до captures / финальный Check успешны. [Source hashes](source-sha256.json) идентифицируют финальные
production/test/CMake inputs. Audit проверяет provenance, равенство captures, исторические
байты, финальные source hashes, парные Markdown links и diff whitespace; UTC log.
Технические EN/RU утверждения и ссылки на доказательства синхронизированы. Результат
готов для независимого ревью, сам отчёт не является независимым ревью.

Поддерживаются только обычный точный type 8 и нулевые raw flags. Completed-prefix retention, конечные
ресурсы и error positions — проектная C++ политика, а не Java callback parity. Не выполнялись
allocation-failure injection, огромные ресурсы, 32-bit platform, полная event/time/conversion/session/
transport или performance validation. Публичные API остаются предварительными.
