# Финальная валидация символов

[English](VALIDATION.md). Английская версия является нормативной.

## Исходные проверки реализации (исторические)

записанные проверки выполнены 2026-09-21 UTC после доработок integration/tests и читаемости. Это не
отчёт независимого ревью. В этих проверках ошибок не обнаружено; полная совместимость протокола не заявляется.

| Финальный профиль | Результат / UTC evidence |
| --- | --- |
| MSVC 19.42.34444, Debug / Release, x64 | Пять suites проходят: Debug, Release |
| ClangCL 19.1.5, Debug / Release, x64 | Пять suites проходят: Debug, Release |
| WSL GCC 13.3.0, Debug / Release, x86_64 | Пять suites проходят: Debug, Release |
| GCC Debug ASan+UBSan | Пять suites проходят; -fsanitize=address,undefined -fno-omit-frame-pointer: log |
| Production-only GCC, BUILD_TESTING=OFF | Configure/build проходят с FETCHCONTENT_FULLY_DISCONNECTED=ON; каталога _deps нет: build |
| Закреплённое форматирование | Uncrustify 0.83.0 затем clang-format 20.1.8, 20 файлов: Check; google-java-format 1.36.1/JDK21, семь Java файлов: Check |
| Negative/baseline fixture checks | Десять mutations/missing inputs отвергнуты, неизменённый baseline проходит: [proof](negative-proof.json), log |
| Прямой symbol suite | Два теста, 29130 assertions: counts |

Четыре прежних suites compact/UTFString, framing, description и binding проходят вместе с новым
symbol suite. Build logs с соответствующими именами сохранены в logs.
[Build profile manifest](build-profile.json) фиксирует compiler configuration и sanitizer flags;
[хеши исходников](source-sha256.pre-tsv-review.json) определяют финальные code/CMake/test inputs. Все build/test
команды выполняются через tools/run_utc.py. Команды конфигурации — в
[CPP-BUILD](../../../../docs/CPP-BUILD.ru.md); QD_SYMBOL_FIXTURES использовать только для временных
negative tests. Повторяемый [negative checker](../../check_symbol_negative.py) не записывает goldens.

[Java validation](../../../fixtures/encoding/symbols/VALIDATION.ru.md) фиксирует два одинаковых чистых capture,
negative/baseline Verify, полную provenance и 706 неизменённых исторических fixture/lock paths.
Финальный аудит проверяет точные текущие tool/input hashes, равенство captures,
historical hashes, source hashes, относительные ссылки и whitespace diff. Утверждения EN/RU и ссылки
на доказательства синхронизированы. Коммит не создан; реализация передана на независимое ревью.

Первоначально sandbox отказал в доступе к Windows SDK/WSL; разрешённое выполнение с доступом прошло.
Первый локальный Linux runner завершился на пустой CRLF строке после успешных команд; полная матрица
после исправления LF прошла. Ранние проверки после отказа сборки не приняты как финальные доказательства.
Финальная матрица повторена после всех правок кода и очистки CMake; семантика source/header затем не менялась.

Пределы: только codec EMPTY_SET, предварительный API, явный caller reset, атомарные terminal ошибки C++
вместо продолжения Java. Нет инъекции allocation failure, огромных ресурсов, 32-битного target,
полного перебора penta/Unicode/flags, resolver, negotiated/event/time semantics, event decoder,
writer, importer, установки incoming-ID, PART, транспорта, performance или полной совместимости протокола.
См. [покрытие](COVERAGE.ru.md) и [контракт](../../../../spec/encoding/SYMBOLS.ru.md).

## Доработка TSV по ревью

Повторная исходная проверка 2026-09-21 UTC после правки только TSV-тестов; это не независимое ревью.
Все 13 типов строк имеют точную проверку колонок/схемы и явное сравнение либо диагностическую обработку.
Ровно один complete должен завершать файл. См. [схему/покрытие](COVERAGE.ru.md#доработка-tsv-по-ревью).
Production-код, Java tooling, fixtures и upstream.lock не менялись и не перегенерировались.

| Новый негативный случай | Фактический результат |
| --- | --- |
| unknown-row, до complete | Отклонён, exit 1; Unknown Java TSV row type |
| wrong-column-count, конечная пустая колонка profile | Отклонён, exit 1; Wrong TSV column count |
| duplicate-complete | Отклонён, exit 1; observations after complete prohibited |
| row-after-complete, корректный parse.begin | Отклонён, exit 1; observations after complete prohibited |

Checker требует именно эти диагностики, сохраняет все прежние десять негативных случаев и проверяет
неизменность каждого временного входа и baseline. Все четырнадцать негативных случаев отклонены;
неизменённый baseline прошёл. [Proof с диагностикой и SHA-256 baseline](review-tsv-negative-proof.json),
UTC log. Мутации выполняются только на временных копиях.

Финальная матрица после изменений кода: пять suites проходят на MSVC 19.42.34444
Debug / Release,
ClangCL 19.1.5 Debug / Release,
GCC 13.3.0 Debug / Release
и GCC ASan+UBSan. Новый production-only каталог использует
BUILD_TESTING=OFF / FETCHCONTENT_FULLY_DISCONNECTED=ON и не содержит _deps:
configure, build,
[profile manifest](review-tsv-build-profile.json). Symbol suite: два теста / 29245 assertions:
counts. Негативные мутации запускались с MSVC Debug executable.

Закреплённые C++ Format, затем Check и
Java Check проходят. Java sources не правились; повторный запуск
Java oracle не требовался. Evidence audit проходит с обновлёнными хешами
только двух изменённых тестовых исходников в [текущем manifest](source-sha256.json). Предыдущий manifest
сохранён [побайтово](source-sha256.pre-tsv-review.json). Прежние логи, negative proof и build-profile
не затронуты. Отчёт сохранности фиксирует сравнение SHA-256 до/после
для production, tooling, fixtures, lock и прежних evidence. EN/RU coverage/validation и ссылки синхронизированы.
В этих проверках ошибок не обнаружено. Прежние ограничения покрытия protocol/allocator/architecture
сохраняются. Коммит не создан.
