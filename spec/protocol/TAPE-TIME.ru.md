# Явные метаданные времени события FIELD

[English version](TAPE-TIME.md). Английская версия нормативная.

Библиотека протокола разделяет физические слова FIELD, обычные бизнес-поля, индексное время записи
и состояние heartbeat MESSAGE. Этот контракт расширяет конечную
[политику бинарных сообщений](BINARY-MESSAGES.ru.md). Новый формат инспектора или политика replay
из него не следуют.

## Выбор и совместимость

Прежние конструкторы и `StreamDataContext::create` выбирают `FieldTimePolicy::DISABLED`.
Привязка NONE/MESSAGE, обработка удаления и байты вывода сохраняют прежний смысл;
дескриптор FIELD по-прежнему получает `UNSUPPORTED/TIME`.

READ выбирается явно через
`TapeReader(schema, limits, TapeMode::BINARY_CATALOG_V1, FieldTimePolicy::READ)`,
`TapeReader(limits, TapeDiscovery::BINARY_CATALOG_V1, FieldTimePolicy::READ)` или
`StreamDataContext::createWithFieldTimePolicy(schema, descriptions, limits, FieldTimePolicy::READ)`.
Прежняя фабрика с тремя аргументами остаётся однозначной, включая взятие адреса функции.
READ требует BINARY_FIELDS_V1, а для TapeReader — BINARY_CATALOG_V1. Недопустимая policy и
сочетание старого режима с READ отклоняются до входных данных и строительства схемы. Само создание
диагностического исключения может выделять память. Фабрика контекста возвращает
`UNSUPPORTED_PROFILE` для недопустимой policy.

READ допускает точные FIELD/field вместе с прежними NONE/none/MESSAGE/message в дескрипторе.
Префикс распознаётся независимо от текста дескриптора, включая headerless, NONE и MESSAGE.
Это соответствует явному включению production parser `readEventTimeSequence`; текст дескриптора
не включает возможность скрыто. Ambient configuration и расширенное сравнение регистра не добавлены.

## Точное распознавание и привязка

Распознавание рассматривает исходное физическое описание до исключения полей VOID нулевой ширины:

1. Первое поле должно называться ровно EventTime и иметь полный тип56.
2. Только следующее непосредственно за ним исходное EventSequence/full72 становится вторым словом.

Префикс из одного поля допустим. Неверные полные типы, aliases, другой регистр, перестановка и поздние
вхождения остаются обычными действиями точного типа. VOID перед EventTime исключает распознавание;
VOID между EventTime и EventSequence оставляет только первое слово. Имена не определяют indexed layout.

Общий planner применяет приоритет виртуального префикса до поиска бизнес-полей, включая каждую
промежуточную замену, которую позже перезапишут в том же пакете. Совпавшее по имени бизнес-поле
переданной схемы остаётся в типизированном raw-zero default, если другое обычное действие его не
записывает. Discovery сохраняет точные исходные описания и действия, исключает только распознанные
метаданные из обычных локальных полей и сохраняет накапливаемый реестр имя/полный тип между поколениями.
Самостоятельный `bindRecord` сохраняет прежнюю обычную привязку.

`StreamRecordPlan::eventTimeFieldCount` равен0..2.
`removalPrefixFieldCount` — отдельная исключающая верхняя граница действий: после фактического
исключения VOID все ведущие имена с началом Event входят в REMOVE-префикс. Явные VOID сохраняют
затраты wire-field нулевой ширины и не завершают префикс. REMOVE читает весь префикс существующими
действиями точного типа, затем compact-long removalTime. Ведущий бизнес EventPrice или EventSequence
неверного типа сохраняет переданное значение; обычные поля после префикса получают defaults.
Явно доказанный вызывающей стороной indexed layout получает removalTime через прежнее отдельное
отображение.

## Владение и проекции

`StreamDataRecord::fieldTime` — владеющий optional `FieldEventTime`. Отсутствие префикса, переданный
ноль, отсутствие sequence и явный нулевой sequence различаются. `rawSeconds` и optional `rawSequence`
сохраняют полные знаковые compact-long до production Java-преобразования в signed32.
Указатель на источник, callback и дополнительное shared ownership не сохраняются.

`packed()` объединяет low32(rawSeconds) как старшее слово и low32(rawSequence или0) как младшее
через беззнаковую арифметику и bit-cast. `eventTimeMs()` расширяет signed32 seconds до умножения
на1000 и добавляет беззнаковые биты31..22 sequence. Значения millisecond subfield1000..1023 сохраняются
точно; предполагаемая нормализация, насыщение и отказ не добавлены. Крайние signed64 допустимы.

`sourceOrdinal` — номер попытки исходной wire-записи от нуля, включая описанные неизвестные записи,
которые пропускаются. Он продолжается между всеми четырьмя data families и ошибочными попытками,
независимо от callback fanout и индекса выданных записей. Самостоятельный decoder начинает с0;
TapeReader прибавляет проверенную предыдущую работу перед возвратом завершённых записей.
Переполнения счётчиков и арифметики ordinal не допускаются.

`TapeFeedResult::timing` остаётся отдельным владеющим состоянием MESSAGE/heartbeat с прежними
правилами насыщения. Production cursor использует ненулевой внешний MESSAGE packed time вместо
любого FIELD; ноль оставляет FIELD packing. Исходные метаданные FIELD в C++ сохраняются.
Приложение может применить эту проекцию без дополнительной копии времени в каждой записи.
Планирование replay не меняется.

## Границы, ошибки и публикация

Прежние primitive, wire-field, processed-record и payload budgets действуют и для пропущенных
метаданных. READ добавляет две логические snapshot/construction units на план, включая каждую
промежуточную валидацию, с учётом пика old/candidate/temporary. Отказ публикации сохраняет прежние
схему, описания, контекст и адреса borrowed views; принятая работа строительства не возвращается.
Успешная публикация сохраняет установленную границу инвалидирования views.

`StreamDataLimits::ownedFieldTimeRecords`, по умолчанию0, ограничивает владельцев завершённых и
текущих метаданных. Engaged output стоит одну фиксированную unit; mapped output проверяет её на
OUTPUT до dense defaults и выходного владения. Пропуск неизвестной записи расходует primitive/work
allowances без выходной metadata charge. `StreamDataResult::ownedFieldTimeRecords` сообщает
завершённых владельцев; TapeReader накапливает их между возвращёнными families без возврата.
Пустые кадры и заголовки не требуют metadata owner.

Незавершённая текущая запись не публикует частичные поля или метаданные. Готовый префикс сохраняется
при последующих primitive, budget и allocation failures. Ошибки полей сохраняют FIELD stage,
offset и codec diagnostics; removal time — отдельный REMOVAL_TIME stage. Владеющие контексты и
результаты переживают уничтожение источника/схемы/input/reader, перемещение и замену.
Пустые timed messages и heartbeat не подтверждают EOF.

Production Java выбрасывает NullPointerException в измеренном случае неизвестного consumer с
распознанными FIELD actions. C++ намеренно сохраняет прежний ограниченный безопасный пропуск.
Эта более сильная гарантия не называется Java callback parity.

## Представление и доказательства

Все прежние formatter entrypoints отклоняют любые engaged FIELD metadata, включая ноль, до
строительства строки и обработки output budget. Старые версии вывода не теряют эти слова молча.
Прежний tape-write использует DISABLED; его record/view bridge также отклоняет engaged FIELD.
Поле OutgoingRecordView, writer grammar и CLI-выбор FIELD не добавлены.

Сопровождаемый [production oracle](../../tools/upstream/field-types-oracle/README.ru.md) разделяет
сконструированные физические входы и фактические наблюдения FileWriter/FileReader/parser/composer.
[Отчёт валидации](../../tests/conformance/protocol/field-types/FIELD-TIME-VALIDATION.ru.md) различает
исполненные producer pins, исходные записи, callback projections и native checks. Прежние
NONE/MESSAGE captures остаются неизменными.
