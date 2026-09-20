# Контракт стиля C++

[English](CPP-STYLE.md). Английская версия является основной.

Принят владельцем 2026-09-21 для будущего C++ в qd-cxx. Образец —
[ttldtor/dxfcxx_event_test на d96ef76](https://github.com/ttldtor/dxfcxx_event_test/tree/d96ef76f3c4c6649437ac438bd0fa30f91c12f48),
а не подвижная ветка `default`. [Manifest образца](../tools/format/cpp-style-reference.json)
содержит полный commit, изученные файлы, Git blob IDs, SHA-256 и версии форматтеров.

Контракт принимает предпочтения владельца по именам и наблюдаемые соглашения проекта-образца.
Таблица разделяет свидетельства и правила qd-cxx; она не утверждает единообразие каждого имени
в образце. Файлы форматтеров не проверяют имена, документацию API или качество проектирования.
Google Java Style остаётся отдельным правилом для Java.

## Именование

| Категория | Правило qd-cxx | Свидетельство из образца |
| --- | --- | --- |
| Классы, структуры и типы enum | `UpperCamelCase` (PascalCase), например `RecordCursor`, `FieldKind` | `TaskPattern`, `EventKind`, `SourceTimeMethodComparison` в [core.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/core.hpp). |
| Псевдонимы типов | `UpperCamelCase` | `Clock`, `TimePoint` в [monitoring.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/monitoring.cpp); `Process` в [resources.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/resources.cpp). |
| Методы и свободные функции | `lowerCamelCase`, например `readCompactInt`, `parseRecord`; обычные имена конструкторов/операторов C++ | `eventCount`, `marketSymbolCount`, `parseTask`, `isMarkerSymbol` в core.hpp/core.cpp. |
| Параметры и локальные переменные | `lowerCamelCase`; имя должно объяснять роль | `sortedValues`, `sourceTimeMs`, `observedAtNs` в core.hpp; `encodedSequence` в [client.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/client.cpp). |
| Публичные поля, особенно структур данных | `lowerCamelCase`, без суффикса | `publishPeriod`, `subscribedSymbolCount`, `regionalSourceCount` в core.hpp; `successCriteria` в [runner.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/runner.hpp). |
| Инкапсулированные приватные поля | `lowerCamelCase_`, без префикса `m_` | `lastBySeries_`, `lastGlobal_` в core.hpp; `initialCpu_`, `rssMaximum_` в [resources.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/resources.hpp). Для protected-полей принимается тот же суффикс как правило согласованности qd-cxx; отдельная закономерность protected-полей не установлена. |
| Значения enum | `UPPER_SNAKE_CASE` (BIG_SNAKE_CASE в формулировке владельца), с квалификацией типом enum | `EventKind::QUOTE`, `TRADE_ETH`, `TIME_AND_SALE` в core.hpp; `ClientRole::…` в client.cpp. |
| Именованные константы | `UPPER_SNAKE_CASE`, без префикса `k` | `MARKER_SYMBOL_PREFIX` в [core.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/core.cpp); `MONITORING_STAT_PROPERTY`, `CALLBACK_QUIET_PERIOD` в client.cpp. |
| Пространства имён | Нижний регистр; для многословных компонентов — `lower_snake_case` как правило qd-cxx | В образце — namespace `latency` и безымянные namespace реализации. Закономерность многословных namespace не установлена. |
| Имена файлов | `lower_snake_case.hpp` / `lower_snake_case.cpp` для новых собственных файлов | `core.hpp`, `resources.cpp`, `runner_main.cpp`, `graal_delivery_client.cpp`. Сторонние заголовки не переименовываются. |

Не нужно переводить каждую переменную `const` или `constexpr` в верхний регистр. Неизменное состояние
объекта вроде `expectedPerBatch_` следует именованию полей; обычные константные локальные переменные
`largest`, `interval`, `numericNames` сохраняют lowerCamelCase в образце. Верхний регистр предназначен
для значений в роли именованных констант. Предпочитайте `enum class` и квалифицированные значения.
Аббревиатуры по возможности рассматриваются как слова (`sourceTimeMs`, `initialCpu_`, `rssMaximum_`);
написание внешних типов/API сохраняется. Имена точек настройки стандартной библиотеки, обязательных
override, C ABI и внешних макросов остаются предписанными. Wire-идентификаторы и JSON-ключи нельзя
переименовывать ради стиля C++.

## Оформление и полный процесс форматирования

Корневые [.clang-format](../.clang-format) и [.uncrustify.cfg](../.uncrustify.cfg) точно скопированы
из закреплённых blobs образца с сохранением copyright/SPDX. Лицензия скопированных настроек —
[BSL-1.0](../tools/format/licenses/dxfcxx_event_test-BSL-1.0.txt); это не выбор лицензии для самого
qd-cxx. [.editorconfig](../.editorconfig) задаёт настройки редактора для C++ отдельно от Java.

- Основа LLVM, отступы блоков и продолжений по 4 пробела, без табуляции, целевой предел 120 колонок.
- Открывающая скобка остаётся на строке объявления/управляющей конструкции; тела имеют явные скобки.
  Непустые функции, лямбды, управляющие тела и циклы не сжимаются в одну строку.
- Определения и логические группы разделяются пустыми строками, не более одной подряд.
  Нет пустых строк в начале/конце файла или начале блока; файл заканчивается переводом строки.
- Полные блоки `if`, `for`, `while`, `switch`, `do` отделяются от соседних операторов.
  `return` отделяется от предыдущих операторов того же блока; перед первым оператором return
  искусственная пустая строка не нужна. Эти правила операторов обеспечивает Uncrustify.
- Сохраняется размещение меток доступа образца: `IndentAccessModifiers: false` и
  `AccessModifierOffset: 0`. В изученных классах `public:` выровнен по отступу членов (четыре пробела
  внутри класса), а не по строке объявления класса. Автоматической пустой строки после метки нет;
  перед ней используется разделение логических блоков.
- Сохраняется размещение указателей/ссылок (`const T &value`, `T *value`) и обычное присоединённое
  `else`. Переносы строк и сортировку include определяет закреплённый форматтер.

[Драйвер форматирования образца](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/tools/format.cmake)
требует **Uncrustify 0.83.0 → clang-format 20.1.8**, именно в таком порядке. Uncrustify дополняет
правила, clang-format выполняет последний проход оформления. Только clang-format, обратный порядок
или предположение о defaults LLVM другой версии не эквивалентны этому процессу.

Когда утверждённая задача введёт C++-исходники, нужно добавить и проверить собственный wrapper
Format/Check с этими точными версиями. Check должен прогонять временные копии через оба инструмента,
сравнивать байты и завершаться ошибкой при отличиях без перезаписи входов. Проверить идемпотентность
составного процесса и намеренное нарушение оформления. Нельзя молча игнорировать отсутствие
инструментов или неизвестные ключи конфигурации. Здесь фиксируется будущий процесс: C++-форматтеры
не запускались и не устанавливались, CMake target не добавлялся.

## Организация исходников и документация

Для собственных заголовков используется `#pragma once`. Реализация сначала включает соответствующий
заголовок проекта, затем явно разделённые группы зависимостей/стандартной библиотеки. Заголовки сами
включают нужные зависимости. Вспомогательные детали реализации размещаются в безымянных namespace
файлов `.cpp`; широкие `using namespace` в публичных заголовках не используются. Закрывающий
namespace сопровождается комментарием. Правила следуют изученным заголовкам и реализации, но не
переносят структуру модулей или публичный API образца.

Согласно [CONTRIBUTING.md образца](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/CONTRIBUTING.md),
нужны содержательные Doxygen-комментарии для типов, значений enum, публичных полей, публичных
методов/свободных функций и utility-функций. В образце встречаются и `/** … */`, и `///`.
Объясняйте контракты, единицы измерения, время жизни, условия ошибок и неочевидные ограничения,
а не повторяйте имя. `@param` / `@return` используются там, где проясняют интерфейс.

Образец также использует `std::format`, raw strings, средства стандартного C++20/23 и doctest.
В qd-cxx понятный стандартный C++23 остаётся прежней целью; выбирайте raw strings, когда они яснее,
и `std::format` для составного человекочитаемого текста, когда это уместно. Принятие стиля **не**
выбирает doctest, менеджер зависимостей, систему сборки, транспорт или архитектуру. `[[nodiscard]]`,
`const`, `noexcept` и владение выражают реальные контракты и не добавляются механически ради вида.
Семантика протокола по-прежнему определяется закреплёнными доказательствами Java QD.

## Проверки и ограничения

Изучены все четыре публичных заголовка, представительные реализации (core, client, runner,
monitoring, resources), core tests, руководство участника, обе конфигурации и драйвер форматирования
на зафиксированном чистом checkout. Копии настроек/лицензии побайтово сравнены с
`git show <commit>:<path>`; хеши находятся в manifest. Проверены структура EN/RU, относительные
ссылки и `git diff --check`. Внешний checkout остаётся игнорируемым.

Это анализ исходников/конфигураций и решение о будущем стиле, не результат исполнения C++ formatting,
build, test или compatibility. Production C++, importer, правки Java/oracle, изменения fixtures
и новые build/test dependencies не входят в обновление. Именование сейчас проверяется при review;
отдельно проверенный linter можно добавить при введении C++ tooling — эти два форматтера имена не проверяют.
