# Эталон tape

[English version](README.md). Английская версия является нормативной.

Это исследовательские инструменты Java QD FileWriter/FileReader, а не реализация tape. См.
[профиль и инвентаризацию исходников](../../../spec/protocol/TAPE-FILES.ru.md) и
[свидетельства](../../../tests/fixtures/protocol/tape/VALIDATION.ru.md). Workflow не использует новых
Python-скриптов или зависимостей от Python; существующие Python-инструменты не изменены.

## Capture и Verify

Нужны PowerShell 7.4+, Git, чистый checkout закреплённого upstream, JDK 8, Maven и собранный текущий
`qdcxx-qtp-inspect`. Для закреплённого Java formatter отдельно использовать JDK 21+.
Запускать из корня репозитория:

```powershell
$oracleArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/apache-maven/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Capture -RunDirectory .work/tape-review-a
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Capture -RunDirectory .work/tape-review-b
$java = Join-Path $oracleArgs.JavaHome 'bin/java.exe'
$classes = '.work/tape-review-b/harness'
& $java -cp $classes qdcxx.oracle.TapeVerify compare .work/tape-review-a/artifacts .work/tape-review-b/artifacts
& $java -cp $classes qdcxx.oracle.TapeVerify negative tests/fixtures/protocol/tape/expected .work/tape-review-negatives
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Verify -ExpectedDirectory .work/tape-review-negatives/value -RunDirectory .work/tape-review-negative-verify
# Предыдущая команда ОБЯЗАНА завершиться ошибкой. Baseline должен пройти:
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Verify -RunDirectory .work/tape-review-verify
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

Заменить машинные пути; метаданные captures содержат фактические пути executable и массивы аргументов.
Проверять каждый exit code; намеренный отказ negative Verify не считать отказом baseline.
Все каталоги запусков/мутаций должны быть новыми. Capture никогда не обновляет ожидания; перенос
в эталон — отдельное действие ревью. Verify читает оба набора артефактов и не переписывает ни один.

Каждый запуск последовательно выполняет upstream `-pl qds-file -am clean compile`, пропускает upstream
tests, отключает Maven build cache, компилирует новые Java-исходники и неизменённый SchemaOracle
для наблюдения окружения, затем запускает настоящие файловые пути. Не запускать два launcher
одновременно на одном checkout upstream. Maven использует существующий локальный кэш schema-oracle;
классы reactor собираются начисто. Зависимости runtime/build проекта не добавлены.

Launcher отвергает грязный/неверный upstream, неверные origin/tag/POM/release notes и внешние
переменные внедрения JVM. Требуется JDK 8. Логи дочерних процессов используют TZ/Java timezone UTC;
настройки хоста восстанавливаются. Процесс capture ограничен 30 секундами (включая обрезанный файл),
процессы инспектора — 10 секундами. Timeout завершает дерево дочернего процесса и даёт отказ,
никогда не становится принятым свидетельством EOF. Время Maven build не ограничено лимитом probe.
В harness нет sleep.

## Артефакты и проверка

`artifacts/` содержит исходные файлы writer, явно производный обрезанный файл, TSV настоящего reader,
диагностический TSV конвертов и точные stdout/stderr/exit инспектора. Переписывания байтов, удаления
сообщений или нормализации нет. `raw/tape.stderr` сохраняет production-логи, включая ожидаемую
диагностику EOF. Метаданные фиксируют команды/времена/exits, идентичность upstream, версии JDK/Maven,
хеши input/tool/harness-class, сводные хеши содержимого каталогов classpath, хеши JAR-зависимостей
кэша и executable инспектора. Для classpath хешируются отсортированные строки `relative-path<TAB>sha256`,
соединённые LF без финального LF. Инвентаризация зависимостей включает JAR из кэша и не утверждает,
что каждый из них загружен в runtime. Использование каталогов классов вместо JAR объясняет QDS-UNKNOWN.

TapeOracle проверяет арность/профиль входов и каждый доставленный локальный идентификатор, символ,
значение, флаг и время относительно входа, включая перестановку полей R. Фиксированный профиль —
семь записей в трёх входных сообщениях; каждый артефакт не превышает 64 KiB. Проверяется полный replay
либо частичный префикс с одной ошибкой EOF; verifier требует ровно четыре записи при обрезании. TapeVerify использует только JDK API и проверяет
точный набор 22 файлов, конечные непустые файлы, каждый тип/арность/домен значений строк TSV,
счётчики/порядок/завершение, последовательность/границы конвертов, происхождение обрезания, результат
инспектора и точные raw-байты против ожиданий. Точное сравнение также ограничивает содержимое свойств,
значения и time sequences сверх структурной валидации.

19 отрицательных случаев покрывают мутации байта/значения, пустые/отсутствующие/лишние артефакты,
неизвестную строку, отсутствие завершения, stdout/stderr/exit инспектора и неверную арность всех
девяти типов строк наблюдений (profile, descriptor, property, heartbeat, record, end, complete, error,
frame). Мутации выполняются только в новых копиях. Validator/comparator и полный чистый Verify
отвергают намеренную мутацию значения; последний сохраняет логи отказа и метаданные.

Это намеренно строгий comparator небольших fixtures, не общий validator tape. Например, все длины
кадров в captures помещаются в однобайтовый compact-заголовок. Эти ограничения нельзя переносить
на Java QD. Функции resync, коннекторов и более широких форматов из исходников этим harness не проверены.
C++ остаётся неизменённым.
