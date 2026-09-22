# Покрытие Java-доказательств стандартного Quote

[English version](COVERAGE.md). Английская версия является основной.

Закреплённый QD 3.355 (`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`), полная встроенная
DXFeedScheme без переопределения свойств, прямой production-путь файла. Этот конечный набор
не доказывает исчерпывающую совместимость decimal или Unicode. Хеши исходников и фактических
команд приведены в [захватах](CAPTURES.ru.md).

| Доказательство | Наблюдаемое покрытие |
| --- | --- |
| [schema.tsv](expected/schema.tsv) | Все 331 стандартные записи, точные локальные имена/property aliases/полные типы, int- и object-слоты. Composite Quote имеет ID 0, 12 int-слотов, нет object-слотов, hasTime=false. Восемь логических полей: 56/2/120/120/56/2/120/120; хвосты long остаются локальными VOID-слотами. Региональные записи сохранены в схеме. |
| [default-quote.qtp](expected/default-quote.qtp), [inputs.tsv](expected/inputs.tsv), [put.tsv](expected/put.tsv), [default-quote.tsv](expected/default-quote.tsv) | Пять реальных объектов Quote: IBM/MSFT/IBM/SPECIAL/IBM, исходные байты FileWriter, raw-результат delegate и доставки FileReader, все поля и double bits. BINARY/NONE/EMPTY_SET/STREAM_DATA; flags и event-time metadata нулевые. |
| [primitives.tsv](expected/primitives.tsv) | 14 записей UTF_CHAR, 27 чтений с consumed positions; пять границ signed TIME_SECONDS; 19 raw compact-long/decimal случаев и 13 случаев composition из double. |
| char-1 — char-11 | Приём production field/parser обычных байтов, overlong форм из 2/3/4 байт, изолированных суррогатов, U+FFFF, U+10000 и U+10FFFF. Raw int сохранён; exchange Quote использует младшие 16 бит. |
| time--2147483648 — time-2147483647 | Пять production parser/mapping случаев: секунды -2147483648, -1, 0, 1, 2147483647 становятся точными int64-миллисекундами. |
| decimal-0 — decimal-18 | Сохранение raw production parser и фактический Quote mapping для NaN, канонического/неканонического нуля, знаковых канонических/неканонических бесконечностей, +/-1, крайних rank, границ signed int64 и significand около 53-битной границы. |
| [missing.tsv](expected/missing.tsv) | Отсутствующие входящие поля получают raw-ноль. Для price/size raw-ноль означает NaN, для time/exchange — ноль. |
| [duplicate.tsv](expected/duplicate.tsv) | Две точные цели BidPrice: побеждает позднее raw 640, отображённое значение 2. |
| [reordered.tsv](expected/reordered.tsv) | Входящие поля в обратном порядке привязываются к восьми обычным локальным целям. |
| [unknown-fields.tsv](expected/unknown-fields.tsv), [unknown-record.tsv](expected/unknown-record.tsv) | Неизвестные поддерживаемые поля 56/2/120 потребляются перед известным значением; неизвестная запись пропускается без доставок и ошибок повреждения. |

Исходный production tape доказывает сквозной путь реального события. Все остальные QTP-пробы —
диагностически построенные сообщения для production FileReader; это не наблюдения FileWriter.
Точечное чтение символа выполняется через `DataIntField.readInt(BufferedInput)`, реализованное
`CompactCharField` и `BufferedInput.readUTFChar`. Запись проверяет соответствующее production-поле.
Проверки повреждённых/обрезанных символов относятся к отдельному примитиву, а не callback целого файла.

Наблюдаемые потери явные: стандартные optional Sequence/TimeNanoPart отсутствуют. Входные bid/ask
миллисекунды .123/.456, sequence 17 и nano part 987654 возвращаются как целые секунды и нулевые
sequence/nano. Decimal -0.0 возвращается каноническим +0.0; Double.MIN_VALUE преобразуется в decimal
ноль, Double.MAX_VALUE — в положительную бесконечность. Вход 1.2345678901234567 округляется до
decimal 1.234567890123457 и меняет выходной IEEE double на один бит. Это наблюдения Java, а не
идеализированное равенство входу.

Raw wide сохраняет все 64 бита. Семантика Java decimal использует signed significand `raw >> 8`,
rank `raw & 255` и конечный exponent `128 - rank`; rank ноль классифицирует NaN/бесконечность по
знаку significand. Java `WideDecimal.toDouble` выполняет корректно округлённое преобразование,
включая переход через decimal-строку за пределами точной арифметики. Fixture хранит написание
Java и точные канонические NaN double bits (`Double.doubleToLongBits`); payload NaN входного double
не сохраняется.

UTF_CHAR utility принимает U+0000..U+10FFFF, включая изолированные суррогаты и неканонические
кодировки. Значения выше U+10FFFF, неверные начальные/продолжающие байты и неполные хвосты
отклоняются. Сохранённая позиция включает все потреблённые примитивом байты; ветви 3/4 байт
сначала читают хвост целиком, затем проверяют continuation bits, если байтов достаточно.
UTFString имеет отдельный контракт.

Не доказаны матрица преобразований, реализация decimal composition в C++, publisher endpoint,
mapping региональных событий, RAW_DATA/MESSAGE, сжатие, восстановление, транспорт или поддержка
личного tape. Прежние tape/symbol/encoding fixtures остаются отдельными регрессионными
доказательствами. См. отдельно определённые [контракт значений](../../../../spec/encoding/QUOTE-VALUES.ru.md)
и [профиль стандартного Quote](../../../../spec/protocol/DEFAULT-QUOTE.ru.md).
