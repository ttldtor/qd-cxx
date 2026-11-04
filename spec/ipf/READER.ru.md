# Контракт локального IPF reader {#local-ipf-reader-ru}

[English version](READER.md). Английская версия нормативная.

Независимый reader `qdcxx_ipf` принимает обычный несжатый UTF-8 Instrument Profile Format.
Эталон — Java QD 3.355, ревизия `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`:
[InstrumentProfileParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/impl/InstrumentProfileParser.java),
[CSVReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/CSVReader.java),
[InstrumentProfileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileReader.java).
[42 фактических Java-случая](../../tests/fixtures/ipf-reader/README.ru.md) сохраняют входные bytes,
порядок целых profiles/controls, первые ошибки и результат завершённости полного reader.
Значения полей следуют отдельному [контракту значения](PROFILE.ru.md).

## Грамматика и данные

Format-row начинается с `#<type>::=TYPE`, за ним идут точные имена полей. Последующая data-row
начинается с этого type и обязана содержать ровно объявленное число остальных полей. Форматы
атомарно переопределяются, поддержаны пользовательские/пустые type и имена полей. Неопределённые
форматы и запрещённые имена metadata/control дают явную ошибку. Известный формат ищется до
толкования начального `#` как metadata; пользовательские type, похожие на metadata, поддержаны.

CSV использует запятые и двойные кавычки. Кавычка начинает поле только с его первой единицы;
парные кавычки внутри quoted-текста дают одну. Текст после закрывающей кавычки, кавычки в unquoted
поле и EOF внутри quoted-поля дают ошибку. Quoted-поля сохраняют запятые и физические CR/LF/CRLF.
Вне кавычек все три формы newline завершают строку; CRLF считается один раз. Последняя закрытая
строка без newline допустима. Короткая/избыточная data-row отклоняется без частичного profile.
Синтаксическая ошибка в избыточном хвосте обнаруживается до ошибки числа полей. Стандартное поле
преобразуется при завершении ячейки: раннее неправильное число имеет приоритет над поздней CSV-кавычкой.

Непустые ячейки применяются по порядку через модель значения profile. Пустые пропускаются.
Дубликат поля поэтому сохраняет прежнее непустое значение при следующей пустой ячейке;
следующее непустое значение заменяет его. Столбец `TYPE` может заменить исходный type формата.
Пользовательские имена и регистр точны. Выбор заголовков, уникальность символов и бизнес-валидация
типов не навязываются сверх настроенных операционных лимитов.

UTF-8 decoder следует измеренным правилам замены JDK. Ошибочные начальные bytes и malformed
prefixes заменяются на U+FFFD; оборванная последняя последовательность становится заменой,
сама по себе не синтаксической ошибкой. Кодировка суррогата целиком потребляет три bytes и даёт
одну замену. UTF-8 BOM сохраняется как U+FEFF и не превращает первый BOM-prefixed header в
распознанный заголовок. Дополнительный Unicode scalar становится точной парой UTF-16 units.
Между вызовами decoder держит не более четырёх bytes и одного pending low surrogate.

## Controls, EOF и частичные результаты

`##` выдаёт FLUSH, `##COMPLETE` — COMPLETE после проверки всего CSV-хвоста. Остальные metadata-row —
комментарии. Controls могут содержать игнорируемые дополнительные поля, но их CSV-синтаксис
значим. Completion — свидетельство, а не остановка: последующие profiles, controls и ошибки
обрабатываются. Пустые строки пропускаются.

`completionSeen` сохраняет принятый marker даже при поздней ошибке. `finished` требует чистого
явного EOF по выбранной политике; `complete` — ровно `finished && completionSeen`.
По умолчанию `ALLOW`: чистый EOF без marker успешен, но неполон, что соответствует default
warn-and-return закреплённого Java reader без ambient logging. `REQUIRE` сообщает MISSING_COMPLETE
на чистом EOF и сохраняет ранее принятые profiles. Java system properties не читаются.
Пустой feed и пауза работы не являются EOF.

Вызов parser возвращает один независимый item, требуемый input/work, FINISHED или первую
терминальную ошибку FAILED. `accepted` — точный потреблённый префикс заимствованного span текущего
вызова; вызывающая сторона повторно передаёт остаток. Ранее выданные profiles живут после новых
feeds и уничтожения parser. Частичный profile не выдаётся. Терминальные вызовы принимают ноль bytes.

Диагностика содержит фиксированную категорию, логический byte cursor, физическую строку/CSV-row
с единицы, поле с нуля, начальные строки row/field и optional ProfileError. Синтаксическая ошибка
указывает первый byte ошибочной кодовой единицы; незакрытая кавычка — EOF. Ошибка преобразования
указывает начало поля; неопределённый/запрещённый формат — начало row. Накопительные бюджетные
ошибки указывают исследованный cursor parser. Эти позиции не являются физическим prefetch-offset
Java InputStreamReader или гарантией seek/rollback файловой системы.

## Работа, хранение и владение

`InstrumentProfileParser` владеет UTF-8/CSV-состоянием, форматами, текущими полем/profile. Он не
выполняет I/O, callbacks, фоновую работу, не содержит очередь результатов. Copy/move запрещены;
вызывающая сторона сериализует доступ. Options копируются при создании. Входные spans заимствуются
только на время вызова; результаты не содержат ссылок на input или header storage.

`IpfReadWork` списывает input bytes, decoded UTF-16 units, завершённые поля и операции приёма row.
Для старта/продолжения цикла все четыре остатка должны быть ненулевыми; допустим консервативный
WORK_EXHAUSTED даже если конкретной pending-операции не нужен исчерпанный вид работы. Нулевой work
не принимает input. Вызывающая сторона пополняет его и продолжает с остатком либо очередным finish.
Неуспешная работа не возвращается. Синхронные drivers проверяют положительные turn quotas до I/O,
сами пополняют их; накопительные лимиты ограничивают чтение целиком.

Инженерные значения по умолчанию, независимо настраиваемые для reader:

| Граница | По умолчанию | Учёт |
| --- | --- | --- |
| Input | 67108864 bytes | Накопительные принятые parser UTF-8 bytes |
| Field | 65536 units | Unescaped значение UTF-16 |
| Активная row | 1048576 units | Decoded syntax/text без завершающего разделителя |
| Fields/row | 256 | Включая первое/type поле |
| Rows | 1000000 | Включая blank/comment/header/control |
| Formats | 256 | Различные сохранённые определения type |
| Header storage | 1048576 units | Type names плюс объявленные field names |
| Profiles | 100000 | Целиком выданные profiles |
| Profile output | 16777216 units | Накопительные стандартные значения и custom names/values |
| Turn work | 4096/4096/256/256 | Bytes/decoded units/fields/rows |

Каждый profile сохраняет независимые `ProfileLimits`. Временная замена header ограничивается
active-row/field limits, пока прежнее определение сохраняется; retained header accounting
проверяется до commit замены. Текущие profile/cell и возвращённые владеющие результаты имеют
отдельные лимиты. Арифметика проверяется до сложения/роста; лимиты описывают логические
единицы/количества, а не точную ёмкость allocator, RSS или CPU deadline.

## Граница локального stream/file

`readInstrumentProfiles(std::istream&, options)` читает с текущей позиции, не закрывает, не seeks
и не откатывает заимствованный stream. Определённые field/row outcomes обрабатываются до новой I/O.
Streambuf может предварительно прочитать физические bytes; byte для различения CRLF может оказаться
впереди логического потребления при возвращении ранней ошибки. `acceptedBytes` сообщает только
логическое потребление. Resource/work limits не задают deadline блокирующей I/O. Обычный EOF остаётся
обычным при включённых stream exception masks. Маска исключений вызывающей стороны сохраняется.
Если `std::bad_alloc` выходит из stream-операции, результат содержит ALLOCATION_FAILURE;
если stream подавляет это исключение и устанавливает `badbit`, наблюдаемая ошибка даёт IO_ERROR.
Иные stream-ошибки дают IO_ERROR. Ошибки выделения памяти parser/output дают ALLOCATION_FAILURE.
Каждая типизированная ошибка сохраняет владеющий полный префикс profiles.

`readInstrumentProfilesFile(path, options)` открывает собственный binary ifstream и закрывает
только его. Binary mode сохраняет quoted CRLF на Windows. Ошибка открытия даёт IO_ERROR.
Нет URL resolution, HTTP, credentials, decompression, writing, live collection, option-chain и
толкования schedule. См. [прикладное руководство](../../docs/INSTRUMENT-PROFILES.ru.md).

URL/file/HTTP(S) acquisition описан отдельно в [URL-READER](URL-READER.ru.md); локальные функции этого документа не разрешают URL.
