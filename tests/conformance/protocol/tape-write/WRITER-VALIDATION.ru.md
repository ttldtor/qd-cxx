# Валидация writer в памяти

[English version](WRITER-VALIDATION.md). Английская версия является нормативной.

Реализация следует [TAPE-WRITER](../../../../spec/protocol/TAPE-WRITER.ru.md) и
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.ru.md). Она формирует ограниченные владеющие
chunks без владения filesystem, clock, sink или transport. Отчёт разделяет локальные проверки
writer и отдельные gates production Java parser и интеграции.

## Выполненные focused проверки

### Начальный snapshot

MSVC 19.42.34444.0, Visual Studio 17 2022, Windows SDK 10.0.26100.0, x64 Release собрали
`qdcxx_tape_writer_tests` с обычными targets protocol/core/encoding и прежними targets
Quote/Profile. Запуск 2026-10-11T22:17:40Z прошёл 15 сценариев и 2365 assertions,
включая выдачу interoperability corpus. В этих проверках ошибок не обнаружено.

Тесты используют проверенные [контрактные fixtures](../../../fixtures/protocol/tape-write/contracts):
`schema.json` задаёт локальные объявления; `writer-cases.json` — явные outgoing layouts,
все упорядоченные действия и 18 отрицательных mutations; `raw-values.tsv` — 24 точных
tag/range witnesses; `budgets.tsv` — все четыре группы лимитов layout и семнадцать writer;
`primitives.tsv` — пять побайтовых symbol witnesses raw/repeat/new-frame.
Конечный test-only ASCII reader fixtures ограничивает bytes, elements и depth и читает
проверенную грамматику declarations/actions. Он не является общим JSON validator: его integer
reader также принимает ведущие нули, отсутствующие в этих сохранённых корректных JSON fixtures.
Новой production JSON dependency нет; reader не участвует в production APIs.

Проверены явные permutation/aliases полей, sparse sender IDs, полное покрытие полей,
каждый поддержанный tag, signed raw extremes, null versus empty type-10,
NUL/lone/pair surrogate units, первые empty/wildcard/IBM/long/raw symbols, точные повторы FF
и сброс на следующем data frame. Каждый writer/layout allowance имеет witnesses zero,
exact, one-below и one-above; границы полной schema copy проверяются отдельно. Логические
charges фиксированных входов сверяются независимо от успешного прохождения budgets.

Прежний `TapeReader` выполняет семантический round trip в режиме `RAW_MESSAGE_V1`. Тесты
сравнивают входной порядок действий/записей, mapping sender-to-local, точные tags/raw/nullable
units, symbol units, descriptor absence/empty/spelling/send distinctions и signed raw/packed
timing. Покрыты absent/zero/timestamp heartbeats, отрицательные значения, repetition, decrease,
signed64 saturation, пустые batches/groups, явные markers/trailing heartbeats и finish
без неявных байтов. Побайтовое совпадение целого tape с Java не утверждается.

Ordinary allocation получает fault injection только во время реальных владеющих вызовов
construction/frame в тестовом executable. Копии layout/schema, независимая конфигурация writer,
description envelopes и retained long-symbol storage преобразуют allocation failures без
выданных байтов или зафиксированных announcements/output/frame counters. Неудачные попытки
сохраняют charges; каждая следующая операция возвращает неизменную первую diagnostic.
Проверены независимые экземпляры, caller-owned chunks, изменение исходной metadata после
копирования и moved/finished states.

Изолированный закреплённый pipeline Uncrustify 0.83.0 → clang-format 20.1.8 Format и Check
прошёл для `tape_writer.hpp`, `tape_writer.cpp` и `tape_writer_tests.cpp`. Полные локальные логи —
`.work/writer-033/writer-focused-build-final.log`, `writer-focused-test-final.log`,
`writer-format-final.log` и `writer-format-check.log`. Предшествующие логи сохраняют исходный
sandbox refusal чтения SDK registry, пропущенную qualification member при механическом
разделении helpers и первоначальное несоответствие sender endpoint.

Две изолированные source mutations проверили чувствительность тестов. Удаление flags check
привело к отказу двух сценариев/пяти assertions, включая W07 и проверки failure precedence
последующей записи. Commit output bytes до allocation привёл к отказу трёх сценариев/пяти
assertions, включая exact output budget и atomicity при реальном allocation failure.
Их сборки MSVC Release прошли; ожидаемые неудачные запуски тестов сохранены в
`.work/writer-033/writer-flags-mutant-test.log` и `writer-atomic-mutant-test.log`.
Затем неизменный production binary прошёл 15 сценариев/2258 assertions без emission
в 2026-10-11T22:22:04Z (`writer-focused-restored-test.log`). Это post-implementation
mutation controls, а не утверждение о pre-implementation test-first execution.

### Исправление descriptor-accounting после ревью

Ревью обнаружило, что сравнения descriptor policy учитывали логическую работу, но пропускали
caller-text visits в `textUnits`. Теперь каждое сравнение учитывает одну caller unit до двух
work-unit reads; статическое policy spelling является работой, а не caller descriptor text.
Неудачное uppercase сравнение для lowercase `message` тоже учитывается. Отказ до недопущенного
сравнения сообщает `descriptor-text-comparison`; если text допущен, а work нет,
сообщается `construction-work`.

Новые exact/below/above тесты покрывают uppercase `MESSAGE` и lowercase `message` с `hs`:
construction text charges равны соответственно 27 и 28, work charges — 186 и 188.
Text charge полного контракта теперь 199; work charge остаётся 440.
Изолированный исправленный MSVC x64 Release прошёл 16 сценариев/2396 assertions с emission
corpus в 2026-10-11T22:28:07Z и 16 сценариев/2289 assertions без emission в 22:29:21Z.
Логи: `.work/writer-033/writer-review-fix-{configure,build,test}.log`,
`writer-review-fix-test-restored.log`, `writer-review-fix-format.log` и
`writer-review-fix-check.log`. Format/Check прошли все три scoped C++ files.

Точная копия предшествующего frozen source, собранная с новыми тестами, затем не прошла
два сценария/три assertions: B12 ledger/one-below и новую границу descriptor comparison.
Доказательства: `writer-review-old-{configure,build,test}.log`. Исправленные source и tests
являются текущим snapshot; предшествующие full/production observations ниже относятся
к версии до этого исправления accounting и требуют итогового интеграционного повторного запуска.

## Production-parser corpus и ограничения

Необязательный вызов тестов:

```text
qdcxx_tape_writer_tests tests/fixtures/protocol/tape-write/contracts --emit-dir OUTPUT
```

Он выдаёт пять сценариев: `contract`, `all-types`, `descriptor-opaque`, `standard-quote` и
`standard-profile`. Каждый содержит `tape.qtp`, точные объявления `schema.tsv` и полученные
из входов строки `expected.tsv` после проверенного C++ round trip. Standard cases дополнительно
содержат `schema-mode.txt`, выбирающий неизменную Java DXFeedScheme. Они используют прежние полные
default Quote/Profile APIs и каждое captured non-VOID property field без сокращения схемы.
Отдельный production parser harness — [TapeWriterInteropOracle.java](../../../../tools/upstream/tape-write-oracle/TapeWriterInteropOracle.java).

Отдельный production запуск 2026-10-11T22:21:30Z–22:21:49Z прошёл все пять сценариев через
закреплённые Java FileReader/BinaryQTPParser: 18 wire-record projections, ноль parser errors,
54 RAW fanout callback records и 152 сравниваемые строки. Сравнение выбирает STREAM_DATA
projection один раз и не принимает triple RAW fanout за три wire records.
Verifier проверяет полное покрытие неизменных Java Quote/Profile fields и сохраняет hashes
C++ producer, source/input bytes и фактических Java provider trees до и после запуска.
Доказательства: `.work/writer-033/interop-final-stable.log` и stdout/stderr, `bounds.tsv`,
`provenance.tsv`, `classpath-receipt.json` в `interop-final-stable/`.

Отдельный запуск Doxygen `doxygen tools/docs/Doxyfile` завершился с exit 0 в
2026-10-11T22:20:14Z; `.work/api/warnings.log` пуст, начальная сгенерированная страница —
`.work/api/html/index.html`. Закреплённые Java Format/Check прошли 26 файлов с
google-java-format 1.36.1, formatter JDK 23.0.2 и чистым вызовом PowerShell.
Логи: `.work/writer-033/doxygen.log`, `java-format-no-profile.log` и
`java-format-check-no-profile.log`.

Окончательная конфигурация Doxygen использует `EXTRACT_ALL=YES` для включения документированных
free functions без прежних file groups. Parameter/markup warnings остаются включёнными;
missing-member warnings подавлены. Поэтому пустой warning log не доказывает документированность
каждого legacy member.

В итоговом разделе ниже зафиксированы выполненные интеграционные gates writer. Прежние focused
результаты выше относятся к указанным для них snapshot.

Исходный witness sender INT32_MAX выявил signed overflow `id + 1` в remapping path закреплённой
production Java. Outgoing контракт теперь отказывает этой границе в `record-id`; нативный
construction witness MAX-1 по-прежнему использует два sparse entries и намеренно не передаётся Java.
Другие очень большие ID остаются зависимыми от ограничений Java dense-array length и heap.
Прежние reader IDs и исторические fixtures не изменены.

Фиктивные oversized views не используются для INT32_MAX character-array count или multi-gigabyte
домена UTFString. Эти representability guards проверены по исходникам; проверяются конечные входы
с реальным backing storage, все обычные точные границы и реальные allocation failures. Логические
scratch counts не подтверждают точный heap usage, allocator capacity или успех caller I/O.

## Итоговая интеграционная проверка

Финальный исходник под Windows прошёл полный набор MSVC 19.42 x64 Release: 47/47 тестов CTest,
включая 16 writer cases/2289 assertions без emission и 4 char-array cases/323 assertions. Полный
CTest занял 213.61 секунды. Логи: сборка
и CTest.

На Ubuntu 24.04 в WSL2 с GCC 13.3 финальный исходник прошёл инкрементальную Debug-сборку
ASan+UBSan и обе затронутые записи CTest (writer и char-array, 2/2). Также успешно собраны обе
production Release-конфигурации с выключенным JSON: одна с выключенными tools, другая с включёнными.
Linux CTest 47/47 относится к начальному snapshot до принятого исправления учёта `textUnits`;
полный Linux CTest после исправления не запускался. Журналы финального исходника перечислены
в архиве доказательств writer. Предыдущий полный прогон остаётся
регрессионным доказательством для того более раннего snapshot.

Отдельный финальный запуск native emission прошёл 16 тестовых случаев/2396 assertions и создал пять tape. Затем Java 8 FileReader
прочитал все пять cases (`contract`, `all-types`, `descriptor-opaque`, `standard-quote` и
`standard-profile`): 18 STREAM records, 54 callback projections, 152 сравниваемые строки и ноль
ошибок parser. В Quote/Profile сохранены все неизменённые поля — 8/16 соответственно на запись.
Входы и наблюдаемые результаты находятся в `inputs/`,
`oracle/observed/`, а также в архивных файлах
`provenance.tsv`,
`classpath-receipt.json` и
`bounds.tsv`; полный лог interop —
`interop-reviewed.log`.

Независимая critical recheck закрыла обязательную находку по учёту descriptor text; других
обязательных замечаний в назначенной области не осталось. Её свежий MSVC Release запуск прошёл
2/2 целевых записи CTest: writer — 16 cases/2289 assertions, char-array — 4 cases/323 assertions.
Эта целевая проверка не расширяет матрицу платформ: Clang-cl и macOS для данного изменения
не запускались. Отдельная сертификация standalone LeakSanitizer не заявляется.

Команды Linux sanitizer не задавали явные `ASAN_OPTIONS` или `UBSAN_OPTIONS`; использовались
настройки GNU по умолчанию, включая обнаружение утечек. В этих запусках диагностики sanitizer
не обнаружено. Production-сборки обеих платформ использовали `BUILD_TESTING=OFF` и JSON OFF;
`_deps` отсутствовал при выключенных tools и при включённых обоих inspection tools. GCC Release
выдал нефатальное предупреждение `-Wstringop-overflow` в неизменённом
`src/core/wide_decimal.cpp:68`; обе сборки завершились с кодом 0.

Полная закреплённая проверка C++ форматирования прошла для всех 88 выбранных файлов репозитория
в 2026-10-11T22:44:46Z. Java Format и Check прошли для 26 файлов в 22:31:13Z и 22:31:17Z.
Прямое преобразование в hex совпало с прежним formatter для всех 65536 UTF-16 units в
представлениях String и char-array; также проверены null/empty и отказ oversized inputs.
Положительные/отрицательные parser probes и ограничения описаны в
обзоре архива.
