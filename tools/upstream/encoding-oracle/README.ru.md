# Oracle примитивов (QDCXX-009)

[English](README.md). Английская версия является основной.

Независимо написанные ограниченные входы вызывают методы закреплённого Java QD напрямую. Реализация кодека и исходники upstream не копируются. Используются соглашения capture/provenance из [schema](../schema-oracle/README.md) и [protocol](../protocol-oracle/README.md), штатный classpath исходной сборки из 12 модулей. Готовые QD-библиотеки не заменяют исходную сборку.

Запускайте последовательно в PowerShell 7.4+ из корня репозитория:

```powershell
$upstream = "$env:TEMP/qdcxx-003-qd"
$jdk = 'C:/Program Files/Zulu/zulu-8'
$maven = Get-ChildItem "$env:USERPROFILE/.m2/wrapper/dists/apache-maven-3.9.16" -Recurse -Filter mvn.cmd | Select-Object -First 1 -ExpandProperty FullName
$runner = './tools/upstream/encoding-oracle/Invoke-Oracle.ps1'
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven -RunDirectory tools/upstream/encoding-oracle/.work/google-a
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven -RunDirectory tools/upstream/encoding-oracle/.work/google-b
# После проверки явно перенесите google-a/normalized/*.tsv в tests/fixtures/encoding/expected.
./tools/upstream/encoding-oracle/Test-Verify.ps1 -Upstream $upstream -JavaHome $jdk -Maven $maven -TestDirectory tools/upstream/encoding-oracle/.work/google-verify
```

Каждому запуску нужен новый каталог; origin/HEAD/tag/version и чистота исходников проверяются до и после штатной чистой сборки. Тесты и повторное использование build-cache отключены; enforcer JDK 8 включён. Maven cache и рабочий каталог JAXB настроены как в предыдущем harness. Лимит процесса — 30 секунд (Maven: 600 секунд); heap oracle JVM — 64 MiB. Ручные байтовые входы ограничены 1024 байтами; фактические входы меньше. Проверяются только малые положительные длины строк и длины, отвергаемые до выделения памяти. Допустимые экстремальные длины намеренно отложены.

Capture не обновляет эталоны. Verify сравнивает точные наборы файлов и SHA-256 без записи эталонов. Test-Verify меняет только временные байты writer I-zero с 00 на 01, требует сбоя ровно integers.tsv, затем проверяет baseline и неизменность обоих наборов эталонов.

В cases.json перечислены три семейства с отдельными JVM; каждое чтение получает новый ByteArrayInput с нулевой позицией. Reset/rollback не выполняется. Строки writer: вид, ID вектора, метод, десятичный вход либо hex кодовых единиц UTF-16, точные байты. Строки reader: вид, ID, метод, hex входа, ok/error, десятичный результат/кодовые единицы либо класс исключения, consumed position, оставшиеся байты, необязательное сообщение исключения. Пустые ячейки обозначают пустые значения; `\N` — null. Ошибка относится к указанному прямому методу; внутренняя стадия не предполагается. Суффикс `/prefix-N` обозначает каждый строгий префикс, включая нулевой. Строка complete и успешный код выхода контролируют завершённость. Перехватывается только IOException; неожиданные ошибки runtime/JVM прерывают запуск.

Сохраняются исходные stdout/stderr. Нормализация меняет только CRLF на LF. Метаданные содержат точные команды/cwd, JDK/Maven/окружение, upstream, хеши инструментов и manifest, байткода harness, упорядоченного classpath и список cache jars (надмножество, а не runtime dependencies). Хеш Java-кода идентифицирует программные и ручные входы. Десятичные строки сохраняют все 64 бита; строки не проходят текстовую нормализацию Unicode. См. [покрытие](../../../tests/fixtures/encoding/COVERAGE.ru.md) и [валидацию](../../../tests/fixtures/encoding/VALIDATION.ru.md).
