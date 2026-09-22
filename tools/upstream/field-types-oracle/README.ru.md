# Production oracle точных типов полей

[English version](README.md). Английский текст является основным.

Oracle запускает неизменённый закреплённый QD через production-методы `StringField`/`IOUtil`,
методы целочисленных полей, `BinaryQTPComposer`/`BinaryQTPParser` и `FileWriter`/`FileReader`.
Он сохраняет исходные файлы BINARY без сжатия в режимах STREAM/NONE и RAW/MESSAGE с options
EMPTY_SET и нулевыми flags. Частный tape не читается. Полное отображение события Profile
не проверяется и не реализуется.

`FieldTypes` — диагностическая обычная запись. Логический порядок: Text(10), Short(40),
Sequence(72), Date(88), Count(8), Char(2), Seconds(56), Wide(120), Tail(10). Java хранит Text/Tail
в object-позициях 0/1; семь скалярных полей занимают int-позиции 0..6, за ними VOID tail в 7.
`schema.tsv` сохраняет эту проекцию и каждую физическую позицию, локальное имя и property name
неизменённого стандартного Profile. У Profile 24 физических int-позиции, два объекта,
нет временной индексации и 16 логических полей; поля и настройки схемы upstream не удаляются.

Отдельно измеряются три способа чтения строк: обычный ByteArrayInput (`buffered`),
DataInputStream (`data-input`) и production-граница сообщения BufferedInputPart (`message-part`).
Последний проверяет число символов относительно оставшихся байтов сообщения до выделения памяти.
Допустимый count INT_MAX проверяется только на этой границе; count вне диапазона Java int
безопасно проверяются примитивами. У процесса бюджет heap 128 MiB и времени 30 секунд.
Таймаут означает ошибку. Огромный массив ради демонстрации исчерпания памяти не выделяется.

Каждому запуску нужен новый каталог. Сначала launcher выполняет прежний tape baseline Verify:
проверку закреплённого чистого upstream, чистую сборку reactor, хеши classpath/providers/dependencies
и побайтовое сравнение прежних tape expectations. Нужны PowerShell 7.4+, JDK 8, Maven и прежний
инспектор. Formatter использует отдельный JDK 21+.

```powershell
$fieldArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-a
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-b
& tools/upstream/field-types-oracle/Compare-FieldTypes.ps1 -Actual .work/field-types-a/field-types -Expected .work/field-types-b/field-types
& tools/upstream/field-types-oracle/Test-Negatives.ps1 -RunDirectory .work/field-types-negatives
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -Mode Verify -RunDirectory .work/field-types-verify
```

Comparator проверяет конечный набор артефактов, грамматику строк, канонические целые, UTF-16,
инварианты nullable/scalar, полные tags, порядок и завершение, затем сравнивает все байты.
Capture проверяет структуру относительно себя; независимые cross-capture и Verify устанавливают
равенство. Verify никогда не заменяет expectations. Отрицательные проверки меняют bytes, tags,
values, units, nullability, порядок, identity, отсутствующие/повторные/неизвестные строки,
число колонок, завершение и потребление примитива.

См. [покрытие](../../../tests/fixtures/encoding/field-types/COVERAGE.ru.md),
[захваты](../../../tests/fixtures/encoding/field-types/CAPTURES.ru.md),
[проверки](../../../tests/fixtures/encoding/field-types/VALIDATION.ru.md) и
[контракт полей](../../../spec/encoding/FIELD-TYPES.ru.md). Времена и пути metadata/logs —
происхождение конкретного запуска; байты наблюдаемых артефактов не нормализуются.
