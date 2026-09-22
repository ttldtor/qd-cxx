# Валидация переименования executable утилиты

[English version](VALIDATION.md). Английский документ является основным.

Доработка по запросу владельца поверх `435c6d2`, выполнена 2026-09-22 UTC. Единственная пользовательская
CMake-утилита теперь называется `qdcxx-qtp-inspect` (`.exe` в Windows); таргет остаётся `qdcxx_qtp_inspect`.
Help и актуальные EN/RU-команды используют новое имя. Имена внутренних тестовых executable и публичные
include-пути неизменны. Правило будущих имён утилит закреплено в AGENTS.md и [NAMING](../../../docs/NAMING.ru.md).
Идентификаторы формата TSV/human версии 1 остаются `qd-qtp-inspect`; fixtures и wire-поведение неизменны.

Проверки автора, не независимое ревью:

- MSVC 19.42.34444 x64 Release: сборка, 10 suites.
- WSL GCC 13.3.0 x86_64 Release: сборка, 10 suites.
- Uncrustify 0.83.0 → clang-format 20.1.8: Format, Check.
- Новые binary/help, выбор executable в CTest, исторические байты/diff и EN/RU-ссылки:
  аудит, лог, результат/хеши исходников.

В этих проверках ошибок не обнаружено. Неизменные suites включают 72 прежних CLI-случая, 84 human-случая,
сравнения default/explicit TSV, лимиты вывода, ошибки I/O и девять human-мутаций/baseline. Команды
и UTC timestamps сохранены в логах через tools/run_utc.py. Сборки использовали настроенные Release-каталоги
с повторной генерацией CMake; это не новая clean-build или полная compiler/sanitizer матрица. Debug,
ClangCL, sanitizers, production-only профили и Java captures для изменения имени/help не перезапускались.
Прежние отчёты и снимки source hashes остаются историческими.

Alias со старым именем не собирается; в повторно используемом build-каталоге может остаться старый
executable. Обновите команды/ярлыки на новое имя. Wire/TSV/human-грамматика, объём протокола и зависимости
не менялись. Коммит не создан.
