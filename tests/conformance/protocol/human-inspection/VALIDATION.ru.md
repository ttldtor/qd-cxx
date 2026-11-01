# проверка QDCXX-018

[English version](VALIDATION.md). Английский документ является основным.

Выполнено 2026-09-21 UTC (локальная календарная дата 2026-09-22). В финальных проверках ниже ошибок
не обнаружено. Это записанные проверки; независимое ревью QDCXX-018 ожидается. Коммит не создан.
[Покрытие](COVERAGE.ru.md) определяет проверенный профиль и исключения.

| Профиль/проверка | Доказательства UTC |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 10 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 10 suites | Debug, Release |
| WSL Ubuntu GCC 13.3.0 x86_64 Debug / Release, 10 suites | Debug, Release |
| GCC Debug ASan+UBSan, 10 suites | Тест, сборка |
| BUILD_TESTING=OFF, инструмент ON / OFF, без _deps | ON configure, ON build, OFF configure, OFF build |
| Uncrustify 0.83.0 → clang-format 20.1.8, 30 файлов | Format, Check, отклонение без перезаписи/baseline/идемпотентность |
| Прежние 17 отклонённых мутаций и baseline | Лог, [новый proof](legacy-negative-proof.json) |
| 84 точных human-случая, 9 отклонённых мутаций и baseline, байтовые лимиты | Лог |
| Воспроизведение designed артефактов, текущие source/profile данные | Лог, [генератор](design_fixtures.py), [подготовка метаданных](prepare_evidence.py) |
| Исторические байты, EN/RU evidence links, аудит diff | Лог, результат, скрипт аудита |

Каждый тестовый профиль включает восемь неизменных прежних suites и два human suites. Финальные
сборки/тесты выполнены после последних изменений C++-заголовка/форматирования. Соответствующие
configure/build логи — в logs. [Профили сборки](build-profile.json) фиксируют фактические
compiler IDs, флаги, sanitizers и отсутствие зависимостей. Production-only сборки обходят тестовые
FetchContent и поиск Python. Финальные production сборки были инкрементальными после успешных первых
сборок; no-work не выдаётся за чистую пересборку. Предупреждение CMake об устаревших настройках
закреплённого doctest не является ошибкой теста.

Первый запуск Windows в sandbox завершился ошибкой обнаружения SDK (неудачная попытка);
разрешённый повтор прошёл. Неудача не засчитывается как успешная проверка.
Java-исходники не менялись, новые Java captures не запускались. Использованы прежние
[доказательства штатного Java composer/parser](../../../fixtures/protocol/offline-inspection/VALIDATION.ru.md);
исторические исходные отчёты/хеши не изменены. QDCXX-017 закрыта по её коммиту и указанному в QDCXX-018
объёму прежнего ревью: MSVC Release/GCC ASan+UBSan, восемь suites, 17 мутаций/baseline и проверки
хешей/diff. Остальные профили/captures QDCXX-017 остаются результатами исходных запусков.

## Измеренные бюджеты закодированного stdout

Учтены заголовок, кадры, экранированные записи и итог. Точный размер проходит; на байт меньше
даёт ошибку до итога. Дополнительно проверяются точные бюджеты префикса строк и нехватка одного
байта для следующей полной строки: требуется точный предыдущий префикс и resource exit 6 на `write`.
Эти проверки применены ко всем 25 успешным случаям без дополнительных аргументов; полные измерения —
в логе. Выбранные значения:

| Designed-вход | Требуемые байты |
| --- | --- |
| empty (пустой символ, одна запись S) | 308 |
| ascii | 311 |
| cyrillic | 321 |
| supplementary | 313 |
| lone-high / lone-low | 314 / 314 |
| lookalikes | 329 |
| injection | 358 |
| c0 / del-c1 | 489 / 507 |
| line-bidi | 429 |
| legacy-duplicate-target-skip | 512 |

## Воспроизведение и ревью

Из корня репозитория, с настроенным профилем, включающим инструмент и тесты:

```powershell
python tools/run_utc.py --log .work/review-018-build.log -- cmake --build .work/msvc --config Release
python tools/run_utc.py --log .work/review-018-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
python tests/conformance/check_human_inspection.py .work/msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/human-inspection
python tests/conformance/check_offline_negative.py .work/msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/offline-inspection --proof .work/review-018-negative.json
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/protocol/human-inspection/audit.py
```

Первая строка каждого UTC-лога содержит точную выполненную команду и cwd; GCC-команды выполняются
в WSL в `/work/qd-cxx`. [Хеши исходников](source-sha256.json) описывают текущие build/test/tool inputs;
[хеши fixtures](fixture-sha256.json) охватывают все 16 новых designed артефактов. Старые source hashes
остаются историческими снимками, а не заявлениями о текущих исходниках; новый аудит намеренно
не сравнивает текущий форматтер с хешами QDCXX-017. Он проверяет прежние хеши fixtures/captures,
неизменность библиотек/oracle tooling и старых conformance-файлов относительно коммита QDCXX-017.

Для воспроизведения designed fixtures запустите генератор с `--output`, указывающим новый каталог;
существующие назначения отклоняются. Лог подготовки метаданных фиксирует побайтовое воспроизведение
всех 16 артефактов в новом временном каталоге. Верификация читает ожидания и никогда их не генерирует.
Девять отрицательных human-проверок мутируют копии в памяти и используют то же точное сравнение,
что и baseline. Объяснения EN/RU просмотрены на совпадение утверждений; аудит проверяет взаимные/
относительные ссылки и одинаковые ссылки на доказательства вне документов, а не семантику перевода.

Не заявлены инъекции allocation failure, огромные ресурсы/граница RSS, 32-bit запуск или новые Java-измерения.
Физические ошибки write/flush могут оставить частичные байты или итог при ненулевом exit. Это только
ограниченная инспекция raw QTP с начальными описаниями/type-8/zero-flags: нет tape/compression/time-file
reader, protocol descriptors, importer, transport, динамических сессий, расширенных полей или общей поддержки dxFeed.
