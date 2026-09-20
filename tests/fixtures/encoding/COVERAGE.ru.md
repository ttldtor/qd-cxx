# Исполненное покрытие примитивов

[English](COVERAGE.md). Английская версия является основной.

QD 3.355, закреплённый commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; QDCXX-009. [Driver](../../../tools/upstream/encoding-oracle/EncodingOracle.java), [manifest семейств](cases.json), [контракт/исходники](../../../spec/encoding/PRIMITIVES.ru.md). Входы написаны независимо; ожидаемые байты и результаты получены запуском Java. Новый ByteArrayInput для каждого чтения; только прямые методы BufferedInput/Output.

| Файл | Строки writer | Строки reader | Строгие префиксы | Строки ошибок |
| --- | --- | --- | --- | --- |
| [integers.tsv](expected/integers.tsv) | 96 | 504 | 396 | 396 |
| [strings.tsv](expected/strings.tsv) | 13 | 200 | 187 | 187 |
| [forms.tsv](expected/forms.tsv) | 0 | 333 | 275 | 287 |

Всего: 109 наблюдений writer, 1037 reader, 858 чтений строгих префиксов. Префиксы включены в число reader. Все 583 канонических префикса дают EOF на переданной длине; префиксы некорректных форм сохраняют фактические разные исходы.

## ID целых и переходы

`I` — int, `L` — long. Для каждого b из I={6,13,20,27}, L={6,13,20,27,34,41,48,55} исполнены все шесть ID `{I|L}-b{b}-{neg|pos}-d{-1|0|1}`. Значение: знак × 2^b + delta. [Таблица контракта](../../../spec/encoding/PRIMITIVES.ru.md#канонический-compact-writer) связывает границы с ветками исходника. `{I|L}-{zero,minus1,plus1,minus2,min,max}` покрывают ноль/знак и экстремумы. Каждый такой вектор имеет writer/reader и все строгие префиксы `/prefix-N`.

`X0–X7` записаны как long: −2147483648, 2147483647, −2147483649, 2147483648, 4294967296, −4294967297, Long.MIN_VALUE, Long.MAX_VALUE. Каждый имеет long reader, `/as-int` и все строгие префиксы последнего. `XI-2147483648`, `XI-1`, `XI0`, `XI2147483647` имеют int writer/reader и `/as-long`.

N0/N1, N2/N3, N4/N5, N6/N7, N8/N9, N10/N11, N12/N13, N14/N15 независимо задают избыточные 0/−1 длиной 2,3,4,5,6,7,8,9. N16/N17 задают f100000000/f600000000. Каждый N ID читается обеими ширинами со всеми строгими префиксами. Полные ограниченные compact-входы принимаются; некорректный compact проверяется усечением, без придуманного правила отказа по зарезервированному префиксу.

## ID строк

Writer/reader и все строгие префиксы: `S-null`, `S-empty`, `S-ascii`, `S-nul`, `S-bmp`, `S-supplementary`, `S-high`, `S-low`, `S-reversed`, `S-high-ascii`, `S-utf-boundaries`, `S-len63`, `S-len64`. Граничная строка содержит кодовые единицы 007f,0080,07ff,0800,ffff,dbff,dfff; покрыта и пара максимального скаляра. Значения сохраняются как исходный UTF-16 hex, включая одиночные суррогаты.

Независимые формы только reader, каждая со всеми строгими префиксами:

- Дополнительные принимаемые формы: `F-overlong2`, `F-overlong3`, `F-overlong4`, `F-cesu-pair`, `F-surrogate4`, `F-overlong-null`, `F-overlong-empty`, `F-overlong-length`.
- Некорректный UTF: `F-bad-cont2`, `F-bad-cont3`, `F-bad-cont4`, `F-continuation`, `F-lead-f8`, `F-lead-ff`, `F-too-large`.
- Объявленная длина/тело и остаток входа: `F-short-declared`, `F-short-declared-eof`, `F-long-declared`, `F-trailing`.
- Безопасный отказ до выделения памяти: `F-negative-length` (−2), `F-limit-plus1` (8589934589), `F-longmax-length` (9223372036854775807).

## Ограничения

Допустимые большие длины не выделяются. Максимальное тело канонической строки — 64 байта; предел входа — 1024 байта, heap JVM — 64 MiB, timeout oracle — 30 секунд. Экстремальные допустимые длины, выделение памяти/OOM, другие буферы, эквивалентность IOUtil/DataInput, прочие UTF API, PentaCodec, maps свойств, полный QTP parser и C++ conformance не исполнялись. Нет утверждений о полном Unicode/fuzzing и нового wire-лимита. [Валидация](VALIDATION.ru.md) фиксирует воспроизводимость и неизменность эталонов.
