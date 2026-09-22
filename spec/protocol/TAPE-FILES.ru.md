# Файлы tape Java QD: свидетельства и ограниченный следующий профиль

[English version](TAPE-FILES.md). Английская версия является нормативной.

QDCXX-019 — исследование QD 3.355, коммита
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` из [upstream.lock](../../upstream.lock).
Документ не расширяет [контракт текущего инспектора](OFFLINE-INSPECTION.ru.md).
«Измерено» означает новые запуски FileWriter/FileReader; «исходники» — прочитанный код,
а не выполненный upstream-тест. Будущий профиль предложен и требует одобрения владельца.

## Источники и производственные пути

Все ссылки закреплены на одном коммите; исходники upstream не изменялись и не копировались в harness.
| Ссылка | Существенный исходник / контракт |
| --- | --- |
| F1 | [FileFormat](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileFormat.java): `valueOf`, `detectFormat`, `createQTPComposer`, `createQTPParser`, `getTimestampsType`, `isBareBones` |
| F2 | [FileWriter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriter.java), [FileWriterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java): constructor, `open`, `visitData`, `updateCurTime`, `writeData`, `writeHeader`, `writeFooter`, `close` |
| F3 | [FileWriterParams](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterParams.java), [FileReaderParams](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReaderParams.java): значения по умолчанию прямого API |
| F4 | [FileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java): `readInto`, `openFiles`, `process`, `createParserOnFirstChunk`, `configureParser`, `processHeartbeatMessage`, `processRecordSourceMessage`, `doNotWait` |
| F5 | [BinaryFileQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/BinaryFileQTPParser.java): необязательный `schemeKnown` и эвристический `resyncOn` |
| F6 | [TimestampsType](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TimestampsType.java), [TimestampedPosition](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TimestampedPosition.java), [FileUtils](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileUtils.java): режимы времени и имена сопутствующих файлов |
| F7 | [StreamCompression](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/StreamCompression.java): NONE/GZIP/ZIP, расширения и определение по заголовку |
| F8 | [ProtocolDescriptor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java), [ProtocolOption](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolOption.java), [QDFactory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDFactory.java): тип/версия/опции tape; null в свойствах превращается в пустую строку |
| F9 | [AbstractQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPComposer.java), [BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java): автоматические описания и маска поддерживаемых флагов |
| F10 | [TextDelimiters](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/text/TextDelimiters.java), [BlobQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/blob/BlobQTPComposer.java), [BlobQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/blob/BlobQTPParser.java) |
| F11 | [TapeConnector](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/TapeConnector.java), [FileWriterHandler](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterHandler.java): путь вызовов коннектора и RAW_DATA по умолчанию |
| F12 | [FileWriterTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/FileWriterTest.java), [TapeConnectorFlagsTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/TapeConnectorFlagsTest.java), [FileConnectorCorruptedTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/file/test/FileConnectorCorruptedTest.java): кандидаты для более широких тестов, только прочитаны |

Harness вызывает `FileWriter.open → FileWriterImpl.open`, затем `visitHeartbeat/visitData →
composer → writeData → ParallelWriter` и `close` перед чтением. Прямого создания raw QTP нет.
Чтение проходит через `FileReader.setScheme/readInto → openFiles → createParserOnFirstChunk →
FileFormat.createQTPParser → BinaryFileQTPParser/TextQTPParser → FileReader.Consumer → observer`.
Нет подкласса парсера, рефлексии, подмены часов, строки версии или коннектора.
Отдельный сканер конвертов использует `ByteArrayInput.readCompactInt` и доказывает только framing.

## Инвентаризация исходников и точные значения по умолчанию

| Поверхность | Поведение закреплённого исходника | Выполненное покрытие |
| --- | --- | --- |
| BINARY | Формат writer по умолчанию независимо от обычного расширения имени; конверты QTP, автоматические описания записей, заголовок протокола и завершающий пустой heartbeat. Время по умолчанию MESSAGE. | Обычный и ограниченный несжатые файлы |
| TEXT | Текст с табуляцией, префикс сообщений `==`, описаний записей `=`. Время по умолчанию FIELD. | Один файл с временем/опциями по умолчанию |
| CSV | Text composer/parser с запятыми и описаниями `#=`; bare bones без заголовка/окончания протокола; parser предполагает STREAM_DATA. Время по умолчанию FIELD. | Только исходники |
| BLOB | Явный `blob:<record>:<symbol>`, фиксированные локальные запись/символ, сериализация полей без конвертов QTP; composer принимает HISTORY_DATA/RAW_DATA, parser доставляет HISTORY_DATA. Bare bones, время по умолчанию NONE. | Только исходники |
| Выбор формата | Явный `format` writer либо BINARY. Reader использует явный формат либо определение по первому буферу: сначала префикс TEXT, затем CSV, иначе BINARY. BLOB требует явной настройки. Определение не является общей валидацией. | Автоопределение BINARY/TEXT |
| Сжатие | Writer: явное сжатие либо расширение `.gz`/`.zip`, иначе NONE. Reader: явная настройка/расширение, затем заголовок, если выбор ещё не сделан; GZIP `1f8b`, ZIP `504b0304`. | Только NONE; сжатые файлы не получались |
| Время NONE | Время не сохраняется. Заголовок/окончание остаются у форматов, не являющихся bare bones. | Ограниченный файл |
| Время LONG / TEXT | Отдельный файл `.time`: миллисекунды либо форматированное время, затем `:` и позиция в байтах. Имя заменяет расширение данных, сохраняя расширение контейнера. Reader делит вход по позициям из sidecar. | Только исходники |
| Время FIELD | Поля метаданных EventTime/EventSequence; обычные прикладные поля остаются отдельными. | EventTime в TEXT, нулевая sequence |
| Время MESSAGE | Время в heartbeat на блок данных; reader преобразует его в event-time sequence доставленных курсоров. | BINARY по умолчанию, три времени |
| Дескриптор | `type=tape`, `version=QDFactory.getVersion()`, настроенный `opt`; `time`, если не NONE. `saveAs` добавляет отправляемый тип. Системное свойство `noHeaderAndFooter` по умолчанию false. | Оба бинарных заголовка сохранены, не удалялись |
| Флаги/опции | Поддерживаемый набор без переопределения свойством — `hs`. Binary composer маскирует флаги по текущему сообщению/опциям; `opt=""` означает EMPTY_SET. Наличие `hs` не означает ненулевые флаги каждой записи. | Только нулевые входные/выходные флаги |
| Размещение описаний | Первое использование может вызвать описание прямо перед данными этой записи, даже после предыдущих данных. | S, затем R, затем E с чередованием данных |

Прямой `FileWriterParams.Default`: split/compression/format/time/saveAs/tmpDir равны null,
storageTime unlimited, storageSize `Long.MAX_VALUE`, opt `SUPPORTED_SET.toString()` (здесь `hs`).
Разрешённые defaults для нашего имени `.qtp`: BINARY/сжатие NONE/MESSAGE; без разбиения и retention.
`TapeConnector` отличается: по умолчанию `saveAs=RAW_DATA` (F11). Эти captures доказывают прямой
путь FileWriter со STREAM_DATA, а не настройки коннектора по умолчанию или события dxFeed в целом.

Прямой `FileReaderParams.Default`: пустые user/password; start/stop/delay `FileConnector.NA_TIME`;
compression/format/time/readAs/resyncOn/fieldReplacer null; ignoreTime/cycle/schemeKnown false;
speed 1; stats `QDStats.VOID`. Наши единственные переопределения — speed `MAX_SPEED=Double.MAX_VALUE`
и явное cycle=false; локальная схема передаётся через `setScheme`. Не включены задержки воспроизведения,
follow, field replacers, resync или предустановленная схема отправителя. Единственное буквальное имя
файла даёт `doNotWait()` true. Слежение за несколькими файлами с временными именами изучено только
по исходникам, не измерено.

## Измеренные файлы и воспроизведение

[Покрытие](../../tests/fixtures/protocol/tape/COVERAGE.ru.md),
[валидация](../../tests/fixtures/protocol/tape/VALIDATION.ru.md) и
[входы](../../tests/fixtures/protocol/tape/inputs.tsv) указывают артефакты и воспроизведение.
Три записи файлов потребляют одинаковые семь строк / три сообщения. ID/порядок полей отправителя:
R=0(x,y,z), S=1(v), E=2(); локальные ID/поля: S=0(v), R=1(y,x,z), E=2(). Все поля — обычный
type 8, hasTime=false. Символы включают повтор ABC, U+2603 и пустой; значения — оба предела int32.
Входные heartbeat несут `1700000000123`, `1700000001123`, `1700000002123` мс.
Курсоры FLAGGED_DATA имеют нулевые флаги и не содержат входной event-time sequence.

| Файл | Настройки writer | Размер raw | Результат настоящего Java reader | Типы конвертов / результат инспектора |
| --- | --- | --- | --- | --- |
| default.qtp | Без setter-вызовов | 165 байт | 7 записей, 3 data callbacks, 3 временных heartbeat, 0 ошибок | `1,0,2,15,0,2,15,0,2,15,empty`; inspector exit 4 на type 1 |
| constrained.qtp | BINARY, compression NONE, time NONE, opt пустой, saveAs STREAM_DATA | 137 байт | 7 записей, 1 data callback, без heartbeat callback, 0 ошибок | `1,2,15,2,15,2,15,empty`; inspector exit 4 на type 1 |
| text.txt | format TEXT; прочие defaults writer | 432 байта | 7 записей, 1 data callback, callback пустого heartbeat, 0 ошибок | Бинарный scan не выполнялся; inspector exit 4 после трактовки начала текста как type 61 |
| truncated.qtp | Диагностическая копия constrained.qtp без последнего байта данных и footer | 135 байт | 4 записи, 1 data callback, 1 corrupted-stream callback, readInto вернулся | Inspector по-прежнему выходит с 4 на начальном type 1; до обрезания не доходит |

Обычный BINARY и TEXT доставляют одинаковые упорядоченные локальные идентификаторы, единицы символов,
значения в локальном порядке, нулевые флаги и event-time sequences. Последовательности:
`7301444403715899392`, `7301444408010866688`, `7301444412305833984`;
у constrained/truncated sequence равна 0. Группировка callbacks различается и не является контрактом
границ записей. Пустой бинарный footer виден в raw-байтах и строках конвертов, но не вызвал heartbeat
callback наблюдателя; TEXT footer вызвал. Наблюдения протокола включают отсортированные свойства,
наборы send/receive и null endpoint: у default/TEXT send пуст, у constrained/truncated — STREAM_DATA;
receive пуст. Runtime из каталогов классов сообщает `version=QDS-UNKNOWN`; значение сохранено буквально.
Идентичность upstream устанавливают проверки commit/tag/POM/чистоты дерева, а не эта строка runtime;
поддельная версия не задавалась.

Два чистых запуска дали одинаковые raw-байты tape и все 22 сравниваемых артефакта без нормализации.
Логи и метаданные сохраняют разные пути запусков и времена выполнения UTC. Sidecar-файлы не создавались.
CRLF в текстовом файле сохранены; TSV наблюдений и вывод инспектора используют LF. Java EOF probe
завершается в пределах 30-секундного лимита процесса и пишет в лог 13 неразобранных байт. Это ошибка
неполного кадра на EOF, а не timeout или обычный пустой EOF. Полные файлы возвращаются без ошибок.
Этот probe не доказывает общее восстановление повреждений, file-follow или безопасность ресурсов.

## Таблица пробелов текущего инспектора

| Область | Результат и пробел | Класс свидетельства |
| --- | --- | --- |
| Декодирование файла/контейнера | Выбранные BINARY-байты непосредственно являются конвертами QTP; не каждому tape нужен дополнительный бинарный контейнер. TEXT/CSV/BLOB и сжатие — отдельные форматы, отсутствующие у инспектора. | BINARY/TEXT измерены; остальные форматы по исходникам |
| Типы сообщений | DESCRIBE_PROTOCOL type 1 сразу отвергает оба полных бинарных образца. RAW_DATA коннектора — ещё один исключённый тип. | Первый отказ измерен; коннектор по исходникам |
| Время | Непустой heartbeat type 0 и время FIELD/sidecar вне инспектора. Первый отказ скрывает последующие пробелы; обходов не делалось. | Измерены default heartbeat-байты; пробел инспектора выведен; sidecar по исходникам |
| Флаги | В образцах флаги нулевые; `hs` по умолчанию не делает их ненулевыми. Общее поведение флагов не поддерживается и здесь не измерено. | Нули измерены; маска по исходникам; существующий контракт |
| Схема | Обеим сторонам нужна явная локальная схема; wire ID переназначаются, поля R переставляются. XML/JSON importer и схема dxFeed не подразумеваются. | Измерены доставки FileReader |
| Типы/конверсии | Только обычный type 8; TEXT добавляет временные метаданные. Прочие wire types и конверсии требуют отдельных свидетельств. | Выбранные поля измерены; более широкий пробел выведен |
| Порядок описаний | Поздние описания R/E writer нарушают политику инспектора «только начальные описания» даже в constrained. Фактический отказ остаётся более ранним type 1. | Последовательность измерена; последующий отказ выведен из контракта |
| Политика ресурсов/ошибок | У инспектора конечные бюджеты/терминальные ошибки. Java на EOF одного файла сообщает повреждение после доставленного префикса; необязательные эвристические resync/follow — отдельное поведение. Эквивалентный совокупный бюджет Java не проверялся. | Обрезание измерено; resync/follow по исходникам; существующий контракт |

## Рекомендуемая следующая задача, не одобрение реализации

Предлагается один **явный несжатый BINARY, time NONE, EMPTY_SET, STREAM_DATA** профиль чтения файла
с точной конфигурацией constrained writer выше. Сохранять заголовок и footer; байты не предобрабатывать.
Требовать предоставленную вызывающим неизменяемую локальную схему обычного type 8, hasTime=false,
нулевые флаги и конечные бюджеты файла/кадра/описаний/записей/полей/символов/вывода. Принимать один
начальный ограниченный дескриптор `type=tape` с пустым opt и отсутствующим/NONE time, затем добавочные
описания при первом использовании и STREAM_DATA, а также пустой heartbeat/footer.
Разрешать первое объявление ID после данных, но отвергать повторное объявление, reset или замену.
Это предложение ограниченного только добавляемого отображения, не разрешение общих динамических сессий.

До реализации отдельная задача должна определить ADR для принятия дескриптора (включая неизвестные
свойства, версию только для диагностики, send sets и отсутствие time), добавочного binding и времени
жизни неизменяемых планов, а также политики EOF/завершённого префикса относительно Java batching.
Нужны узкие Java-свидетельства вариаций дескриптора, повторных/переопределённых ID, отсутствующих описаний,
неизвестных записей, обрезания заголовка/описания/данных и полных повреждённых кадров. До правок C++
определить точные лимиты, unsupported exits и политику пустого файла/footer. Использовать этот raw
constrained как положительный conformance-вход; default/TEXT оставить явно неподдерживаемыми случаями.

Исключить сжатие, sidecar, время MESSAGE/FIELD, follow/cycle/split/retention, сеть/transport, writer,
RAW_DATA/TICKER/HISTORY/subscriptions, `hs`/ненулевые флаги, resync/schemeKnown, замену описаний,
конвертеры, importer и реальные схемы событий dxFeed. Runtime-зависимости и общая поддержка tape
не одобрены. Изменений исходников/build/test C++ или инспектора в QDCXX-019 нет.

## Оставшиеся неизвестные

Побайтовая совместимость сжатия/sidecar, replay CSV/BLOB, комбинации ненулевых флагов/time-sequence,
версия в метаданных упакованного JAR, выполнение defaults коннектора, follow/recovery и предельные
ресурсы здесь не измерены. Тесты F12 — направления исследования, а не независимо выполненные проверки.
Условия решений и свидетельств приведены в [открытых вопросах](../../docs/upstream/OPEN-QUESTIONS.ru.md).
