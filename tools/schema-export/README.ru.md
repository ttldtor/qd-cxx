# Офлайн-экспорт Java-схемы

[English version](README.md). Английский документ является нормативным.

Явно запускаемый инструмент экспортирует разрешённую закреплённую Java `DataScheme` в существующий
native JSON версии 1. [Контракт экспорта](../../spec/schema/JAVA-EXPORT.ru.md) определяет точные
правила источников, проекции, бюджетов и публикации. Java 8, Maven и Git нужны разработчику для
подготовки/регенерации; обычные C++ загрузка, сборка и CTest используют сохранённые файлы без Java,
Maven, Python и PowerShell. Самим CMake launcher нужны CMake 3.25+ и нативные Git/JDK.
PowerShell ниже лишь иллюстрирует создание файла аргументов; JSON-массив можно написать любым редактором.

## Подготовка и экспорт

Используйте отдельный чистый checkout с origin, tag и commit из [upstream.lock](../../upstream.lock).
Задавайте абсолютные пути. `RUN` при каждом запуске должен быть новым временным каталогом.
Подготовка собирает закреплённый reactor `qds-file` с явными путями Maven/cache и создаёт квитанцию
classpath; получение зависимостей может требовать сеть. Экспорт повторно проверяет квитанцию и
идентичность чистых исходников, компилирует только добавленные exporter/tests и запускает новую JVM
офлайн. Endpoint никогда не подключается. Расширения classpath не поддерживаются. Непустые
`JAVA_TOOL_OPTIONS`, `_JAVA_OPTIONS` и `JDK_JAVA_OPTIONS` запрещены; подготовка также отклоняет ambient
`MAVEN_OPTS`, `MAVEN_ARGS` и `MAVEN_CONFIG`. Не изменяйте исходники upstream ради подготовки инструмента.

Следующие пути иллюстративны и должны указывать на установленные инструменты и существующий закреплённый checkout:

```powershell
$repo = (Get-Location).Path.Replace('\', '/')
$jdk8 = 'C:/tools/jdk8'
$upstream = 'C:/work/QD-locked'
$maven = 'C:/tools/maven/bin/mvn.cmd'
New-Item -ItemType Directory -Force "$repo/.work/schema-export" | Out-Null
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DMAVEN=$maven" `
  "-DCACHE=$repo/.work/schema-export/maven-cache" "-DRUN=$repo/.work/schema-export/prepare" `
  -P tools/schema-export/Prepare.cmake
$receipt = "$repo/.work/schema-export/prepare/classpath-receipt.json"
$exportArgs = @('--source', 'endpoint', '--output', "$repo/.work/schema-export/events", `
  '--record', 'Quote', '--record', 'Profile', '--set', 'dxscheme.nanoTime=true')
$exportArgs | ConvertTo-Json | Set-Content -Encoding utf8NoBOM "$repo/.work/schema-export/args.json"
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DARGS=$repo/.work/schema-export/args.json" "-DRUN=$repo/.work/schema-export/export-run" `
  -P tools/schema-export/Export.cmake
```

В Linux используйте те же параметры CMake с абсолютными POSIX-путями, JDK 8 и исполняемым файлом Maven.
`ARGS` — UTF-8 JSON-массив строк, не более 65536 байт/256 аргументов. Пути внутри него разрешаются
от `RUN`; используйте абсолютные пути выхода/входов. `--provenance` зарезервирован launcher.
Timeout сборки при подготовке — 600 секунд, компиляции — 60; выполнение команды ограничено
30 секундами и `-Xmx256m`. Это не точная граница resident memory при построении upstream.

Успешный bundle содержит `schema.json` и детерминированный `provenance.json`: относящиеся к делу
настройки источников, hashes входов/exporter/classpath и отображение исходных слотов/экспортированных ID.
Родительский каталог выхода должен существовать, само назначение — отсутствовать, даже как пустой
каталог. Файлы полностью записываются в соседний временный каталог перед публикацией. Отклонённый
вход не публикует bundle; существующее назначение сохраняется. Время запусков остаётся в ignored `.work` logs.

Без аргументов и с отдельным `--help` Java-команда выдаёт одинаковую справку без открытия файлов.
Для этих путей через launcher задайте `[]` или `["--help"]` в `ARGS`; операционный launcher всё равно
проверяет и компилирует. Остальные неверные/повторные аргументы отклоняются. Ошибка Java — одна
ограниченная строка stderr `schema-export-error<TAB>1<TAB>CODE`; коды выхода: 2 usage, 3 I/O/publication,
4 unsupported, 5 invalid input/configuration/schema, 6 resource limits. Ошибки запуска CMake используют его диагностику.

## Источники, настройки и выбор записей

`--source default` использует `DXFeedScheme` с `--startup KEY=VALUE`; properties-файлы и `--set`
запрещены. `--source endpoint` использует FEED endpoint без подключения: startup settings, один явный
`--properties FILE` и `--set KEY=VALUE` сохраняют сведения об источниках. Для измеренного подмножества
builder overrides приоритетнее файла, файл — startup settings. Неявный файл не ищется.
Java properties-файлы используют синтаксис `Properties.load(InputStream)`; повторные декодированные ключи запрещены.

Допустимы только `dxscheme.nanoTime`, `dxscheme.wide`, `dxscheme.enabled.Sequence`,
`dxscheme.enabled.TimeNanoPart`, `dxscheme.price`, `dxscheme.size` и `dxscheme.shares`.
Boolean — строго `true`/`false`; селекторы полей соответствуют `[A-Za-z*][A-Za-z0-9*]*`, не длиннее
128 символов, и только `*` означает wildcard; представления — `int/wide/tiny/decimal` без учёта
регистра. Разрешённые настройки могут дать неподдерживаемые полные типы, отклоняемые при проекции.
Неизвестные ключи, параметры подключений и настройки, не применяемые выбранным источником/options, запрещены.

Для доверенного локального XML задайте упорядоченные корни/overlays; этот запускаемый пример фикстуры
сохраняет пользовательскую запись, aliases и дополнительное поле без ручного редактирования JSON:

```json
["--source","xml","--output","C:/qd-cxx/.work/schema-export/custom",
 "--input-root","C:/qd-cxx/tests/fixtures/schema/export/inputs",
 "--xml","root.xml","--xml","overlay.xml"]
```

Альтернатива — `--source xml --resource dxlink --record Quote --record Profile`.
Закреплённый ресурс и его зависимости предшествуют локальным overlays. XML property options
`--xml-sysprops true|false` и `--xml-dxprops true|false` по умолчанию false. Startup settings требуют
sysprops; явные файл/overrides требуют dxprops. При включении обоих явные XML settings перекрывают
startup. XML отклоняет `dxscheme.shares`, который этот источник не применяет.

Preflight копирует в snapshot только ограниченные локальные imports внутри `--input-root` или
явное замыкание закреплённого ресурса. Пути: 1–240 ASCII-букв/цифр, `_`, `.`, пробел, `/`, `-`;
внутренний `..` разрешён только внутри корня. Удалённые/абсолютные imports, выход за корень, циклы,
DTD/entities, XInclude и `xml:base` отклоняются. Допустимо только точное закреплённое объявление
`dxfeed-schema.xsd`. Preflight с защитными настройками запрещает внешний доступ к DTD/schema;
upstream JAXP допускает только schema `file` для проверенного объявления snapshot и скопированной
закреплённой XSD. Это мост для доверенных локальных входов, а не sandbox враждебного XML.
Стандартные/максимальные бюджеты входов: 64 файла, 1048576 байт и глубина imports 16, включая
properties-файл и закреплённую XSD; переопределения могут их снижать. Бюджеты выхода/количеств/имён даны в контракте.

Без `--record` должны поддерживаться **все** записи. Полные default/dxlink содержат неподдерживаемые
записи; выбор Quote/Profile явно ограничивает область. Неизвестные/повторные имена отклоняются.
Порядок запроса не меняет исходный порядок; экспортированные ID последовательны и записаны в sidecar.
Поддержаны только точный обычный `DefaultRecord`, полные типы 2/8/10/40/56/72/88/120 и проверенные
физические VOID tails. Все реальные lookup keys сохранённых полей остаются; ключи удалённых tails
учитываются отдельно. Indexed schemas, отдельный VOID, full24/full137, произвольные подклассы и
частичное угадывание lookup отклоняются.

## Использование и регенерация

Соберите с `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` и
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON` по [CPP-BUILD](../../docs/CPP-BUILD.ru.md).
Полученный `schema.json` используется напрямую:

```powershell
& .work/schema-json-msvc/Release/qdcxx-tape-inspect.exe --schema .work/schema-export/events/schema.json `
  --input tests/fixtures/schema/export/expected/set-before/Quote-stream.qtp
& .work/schema-json-msvc/Release/qdcxx-tape-inspect.exe --schema .work/schema-export/events/schema.json `
  --tape-mode raw-message-v1 --input tests/fixtures/schema/export/expected/set-before/Quote-raw.qtp
```

Инспектор сохраняет generic output версии 6. Приложения вызывают `loadJsonSchema`, затем
`PreparedQuote::prepare` / `PreparedProfile::prepare` с явными экспортированными ID и тем же
владеющим контекстом схемы; ID не выводятся из storage slots Java. Generic пользовательские и
дополнительные поля остаются доступными. Тесты production captures охватывают оба режима tape и
generic/event values; сама сериализация JSON не служит доказательством совместимости.

Регенерируйте в новые временные каталоги, сравните два свежих запуска, затем сохранённые ожидания.
Эти команды никогда не заменяют ожидания:

```powershell
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/capture-one" -P tools/schema-export/tests/Capture.cmake
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/capture-two" -P tools/schema-export/tests/Capture.cmake
cmake "-DACTUAL=$repo/.work/schema-export/capture-two/expected" `
  "-DEXPECTED=$repo/.work/schema-export/capture-one/expected" -P tools/schema-export/tests/Verify.cmake
cmake "-DACTUAL=$repo/.work/schema-export/capture-one/expected" `
  "-DEXPECTED=$repo/tests/fixtures/schema/export/expected" -P tools/schema-export/tests/Verify.cmake
```

Необязательные Java/command suites используют те же проверенные входы и новые временные каталоги:

```powershell
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/commands" -P tools/schema-export/tests/CommandTests.cmake
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/launcher" -P tools/schema-export/tests/LauncherTests.cmake
```

Capture использует отдельный бюджет JVM 120 секунд для production writer/reader observations;
бюджет команды экспорта 30 секунд не меняется. Проверка исходников Git ограничена 180 секундами.
В WSL используйте checkout/classpath на Linux filesystem, если Windows mount превышает бюджеты процессов.

Hashes исходников/classpath относятся к фактическому capture. Изменённый инструмент требует новых
доказательств новой проверки, а не исправления старого hash. [Проверка](../../tests/conformance/schema-export/VALIDATION.ru.md)
фиксирует выполненные проверки, ограничения платформ/provenance и открытую независимую приёмку.
Обычные CTests `schema_export_consumption` и `schema_export_cli` используют только сохранённые наблюдения.
Native XML import, общие настройки, новые identities/conversions, замена во время работы, транспорт
и частный tape остаются вне области; исторические fixtures неизменны.
