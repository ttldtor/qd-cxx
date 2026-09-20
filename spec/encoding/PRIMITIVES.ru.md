# Контракт закреплённых примитивов

[English](PRIMITIVES.md). Английская версия является основной.

QDCXX-009 описывает Java QD 3.355 на `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Правила из исходников подтверждены ограниченными [исполненными векторами](../../tests/fixtures/encoding/COVERAGE.ru.md); это не полное покрытие Unicode или C++ conformance. Точные результаты получены Java, а не независимым кодеком harness.

## Точные методы и граница исходников

- [BufferedOutput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedOutput.java): `writeCompactInt`, `writeCompactLong`, `writeUTFString` (строки 323, 362, 453).
- [BufferedInput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedInput.java): `readCompactInt`, `readCompactLong`, `readUTFString`, `readUTFBody`, `readUTF2/3/4`, `checkEncapsulatedLength` (484, 509, 610, 635, 922–944, 121).
- [BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java) `describeRecord` пишет имена записей/полей через `msg.writeUTFString` (214/217); [BinaryQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java) `parseDescribeRecords` читает через `msg.readUTFString` (362/369).
- [BinaryRecordDesc](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java) отдельно использует UTF char, строки IOUtil char-array и UTFString (349/361/367, 541/555–583). Другие представления и PentaCodec исключены. Эквиваленты IOUtil документированы upstream, но здесь отдельно не исполнялись. Политика Unicode в JSON не определяет эти wire-примитивы.

## Канонический compact writer

Знаковые значения используют префикс длины и big-endian биты данных, не LEB128 или ZigZag. Таблица длины long из исходников:

| Байты | Включительный диапазон | Префикс | Исполненные ID границ |
| --- | --- | --- | --- |
| 1 | −2^6 … 2^6−1 | 0xxxxxxx | I/L-b6-* |
| 2 | −2^13 … 2^13−1, кроме более коротких | 10xxxxxx | I/L-b13-* |
| 3 | −2^20 … 2^20−1, кроме более коротких | 110xxxxx | I/L-b20-* |
| 4 | −2^27 … 2^27−1, кроме более коротких | 1110xxxx | I/L-b27-* |
| 5 | −2^34 … 2^34−1, кроме более коротких | 11110xxx | L-b34-* |
| 6 | −2^41 … 2^41−1, кроме более коротких | 111110xx | L-b41-* |
| 7 | −2^48 … 2^48−1, кроме более коротких | 1111110x | L-b48-* |
| 8 | −2^55 … 2^55−1, кроме более коротких | fe + 7 байт данных | L-b55-* |
| 9 | остальные знаковые 64-битные значения | ff + 8 байт данных | L-min/max, L-b55-* |

При длине 1–7 оставшиеся биты префикса входят в знаковые данные; длина 8 содержит 56 знаковых битов данных. Int writer использует длины 1–5: пятибайтная форма имеет f0 для неотрицательных и f7 для отрицательных значений, затем 32 исходных бита. Long writer делегирует значения диапазона int методу int writer. `I/L-zero`, `minus1`, `plus1`, `minus2`, `min`, `max` покрывают знак и экстремумы. Каждая граница `b` имеет `neg/pos-d-1/d0/d1`: −2^b или +2^b плюс −1/0/+1. Покрыты обе стороны всех переходов длины; значения вне диапазона int его writer не передаются.

## Дополнительные формы compact reader

[integers.tsv](../../tests/fixtures/encoding/expected/integers.tsv) сохраняет байты writer и отдельные наблюдения reader. `X0–X7/as-int` показывают полное потребление с возвратом знаковых младших 32 битов, включая значения вне int; отказа по переполнению в этом наборе нет. `XI*/as-long` проверяет четыре входа int writer через long reader.

[forms.tsv](../../tests/fixtures/encoding/expected/forms.tsv) N0–N15 независимо задают ноль и −1 каждой длины 2–9. Оба reader принимают их. N16/N17 используют f1/f6 с нулевыми младшими битами: long возвращает 4294967296/−8589934592, int — 0. Принятие reader не делает их каноническим представлением нуля у writer. Полная битовая грамматика следует из исходника; пробы не перебирают все комбинации.

## UTFString writer и reader

Writer предваряет строку длиной UTF в байтах через compact long; −1 означает null (`7f`), ноль — пустую строку (`00`). ASCII/NUL занимают один байт, BMP — от одного до трёх, правильные пары суррогатов — четырёхбайтную кодовую точку, одиночные суррогаты — трёхбайтные формы CESU-8. [strings.tsv](../../tests/fixtures/encoding/expected/strings.tsv) сохраняет исходные кодовые единицы UTF-16: NUL в A/NUL/B — `03410042`, U+1F600 — `04f09f9880`, одиночный d800 — `03eda080`, dc00 — `03edb080`. Обратная пара остаётся двумя отдельными единицами. Длина считает байты, не единицы UTF-16; len63/len64 проверяют границу compact-длины.

Дополнительные формы reader: F-overlong2/3/4 декодируют NUL; F-cesu-pair декодирует d83dde00 из шести CESU-байтов; F-surrogate4 принимает d800 в четырёх байтах. F-overlong-null/empty/length принимают избыточные compact-длины. Нормализации и вставки символа замены нет. Это отличается от строгих валидаторов UTF-8 и Java DataInput modified UTF; их правила нельзя переносить сюда.

## Измеренные ошибки и consumed position

Каждое прямое чтение начинается с нуля на новом ByteArrayInput; позиция измеряется через `totalPosition()` без mark/reset. Зафиксирован каждый строгий префикс канонических целых/строк. В этих наблюдениях EOF потребляет весь переданный префикс, включая часть данных. Это не обещает rollback и не характеризует все подклассы BufferedInput или внешнюю буферизацию QTP.

Некорректные формы — независимые ручные входы, не результаты writer. Неверные continuation-байты, continuation вместо начального, начальные f8/ff и >U+10FFFF дают `java.io.UTFDataFormatException` на позициях из forms.tsv. `01c280` потребляет 3 байта до отказа из-за выхода за объявленное однобайтное тело; `01c2` даёт EOF на 2. `0241` даёт EOF на 2. `014142` успешно завершается на 2, оставляя байт. Префиксы некорректных форм могут быть успешными или давать другие ошибки; сохранены реальные исходы, а не предположение EOF для каждого префикса.

Проверка длины в исходнике допускает −1 … 8589934588 (Integer.MAX_VALUE × 4). F-negative-length отвергает −2 на позиции 1; F-limit-plus1 проверяет 8589934589 (на один больше максимума из исходника) на позиции 5; F-longmax-length отвергает Long.MAX_VALUE на 9. Во всех случаях IOException возникает до выделения памяти. Допустимые огромные длины и точные пределы выделения памяти отложены; огромные выделения не запускались. Ограничения harness — эксплуатационная защита, не предлагаемые wire-лимиты.

## Границы

См. [валидацию](../../tests/fixtures/encoding/VALIDATION.ru.md): независимые чистые сборки, точное сравнение и обнаружение мутации. Не добавлены production C++, importer, транспорт, универсальный fuzzing, PentaCodec, другие UTF API, полная политика ошибок parser или результаты upstream test suite. Экстремальные длины/ресурсы и другие буферы требуют отдельных задач. [Контракт буферизации QTP parser](../protocol/README.md) остаётся отдельным.
