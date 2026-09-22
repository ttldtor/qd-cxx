# Default composite Quote через ограниченный tape reader

[English version](DEFAULT-QUOTE.md). Английский документ является основным.

Контракт QDCXX-022 записан после первого Java runtime capture и до реализации C++.
Исходный HEAD: fa5a0b967e9c021e656846cdd3d6faee5faa4776. Полная стандартная Java DXFeedScheme содержит
331 запись; выбранный composite Quote имеет ID 0, hasTime=false, 12 int-слотов и ни одного object-слота.
Production event/delegate/file round trip и отдельные проверки parser находятся в
[доказательствах Quote](../../tests/fixtures/protocol/quote/COVERAGE.ru.md). Региональные записи
сохраняются в Java-схеме, но не входят в этот C++ adapter. Исходный production tape читается без изменений.

## Локальный адаптер и отображение

Небольшой повторно используемый компонент qdcxx_quote зависит только от core, отдельно от protocol,
encoding IO и CLI. defaultQuoteSchema(limits) возвращает собственную проверенную схему с
LocalProfile::QUOTE_FIELDS_V1, одной записью Quote, локальным ID 0 и восемью логическими полями
в следующем порядке:

| Локальное имя | Исходящее property name | Полный тип | Java int head |
| --- | --- | --- | --- |
| Bid.Time | BidTime | 56 | 0 |
| Bid.Exchange | BidExchangeCode | 2 | 1 |
| Bid.Price | BidPrice | 120 | 2 |
| Bid.Size | BidSize | 120 | 4 |
| Ask.Time | AskTime | 56 | 6 |
| Ask.Exchange | AskExchangeCode | 2 | 7 |
| Ask.Price | AskPrice | 120 | 8 |
| Ask.Size | AskSize | 120 | 10 |

У каждого поля явно заданы полное локальное имя с Quote., локальное имя и исходящее property lookup.
Нет угаданных aliases, Java VOID tails, внешнего импортера или иерархии событий. Входной sender ID
не зависит от локального ID. Отсутствующие поля, повторы, skips и значения следуют
[QUOTE-VALUES](../encoding/QUOTE-VALUES.ru.md). Все восемь включённых полей сохраняются в локальной схеме.

mapQuote(symbol, fields, symbolUnits) проверяет восемь точных тегов/диапазонов, явный бюджет копирования
символа и возвращает владеющий данными предварительный Quote или структурированную ошибку
shape/resource/allocation. Сохраняются четыре WideDecimal raw с представлениями toDouble/toString,
два точных времени в миллисекундах и два кода биржи из младших 16 бит. timeMs равен
max(bidTimeMs, askTimeMs); eventTime, sequence и timeNanoPart равны нулю. Результат не зависит от
времени жизни decoder или файла. Default Java scheme теряет subsecond/sequence/nano до записи;
измеренная decimal composition может округлять входной double и преобразует negative zero в
positive zero. C++ сравнивается с наблюдаемым Java replay, а не идеализированным исходным Quote.

## Профили tape и вывода

Транспорт tape остаётся точно в границах [TAPE-READER](TAPE-READER.ru.md): BINARY, без сжатия,
время NONE, явный EMPTY_SET, STREAM_DATA и нулевые raw flags. Типы выбираются проверенным
локальным профилем отдельно от транспорта. Прежние oracle-sre-v1 и QTP inspector сохраняют
принимаемые данные, значения по умолчанию, ошибки и байты вывода.

qdcxx-tape-inspect добавляет явный --profile default-quote-v1. Заголовок TSV по умолчанию:
`format<TAB>qdcxx-tape-inspect<TAB>2<TAB>default-quote-v1`. Грамматика строк frame, descriptor,
markers и complete сохраняется. Новые строки `typed-record` содержат индекс frame, выходной индекс
записи, local ID, локальное имя в UTF-16 hex, символ в UTF-16 hex и разделённые запятыми пары
`type:raw` в локальном порядке. Raw — каноническое знаковое десятичное число, type — полный ID.
Следующая строка `quote` содержит выходной индекс, bidTimeMs, bidExchangeCode беззнаковым десятичным,
биты bidPrice double, биты bidSize, askTimeMs, askExchangeCode, биты askPrice, биты askSize, timeMs,
sequence, timeNanoPart, eventTime. Биты — 16 строчных hex-цифр; NaN имеет канонические quiet NaN
биты. Raw поля сохраняют неканонические представления NaN/нуля/бесконечности. Это отдельная версия
представления значений без потерь, а не новый формат tape.

Human-заголовок указывает version 2/default-quote-v1. Запись экранирует символы/имена в кавычках,
называет восемь полей, показывает время в миллисекундах с единицами, биржу как экранированный UTF-16,
цены/размеры — десятичным текстом вместе с точным type/raw. Применяется прежнее безопасное
экранирование терминала. До динамического выделения выходной строки считается её полный размер;
временный decimal text ограничен 32 байтами. Общие бюджеты output/record/value/symbol обязательны.

## Структурированные причины отклонения

TapeRejection и TapeDiagnostic принадлежат qdcxx_protocol и содержат только данные отказа.
Функции статического имени/пояснения tapeRejectionName и tapeRejectionExplanation находятся в
tools/tape-inspect/diagnostics.hpp, в qdcxx_tape_inspect_tool. Имена CLI-профилей, советы по выбору
и сериализованное/human представление остаются в утилите; библиотека протокола от них не зависит.
Эта граница представления не меняет идентичности причин, версии форматов, байты или позиции.

TapeDiagnostic получает ограниченную статическую причину TapeRejection: NONE, TIME, OPTIONS,
MESSAGE, TYPE, LOCAL_PROFILE, DESCRIPTOR или SEQUENCE. Недоверенного текста в ней нет. Числовые
error/detail/stage и все точные позиции сохраняют прежний смысл. Диагностика version 2 использует
`incomplete-v2`, прежние восемь data cells (без старого имени строки) и финальное имя причины.
Human version 2 дополнительно даёт фиксированное понятное объяснение; входные строки никогда не
подставляются без экранирования. Диагностика прежнего профиля побайтно сохраняется, включая human.
Неизвестный local profile отклоняется до открытия файла; явный --diagnostics-version 2 выбирает
объяснение даже при ошибке выбора профиля. Этот opt-in доступен и с oracle-sre-v1; без него прежняя
диагностика точна. Позиции не заменяются текстом. Новый профиль по умолчанию использует version 2;
явная version 1 сохраняет прежнюю грамматику. Неизвестные версии — usage error.

API потоковой инспекции также отклоняет несовместимые профили схемы/вывода до чтения входа или
записи stdout: exit 4, stage profile, причина LOCAL_PROFILE в диагностике версии 2. Новый вывод
требует QUOTE_FIELDS_V1 с одной записью Quote и её восемью точными типами по порядку; прежние
форматы требуют COMPACT_INT_V1. Вызывающая сторона отвечает за явные lookup-связи своей схемы.

## Границы и проверки

Новые типы сохраняют offsets начала примитива, фактическое число прочитанных байтов, terminal failure,
завершённые префиксы записей, новые symbols на сообщение и атомарные снимки descriptions.
Context копирует один типизированный defaults-набор на локальную запись после проверки бюджетов;
construction учитывает эти логические поля прежними списаниями работы со схемой при каждом rebuild.
Неизвестные записи не выделяют выходные поля. Typed и legacy векторы взаимоисключающие, считаются один раз.

Приёмка сравнивает порядок реальных символов, все восемь raw-полей, точный decimal text и binary64
биты, отображённые timestamps/exchanges/потери, дополняя это синтетическими edge/budget/lifetime
тестами. [Валидация](../../tests/conformance/protocol/quote/VALIDATION.ru.md) фиксирует только выполненные
проверки; независимое ревью отдельно ожидается. RAW_DATA, время MESSAGE, общий real tape,
regional Quote, другие события, сжатие, importer, dynamic replacement и transport не заявляются.
