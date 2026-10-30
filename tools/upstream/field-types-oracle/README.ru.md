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
  Upstream = 'C:/src/qd-reference'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/tools/apache-maven-3.9.16/bin/mvn.cmd'
  Inspector = 'C:/src/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/tools/jdk-23
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/tools/jdk-23
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

## Расширение двоичных скаляров

Тот же запуск также записывает `field-types/scalars`: 25 дополнительных артефактов,
с 1106 production parser samples в STREAM и RAW для каждого закреплённого producer.
82 исторических корневых артефакта остаются побайтово прежними. Verify проверяет
конечную грамматику скаляров и все байты; Test-Negatives включает шесть scalar mutations,
всего 23 отвергнутые мутации. Metadata артефактов теперь включает дочерние пути.

BYTE/SHORT используют requested `BinaryRecordDesc` и production composer с
CompactIntField consumers (wire1/3, consumer8). Их default factories возвращают
null. Остальные новые non-VOID типы используют реальные consumers PlainIntField/
DecimalField/LongField/TimeMillisField. Десять исходных записей FileWriter/FileReader
покрывают 0/4/24/104/152 в STREAM/NONE и RAW/MESSAGE. Producer удаляет VOID;
приём explicit wire VOID и default-zero — отдельное наблюдение parser.
Физические long tails никогда не перечисляются как логические поля.

Для уже собранного чистого закреплённого classpath внутренний harness также принимает
`FieldTypesOracle <new-directory> scalar-only`. Он выполняет только scalar capture;
не собирает upstream и не проверяет expectations. Сначала скомпилируйте тот же source
для закреплённого JDK8 classpath, сохраните хеши class/source/runtime, затем сравните
полный набор из 25 файлов с рассмотренным каталогом соответствующего pin. Обычный
launcher сохраняет полный workflow чистого baseline Verify. Scalar capture/repeat
второго pin и чтение C++ отражены в
[scalar validation](../../../tests/conformance/protocol/field-types/SCALAR-VALIDATION.ru.md).
Новая универсальная CLI или default profile инспектора не добавлены.

## Расширение владеющих байтовых, строковых и объектных значений

Регламентированный запуск дополнительно пишет `field-types/objects`: 49 детерминированных
артефактов и 803 строки наблюдений на закреплённую ревизию. `ObjectFieldsOracle.java` —
внутренний помощник, компилируемый и вызываемый прежним средством запуска, а не отдельная
утилита. Он захватывает null/empty/content компоновщика с явно заданными типами и
FileWriter/FileReader в STREAM и RAW, реальные случаи парсера с известными и неизвестными
полями, неизвестными записями и повторяющимися целями, прямой производственный FieldReader
поверх BufferedInputPart и вспомогательные ограниченные примитивы. Прежние 82 корневых
и 25 скалярных артефактов по-прежнему проверяются первыми.

ByteArrayField — локальный обработчик 9/233; только компоновщик с явно заданными типами
доказывает wire233, а его FileWriter использует фактический тип 9. STRING использует
StringField(true), SERIAL_OBJECT — MarshalledObjField и только getBytes. getObject/unwrap
и десериализация Java не вызываются. Разветвление обратных вызовов RAW отличается от
числа записей в wire-потоке. Явные курсоры с временными метками делают RAW-файлы
детерминированными; строки наблюдений используют явный LF, поэтому текстовые атрибуты
fixtures не меняют захваченные байты. Последующей нормализации захватов нет.
Известный 249 использует проецированный INT; Java при пропуске неизвестных объектов
использует байты с длиной LONG и обходит проверку UTF для 137. Точная универсальная
грамматика C++ задана независимо от отображения согласно
[FIELD-TYPES](../../../spec/encoding/FIELD-TYPES.ru.md).

Прежняя внутренняя точка входа также принимает `FieldTypesOracle <new-directory>
object-only` для захватов обеих закреплённых ревизий с заранее собранным classpath.
Она не пересобирает и не проверяет upstream; обычное регламентированное средство
запуска сохраняет полный Verify с чистой сборкой. Compare проверяет конечные грамматики
строк и артефактов и все байты; Test-Negatives добавляет пять объектных мутаций,
всего 28 отклонённых мутаций. Исторические протоколы не меняются.
[Проверка объектных полей](../../../tests/conformance/protocol/field-types/OBJECT-VALIDATION.ru.md)
описывает новые доказательства отдельно.

## Наблюдения индексных схем и активных описаний

Прежний регламентированный запуск дополнительно создаёт `field-types/indexed`: 26 артефактов
и 572 канонические строки LF на ревизию. `IndexedSchemasOracle.java` компилируется прежним
средством запуска вместе с неизменённым экспортёром native-v1 как помощником наблюдений;
это не отдельная CLI-утилита. Одиннадцать исходных случаев FileWriter/FileReader сохраняют
по пять произвольных знаковых 64 значений времени, включая стандартный TimeAndSale с полной
фактической схемой источника. Проекция слотов и lookup, реальное связывание описаний,
опущенные хвосты, aliases, отсутствующие/переставленные поля, равные/новые/переопределённые
отправители и производственный reset учитываются отдельно.

Четыре успешных пакета повторного sender ID внутри одного сообщения доказывают последнее
действующее описание. Собранные пакеты с поздней и промежуточной ошибкой показывают фактическую
частичную мутацию Java; следующие за ними данные остаются неизменёнными байтами компоновщика.
Это не исходные записи FileWriter. Экспортёр native-v1 по-прежнему возвращает
UNSUPPORTED_INDEXED. C++ выбирает точные типы и откат целого пакета согласно
[INDEXED-SCHEMAS](../../../spec/schema/INDEXED-SCHEMAS.ru.md).

Внутренняя точка входа `FieldTypesOracle <new-directory> indexed-only` поддерживает захваты
с отдельно квалифицированным заранее собранным classpath. Она не пересобирает и не проверяет
upstream. Обычное средство запуска сохраняет полный Verify с чистой baseline-сборкой;
метаданные фиксируют четыре Java-исходника, оригинальные артефакты и фактические runtime/classpath.
Строгий comparator проверяет 26 индексных файлов после прежних наборов 82/25/49; шесть
индексных мутаций увеличивают общее число отрицательных проверок до 34.
[Проверка индексных схем](../../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.ru.md)
различает чистую baseline-сборку и заранее собранную текущую ревизию.

См. [покрытие](../../../tests/fixtures/encoding/field-types/COVERAGE.ru.md),
[захваты](../../../tests/fixtures/encoding/field-types/CAPTURES.ru.md),
[проверки](../../../tests/fixtures/encoding/field-types/VALIDATION.ru.md) и
[контракт полей](../../../spec/encoding/FIELD-TYPES.ru.md). Времена и пути metadata/logs —
происхождение конкретного запуска; байты наблюдаемых артефактов не нормализуются.

## Расширение флагов событий и времени удаления

То же управляемое средство запуска теперь записывает `field-types/flags`:105 дополнительных
артефактов. FlagsOracle — внутренний помощник FieldTypesOracle, а не отдельный launcher. Хеши его
исходника и классов входят в обычный отчёт происхождения. `FieldTypesOracle <new-directory> flags-only`
поддерживает явно квалифицированный готовый classpath без пересборки upstream и проверки эталонов.
Обычный запуск сохраняет полный Verify с чистой baseline-сборкой.

Набор содержит 104 исходных двоичных артефакта и 4300 строк канонических наблюдений. Отдельно помечены
реальные файлы FileWriter/FileReader, входы и маски composer, составленные изменения сообщений,
наблюдения production parser и чтение примитивов Penta. Все известные подмножества 0x5f наблюдаются
во всех четырёх семействах данных. Три RAW callback сравниваются до выбора одной STREAM-проекции.
Фиксируются точные отсутствие/пустая строка/hs и неизвестные/повторённые опции; ненастроенный
FileWriter фактически выбирает hs. REMOVE_EVENT покрывает исходные signed 64, обычные/индексные/
неизвестные/нулевые по числу полей записи, пропущенные defaults, следующий sentinel и усечения времени.

Compare-FieldTypes проверяет точный набор 105 файлов, конечную грамматику и число строк, исходные
значения, кратность проекции и все байты артефактов. Test-Negatives добавляет восемь мутаций флагов
к прежним34 случаям: всего 42 отказа. Исторические 182 артефакта сохранены; полный проверяемый набор
содержит 287 файлов. [Проверка флагов](../../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.ru.md)
разделяет поведение Java, выбранные отказы C++, политику завершённого префикса и оставшиеся границы.



## Дополнение для конечной диспетчеризации

DescriptorOracle расширяет тот же процесс Capture/Verify до 609 артефактов, сохраняя все 287 прежних
файлов побайтно. Обычный захват содержит 305 артефактов (303 исходных tape, 668 наблюдений, карту
57 диапазонов UID); отдельный ограниченный дочерний процесс с явным noHeaderAndFooter — 17 артефактов
(16 исходных tape, 25 наблюдений). Реальные FileWriter/FileReader покрывают четыре семейства с
NONE/MESSAGE; управляемые изменения дескрипторов/управления/EOF выделены отдельно. Исходные случайные
синтетические UID конечных точек сохраняются вместе с хэшами каждого запуска. Только 57 структурно
определённых по исходному коду диапазонов UID нормализуются в памяти при сравнении; остальные байты
и форма карты должны совпадать. Все 16 tape без заголовка совпадают побайтно. Новые сопровождаемые
launcher и зависимости не добавлены. Прежний отрицательный процесс проверяет 50 мутаций
(42 прежних и 8 для диспетчеризации). См. [валидацию диспетчеризации](../../../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.ru.md)
и [манифест](../../../tests/fixtures/encoding/field-types/dispatch-captures.json). Исторические
разделы выше сохраняют исходные количества подмножеств и хэши исходников.
