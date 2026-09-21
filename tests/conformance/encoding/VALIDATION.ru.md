# Валидация кодирования C++

[English version](VALIDATION.md). Английский документ является основным.

## Текущая валидация QDCXX-011 — 2026-09-21 UTC

Проходят все 1146 неизменных Java observations, включая 314 UTFString и 832 compact-регрессии;
четыре doctest cases / 43 375 assertions. Свидетельства QDCXX-010 ниже являются историческими.

| Выполненная проверка | Свидетельства |
| --- | --- |
| MSVC 19.42.34444.0 Debug / Release | [Debug](utf-msvc-Debug.log), [Release](utf-msvc-Release.log) |
| ClangCL 19.1.5 Debug / Release | [Debug](utf-clang-Debug.log), [Release](utf-clang-Release.log) |
| WSL Ubuntu 24.04.4, GCC 13.3.0 Debug / Release | [Debug](utf-linux-Debug.log), [Release](utf-linux-Release.log) |
| GCC Debug AddressSanitizer + UndefinedBehaviorSanitizer | [Фактическое исполнение](utf-asan.log), ошибок не обнаружено |
| UTF byte/value/parser/empty-coverage/CTest negatives, MSVC Release | [Отчёт](utf-negative-proof.json), [журнал](utf-negative.log) |
| Те же UTF negatives, GCC Release | [Отчёт](utf-linux-negative-proof.json), [журнал](utf-linux-negative.log) |
| Compact negatives и регрессии formatter mismatch/idempotence/missing-tool | [Отчёт](utf-compact-negative-proof.json), [журнал](utf-compact-negative.log) |
| Сборка только библиотеки | [GCC build](utf-library.log), BUILD_TESTING=OFF, каталог `_deps` отсутствует |
| Решение о переиспользовании до реализации; последующее точное сравнение | [Первоначальное](utf-reuse-initial.log), [точный probe](utf-reuse.log), [контракт](../../../spec/encoding/UTFSTRING.ru.md) |
| Аудит oracle evidence | [UTC-журнал](utf-audit.log) |
| Итоговые Format → Check, ссылки, diff и неизменность | Отчёт |

Все запрошенные среды компиляторов были доступны. CMake 4.4.2, Windows VS 2022/SDK 10.0.26100.0
и Linux Ninja 1.11.1 соответствуют [инструкциям сборки](../../../docs/CPP-BUILD.ru.md). Первоначально
sandbox блокировал WSL и Windows SDK; разрешённые запуски с повышенным доступом успешны. Один
начальный WSL shell loop потерял аргумент каталога сборки до компиляции; исправленный скрипт
выполнил записанные прогоны. Флаги sanitizer: `-fsanitize=address,undefined -fno-omit-frame-pointer`;
его configure использовал существующий закреплённый исходник doctest через
`FETCHCONTENT_SOURCE_DIR_DOCTEST`, не изменяя его.

Java повторно не запускалась: хэшируемые Java tools и входы неизменны. Каждый исходный fixture-файл
и upstream.lock совпадает с инвентарём SHA-256 до изменений. Ни один эталон не пересоздан. Новые
журналы используют `tools/run_utc.py`; исторические сохраняют исходные даты. Именование, Doxygen,
самодостаточные заголовки, целочисленная арифметика и заимствование/владение проверены отдельно от
форматирования.

Повторить UTF negatives: `python tests/conformance/check_utf_string_negative.py
.work/msvc/Release/compact_tests.exe --report .work/utf-negative-proof.json` (одна команда), либо
путь Linux executable. Существующий compact negative driver повторяется по инструкции ниже.
Оба изменяют только временные копии fixtures, требуют ожидаемые diagnostic/case, проверяют
неизменность копий и повторяют настоящий baseline. Мутация S-ascii writer: `03414243 → 03414244`;
единицы reader меняются `004100420043 → 004100420044`. Release CTest обязан отклонить временное
несовпадение.

Не проверены огромные выделения, инъекция отказов аллокатора, 32-bit target, macOS, другие Linux,
другие BufferedInput, benchmark, полный Unicode/QTP или transport/importer. Production-зависимость
UTF не добавлена. API остаётся предварительным.

Исходная реализация: 2026-09-21 по местной дате, reasoning High. Дополнение владельца: 2026-09-20 UTC
(точные моменты UTC указаны в новых журналах ниже). [Инструкции сборки/API](../../../docs/CPP-BUILD.ru.md)
фиксируют точные инструменты, генератор, SDK и команды configure/build/test. MSVC 19.42.34444.0
и ClangCL 19.1.5 прошли Debug и Release (один CTest на конфигурацию), каждый исполнил
96 writer/736 reader oracle rows. Ubuntu 24.04.4 WSL2 x86_64 с GCC 13.3.0/libstdc++ также
проходит Debug/Release: два случая doctest и 28 358 assertions на запуск. В Linux использованы
CMake 4.4.2 и Ninja 1.11.1. Это проверяет измеренные профили Windows/Linux, а не универсальную
переносимость. Sanitizers, macOS, другие дистрибутивы Linux и другие архитектуры не проверялись. Повторный Java-запуск не нужен: хешируемые Java tools,
oracle inputs и captures неизменны.

## Результаты и сохранённые доказательства

| Проверка | Результат/доказательство |
| --- | --- |
| Сравнение с Java vectors | Все 832 выбранных наблюдения успешны; 101 UTFString row явно исключена. [Точные ID](cases.tsv), [покрытие](COVERAGE.ru.md). |
| MSVC / ClangCL Debug/Release | Новые журналы CTest в UTC: [MSVC Debug](msvc-debug.log), [MSVC Release](msvc-release.log), [ClangCL Debug](clang-debug.log), [ClangCL Release](clang-release.log). |
| Linux GCC Debug/Release | [Linux Debug](linux-debug.log), [Linux Release](linux-release.log); те же неизменные случаи и assertions doctest. |
| Границы writer / sentinel reader | Успешны 1056 ограниченных записей и 144 чтения с добавленным sentinel. |
| Временные изменения ожидаемого байта/значения | Оба обнаружены с ID случая и ожидаемыми/фактическими данными; [negative-proof.json](negative-proof.json). |
| Ошибки parser | Отклонены неверные decimal, hex, status, категория исключения и пустое покрытие. |
| Передача ошибки через CTest | Временная регистрация с изменённым writer expectation завершается кодом 8 и диагностикой ожидаемого/фактического. |
| Форматирование | Закреплённые Format и Check прошли для всех трёх C++-файлов; последующий read-only Check тоже прошёл. Временное расхождение отклонено без записи; Format/Check/Format/Check доказывает идемпотентность на временном образце. Отсутствующий инструмент отклонён. |
| Проверка стиля | Отдельно проверены имена, явные скобки, самодостаточность публичного заголовка, комментарии пространств имён, Doxygen и ограниченное владение. |
| Существующий encoding evidence audit | Успешны все проверки хешей/provenance и прежних Java observations. |
| Неизменность | SHA-256 сравнение 339 исходных fixture-файлов и upstream.lock (340 записей) успешно. Эталоны/конфигурации не редактировались. |
| Документация / пробелы | Проверены утверждения EN/RU, взаимные/относительные ссылки; git diff --check прошёл. |

## Воспроизведение дополнительных проверок

После сборок выше, из корня репозитория:

```powershell
python tools/format/setup_cpp.py
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
python tests/conformance/check_compact_negative.py .work/msvc/Release/compact_tests.exe --report .work/negative-proof.json
python tests/conformance/check_encoding_evidence.py
git diff --check
```

[Драйвер негативных проверок](../check_compact_negative.py) использует временные копии, требует
конкретной семантической диагностики (а не любого сбоя процесса), проверяет неизменность реальных
и изменённых ожиданий и повторяет baseline. Отчёт сохраняет хеши fixtures. Временная writer-мутация —
байт `I-zero` `00 → 01`; reader-мутация — значение `0 → 1`. Временные каталоги удаляются.
[Lock форматтеров](../../../tools/format/cpp-tools.lock.json) закрепляет загружаемые архивы и хеши;
[процесс форматирования](../../../tools/format/CPP.ru.md) описывает область и проверки версий.

## Ограничения и следующая граница

API предварительный и покрывает только ограниченные compact int/long. EOF прямого Java reader
отображается в `INSUFFICIENT_INPUT` с измеренным потреблением; текст Java exception не является
обещанием C++ API. Wire semantics не изменены. Существующие schema/protocol/encoding fixtures
и upstream.lock побайтово неизменны. Не добавлены production runtime-зависимости, широкий CI,
UTF codec, importer, QTP framing, socket layer, packaging и заявления о производительности.
Будущая UTFString-задача требует явного представления с сохранением суррогатов. Для будущей интеграции
по-прежнему нужны проверки поддерживаемых Java tape formats и локальный qds-tools replay либо
Java publisher по необходимости; здесь это не реализовано.

## Доказательства дополнения владельца

- [Windows negative proof](negative-proof.json) создан заново после миграции doctest; [Linux negative
  proof](linux-negative-proof.json) и [Linux negative log](linux-negative.log) подтверждают те же
  отказы для мутаций байта/значения/parser/CTest. Linux явно пропускает проверки Windows-форматтеров.
- Конфигурация/сборка Linux с `BUILD_TESTING=OFF` успешна и не создаёт `_deps/`, подтверждая, что
  загрузка doctest ограничена тестами. Исходник production codec и хеши исходных fixtures неизменны.
- Публичный API и тестовые утилиты теперь содержат явные описания `@param`, `@tparam` и результатов
  не-void через `@return`. Закреплённые Format/Check и неизменный Java evidence audit проходят после правок.
- [UTC runner](../../../tools/run_utc.py) добавляет строкам журналов полное время ISO 8601 UTC и
  задаёт часовой пояс дочернего процесса. Намеренная команда с exit 7 подтверждает передачу ошибки
  и вывод UTC. CTest LastTest.log и новые сохранённые C++-журналы используют UTC. Новые негативные
  отчёты содержат `executedAtUtc`; исторические Java oracle logs сохранены без правки времени.

Обновлённые [инструкции сборки](../../../docs/CPP-BUILD.ru.md) содержат команды Windows/WSL и
журналы UTC. Предупреждение об устаревшей минимальной CMake policy в upstream doctest нефатально
и сохранено в локальных configure logs; исходники зависимости не исправлялись, версия не подменялась.
