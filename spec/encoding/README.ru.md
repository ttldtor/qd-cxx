# Спецификация кодирования

[English](README.md). Английская версия является нормативной.

Этот раздел описывает примитивные и stateful кодировки схем и QTP: канонические формы, диапазоны,
время жизни состояния, ошибочный и частичный вход.

Правила кодирования должны подтверждаться upstream source/test evidence и представительными
fixtures. Функциональный вид публичного API не означает отсутствие состояния codec.

## Перечень зависимостей QDCXX-004

Описательные **S** (исходники) / **D** (документация) наблюдения для Java QD 3.355, commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; **U** — неразрешённое покрытие. Это не полная спецификация
примитивного кодирования. Неизменяемые ссылки — в [индексе QTP](../protocol/SOURCES.md#encoding-and-core).

| Зависимость | Наблюдаемое использование и состояние | Продолжение |
| --- | --- | --- |
| Compact integers/longs | **S/D:** BufferedInput/Output и IOUtil кодируют знаковые compact numbers; framing читает long и проверяет диапазон. readCompactInt потребляет всё число, но возвращает знаковые младшие 32 бита вне диапазона int. Нельзя подменять unsigned LEB128 или универсальным отказом при overflow. | **U:** canonical writer против принимаемых overlong форм, все переходы ширины/знака и усечения; P01/P02/P08. |
| UTF strings и properties | **S/D:** UTFString использует compact byte length, -1 null, 0 empty. Reader принимает overlong UTF-8/CESU-8 surrogates по Javadoc и процедурам декодирования. Properties используют compact count и UTFString key/value pairs; readProperties проверяет compact long count. | **U:** ошибочный Unicode, duplicate/null keys, длины/ресурсные пределы и асимметрия writer/reader; P02/P08/P09. |
| Symbols/event flags | **S:** PentaCodec.ReaderImpl/WriterImpl сохраняют последний символ и MRU flags внутри сообщения; QTP сбрасывает их на сообщение. Repeat marker 0xFF требует определённого символа; 0xF8 повторяет MRU flags, 0xF9 вводит compact flags. Reader распознаёт UTF-8/CESU-8 и отвергает reserved prefixes. | **U:** полная битовая грамматика и Unicode limits; P01/P11. Простые encode/decode tests не являются oracle session state. |
| Field serial types | **S:** BinaryRecordDesc выбирает byte, UTF char, short, int, compact long, char-array string, byte-array/UTF string/serialized object readers по wire type и local field class. VOID не потребляет payload. | **U:** точная матрица object/numeric mismatches и неизвестных representations; P05/P06/P08. |
| Numeric representation | **S/D:** Биты representation SerialFieldType выбирают decimal, wide decimal, time, sequence или long; BinaryRecordDesc преобразует или повторяет local representation. FieldAdaptationTest проверяет выбранные truncation, special-value и time behaviors. | **U:** полная матрица conversion/rounding, Profile.Shares и двунаправленные requested layouts; P06. |
| Event metadata/time | **S:** Опциональные поля EventTime/EventSequence; HISTORY_ADD и REMOVE_EVENT используют compact-long time в разных payload paths. EventFlag masks зависят от message type и options. | **U:** взаимодействие с unknown records и все комбинации flags; P07/P11. |

**S:** Primitive calls могут потребить байты до EOF. Mark/reset BinaryQTPParser и проверка complete-frame
обеспечивают incremental stream behavior; primitive decoder сам этого контракта не даёт. Description
maps и PART assembly — protocol state, не primitive codec state. См.
[границы QTP](../protocol/README.md) и [обмен схемами](../protocol/SCHEMA-EXCHANGE.md).

QDCXX-006 добавляет [точные Java bytes P01](../../tests/fixtures/protocol/expected/P01.tsv): ABC repeat
marker ff внутри frame и новые 0443 bytes в следующем frame, compact-int fields и framing. Это
наблюдения composer/parser, не независимый набор primitive fixtures; upstream tests не запускались.
Сценарии отсылают к [плану captures](../../tests/fixtures/protocol/README.md); остальные решения — в
[вопросах QT](../../docs/upstream/OPEN-QUESTIONS.md#qtp-investigation-qdcxx-004).

## Ограниченные символы QDCXX-015

[SYMBOLS](SYMBOLS.ru.md) определяет реализованный reader EMPTY_SET в qdcxx_encoding с собственными
точными UTF-16/cipher/flags, message-local repeat/MRU, явным reset, бюджетами и terminal errors.
[Java coverage](../../tests/fixtures/encoding/symbols/COVERAGE.ru.md) и
[валидация C++](../../tests/conformance/encoding/symbols/VALIDATION.ru.md) заменяют прежний source-only
статус строки symbols для этого ограниченного подмножества. Нет writer, event decoder, negotiation и транспорта.
