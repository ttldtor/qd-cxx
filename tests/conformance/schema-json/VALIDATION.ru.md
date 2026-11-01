# проверка внешних JSON-схем

[English version](VALIDATION.md). Английская версия является нормативной.

Дата: 2026-09-24 UTC. Область: QDCXX-029 — ограниченная загрузка native JSON схем.
Фактический чистый исходный HEAD: `3bc0e5ee37cf1dd68306d8016a5b65f68c2da2e0`.
Реализация и проверка исходников завершены;
независимое ревью/приёмка открыты.

<a id="binary-json-profile-qualification"></a>

## Квалификация binary JSON profile

Этот отдельный раздел задаёт квалификацию
[binary profile](../../../spec/schema/NATIVE-BINARY-JSON.ru.md). Исторические ordinary-profile
результаты остальных разделов сохраняют дату/область и не доказывают приёмку окончательного binary
source. Следующие проверки binary-добавления выполнены 2026-10-31 UTC. Windows: MSVC 19.42
x64 Release; Linux: GCC 13.3 x86-64 Debug с ASan+UBSan, LSan отключён.

| Выполненная проверка | Windows | Linux |
| --- | --- | --- |
| Полный module suite, JSON ON, CLIENT/RUNTIME OFF | 69/69 прошли | 69/69 прошли |
| Полный module suite, JSON OFF, CLIENT/RUNTIME OFF | 65/65 прошли | 65/65 прошли |
| Исправленный allocation/resource contract | 19 cases, 1221 assertions прошли | 19 cases, 1221 assertions прошли |
| Чистый production JSON ON, CLIENT/RUNTIME OFF; HISTORY consumer из перенесённого prefix | 1/1 прошёл | 1/1 прошёл |
| Чистый production JSON/CLIENT/RUNTIME ON; schema, endpoint и controlled-TCP consumers из перенесённого prefix | Каждый из трёх прошёл 1/1 | Каждый из трёх прошёл 1/1 |

Production-профили использовали BUILD_TESTING=OFF и отключённые CLI-утилиты. Прежние test-enabled
module packages остаются артефактами валидации; их testing extras не входят в заявление о чистом SDK.
Полные suites предшествуют исправлению выравнивания тестового allocator; production-поведение
не менялось, а окончательный allocation/failure/backend contract повторён отдельно на обоих компиляторах.
Наблюдения ordinary requests исключают instrumentation headers и ненаблюдаемые process allocations.

Официальный production Java QD control capture и проверки по обоим native outputs прошли.
Control stream 344 bytes и callback projection 4778 bytes остались идентичными; изменились только
CMake source provenance и его manifest. Это прежнее control-session доказательство, а не квалификация
Java HISTORY producer или полного адаптера стандартного события.

Source tests охватывают девятнадцать full identities, pair/single/ordinary layouts, malformed
members/numbers, независимое provenance, zero/equality/one-short/overflow budgets, tiny input при
огромных максимумах, пересечение defaults, long-prefix name comparisons, field coverage и cleanup.
Квалификация исследует все девять закреплённых backends; source bounds отделены от наблюдений
sizeof/requests включённых backends. Наблюдения capacity MSVC 19.42 x64/STL и GCC 13.3 x64/libstdc++
не являются переносимыми heap limits. Ordinary allocation instrumentation обязана сохранять
default-new alignment; её headers и не перехваченные/aligned/process allocations вне наблюдаемых requests.

Нужны Windows x64 Release и Ubuntu 24.04 GCC 13.3 Debug ASan+UBSan с
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` и
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LSan исключён. JSON ON/OFF module checks используют
CLIENT/RUNTIME OFF и не квалифицируют combined runtime/client profile. Package checks устанавливают,
физически перемещают пакет и заново собирают независимые ON и pure OFF consumers. OFF не должен
получать/импортировать необязательную зависимость или читать её исходники; другой полный SDK hash
должен отклоняться при ON configure.

[Installed consumer](../schema_json_consumer/main.cpp) загружает pair/single JSON, перезаписывает и
уничтожает вход до использования схемы, затем компонует настоящие прежние `LiveSession` и
`HistoryCollector`. Source session живёт во время admission и `stage`; она уничтожается до открытия
view и `front`/чтения/`pop`. Проверяются raw time, NUL/непарные UTF-16 units payload и symbol,
null/empty payloads, opaque bytes, дополнительные поля, source context и отклонение независимой
чужой загрузки. Staging после уничтожения source не проверяется. Native constructed frames не
являются новыми Java HISTORY production captures и не доказывают полную wire equivalence.

CLI gates покрывают парные селекторы, usage до I/O, OFF до missing-file I/O, одинаковую help/no-argument
справку без входов, static rows и failed flush. Прежние ordinary goldens/API/errors и tape modes
остаются regression gates. EN/RU links/examples, Doxygen и whitespace изменённых Markdown проверяются
вместе с окончательными native evidence.

## Реализация и покрытие

- U1: [native-контракт версии 1](../../../spec/schema/NATIVE-JSON.ru.md) и три небольших примера
  определяют разрешённые обычные схемы, явные aliases, точные полные типы, Unicode, бюджеты и ошибки
  отдельно от неизменённого XML-captured профиля. Общий внешний вывод имеет identity
  `external-schema-v1`, версию 6 в обоих режимах tape и обоих форматах.
- U2: необязательный `qdcxx_schema_json` возвращает владеющую `LocalSchema` с `EXTENDED_FIELDS_V1`,
  включая пустые/all-int схемы. simdjson 4.6.4 закреплён SHA-256 архива в CMake; собирается только
  amalgamation, с приватным связыванием и сохранённой MIT-атрибуцией. Публичные заголовки не содержат
  типов парсера. [Тесты загрузчика](../schema_json_tests.cpp) охватывают повторные декодированные ключи
  на каждом уровне объекта, пропущенные/неизвестные члены и неверные виды значений, неверные UTF-8/escapes,
  точные supplementary/NUL имена, числовые токены/диапазоны, неподдерживаемые полные ID/indexed/profile/version,
  aliases/локальные инварианты, время жизни входа/парсера, независимые identities и нулевые/точные/превышенные бюджеты.
- U3: `--schema` и семь конечных параметров `--max-schema-*` проверяют/загружают схему до I/O tape,
  с ограниченным чтением потока, без fallback и с фиксированной диагностикой схемы.
  [Интеграционные тесты](../schema_json_integration_tests.cpp) загружают смешанные custom/переставленные
  Quote/Profile, подготавливают ID 1/2, сохраняют optional/default/extra значения и отвергают подготовку
  из другого контекста. Неизменённые Java-derived STREAM/RAW tapes и oracle event expectations проверяют
  production-поведение wire/events; детерминированные построенные смешанные tapes проверяют разработанную
  локальную схему. Покрыты общий типизированный TSV/human вывод, null/empty, порционное чтение,
  бюджеты вывода/ошибки write/flush и растущий входной поток.
  [CLI-тесты](../schema_json_cli.cmake) охватывают конфликты/повторы аргументов, эквивалентность help,
  нечитаемые/отсутствующие/неверные/слишком большие схемы, приоритет ошибок, отключённый загрузчик,
  обе версии диагностики, оба режима/формата, пути с пробелами, бюджеты вывода и пустые/empty-field/all-int схемы.
- U4: инструкции сборки/API и CLI, индексы схем, индексы документации обновлены
  на EN/RU. Документированная функция подготовки извлечена, скомпилирована и выполнена на смешанном примере.

API core/protocol/events и wire semantics неизменны. DOM проверяет синтаксис; предварительный проход
без выделений памяти, учитывающий строки, ограничивает вложенность всех контейнеров, включая пустые.
Фиксированные массивы членов исключают наборы проверки ключей, размер которых задаёт атакующий вход.
Логические бюджеты проверяются до пропорциональных владеющих выделений; `LocalSchema::create` остаётся
нормативной проверкой. Непредставимые парсером числа дают `MALFORMED_JSON`; неверные представимые
виды/диапазоны — `INVALID_SCHEMA`, оба с exit 5. Различие записано в спецификации.

## Выполненные проверки

Все текущие логи и сгенерированные мутации/сборки остаются игнорируемыми в `.work/schema-json-029/`.
Нативные инструменты разработки собраны через bootstrap на обеих платформах; последующие логи
сборок/тестов использовали `qdcxx-run-utc --log PATH -- EXECUTABLE ARGUMENTS`.
Логи сохраняют точные векторы аргументов и границы времени UTC.

| Профиль/проверка | Результат |
| --- | --- |
| Windows x64, VS 2022 MSVC 19.42.34444, Release, загрузчик ON, оба инспектора ON | Сборка прошла; полный настроенный CTest: 43/43 прошли |
| WSL Ubuntu 24.04, GCC 13.3.0, Ninja Debug, ASan+UBSan, те же опции | Сборка прошла; полный настроенный CTest: 43/43 прошли |
| Production-only Windows Release, загрузчик ON / OFF, оба инспектора ON | Обе конфигурации/сборки и CLI-скрипты прошли |
| Production-only Linux GCC Release, загрузчик ON / OFF, оба инспектора ON | Обе конфигурации/сборки и CLI-скрипты прошли |
| Финальный расширенный `schema_json_cli` после начала полного прогона | Целевой повтор прошёл на Windows и Linux |
| Функция подготовки из документации | Скомпилирована/выполнена на Windows MSVC 19.51.36257 и Linux GCC 13.3.0; оба отображения подготовлены |
| Изменённые C++ файлы | Точные Uncrustify 0.83.0, затем clang-format 20.1.8: Format и финальный Check прошли, 10 файлов |

Команды полных прогонов: `ctest --test-dir .work/schema-json-029/windows -C Release --output-on-failure`
и `ctest --test-dir .work/schema-json-029/linux --output-on-failure`, после
`cmake --build BUILD --config Release --parallel 6` на Windows и `cmake --build BUILD --parallel 6`
на Linux. Linux Debug использовал `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`,
linker `-no-pie`, `ASAN_OPTIONS=detect_leaks=0` и `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Целевые финальные повторы добавляли `-R ^schema_json_cli$`. Production-only конфигурации использовали
`BUILD_TESTING=OFF`, `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` или `OFF`, а также
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON` и `ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON`.
Команда их CLI-скрипта: `cmake -DPROGRAM=EXE -DROOT=ROOT -DENABLED=ON_OR_OFF -DRUN=SCRATCH
-P tests/conformance/schema_json_cli.cmake`.

Production-конфигурация Windows ON скачала закреплённый архив и проверила хеш. Остальные ON-прогоны
использовали его распакованный исходник через `FETCHCONTENT_SOURCE_DIR_SIMDJSON`; тестовые сборки
переиспользовали существующий закреплённый исходник doctest. В обеих чистых OFF-директориях нет `_deps`
и cache entries simdjson/doctest; во всех production-only cache нет записей doctest. OFF CLI-скрипт
проверил встроенные профили, help и `LOADER_DISABLED` до обращения к отсутствующим файлам.
Обычные команды не добавили требований Python/PowerShell; оболочки хоста лишь запускали нативные
процессы CMake/compiler/CTest/logger.

Исходные целевые прогоны выявили пропущенные явные wire aliases в примере и фиксированную минимальную
ёмкость парсера для коротких неверных документов; оба исправления сделаны до полных прогонов.
Новый CLI-тест граничных случаев сначала использовал CMake-переменную вместо строкового литерала;
исправленные повторы прошли. MSVC также потребовал вынести одно raw-string выражение из doctest macro.
Старый CLI прошёл исходную проверку аргументов и не прошёл новое ожидание отсутствующего файла схемы
(usage 2 вместо IO 3), что показало способность теста различать новый путь. Широкая кампания мутаций не выполнялась.

## Ревью, ограничения и передача

В финальных выполненных проверках ошибок не обнаружено. LSan отключён; покрытие утечек не заявляется.
macOS и принудительное исчерпание памяти не проверялись. Linux Release выдал существующее предупреждение
`-Wstringop-overflow` в неизменённом `wide_decimal.cpp`; отсутствие предупреждений не заявляется.
Новых Java captures, доступа к частному tape, XML/property engine, новых типов/преобразований, транспорта
или миграции Python не было. Отклонений от области задачи нет.

проверка исходников охватила все изменённые/новые файлы и целевые проверки повторного использования,
качества и эффективности; она не закрывает независимую приёмку. Проверены EN/RU-утверждения и
относительные ссылки, пробелы в изменённом/неотслеживаемом Markdown, `git diff --check`, пустой staged diff
и полный список файлов. Ограниченные Git-сравнения подтвердили неизменность исторических fixtures,
upstream locks/tooling и старых примеров схем. Новый архив доказательств не создан.
