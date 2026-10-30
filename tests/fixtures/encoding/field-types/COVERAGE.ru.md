# Покрытие oracle точных типов полей

[English version](COVERAGE.md). Английский текст является основным.

Отдельное расширение владеющих полезных нагрузок 9/137/233/249 описано в
[проверке объектных полей](../../../conformance/protocol/field-types/OBJECT-VALIDATION.ru.md)
и [манифесте захватов обеих закреплённых ревизий](object-captures.json). Его
49 артефактов и 803 наблюдения на ревизию покрывают компоновщик с явно заданными
типами, FileWriter/FileReader, пути парсера с известными и неизвестными полями,
неизвестными записями и повторяющимися целями, отдельные позиции FieldReader
и вспомогательные примитивы. Историческое покрытие ограниченного профиля ниже
сохраняет первоначальные границы и протоколы; само по себе оно не квалифицирует
новый профиль, семантику объектов Java или общее поведение двоичных контейнеров.

[82 ожидаемых артефакта](expected/) содержат 40 полных двоичных входов, 40 production-наблюдений,
инвентаризацию схемы и таблицу примитивов. Используется закреплённый upstream
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; см. [захваты](CAPTURES.ru.md) и
[проверки](VALIDATION.ru.md). Первые измерения предшествовали реализации C++.

| Доказательство | Наблюдаемая граница |
| --- | --- |
| `primitives.tsv`: 9 записей, 219 чтений строк, 324 чтения целых | Production StringField/IOUtil и ShortStringField/SequenceField/DateField; точные bytes, units, status и consumption |
| `mixed-stream`, `mixed-raw` | Исходный FileWriter output прочитан FileReader, по три одиночные записи/data frame; отдельно Java RAW callback fanout |
| `profile-stream`, `profile-raw`, `schema.tsv` | Неизменённый стандартный Profile: все 24 int-позиции (десять VOID tails), два объекта, 16 логических полей; 331 запись в стандартной схеме без overrides |
| `composer` | Описания/данные production BinaryQTPComposer и доставка BinaryQTPParser, полное потребление входа |
| `missing`, `repeat-0..2`, `mixed-order` | Null по умолчанию для объектов, нулевые скаляры, смешанный логический порядок int/object, последнее повторное присваивание включая null/empty/content |
| `unknown-field`, `unknown-record` | Декодирование строки перед пропуском; следующая известная запись доказывает успешное выравнивание |
| `malformed-skipped`, `malformed-unknown-record`, `oversized-skipped` | Повреждённая строка или недопустимый count на границе сообщения вызывает production corruption; Java затем доставляет sentinel |
| `mismatch-int`, `mismatch-object` | Java преобразует несовпадающие целочисленные представления и оставляет несовместимый объект null; exact-match отказ C++ — отдельная более узкая политика |
| `string-wire-0..10` | Production-разбор null/empty/ASCII, overlong BMP, отказ supplementary scalar, неверный continuation, CESU surrogate pair, отрицательный и избыточный count |
| `wide-40-*`, `wide-72-*`, `wide-88-*` | Production compact-long проецируется в signed int32, затем известный скаляр и следующая запись |

Строковые примитивы включают NUL, ASCII, многобайтовый BMP, парные/одиночные суррогаты и
управляющие units; сохранён каждый строгий префикс writer-вектора. Принимаются overlong
2/3/4-байтовые представления BMP; четырёхбайтовый supplementary scalar отклоняется после всех
четырёх байтов. IOUtil читает весь необходимый хвост перед проверкой continuation bytes.
BufferedInputPart отвергает count символов больше оставшихся байтов сообщения на префиксе count,
в отличие от обычного ByteArrayInput. Допустимый INT_MAX проверяется только через BufferedInputPart
до выделения памяти; отрицательные count ниже -1 и count выше INT_MAX отклоняются без огромных
массивов. Успешные примитивы не возвращают частичные строки; status/consumption ошибки записаны отдельно.

Полные tags 40/72/88 сохраняют signed raw int32 даже при более широком или избыточном compact-входе.
Таблица включает знаковые пределы, границы знака/ширины, все строгие префиксы избыточных и широких
форм и проекции младших 32 бит крайних signed int64. Текстовые/календарные преобразования не выводятся.
Значения поля Sequence намеренно отличаются от RAW heartbeat eventTimeSequence.

Profile witness устанавливает различные исходные значения прямо в production RecordCursor и
использует неизменённые production writer/parser/file paths. Это доказательство уровня полей,
не round trip бизнес-события через ProfileDelegate. Физические VOID-позиции сохранены в схеме
и пропущены только в логической проекции значений. Production-файлы сохраняют исходные descriptors,
descriptions, группировку сообщений, timing и footer. Диагностические wire-файлы названы отдельно
и строят envelopes вручную; их наблюдения всё равно получены production FileReader.

Java может продолжить после повреждённого полного сообщения и допускает некоторые преобразования
полей. Эти наблюдения не ослабляют ограниченную terminal/exact-match политику C++ в
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.ru.md). Дополнительные реальные свидетели 40/72
не обязательны: стандартный Trade включает исключённый LONG, а некоторые SHORT_STRING witnesses
индексированы. Схема не упрощается ради заявления о поддержке. Полнота частного файла, сжатие,
flags, изменяемые sessions, индексированные записи, остальные полные serial types и
Profile/event adapters остаются вне границ.

Связь с исходниками: upstream `qd-core/.../kit/{StringField,ShortStringField,SequenceField,DateField}.java`,
`qd-core/.../qtp/BinaryRecordDesc.java`, `dxlib/.../io/{IOUtil,BufferedInputPart}.java` и
`dxfeed-impl/.../market/MarketFactoryImpl.java`; точная ревизия и хеши classpath записаны в metadata.
Исходник harness и команды повторения приведены в [README oracle](../../../../tools/upstream/field-types-oracle/README.ru.md).

Отдельное [расширение флагов](../../../conformance/protocol/field-types/FLAGS-VALIDATION.ru.md)
добавляет известные метаданные событий STREAM/NONE по явной политике, сохраняя прежний обычный
набор без изменений. Прежние режимы, collector, FIELD и форматы writer не расширяются.

Отдельная [валидация конечной диспетчеризации](../../../conformance/protocol/field-types/DISPATCH-VALIDATION.ru.md)
добавляет 322 синтетических артефакта и проверки вывода версии 8, семейств и дескрипторов,
сохраняя все 287 прежних файлов. [Происхождение захватов](dispatch-captures.json) фиксирует каждый
исходный хэш, точное представление сравнения только для 57 диапазонов UID и независимые байты
входов без заголовка. Результаты прежних подмножеств выше остаются историческими.
