# Проверка точных двоичных скаляров

[English version](SCALAR-VALIDATION.md). Английская версия является нормативной.

Отчёт подтверждает обычное расширение скаляров только для чтения из
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.ru.md). Более широкий каталог
из 19 типов, object payloads, индексные схемы, новые политики контейнера, flags
и бизнес-преобразования находятся вне результата. Ревью реализации и итоговая
интеграция отделены от описанных ниже focused-исходных проверок.

## Production evidence

Исполнены обе закреплённые версии Java QD: 3.355 на
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и 3.356 на
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Lock не меняется.
[Capture manifest](../../../fixtures/encoding/field-types/scalar-captures.json)
сохраняет хеши 16 относящихся production sources на pin, source/helper hashes,
runtime hashes, digests полного classpath inventory, настройки и хеш каждого нового
артефакта. Baseline использует чистую reactor build и исходный полный Verify workflow;
второй pin повторно использует проверенные чистые compiled classes и запускает свежие
scalar capture и repeat. Эти основания сборок намеренно разделены.

| Evidence | Фактическое наблюдение |
| --- | --- |
| Default factories | BYTE1 и SHORT3 возвращают null. Синтетическая реализация поля не подставляется. |
| Requested composer | Четырнадцать исходных captures на pin: семь identities в STREAM и RAW. BYTE1/SHORT3 используют production CompactIntField8; остальные non-VOID identities сохраняют собственные consumers. Wire и consumer ID — отдельные колонки. |
| FileWriter/FileReader | Десять исходных записей на pin: 0/4/24/104/152, каждый в STREAM/NONE и RAW/MESSAGE, compression NONE, EMPTY_SET и нулевые flags. |
| Production parser | 1106 samples на pin покрывают положительные и усечённые ширины, знаковые границы, compact overlong/high32 forms и следующий sentinel913. |
| RAW projection | Три реальные callback families на успешную RAW wire record; обязателен ровно один STREAM callback, его значения сохраняются. Все families должны совпадать по scalar и sentinel. C++ выдаёт одну source wire record. |
| VOID | Requested descriptor и FileWriter удаляют VOID. Вручную закодированный explicit VOID description принимается BinaryQTPParser и оставляет независимый cursor slot VoidIntField0 в default zero с правильным sentinel alignment. Доставка логического VOID или запись VOID FileWriter не заявляются. |
| Воспроизводимость | На каждом pin 25 рассмотренных артефактов и свежий побайтово равный repeat. 82 унаследованных корневых артефакта baseline не меняются. |

Первичные исходники:
[SerialFieldType 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L283-L307),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java#L283-L307),
[чтение и хранение BinaryRecordDesc 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L387),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L338-L387),
а также [удаление VOID 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L606-L607),
[3.356](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java#L606-L607).
Чтение исходников объясняет наблюдения и не заменяет исполнение ни одного pin.

Fixtures baseline находятся в [expected/scalars](../../../fixtures/encoding/field-types/expected/scalars/samples.tsv),
второй pin — в [current-scalar-expected](../../../fixtures/encoding/field-types/current-scalar-expected/samples.tsv).
`samples.tsv` сохраняет `sample`, case ID, полный wire type, source family, точный hex
полного data payload, число callbacks, число ошибок, STREAM raw value, sentinel
и examined field bytes. Последняя позиция записана вокруг реального production
BinaryRecordDesc.readRecord до дочитывания испорченного сообщения parser.
Она исключает три байта symbol/record-ID. Ошибочные записи не публикуют callback value.
Строки samples обоих pins совпадают. Полные исходные артефакты сохраняются и
хешируются отдельно для каждого pin без нормализации заголовков.

## Регрессии C++ и границы

`field_type_java_values` исчерпывающе проверяет точную числовую политику, прежние
отказы профилей, знаковые диапазоны и неизвестные полные identities до 512.
`field_type_java_tape` читает 1106 samples обоих pins, сравнивает raw bits, consumed
positions, точные FIELD error offsets, завершённый префикс и нулевые scalar string
charges. Отдельно проверены skipped/replaced values, ошибочный skipped SHORT,
metadata explicit и отсутствующего wire VOID, alignment двух записей и конечные
zero/below/exact/above budgets для payload/record/field/output/symbol.

Container tests читают исходные FileWriter tapes с новой явной схемой в прежних
грамматиках LEGACY и RAW_MESSAGE_V1 блоками 1/7/65536. Для BYTE/SHORT используется
неизменённый production FileWriter INT header с неизменёнными реальными requested
composer frames; эти составленные входы не называются исходными FileWriter
BYTE/SHORT recordings. У LONG/TIME_MILLIS по одному логическому значению без
синтетических физических tails. Записи владеют данными после границ времени жизни
reader/schema/input. Обычное автоматическое discovery не меняется.

`protocol_tape_writer` включает прежнюю allocation failure instrumentation с
нулевым allowance и проверяет ранний типизированный UNSUPPORTED для любого layout
нового профиля, включая пустую schema, пустые declarations, ошибочные declarations
и schema только с прежними типами. Это предотвращает случайный приём read-only
профиля прежним outgoing compact-int codec.

исходная focused build Windows MSVC19.42 Release и все три названных CTest suites
прошли. Первая сборка выявила конфликт Windows VOID macro; `VOID_FIELD` исправляет
публичное имя без изменения macros. Consumer tests компилируют обращения после
Windows headers doctest. Закреплённые Uncrustify0.83.0, затем clang-format20.1.8
прошли Format и Check для всех десяти написанных C++ файлов: выборка реализации
из девяти файлов и отдельное исправление I/O fixture. Oracle comparator
отвергает 23 намеренные мутации, включая шесть scalar value/type/position/byte/
completion/fanout mutations, затем принимает неизменённый baseline. Первая
полная регрессия выявила устаревшую I/O test fixture, считавшую full24/raw0
недопустимым; fixture теперь использует unsupported25. Это исправление не меняет
production contract. Generic presentation v4/v6/v7 по-прежнему представляет
канонические полные tags/raw/units, включая новые допустимые скаляры. Version5
Profile сохраняет фиксированные shape/tags; прежние CLI schema gates требуют
своих прежних профилей.

Итоговая интеграция 2026-10-30 выполнила все 53 CTest suites на Windows MSVC19.42
Release и Linux GCC13.3 Debug с ASan/UBSan. Каждый запуск прошёл 52 suites и
отклонил только устаревшую I/O fixture выше. После исправления только теста
затронутый suite прошёл 1/1 на Windows в 14:17:54UTC и Linux в 14:19:03UTC. Production
code не менялся; остальные 52 результата остаются действующими. Это полный
совокупный результат, а не второй единый запуск 53 suites.

Все три прежних production-only профиля успешно пересобраны на обеих системах;
JSON-OFF wire CLI checks прошли на обеих. Ни одно из шести production-only trees
не получало doctest sources; четыре JSON-OFF trees не имели каталога `_deps`.
Обязательные глобальные Java Format, затем Check прошли для 30 source files
в 14:21:40–47UTC без изменения байтов source. Doxygen завершился 0 в 14:21:41UTC
с пустым warning log. Его настройка EXTRACT_ALL=YES сама по себе не доказывает
полноту документации членов; публичные комментарии рассмотрены вручную.
Независимое final-source review прошло после воспроизведения и исправления
устаревшей fixture без оставшихся замечаний.

## Воспроизведение и оставшиеся ограничения

Для чистого baseline Verify используйте прежний
[запуск oracle](../../../../tools/upstream/field-types-oracle/README.ru.md).
Для второго pin соберите тот же закреплённый production module set с JDK8,
скомпилируйте FieldTypesOracle.java для этого явного classpath, дважды запустите
внутренний `scalar-only` в новые каталоги и сравните полные наборы из 25 файлов
с `current-scalar-expected`. Сохраняйте точные pin/source/classpath/runtime digests;
движущаяся ветка или только совпадающие исходники второй версии не считаются
доказательством исполнения.

Частный tape, live feed, business event mapper, object decoding, индексное время,
flags, дополнительная family или outgoing scalar support не подтверждены. Три
прежние schema policies, обычное discovery и существующие container grammars
сохраняют приём/отказ. Новые комбинации требуют явного выбора BINARY_FIELDS_V1;
default construction и прежние профили не расширяются.
