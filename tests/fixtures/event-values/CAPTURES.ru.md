# Контракт захвата значений событий

[English version](CAPTURES.md). Английская версия является определяющей.

`oracle.tsv` содержит детерминированные raw UTF-8/LF строки
[production oracle](../../../tools/upstream/event-values-oracle/README.ru.md). `provenance.json`
связывает точную закреплённую revision, upstream source hashes, inventory готовых classes,
хеши/версию JDK8, хеши собственных harness/launcher, digest raw fixture и равенство часовых поясов.
Локальные пути выполнения и полные рабочие логи не входят в публичный fixture.

| Вид строки | Входы и настоящий production-результат |
| --- | --- |
| enum | Имя типа, raw signed32 code, настоящий valueOf/getCode либо error |
| flags | Шесть значений IndexedEvent constants |
| bits32 / bits64 | Raw word, mask, shift, replacement bits, getBits result, setBits result |
| nanos | Raw nanoseconds, floor milliseconds, неотрицательный nano part |
| time | Raw milliseconds, насыщенные signed32 seconds, неотрицательные milliseconds |
| combine | Milliseconds, signed nano part, wrapped signed64 nanoseconds |
| packed | Raw Trade timeSequence, настоящие getTime, getSequence |
| settime / setseq | Прежнее raw packed word, новое time/sequence, настоящий packed result либо error |
| day | Year/month/day, настоящий результат DayUtil либо error |
| date | Signed packed date, настоящий day ID DayUtil либо error |
| calendar | Raw signed32 day ID, настоящая packed Gregorian date DayUtil |
| schema | Стандартная запись TimeAndSale, полное имя ExchangeSaleConditions, точный serial type ID40 |
| encode | Nullable source units, full64 code EventUtil либо error, decoded units либо error |
| decode | Выбранные корпусом raw64 patterns, настоящие decoded units EventUtil |
| field | Nullable source units, signed32 результат parse стандартного поля, units его render |

Text cells используют `null` для null, `-` для engaged empty, иначе четыре hex цифры на UTF-16 unit.
Десятичные integer cells сохраняют signed raw bits; `error` означает отказ настоящей Java operation.
Full64 encode/decode отличны от signed low32 проекции стандартного SHORT_STRING поля. Отрицательное
стандартное поле расширяется со знаком перед long decoder; поэтому возможны ведущие units0xFF.
Результат parser не подменяется unsigned интерпретацией четырёх байтов.

Корпус охватывает все именованные/резервные enum-table entries и выходящие за таблицу коды;
zero/all-one/sign-bit masks и отрицательные/большие shifts; timestamp extrema, saturation и выходящие
за диапазон nano parts; границы sequence; Gregorian century/leap/year-zero/negative-year/arbitrary-day,
signed32 extrema и детерминированные raw calendar samples; null/empty/embedded-zero/eight/nine-unit
strings, 0xFF, отказ above-byte/surrogate и каждый ненулевой байт в каждой из восьми позиций.
Наблюдения двух часовых поясов должны совпадать побайтно. Захват доказывает вызванные production
operations, не live feed, будущие event adapters и не готовность платформ/пакета.
