# ADR-0004 — Конечная цель общего чтения бинарных tape

[English version](ADR-0004-broad-binary-tape-reading.md). Английская версия нормативная.

Статус: выбранное техническое направление для итогового ревью контракта. Реализация и приёмка
полного каталога ожидаются. Решение расширяет общее чтение wire, не Java business semantics.

## Контекст

Существующие ordinary profiles намеренно принимают точные field identities и небольшой subset
messages, flags и descriptors. Production tapes могут содержать дополнительные physical field
encodings, индексные local layouts, ненулевые flags и другие data-message families. Структурный
обход до EOF не доказывает value/order/time parity и не обосновывает поддержку всех Java QD inputs.

Эталоны: QD3.355, `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, и QD3.356,
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Отдельные
[producer fixtures](../../tests/fixtures/protocol/real-tape/producer/README.ru.md) и
[проверка](../../tests/conformance/protocol/real-tape/producer/VALIDATION.ru.md) характеризуют три
конкретных production cases и производное усечение для каждого commit. Они не доказывают ожидаемые
расширенные строки. Исходный lock и исторические fixtures сохраняются.

## Решение

1. Задать замкнутую версионированную generic-wire цель в
   [BINARY-TAPE-COVERAGE](../../spec/protocol/BINARY-TAPE-COVERAGE.ru.md) и её
   [матрице](../../tests/fixtures/protocol/real-tape/support-matrix.tsv).
   Сохранять точные source identities, canonical raw values, UTF-16 units/opaque bytes, symbols,
   flags, source family и именованные time metadata. Сохранять generic custom records и extra fields.
   Каждой применимой строке нужны pinned source и фактические production/C++ positive/negative evidence.
2. Добавить явный core field profile `BINARY_FIELDS_V1` и отдельный protocol mode
   `BINARY_CATALOG_V1`; окончательный API spelling проходит review до реализации. Прежние profiles,
   ordinary discovery, admission tokens, default modes и serialized output не меняются.
   Core независим от protocol modes, транспорта, file I/O и ambient configuration.
3. Перечислить 19 candidate full field identities и отдельно reserved TIME_NANOS168. Physical widths,
   compact-int projection, long storage и length/UTF grammar требуют реализации. Общие serialization
   bits не разрешают неизвестный full identity. Не добавлять Java decimal/int/wide/shares/time-unit
   conversions только ради соответствия локальному consumer.
4. Добавить owned nullable bytes для BYTE_ARRAY9, CUSTOM_OBJECT233 и SERIAL_OBJECT249; STRING137
   использует точные nullable UTF-16 units. Сохранять marshalled serialization bytes без исполнения
   объектов и загрузки Java classes. Сохранять различие compact-int marshalled length и compact-long
   byte-array length. Неизвестное содержимое objects непрозрачно и не означает semantic deserialization
   support. Исполнение legacy PlainObjField исключено.
5. Требовать явный caller/source-proven indexed layout. В DESCRIBE_RECORDS нет hasTime bit.
   Характеризовать pair-int32 и single-int64 time layouts через production consumers до приёмки;
   не выводить indexed/business semantics из record names. Physical VOID tails long не являются
   logical values; только реально присутствующее явное wire VOID-поле может давать raw zero.
6. Новый mode поддерживает проверенные immutable generations для замены active sender descriptions.
   Полностью собрать и проверить sparse candidate до атомарной публикации. Сохранённые records и
   context identities переживают новые generations; borrowed views имеют явные точки invalidation.
   Хранить только ограниченное текущее состояние, без неограниченной description history. Ошибка
   сохраняет прежнее поколение; admitted construction work не возвращается. Ordinary discovery
   сохраняет строгий отказ при repeated ID. Equal/redefined IDs и failed replacements ожидают QDCXX-043.
7. Для независимого закрытого source явно создавать новый reader. In-place reset ожидается только
   при production-потребности и review полного state/budget contract. Header/footer не сбрасывают
   состояние автоматически; reset/source changes не обходят cumulative bounds.
8. Декодировать RAW/TICKER/STREAM/HISTORY data как generic wire records, сохраняя family и wire order.
   Сохранять шесть известных data flag bits и специальный compact-long record-time payload REMOVE_EVENT.
   Прежний SymbolValue владеет flags; добавить в record только optional owned removalTime. Это отдельно
   от будущих FIELD event-time metadata. Парсинг полей/flags не добавляет collector или transaction/
   snapshot scheduler. PART, subscription и RMI payload operations явно не поддерживаются этой целью.
9. Характеризовать descriptor tails/properties/deltas и self-described binary без заголовка до
   включения точной ограниченной политики в QDCXX-045. Headerless требует реального положительного
   FileWriter case, а не одной permissiveness parser. Явно отвергать schemeKnown/readAs/resync/ignore/
   continue/cycle alternatives. Допустимая Java alternative вне профиля — unsupported; malformed input
   терминален с документированными completed prefix и coordinates.
10. FIELD time, sidecars, GZIP/ZIP, split sources и расширенный operational streaming ожидают своих
    контрактов. Пакет не добавляет dependency, download, callback framework или source API.
    Compression dependency/license/platform/OFF acquisition и member/entry/CRC/inflation policy —
    будущий gate. Будущим callbacks нужен отдельный lifetime/stop/error/close/EOF contract.

## Альтернативы

- Неявное расширение старых profiles нарушило бы established refusal/output contracts и смешало
  local field support с container/business compatibility.
- Выбор width по младшим битам или автоматические Java conversions теряют точный identity и source
  representation, а также могут неверно прочитать long либо object-length payload.
- Десериализация произвольных Java objects добавляет иную задачу исполнения, dependencies и
  compatibility. Непрозрачные bytes удовлетворяют выбранной generic-wire границе.
- Копирование Java mutable partial remapping и sender-indexed allocation не даёт нужных C++ snapshot
  lifetime, atomicity и finite-resource guarantees.
- Безымянная цель full-QD скрывает неподдерживаемые версии, message families и source modes.

## Последствия и поставка

QDCXX-041 реализует недостающие scalar identities; 042 — owned byte/string/object payloads;
043 — indexed layout и generations; 044 — options/flags/symbol state; 045 — data dispatch и
descriptor policy. Их test/API gates заданы в
[CONTRACTS](../../tests/conformance/protocol/real-tape/CONTRACTS.ru.md).
Поддержка нового профиля ожидается до доказательств для всех применимых строк. Будущие строки046–050
не закрываются и не становятся реализованными этим решением.

Producing harness квалифицирован только на Windows. Переносимые fixtures всё равно требуют
матрицы C++ client Windows MSVC Release и Linux GCC Debug ASan/UBSan; ограничения producer host
её не отменяют. До закрытия критических контрактов нужен независимый final-source ownership/state/
resource review. Документация и характеризация исходников не доказывают runtime compatibility,
completion, прохождение полного каталога или выпущенную версию.
