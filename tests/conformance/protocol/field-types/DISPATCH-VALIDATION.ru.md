# Валидация бинарного dispatch и descriptors

[English version](DISPATCH-VALIDATION.md). Английская версия нормативна.

2026-10-30 явная политика BINARY_CATALOG_V1 прошла полный CTest на Windows x64 MSVC19.42 Release
и Linux GCC13.3 Debug ASan/UBSan:61/61 на каждой платформе; leak detection на Linux отключён.
Прошли все шесть production-only профилей. Новый broad JSON-OFF inspector вернул0 для16 исходных
FileWriter inputs на каждой платформе; все16 TSV побайтно совпали между платформами.
Для написанного C++ прошли закреплённые Uncrustify0.83.0 → clang-format20.1.8 Format/Check.
Независимая проверка охватила владеющие копии, schema/context mismatch, rollback, metadata peaks,
read/write/flush/resource boundaries и финальные копии артефактов.

## Production evidence

[Capture manifest](../../../fixtures/encoding/field-types/dispatch-captures.json) содержит отдельные
запуски QD3.355 `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`, hashes source/harness и каждого raw-артефакта.
Оба проверенных14-root classpath остались неизменными. Отдельный JDK8 governed Verify с чистой
полной сборкой закреплённого reactor прошёл все609 артефактов. Current upstream targets не пересобирались.

Сопровождаемый FieldTypesOracle сохраняет287 прежних файлов побайтно и добавляет322 dispatch:
normal305 (303 raw tape,668 строк observations и карта57 UID ranges), headerless17 (16 raw tape
и25 строк observations). У каждого pin есть отдельный capture/repeat; canonical rows совпали между
повторами и pins. В normal raw tape сохранён фактический случайный synthetic endpoint UID.
Только57 структурно подтверждённых полных/частичных диапазонов UID нормализуются в памяти при
сравнении; остальные байты и границы карты обязаны совпасть. Каждый raw-файл имеет собственный
исходный hash. Headerless tape повторились побайтно; capture-файлы не редактировались ради равенства.
Два отдельных lower-message label сохраняют различие регистра time-тестов на Windows filesystem.

Production FileWriter/FileReader охватывает четыре family с NONE/MESSAGE, включая реальные
noHeaderAndFooter children. Headerless NONE начинается descriptions2, MESSAGE — heartbeat0.
RawDataConsumer наблюдает wire-запись один раз. Контролируемые production cases включают full/late/
delta/repeated/absent descriptors, произвольную version, unknown/duplicate properties, send/receive,
options, endpoint tails, malformed complete против physical partial EOF, controls/unknown IDs и
реальные FileReader schemeKnown/readAs. Designed mutations обозначены отдельно от исходных
FileWriter tapes. Java может сгруппировать15,20,15 как15,15,20; C++ сохраняет wire order, без collector parity.

Все50 oracle mutations отклонены:42 прежних и8 dispatch — value/family/time, raw bytes, UID offset/
length, байт вне UID и headerless. Независимая проверка также вывела57 диапазонов из raw-структуры
протокола и выполнила дополнительные граничные mutations.

## Native-контракты и whole-file scope

Целевые native cases охватывают четыре family, точные поля/владеющие bytes/UTF16, removal0 против
отсутствия, retired name/type refusal, constructor zero/one identity, большой допустимый UTFString
length prefix при tiny input, точные descriptor peak/work/no-op storms, allocation rollback,
physical truncation, v8 empty-batch generation, все19 formatter tags и проверки ожидаемых schema/
context/full shape до zero output budget. HUMAN называет принятые family; old modes/TSV сохраняются.
Старые форматы сохраняют отказы payload/flags/removal. Прежние ordinary profiles/fixtures не изменены.

[Приёмка real-tape](../real-tape/MILESTONE-1.ru.md) завершила93 проверки:44 прежних,
47 явно designed bridge regressions и2 реальных producer STREAM/MESSAGE. Отдельный приватный
whole-file ordinary corpus сравнил333607 records/3136236 values/48 generations, точные source/
order/symbol/raw time, physical EOF/open/close/error/queue proof и digest literal FileReader.
Приватные bytes/names/paths/decoded rows не публиковались. Компаратор принимает лишь прежние восемь
ordinary типов, zero flags и отсутствие removal, в том числе в строгом v8 view. Это не приёмка
всех19 типов, новых flags или бизнес-семантики.

Native completion подтверждает read/physical EOF/reader.finish/write/final flush и exit0, без
owner close-before-complete; Java instrumented close proof отдельный. FIELD, sidecars, compression,
file chains, collectors, conversions и общий owned-source lifecycle исключены.
Точная политика: [BINARY-MESSAGES](../../../../spec/protocol/BINARY-MESSAGES.ru.md).
