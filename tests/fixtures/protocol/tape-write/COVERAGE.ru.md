# Покрытие и пределы исходящих свидетельств

[English](COVERAGE.md). Английский текст является нормативным.

[Корпус](CAPTURES.ru.md) характеризует production Java; он не реализует и не допускает C++ writing
profile. Custom record сохраняет точно full IDs 2/8/10/40/56/72/88/120, два type-10 object fields и
физический VOID tail WIDE_DECIMAL. Исходные схемы Quote/Profile не изменяются.

Входы покрывают int32/int64 minima/maxima, compact boundaries, ноль и отрицательные raw values;
UTF_CHAR ноль, surrogate units, U+FFFF и U+10FFFF; null/empty строки, empty/nonempty `char[]`, NUL,
unpaired surrogates, surrogate pairs и control units. Символы: cipher-backed IBM, повторения, пустой
символ, явная raw string, длинный символ, wildcard и raw UTF-16. Cipher0/null — production negative,
а не допустимый null symbol. Wire repeat marker не является nullable application symbol. Type-10
parser возвращает String даже для входного `char[]`; проверяются точные units и различие null/empty,
а не сохранение класса Java object.

Все одиннадцать production cases проверяют восемь упорядоченных записей STREAM_DATA projection,
ноль parser errors и точные field values. Symbol units совпадают с входами; cipher/string
representation может измениться. MESSAGE сохраняет миллисекунды, а не младшие event-sequence bits;
NONE теряет timing, FIELD имеет другой descriptor layout. Batch grouping FileWriter измерен, но
побайтовое воспроизведение всего его file layout будущим writer не обещается.

## Факты закреплённого исходника

Все ссылки относятся к [locked revision](../../../../upstream.lock); hashes находятся в
[provenance](expected/provenance.tsv). Это source inspection, подтверждённое там, где указано,
production callbacks или отдельно обозначенными descriptor scans.

| Факт | Закреплённый исходник | Подтверждение |
| --- | --- | --- |
| Исходящие имена — property names; int раньше obj, VOID пропускается. | [BinaryRecordDesc:182](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L182), 190/198/204 | Custom [scan](expected/extras/extras-raw-message-wire.tsv), Quote/Profile scans. |
| Новый lookup alias меняет property name; прежние lookup keys остаются. | [DefaultRecord:125](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java#L125), 143 | `Raw.Count`, `RawCount`, `FirstLookup` находят одно поле; outgoing name — `OutgoingCount`. |
| Descriptor записывает scheme record ID и ordered name/full-type pairs. | [BinaryQTPComposer:209](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java#L209) | Wire IDs Quote0, Profile5, custom0; exact identities и порядок в scans. |
| Type 10 записывает String/char-array units, parser читает String. | [StringField:94](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/StringField.java#L94), 121/126 | Production input/read values и `text-input`. |
| Cipher0/null недопустим; одинаковый предыдущий symbol получает repeat encoding. | [PentaCodec:430](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java#L430), 443 | Production rejection и повторные symbols; точные `.qtp` bytes. |
| Timestamped cursors дают миллисекунды и переключают timestamp внутри batch. | [FileWriterImpl:629](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L629), 644/646 | Три RAW frames для timestamped batch, один для regular heartbeat input. |
| FIELD включает built-in time fields; footer — empty heartbeat. | [FileWriterImpl:254](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L254), 482/485 | FIELD scan добавляет EventTime56/EventSequence72; каждый непустой capture имеет terminal frame нулевой длины. |
| Close журналирует footer I/O errors и использует best-effort close helper. | [FileWriterImpl:411](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java#L411), 441/444 | Только факт исходника; storage failures не инжектировались, durability не заявляется. |

## Явные границы

Full24/full137 остаются negative admission witnesses для C++, несмотря на принятие production Java.
NONE/FIELD timing, STREAM output и более широкие conversions Java не расширяют предложенное
RAW/MESSAGE подмножество. Outgoing descriptors — явные wire facts; native JSON lookup keys не
задают canonical outgoing names. Прежние omission/repeated-target diagnostics остаются в прежних
field/event corpora; здесь они не пересоздаются и не переименовываются в production writing.

Новых event adapters, field identities, production C++, compression, transport, connector session,
private tape, disk fault injection и publication guarantees нет. Успешный FileWriter.close сам по
себе не доказывает completion; эти capture дополнительно разбирают завершённые bytes. Конечные
успешные примеры не доказывают все metadata combinations или future strict rewrite admission.
[Отчёт проверок](../../../../tests/conformance/protocol/tape-write/VALIDATION.ru.md) отделяет
фактические positive/negative checks от этих ограничений.
