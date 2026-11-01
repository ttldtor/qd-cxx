# Валидация переименования

[English version](VALIDATION.md). Английский документ является основным.

Доработка по запросу владельца поверх коммита `e1e19e5`, выполнена 2026-09-22 UTC. Меняются только
C++ namespace и именование CMake. [Актуальные имена и миграция](../../../docs/NAMING.ru.md) фиксируют
постоянные правила. Публичные include-пути `qd/...`, имя `qd-qtp-inspect`, имена CTest-случаев и точные
stdout/stderr неизменны. Коммит не создан. Это записанные проверки, не независимое ревью.

| Проверка | Доказательства |
| --- | --- |
| MSVC 19.42.34444 x64 Debug / Release, 10 suites | Debug, Release |
| ClangCL 19.1.5 x64 Debug / Release, 10 suites | Debug, Release |
| WSL GCC 13.3.0 x86_64 Debug / Release, 10 suites | Debug, Release |
| GCC Debug ASan+UBSan, 10 suites | Тест, сборка |
| BUILD_TESTING=OFF, инструмент ON / OFF, без _deps | ON configure, ON build, OFF configure, OFF build |
| Закреплённые Uncrustify 0.83.0 → clang-format 20.1.8, 30 файлов | Format, Check |
| 17 прежних мутаций и baseline | Лог, [proof](legacy-negative-proof.json) |
| 84 точных human-случая, 72 default/explicit TSV случая, 9 human-мутаций и baseline | Лог |
| Исторические хеши, C++ diff только namespace, имена, EN/RU-ссылки и whitespace | Аудит, результат, лог |

В этих финальных проверках ошибок не обнаружено. Новые каталоги сборки изолируют миграцию от
старых QD_* caches. [Профили](build-profile.json) сохраняют фактические compiler IDs, флаги, имя
проекта, новые option/cache names и отсутствие зависимостей. Соответствующие configure/build-команды
и вывод сохранены в logs; каждый лог начинается точной командой/cwd через tools/run_utc.py.
Использованы кешированные исходники doctest; независимый от production тестовый архив по-прежнему закреплён.

Первый Windows-драйвер неверно разбил аргумент с буквой диска и настроил сам doctest
(попытка, прерванная сборка).
Это не успешная проверка проекта. Драйвер исправлен, использованы отдельные каталоги проекта,
а заголовок кеша восстановлен до точных байтов закреплённого архива: случайная сборка doctest
поменяла только окончания строк (восстановление). Финальная Windows-матрица
выполнена после этого. Обычное предупреждение CMake о deprecated-настройках doctest не является ошибкой теста.

[Хеши исходников](source-sha256.json) идентифицируют 36 текущих build/test/audit inputs.
Исторические хеши сохраняют 1396 файлов до миграции, включая fixtures,
upstream.lock, Java oracle tooling и прежние conformance evidence. Старые отчёты/source hashes
сохраняют старые имена, поскольку описывают прежние ревизии; они не переобозначены как текущие проверки.
Аудит сравнивает все 30 C++-файлов с `e1e19e5` после обратной замены namespace и удаления
форматирующих пробелов. Также проверяет префикс qdcxx_ у всех 13 таргетов проекта, префикс
ORG_TTLDTOR_QDCXX_ у восьми option/cache names и прежний OUTPUT_NAME executable. Проверка исходников
дополняет, а не заменяет десять поведенческих suites. Объяснения EN/RU просмотрены; автоматика
проверяет взаимные/относительные ссылки и одинаковые цели evidence, а не семантику перевода.

Повторный запуск из корня репозитория после настройки сборки с новой опцией:

```powershell
cmake --build .work/naming-final-msvc --config Release
python tools/run_utc.py --log .work/naming-review-test.log -- ctest --test-dir .work/naming-final-msvc -C Release --output-on-failure
python tests/conformance/check_human_inspection.py .work/naming-final-msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/human-inspection
python tests/conformance/check_offline_negative.py .work/naming-final-msvc/Release/qd-qtp-inspect.exe tests/fixtures/protocol/offline-inspection --proof .work/naming-review-negative.json
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/naming/audit.py
```

Переименование namespaces/targets требует обновления потребителей и пересборки; старый ABI/namespace
aliases не обещаются. Пути заголовков и CLI-форматы не требуют миграции. Не заявлены новые Java captures,
32-bit и другие платформы, инъекции allocation failure, timing, transport, поддержка tape или
расширенная wire-совместимость. Ограничения профиля вывода QDCXX-018 сохраняются.
