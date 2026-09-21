# Ограниченный reader символов QTP

[English](SYMBOLS.md). Английская версия является нормативной.

QDCXX-015 реализует один message-local reader PentaCodec в qd_encoding, без зависимостей от qd_core,
qd_protocol, схем, binding, доставки событий и транспорта. Предварительный API возвращает собственные
точные UTF-16 и исходные метаданные кодирования; нормализации и преобразования в UTF-8 нет.

## Доказательства и профиль

Oracle: QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
[Входы](../../tests/fixtures/encoding/symbols/inputs.tsv) и
[наблюдения Java](../../tests/fixtures/encoding/symbols/expected/symbols.tsv) разделяют канонические
байты writer, дополнительные формы reader, строгие префиксы, состояние после ошибок и reset.
[Production integration](../../tools/upstream/symbol-oracle/SymbolOracle.java) компонует/разбирает
семь символов в двух сообщениях. Используются реальные callbacks, не диагностический обход.
Исторический P01 сохранён. Первый чистый capture выполнен до этого контракта и реализации C++.

Закреплённые исходники: [PentaCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java)
ReaderImpl/WriterImpl, encodePenta, pentaToString и статическая таблица символов;
[SymbolCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SymbolCodec.java)
Reader/Writer reset, контракт cipher-symbol;
[BufferedInput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedInput.java)
readUTFChar/readUTFBody/readUTF2/3/4; [исходники QTP](../protocol/SOURCES.md) указывают reset в
parseData/parseSubscription parser и writeMessageHeader composer, а также PentaCodecTest.
Эти upstream tests изучены, но не запускались.

C++ API принимает только нулевую маску опций (EMPTY_SET). Иная маска даёт UNSUPPORTED_PROFILE.
Java reader игнорирует набор опций; production parser вызывает reset с SUPPORTED_SET, integration
composer явно использует EMPTY_SET. Формы wire, зависящие от опций, не переинтерпретируются молча:
в закреплённом codec таких форм нет. Исходные знаковые флаги сохраняются даже там, где сообщение/профиль
применяло бы маску; семантика EventFlag/сообщений и negotiation не реализованы.

## Грамматика

| Первый байт | Остальное кодирование |
| --- | --- |
| 00..7f | Один байт: 15-битная big-endian penta |
| 80..bf | Три байта: 30-битная penta |
| c0..df, fa, fb | Зарезервировано; ошибка после первого байта |
| e0..ef | Два байта: 20-битная penta |
| f0..f7 | Четыре байта: 35-битная penta |
| f8 | MRU flags, затем символ |
| f9 | Compact int flags (знаковые младшие 32 бита), затем символ |
| fc | Compact long длина в байтах 0..INT_MAX, затем тело QD UTF |
| fd | Compact long число UTF-16 units 0..INT_MAX, затем столько значений readUTFChar |
| fe | Пустая penta |
| ff | Предыдущий символ; неопределённый после reset вызывает ошибку |

Группы penta декодируются от старшей ненулевой 5-битной группы вниз. 1..26 — A..Z, 27 '.',
28 '/', 29 '$'. 30/31 вместе со следующей группой образуют 10-битный код 960..1023: ASCII
32..126 по возрастанию, исключая A..Z, '.', '/', '$', апостроф и обратную кавычку. Нулевые группы
и конечные 30/31 дают NUL по наблюдениям/исходникам; начальные нули исчезают. Неканонические ширины
принимаются. Penta до 30 бит имеет cipher penta+0x40000000; 35-битное значение с начальным '/' или
'$' имеет старшие биты cipher 10/11 и младшие 30 бит penta; остальные 35-битные значения имеют cipher
ноль и собственную декодированную строку. Cipher ноль никогда не означает пустой декодированный
символ. FC/FD сохраняют cipher ноль даже для кодируемого текста; FE имеет cipher 0x40000000 и пустой UTF-16.

FC использует семантику тела UTFString, включая overlong, изолированные суррогаты, supplementary
pairs и чтение за объявленную длину до ошибки в пределах span. Его длина не допускает null и
ограничена INT_MAX, в отличие от UTFString. FD считает units: каждый readUTFChar занимает 1..4 байта,
принимает overlong/суррогаты, но отвергает декодированную точку выше FFFF после её полных байтов.
Канонический writer использует FD для некодируемых строк, записывая половины суррогатных пар отдельно;
FC доступен только reader. Null cipher/string отвергается Java writer.

## Состояние и результаты

SymbolReader создаётся с обязательными SymbolLimits {inputBytes, codeUnits, stateUnits} и явной
маской опций. beginMessage(mask) сбрасывает символ, MRU=1 и terminal status. Вызывающая сторона
сбрасывает reader между полными сообщениями, не записями/chunks. Вход read — заимствованный оставшийся
span внутри одного полного payload. Читается ровно один символ; хвост остаётся вызывающей стороне.

Каждый вызов начинает с flags=0 и flagBytes=0. F8 берёт MRU и возвращает flagBytes=1. F9 обновляет MRU
и возвращает число байтов compact integer без F9 (поведение Java API). Второй префикс флагов вызывает
ошибку после его чтения. Без префикса MRU не меняется, флаги результата нулевые. Успешный FF сохраняет
символ/cipher, но получает новые флаги текущего вызова.

Результат владеет optional SymbolValue {units, cipher, eventFlags, eventFlagsBytes}. Значение есть
только при OK; consumed считает просмотренные байты вызова. Результаты сохраняются после следующих
чтений, reset и уничтожения reader. Нет resolver, общего кеша, глобального состояния или автоматического
определения границ сообщений. Использование однопоточное и нерекурсивное.

C++ намеренно публикует состояние только при полном успехе и делает любую ошибку terminal до
beginMessage. Следующий read возвращает FAILED с нулевым consumption; reset снимает ошибку. Неподдерживаемая
конфигурация продолжает возвращать UNSUPPORTED_PROFILE до beginMessage с поддерживаемым профилем. Java
может сохранить прежний символ и уже обновить MRU при ошибке, что показывают after-repeat/after-mru.
Эти строки — доказательства, не ожидаемое продолжение C++. Атомарная terminal-политика разрешена
QDCXX-015 и не заявляется эквивалентом Java primitive или production recovery.

## Ресурсы и приоритет ошибок

Бюджеты — конечные size_t счётчики логических элементов, без безлимитных defaults/sentinel;
ноль допустим. inputBytes ограничивает переданный оставшийся span, codeUnits — результат,
stateUnits — сохранённый символ. Эффективный предел units равен min(codeUnits,stateUnits), поскольку
успех сохраняет один символ. Результаты у вызывающей стороны не входят в состояние. Одновременно
могут существовать прежнее состояние, пробный символ и копия результата, каждый ограничен;
capacity контейнеров/накладные расходы allocator не являются точной гарантией heap bytes.

1. Terminal/profile status предшествует бюджету входа; превышение span даёт RESOURCE_LIMIT в нуле.
2. Читаются префикс и необходимые fixed/compact bytes. Усечение потребляет доступный span и даёт
   TRUNCATED_INPUT. Reserved, повтор флагов, неопределённый repeat и ошибочный текст дают
   INVALID_ENCODING в измеренных позициях; недопустимые длины — INVALID_LENGTH.
3. Длина FC/FD проверяется на 0..INT_MAX до выделений по счётчику. Длина FC выше inputBytes и число
   units FD выше эффективного предела дают RESOURCE_LIMIT после длины. Reserve по недоверенному
   счётчику нет. FC делегирует bounded UTFString лишь после более строгой проверки длины.
4. Append penta/FD проверяет эффективный предел до роста. UTFString проверяет бюджет units после
   чтения символа и до проверки выхода за объявленное тело. Repeat проверяет предел до копирования.
   Это ресурсные приоритеты C++, не wire limits Java.
5. bad_alloc/length_error преобразуются в ALLOCATION_FAILURE без частичного результата/состояния.
   Consumed — последняя зафиксированная позиция; бросивший исключение вызов UTFString оставляет её
   у начала префикса длины. Инъекция отказов аллокации не заявляется.

Чтение не пересекает span. Неполный символ полного сообщения не может заимствовать байты другого
frame; terminal state запрещает retry без явного reset. Нет writer, event decoder, установки
incoming-ID, преобразований, importer, time metadata, PART, транспорта или гарантий производительности.
Парные [покрытие](../../tests/conformance/encoding/symbols/COVERAGE.md) и
[валидация](../../tests/conformance/encoding/symbols/VALIDATION.md) указывают выполненные проверки и пределы.
