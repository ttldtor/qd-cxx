# Покрытие C++ default Quote

[English version](COVERAGE.md). Английский документ является основным.

[Java-доказательства](../../../fixtures/protocol/quote/COVERAGE.ru.md) предшествовали контрактам
[значений](../../../../spec/encoding/QUOTE-VALUES.ru.md) и
[отображения/tape](../../../../spec/protocol/DEFAULT-QUOTE.ru.md), затем реализации.
Исходный production tape размером 326 байт неизменен. Другие 40 tapes — диагностические входы,
проигранные production Java, отдельно от настоящего QuoteDelegate/FileWriter round trip.

| Набор | Границы |
| --- | --- |
| quote_java_values | Все строгие primitive rows: 14 UTF_CHAR writes, 27 reads/positions, пять time bounds, 19 raw decimals и 13 composition observations. Ёмкость/атомарность writer, raw compact-long identity, точные text/double bits, NaN/infinities/zero, отдельные decimal boundaries. C++ decimal composition не реализована. |
| quote_java_tape | Все 41 неизменный Java tape целиком, по одному и по семь байтов. Сравниваются каждый символ в порядке, tag/raw поля, decimal text/bits и mapped Quote row, включая missing/reordered/duplicate/unknown и наблюдаемые потери. Строгая проверка case/arity/order/count/completion. |
| quote_java_tape, спроектированные случаи | Явные локальные профили; неподдержанные полные типы и mismatch известного поля; точные supported skips; повторные цели; отсутствующий decimal NaN; compact low-32 projection; обрезанные поля и позиции неверного UTF_CHAR; completed-prefix retention; владение после уничтожения input/schema/reader; границы shape/range/symbol отображения. |
| quote_java_tape, lifecycle/budgets | Точные/нулевые/недостаточные record/field/value/symbol и input/construction bounds; все строгие префиксы исходного tape; поздние descriptions; повторные ID; неуспешный update нескольких descriptions ничего не публикует; накопленные values между сообщениями; новые symbols на сообщение и terminal failure. Прежние tape/schema тесты сохраняют более широкую матрицу границ. |
| quote_inspection_cli | Исходный real tape сравнивается с Java raw/mapped rows; chunk schedules; эквивалентность no-argument/help, неверные/неполные аргументы и версии диагностики; точные/недостаточные TSV/human output, record/value/symbol budgets; явный новый профиль и opt-in диагностики старого. |
| quote_inspection_io | Безопасное точное экранирование symbol/exchange; дробные/особые decimal; точный бюджет пары выходных строк; внедрённые read/write/flush failures; ограниченные причины TIME/OPTIONS/MESSAGE/TYPE/SEQUENCE с сохранением числовых позиций и прежней диагностики. |
| 13 прежних наборов | Прежние compact/UTFString/framing/descriptions/binding/symbol/STREAM_DATA/tape и регрессии обоих инспекторов остаются включены. Старые профили продолжают отклонять новые типы без явного выбора. |
| [Test-Negatives.ps1](Test-Negatives.ps1) | Девять мутаций копий для C++ consumer: full type, raw value, record ID, field order, row arity, mapped bits, missing completion, unknown row, binary byte. Повтор исходного baseline. У Java comparator — отдельные девять мутаций и полный negative/baseline Verify. |

После ревью добавлены восемь независимых подмен тегов mapQuote: каждый корректный слот заменяется
на поддерживаемый COMPACT_INT и должен вернуть INVALID_SHAPE. Прежние проверки формы/диапазонов сохранены.
IO-набор теперь проверяет DESCRIPTOR для type=xape и OPTIONS для ненулевых raw event flags
с точными строками диагностики/позициями в версиях 1/2 и TSV/human. Прежнее покрытие OPTIONS для
непустого opt сохранено. Новые выполненные проверки отделены от исходной матрицы.

IO-набор также отклоняет несовместимые профили схемы/вывода и неверные имя/типы Quote до чтения
входа или записи stdout с диагностикой LOCAL_PROFILE.

Resource policies, immutable contexts, явное отклонение несовпадающих типов и completed-prefix
являются C++ контрактами, а не заявлением совпадения Java exceptions/batching. Raw decimal zero
означает NaN; знак конечного нуля и все не-NaN binary64 сравниваются точно. Неизвестные поддержанные
записи потребляют каждое поле со списанием field/work, но не выделяют выходные значения.

Не охвачены общий tape/RAW_DATA/время MESSAGE, загрузка внешней схемы, regional events,
преобразования, dynamic replacement, recovery, сжатие, транспорт, производительность, allocator
injection, 32-bit и исчерпывающее Unicode/decimal покрытие. [Выполненные проверки и ограничения](VALIDATION.ru.md).
