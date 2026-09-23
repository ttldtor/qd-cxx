# Ограниченные компоненты C++23

[English version](CPP-BUILD.md). Английская версия является нормативной.

## Подготовленные отображения Quote и Profile

Подключайте `qd/events/quote.hpp` или `qd/events/profile.hpp` и связывайте `qdcxx_quote` или `qdcxx_profile`.
Обе библиотеки зависят от `qdcxx_event_mapping`, который зависит только от `qdcxx_core`; получение
источников, протокол и транспорт находятся вне библиотек событий. Production-зависимости не добавлены.
`defaultProfileSchema(limits)` предоставляет все шестнадцать стандартных полей, а
`PreparedQuote::prepare(schema, localId, mappingLimits)` и `PreparedProfile::prepare(...)`
принимают совместимые схемы вызывающей стороны, включая перестановки, дополнительные и необязательные поля.
Прежние `defaultQuoteSchema` и контракт `mapQuote` сохранены.

Подготавливайте отображение один раз по тому же `LocalSchema`, который использует декодер. Передавайте
`{record.context, record.typedValues}` как `RecordValueView` в `map`, вместе с символом и явными
бюджетами владения. Profile принимает `EventValueLimits{symbolUnits, stringUnits, totalUnits}`;
Quote — `symbolUnits`. Оба метода принимают отдельный необязательный `eventTime` в миллисекундах.
Для RAW передавайте `result.timing->eventTimeMs()`; значение по умолчанию — ноль. Возвращаемое событие
владеет символом и строками. Вызывающая сторона, сохраняющая несколько событий, ограничивает суммарное владение/работу.

`LocalSchema::recordContext(id)` и контекст декодированной записи идентифицируют неизменяемые схему
и запись. Копии и `boundedCopy(limits)` сохраняют идентичность; новый `create`, даже с одинаковыми
объявлениями, создаёт независимую идентичность. Уничтожение входов подготовки/декодера безопасно.
Span без контекста или совпадение количества/типов не подтверждают совместимость. См.
[общий API](../spec/events/EVENT-MAPPING.ru.md), [Quote](../spec/events/QUOTE.ru.md),
[Profile](../spec/events/PROFILE.ru.md) и [доказательства конфигурации](../spec/schema/CONFIGURED-EVENTS.ru.md).

Явный preset tape `--profile default-profile-v1` выводит версию 5 в обоих существующих режимах;
[просмотр Profile](PROFILE-INSPECT.ru.md) определяет бюджеты CLI и вывод. Настраиваемые библиотечные схемы
не означают наличия C++ XML/properties loader или полной поддержки dxlink. Разделы ниже описывают
исторические границы своих компонентов.

## Точные расширенные значения полей и ограниченные char arrays

`LocalProfile::EXTENDED_FIELDS_V1` принимает только полные ID 2/8/10/40/56/72/88/120. Старые локальные
профили сохраняют точные наборы. `FieldValue::units` — владеющий optional UTF-16: тип 10 имеет нулевой
raw и null/empty/content в units; скаляры не имеют units payload. `validFieldValue` проверяет эту форму
и инварианты тега/диапазона. Типы 40/72/88 сохраняют знаковые raw low-32-bit значения без текстовых/
календарных преобразований. [FIELD-TYPES](../spec/encoding/FIELD-TYPES.ru.md) задаёт точный контракт.

Подключить `qd/encoding/char_array.hpp` и линковать `qdcxx_encoding`.
`readCharArray(input, limits, boundary)` возвращает владеющие units, потреблённые байты, учтённые units
и явный статус; не сохраняет вход или частичное значение при ошибке. `CharArrayLimits` задаёт
encoded bytes вместе с префиксом и UTF-16 units. Значение по умолчанию `CharArrayBoundary::PRIMITIVE`
соответствует наблюдениям прямых примитивов; `MESSAGE` выбирает production-проверку оставшихся
байтов сообщения. Этот reader отделён от UTFString.

Для данных со строками явно задать последние параметры `StreamDataLimits::fieldString`,
`fieldBytes`, `fieldUnits` и `ownedFieldUnits`. По умолчанию допуск нулевой, старые агрегатные
инициализации сохранены. `StreamDataResult` предоставляет три соответствующих счётчика;
`TapeReader::fieldBytes()`, `fieldUnits()` и `ownedFieldUnits()` — накопленные расходы tape.
Байты/units учитывают пропущенные, повторные и неуспешные попытки; владеющие units считают
завершённый вывод, а допуск дополнительно ограничивает текущую запись и временную замену.
Символы/имена имеют отдельные бюджеты. Типизированные значения читаются из
`StreamDataRecord::typedValues` в STREAM и RAW; прежний `values` не изменён.
Все возвращённые строки владеющие и не зависят от времени жизни входа/контекста/reader.
Публикуются только полные записи; терминальная ошибка сохраняет завершённый префикс.

Локальный для инструмента `oracleFieldTypesSchema` предоставляет только фиксированную синтетическую
схему для `--profile oracle-field-types-v1`; [CLI и пять лимитов](TAPE-INSPECT.ru.md) описывают версию 4.
Неизменённый стандартный Profile проверяется с полной локальной проекцией полей вызывающего кода,
без адаптера события Profile. Новые наборы: `field_type_java_values`, `field_type_fixture_negatives`, `field_type_java_tape`,
`field_type_inspection_io`, `field_type_inspection_cli`; последние два требуют tape inspector.
Цели/параметры сборки и политика зависимостей не изменены.
[Покрытие](../tests/conformance/protocol/field-types/COVERAGE.ru.md) и
[авторская валидация](../tests/conformance/protocol/field-types/VALIDATION.ru.md) фиксируют фактические
проверки; независимое ревью ожидается. Прежние разделы компонентов ниже сохраняют исторические границы.

## API tape RAW/MESSAGE

QDCXX-023 добавляет явный `TapeMode::RAW_MESSAGE_V1` третьим параметром конструктора TapeReader.
Задайте дополнительный конечный `TapeLimits::timingEvents` (по умолчанию ноль); CLI использует 4096.
`TapeFeedResult::heartbeat` владеет принятым явным heartbeat, а `timing` — исходным timestamp
и packed event-time sequence для всех возвращённых записей одного RAW frame.
`TapeTiming::eventTimeMs()` даёт доставленные миллисекунды, которые на границах насыщения Java
могут отличаться от исходного timestamp. Сохраняйте metadata вместе с записями. Состояние времени
не заимствуется из входа. Прежние конструкторы используют LEGACY с прежним поведением.

`decodeRawData` — отдельный метод для точного payload типа 5; `decodeStreamData` остаётся типом 15.
Время и диспетчеризация принадлежат TapeReader, примитивы payload используются совместно внутри.
Третий параметр `mapQuote` по-прежнему бюджет копирования символов, а не timestamp.
[Точный контракт и владение](../spec/protocol/RAW-TAPE.ru.md), [вызов утилиты](TAPE-INSPECT.ru.md).
Зависимости и параметры сборки неизменны. Новые raw tape conformance, IO и CLI suites дополняют
существующие тесты; IO/CLI требуют включённый tape inspector. Остальные полные типы, адаптеры,
импортер, транспорт и совместимость целого частного файла остаются вне границ.

## API default composite Quote

QDCXX-022 добавляет qd/encoding/utf_char.hpp в qdcxx_encoding, qd/core/wide_decimal.hpp в qdcxx_core,
а qd/events/quote.hpp — в qdcxx_quote (зависит только от core). Адаптер Quote возвращает проверенную
собственную LocalSchema с LocalProfile::QUOTE_FIELDS_V1. Передайте её TapeReader с прежними конечными
TapeLimits; прочитайте StreamDataRecord::typedValues и вызовите
events::mapQuote(record.symbol.units, record.typedValues, maxSymbolUnits). Результат владеет символом,
временами в миллисекундах, единицами кодов бирж и четырьмя raw WideDecimal с toDouble/toString.
Прежние схемы по умолчанию используют COMPACT_INT_V1 и сохраняют StreamDataRecord::values.

Опции сборки неизменны. Пример после сборки обоих инструментов:

```powershell
.work/tape/Release/qdcxx-tape-inspect --profile default-quote-v1 --input tests/fixtures/protocol/quote/expected/default-quote.qtp --output-format human
```

[Контракт](../spec/protocol/DEFAULT-QUOTE.ru.md), [CLI](TAPE-INSPECT.ru.md),
[авторская валидация и ограничения](../tests/conformance/protocol/quote/VALIDATION.ru.md).
Новые наборы: quote_java_values, quote_java_tape, quote_inspection_io и quote_inspection_cli.
IO/CLI требуют включённой опции tape tool. Прежние наборы и production-only конфигурация без
fetch/Python поддерживаются. Общий tape, RAW_DATA, время MESSAGE и загрузка схем не добавлены.

[English version](CPP-BUILD.md). Английский документ является основным.

`qdcxx_encoding` — production-библиотека C++23 без сторонних зависимостей с предварительными API
compact int/long и UTFString. QDCXX-010 добавил compact, QDCXX-011 добавляет
[utf_string.hpp](../include/qd/encoding/utf_string.hpp). Неизменные
[Java-свидетельства](../spec/encoding/PRIMITIVES.ru.md) определяют совместимость;
[политика UTFString и решение о переиспользовании](../spec/encoding/UTFSTRING.ru.md) определяют
явные границы C++. PentaCodec, importer, schema runtime, event payload decoding, frame writer, transport и packaging вне объёма.

## Ограниченный tape API и необязательная утилита

QDCXX-021 добавляет qd/protocol/tape_reader.hpp в qdcxx_protocol. Создайте TapeReader(schema, limits)
с конечными TapeLimits; reader владеет ограниченной копией схемы и неизменяемыми контекстами. feed
возвращает не более одного собственного frame и завершённого префикса данных; непотреблённый остаток
передаётся повторно. finish требует фактического EOF и принятого начального заголовка. В библиотеке
нет reset/замены и IO. [Контракт](../spec/protocol/TAPE-READER.ru.md) задаёт грамматику дескриптора,
атомарность, учёт построения, позиции и EOF. API предварительный; прежние контракты не меняются.

```powershell
cmake -S . -B .work/tape -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON
cmake --build .work/tape --config Release
ctest --test-dir .work/tape -C Release --output-on-failure
.work/tape/Release/qdcxx-tape-inspect --profile oracle-sre-v1 --input tests/fixtures/protocol/tape/expected/constrained.qtp
```

Targets qdcxx_tape_inspect_tool/qdcxx_tape_inspect оставляют IO/представление вне protocol/core. Инспекторы
независимо включаются с BUILD_TESTING=OFF; четыре комбинации собраны без загрузки doctest и поиска Python.
Новые тесты protocol_binary_tape, tape_inspection_io и tape_inspection_cli (процессные проверки CMake,
без нового Python); десять прежних наборов сохраняются при включённом старом инспекторе.
[Контракт CLI](TAPE-INSPECT.ru.md), [выполненная матрица и ограничения](../tests/conformance/protocol/tape/VALIDATION.ru.md).

## API DESCRIBE_RECORDS и проверки QDCXX-013

Подключите `qd/protocol/describe_records.hpp` и линкуйте qdcxx_protocol. Вызывайте
`parseDescribeRecords(payload, {4096, 64, 128, 1024, {256, 256}, 4096})` только для полного payload
типа 2 без envelope. Бюджеты: байты payload, descriptions, поля/description, всего полей,
для каждого имени {encoded body bytes, UTF-16 units} и суммарные units имён. Ноль допустим.
Владеющий результат содержит descriptions, status, stage, consumed, errorOffset и primitiveStatus.
Он сохраняет завершённые записи по порядку перед ошибкой, но не частичную текущую запись. Сохраняются
точные UTF-16, повторные ID/имена, VOID и структурно допустимые полные типы 0..255, включая не
поддерживаемые при Java descriptor construction. Это не обещает serialization operations или поддержки
локальных фабрик полей. Schema lookup и map не строятся. Allocation errors сохраняют завершённый
префикс; бросивший primitive не фиксирует consumption дальше своего начала. Точные приоритеты и
позиции описывает [контракт](../spec/protocol/DESCRIBE-RECORDS.ru.md).

CTest добавляет protocol_java_descriptions; qdcxx_description_tests линкует лишь qdcxx_protocol и test-only doctest.
ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES по умолчанию указывает на отдельный P08 descriptions.tsv и допускает временный
отрицательный выбор. Все прежние команды ниже включают новый target. [Покрытие/результаты](../tests/conformance/protocol/descriptions/VALIDATION.ru.md)
фиксируют 81 Java-наблюдение, 50 841 новое assertion, MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan,
BUILD_TESTING=OFF без acquisition, регрессии и неизменность отрицательных проверок. API предварительный;
event payloads, local binding, transport и writer вне объёма.

## API QTP frame reader и проверки QDCXX-012

`qdcxx_protocol` — production-компонент без внешних зависимостей, зависящий только от qdcxx_encoding.
Подключите `qd/protocol/frame_reader.hpp` и линкуйте qdcxx_protocol. Создайте
`org::ttldtor::qdcxx::protocol::FrameReader reader({1024, 1033});`: это бюджеты body и pending, включая запас
девяти байтов prefix. Несовместимые/переполняющиеся бюджеты дают invalid_argument; ошибка
выделения в конструкторе распространяется как исключение. API однопоточный и нереентерабельный.

`feed(input)` выдаёт не более одного владеющего кадра, status и точный accepted. Продвигайте
вход на accepted и повторно подавайте непринятый суффикс. FRAME_READY содержит frame; NEED_INPUT
означает отсутствие полного кадра, FAILED/FINISHED больше не принимают байты. `pending()` —
заимствованный view точных сохранённых байтов до следующей изменяющей операции. Возвращённые кадры
владеют payload и переживают feed/reset/уничтожение reader. `finish()` отличает чистую границу
от усечённого ввода; пустой feed не EOF. `reset()` сбрасывает только framer.

Первый framing error терминален; error()/errorOffset() дают причину и начало ошибочного кадра,
totalAccepted() — принятую позицию. Неверный payload и неизвестный допустимый type выдаются
непрозрачно. EMPTY_HEARTBEAT не содержит type, MESSAGE с type=0 отличается от него. Reader имеет
постоянный буфер body+9, выдаваемый payload — отдельную ограниченную память; сохранённые вызывающим
кадры не входят в суммарный бюджет reader. Ошибка выделения payload даёт ALLOCATION_FAILURE.
Точные wire/ресурсные/lifecycle границы определяет [контракт EN/RU](../spec/protocol/FRAMING.ru.md).

Команды ниже теперь собирают обе библиотеки и запускают также qdcxx_frame_tests/protocol_java_frames.
Новые тесты читают прежний P01 и отдельно добавленный P02; ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES и ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS
позволяют временные отрицательные наборы. MSVC/ClangCL/GCC Debug/Release и GCC ASan+UBSan проходят;
BUILD_TESTING=OFF не загружает doctest. [Покрытие](../tests/conformance/protocol/COVERAGE.ru.md),
[результаты и UTC logs](../tests/conformance/protocol/VALIDATION.ru.md). Дальнейшие payload/transport/
writer-компоненты требуют отдельной утверждённой задачи.

## UTFString API и текущие проверки

`readUtfString(input, {bodyBytes, codeUnits})` возвращает владеющие optional-единицы UTF-16, статус
и consumed. Значение интерпретируется только при OK: отсутствие означает null, присутствующая
пустая строка — empty. Ошибки не возвращают частичное значение; отказы выделения памяти могут
выбрасывать исключения. `writeUtfString(value, output, limits)` заимствует optional UTF-16 view и
атомарно записывает после preflight; вход/выход не должны пересекаться. Оба бюджета обязательны и
являются конечными эксплуатационными границами. Нет нормализации или исправления суррогатов.
Status дополнен INVALID_ENCODING, INVALID_LENGTH и RESOURCE_LIMIT без изменения compact.
Точные приоритеты, Java body overrun и гарантии выделения описаны в контракте.

Существующие команды ниже теперь собирают и проверяют оба codec. Имена executable/CTest сохранены
`qdcxx_compact_tests`/`compact_java_vectors` для совместимости с инструментами; они выполняют четыре
случая, 43 375 assertions и все 1146 Java observations (314 UTFString, 832 compact), без исключений.
MSVC, ClangCL и WSL GCC проходят Debug/Release; GCC ASan+UBSan также проходит. Исторические числа
QDCXX-010 ниже описывают прежний compact-only прогон. `BUILD_TESTING=OFF` по-прежнему ничего не
загружает; production-зависимость UTF не выбрана. Reuse probes — отдельные необязательные
игнорируемые проекты. [Текущая валидация](../tests/conformance/encoding/VALIDATION.ru.md) сохраняет UTC-свидетельства.

Дополнительные команды sanitizer из корня репозитория в WSL (закреплённый doctest из кэша):

```bash
python3 tools/run_utc.py --log .work/utf-asan-configure.log -- cmake -S . -B .work/utf-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' -DFETCHCONTENT_SOURCE_DIR_DOCTEST=/work/qd-cxx/.work/linux-debug/_deps/doctest-src
python3 tools/run_utc.py --log .work/utf-asan-build.log -- cmake --build .work/utf-asan -j 4
python3 tools/run_utc.py --log .work/utf-asan-test.log -- ctest --test-dir .work/utf-asan --output-on-failure
```

## Сборка и тестирование на Windows

Проверено с CMake 4.4.2, генератором `Visual Studio 17 2022`, x64, Windows SDK 10.0.26100.0,
MSBuild 17.14.51, MSVC 19.42.34444.0 (каталог инструментов 14.42.34433). Второй компилятор —
установленный ClangCL 19.1.5 с той же стандартной библиотекой MSVC и SDK. Тесты используют
doctest 2.4.12 через CMake FetchContent; URL архива и SHA-256 закреплены в
[CMakeLists.txt](../CMakeLists.txt). Только `qdcxx_compact_tests`, `qdcxx_frame_tests` и `qdcxx_description_tests` связываются с doctest; `BUILD_TESTING=OFF`
не загружает и не подключает его. Загрузки хранятся в `_deps/` игнорируемого каталога сборки;
первая конфигурация тестов требует сети. Менеджер пакетов не нужен. doctest 2.4.12 выдаёт
нефатальное предупреждение CMake об устаревшей минимальной версии policy; загруженный исходник не менялся. Требуется CMake 3.25+; цели наследуют `cxx_std_23` от библиотеки,
расширения отключены. Для этих toolsets CMake выбирает `/std:c++latest` для поддержки C++23.

Из корня репозитория в PowerShell:

```powershell
python tools/run_utc.py --log .work/msvc-configure.log -- cmake -S . -B .work/msvc -G "Visual Studio 17 2022" -A x64
python tools/run_utc.py --log .work/msvc-debug-build.log -- cmake --build .work/msvc --config Debug
python tools/run_utc.py --log .work/msvc-debug-test.log -- ctest --test-dir .work/msvc -C Debug --output-on-failure
python tools/run_utc.py --log .work/msvc-release-build.log -- cmake --build .work/msvc --config Release
python tools/run_utc.py --log .work/msvc-release-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
python tools/run_utc.py --log .work/clang-configure.log -- cmake -S . -B .work/clang -G "Visual Studio 17 2022" -A x64 -T ClangCL
python tools/run_utc.py --log .work/clang-debug-build.log -- cmake --build .work/clang --config Debug
python tools/run_utc.py --log .work/clang-debug-test.log -- ctest --test-dir .work/clang -C Debug --output-on-failure
python tools/run_utc.py --log .work/clang-release-build.log -- cmake --build .work/clang --config Release
python tools/run_utc.py --log .work/clang-release-test.log -- ctest --test-dir .work/clang -C Release --output-on-failure
```

Сборка выполняется вне корня исходников; `.work/` и `build/` игнорируются. Конфигурация внутри
корня исходников отклоняется. `BUILD_TESTING=OFF` собирает только библиотеки qdcxx_encoding и qdcxx_protocol. `ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES`
может указывать на временные копии fixtures для негативных проверок; по умолчанию используются
эталоны репозитория. Первая попытка обнаружения SDK через MSBuild в песочнице получила отказ
в доступе к локальному каталогу Microsoft SDKs; разрешённые configure/build выше успешно выполнены
с повышением разрешений песочницы. Это ограничение окружения, а не ошибка codec.

## API и владение

Все четыре функции находятся в `org::ttldtor::qdcxx::encoding`, заимствуют принадлежащий вызывающему `std::span`
только на время вызова, не выделяют память и не сохраняют указатели. Результаты владеют значениями
и счётчиками. Вызывающий обеспечивает валидность span на протяжении вызова. Порядок байтов явно
big-endian; чтение не требует выравнивания.

- `readCompactInt` / `readCompactLong` возвращают `ReadResult<int32_t/int64_t>`: `value`, `consumed`,
  `status`. Успех потребляет ровно одно значение; хвост доступен через `input.subspan(consumed)`.
  Избыточные формы принимаются; int возвращает младшие 32 бита со знаком даже для long-кодировки.
- Пустой/неполный ввод возвращает `INSUFFICIENT_INPUT`, нулевое значение и всю длину ввода в
  `consumed`. Это соответствует наблюдаемой категории/позиции Java EOF, но не строкам класса/сообщения
  исключения. Отката нет: сохраните исходные байты и повторите чтение с начала после поступления
  данных. Частично потреблённая позиция не является состоянием для продолжения; транзакционный
  framing реализован отдельным слоем qdcxx_protocol.
- `writeCompactInt` / `writeCompactLong` возвращают `WriteResult`: `written`, `status`. Они создают
  канонические Java bytes (не более 5/9). Недостаточная ёмкость, включая нулевую, возвращает
  `INSUFFICIENT_OUTPUT`, нулевой written и оставляет весь выход неизменным. При успехе изменяются
  только первые `written` байтов. Это политика ограниченного C++ writer, а не утверждение о сбоях
  вывода в Java stream.

Беззнаковое накопление/маскирование и восстановление битов фиксированной ширины сохраняют знак
без signed overflow, отрицательных сдвигов, сдвигов на ширину типа и зависимости от порядка байтов
хоста. Выбор размера writer использует положительные представимые степени двойки. Проверка строгой
каноничности reader не добавлена.

## Проверки и форматирование

[Покрытие](../tests/conformance/encoding/COVERAGE.ru.md) и
[валидация](../tests/conformance/encoding/VALIDATION.ru.md) фиксируют включённые/исключённые случаи,
ошибки и ограничения. [Инструкции форматирования](../tools/format/CPP.ru.md) определяют закреплённый
pipeline из двух инструментов; имена и содержательный Doxygen проверяются отдельно.

## Linux / WSL и журналы UTC

Также проверено в WSL2 Ubuntu 24.04.4 LTS, x86_64, kernel 6.18.40.1-microsoft-standard-WSL2,
GCC 13.3.0 (libstdc++), CMake 4.4.2 и Ninja 1.11.1. Debug и Release проходят те же два случая doctest
(28 358 assertions) и неизменные 832 выбранных Java observations. Конфигурация/сборка только библиотеки
с `BUILD_TESTING=OFF` успешна без создания `_deps/`. Негативные fixture/parser/CTest проверки Linux
успешны; `--skip-format` явно пропускает проверки Windows-форматтеров, уже выполненные на Windows.
Sanitizers, другие дистрибутивы Linux, macOS и другие архитектуры не проверялись.

Из Ubuntu WSL (`wsl -d Ubuntu-24.04`), в смонтированном репозитории:

```bash
cd /work/qd-cxx
for config in Debug Release; do
  build=.work/linux-${config,,}
  python3 tools/run_utc.py --log "$build-configure.log" -- cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$config"
  python3 tools/run_utc.py --log "$build-build.log" -- cmake --build "$build"
  python3 tools/run_utc.py --log "$build-test.log" -- ctest --test-dir "$build" --output-on-failure
done
python3 tools/run_utc.py --log .work/linux-negative.log -- python3 tests/conformance/check_compact_negative.py .work/linux-release/qdcxx_compact_tests --skip-format --report .work/linux-negative-proof.json
cmake -S . -B .work/linux-library -G Ninja -DBUILD_TESTING=OFF
cmake --build .work/linux-library
```

[run_utc.py](../tools/run_utc.py) требует Python 3.11+, ставит каждой сохраняемой строке вывода,
запуску и завершению команды время ISO 8601 UTC (`Z`) и задаёт `TZ=UTC` дочерним процессам.
Это также переводит создаваемый CTest LastTest.log на UTC. Текст вывода и код возврата дочерней
команды сохраняются; часовой пояс хоста не меняется. Текущие C++-журналы созданы новыми запусками
в UTC, а не переобозначены; исторические Java oracle captures сохраняют исходные байты/часовые пояса.
Новые JSON-отчёты негативных проверок содержат `executedAtUtc`. Публичные функции и тестовые утилиты
теперь описывают каждый параметр/параметр шаблона и результат, отличный от void.

## API статического type-8 binding (QDCXX-014)

qdcxx_core владеет проверенным неизменяемым локальным подмножеством и не зависит от protocol/transport.
Подключить qd/core/local_schema.hpp; LocalSchema::create(records, {recordCount, fieldCount, lookupCount,
nameUnits, totalNameUnits}) возвращает std::expected с владеющей схемой или явной ошибкой. Записи содержат
local ID/name/hasTime, полные типы полей и явные record lookup keys с целями в позициях полей.
Поддерживаются только hasTime=false и точный тип 8. Входные файлы/provenance не нужны.

Подключить qd/protocol/static_binding.hpp и линковать qdcxx_protocol. bindRecord(raw, schema,
{fieldCount, nameUnits, totalNameUnits}) возвращает независимый владеющий RecordBinding или явную ошибку.
Отсутствующий localRecordId означает описанную неизвестную запись; отсутствующий targetFieldId — type-8 skip.
Нет описания — нет плана. Defaults, event decoder и session map не подразумеваются. Все бюджеты
обязательны и конечны, считают логические элементы; allocation failure не публикует частичный результат.
См. [контракт](../spec/protocol/STATIC-BINDING.md) и [валидацию](../tests/conformance/protocol/binding/VALIDATION.md).
CTest теперь добавляет qdcxx_binding_tests/protocol_java_binding; ORG_TTLDTOR_QDCXX_BINDING_FIXTURES выбирает проверенный plans.tsv.
Все четыре targets проходят описанную compiler matrix, GCC sanitizers и negative checks. BUILD_TESTING=OFF
собирает qdcxx_core/qdcxx_encoding/qdcxx_protocol без загрузки doctest. Существующие примеры команд применимы без изменений.

## API символов QDCXX-015

Подключить qd/encoding/symbol_reader.hpp и линковать qdcxx_encoding. Создать
`SymbolReader reader({4096, 1024, 1024}, 0);` с бюджетами входных байтов, возвращаемых и сохраняемых units
и нулевой маской EMPTY_SET. `beginMessage(0)` сбрасывает символ/MRU/ошибку; вызывать между полными
сообщениями. `read(remainingPayload)` возвращает одно собственное значение с UTF-16, cipher и raw flags,
consumed count и явным status. Все ошибки read terminal; ненулевые маски не поддерживаются.
[Контракт](../spec/encoding/SYMBOLS.ru.md) и [финальная валидация](../tests/conformance/encoding/symbols/VALIDATION.ru.md).
CTest добавляет encoding_java_symbols / qdcxx_symbol_tests; ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES выбирает неизменяемый Java TSV.
Production dependency и event decoder не добавлены. Прежние исключения PentaCodec выше исторические.

## STREAM_DATA API (QDCXX-016)

Подключить `qd/protocol/stream_data.hpp` и линковать qdcxx_protocol. Сначала вызвать
`StreamDataContext::create(schema, descriptions, limits)` с проверенной LocalSchema и уникальными raw
ordinary type-8 descriptions. Неизменяемый результат владеет планами и числом локальных полей. Затем
вызвать `decodeStreamData(payload, context, limits)` для одного полного type-15 payload без оболочки.
ContextLimits ограничивает schema storage и aggregate descriptions/actions/names. StreamDataLimits
ограничивает payload, processed records включая skips, wire fields, emitted records/values/symbol units
и SymbolLimits. Unlimited defaults нет. Ненулевые raw flags ошибочны до sender ID; каждый вызов сбрасывает
символы. Результаты владеют local ID/symbols/values и сохраняют полный префикс при поздней ошибке.
Точные приоритеты и позиции: [контракт](../spec/protocol/STREAM-DATA.md). Frame overload не предоставляется.

CTest добавляет `protocol_java_stream_data` / `qdcxx_stream_data_tests`; ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES допускает
намеренные временные fixture overrides. Прежние build команды запускают все шесть suites. Полная compiler/
sanitizer/production-only матрица и negative checks: [валидация](../tests/conformance/protocol/stream-data/VALIDATION.md).
Transport, dynamic session, nonzero flags, importer и production dependency не добавлены.

## Офлайн-инструмент QDCXX-017

Включите `-DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON` для ограниченного executable `qdcxx-qtp-inspect`; по умолчанию
OFF сохраняет сборку только библиотек. `-DBUILD_TESTING=OFF` никогда не получает тестовые
зависимости, в том числе при включённом инструменте. [CLI/профиль/лимиты](QTP-INSPECT.ru.md),
[контракт](../spec/protocol/OFFLINE-INSPECTION.ru.md). При включённых тестах и инструменте Python 3
обеспечивает process-level проверки CTest, doctest — переносимые инъекции ошибок I/O. Шесть
прежних suites остаются включены. Новых production-зависимостей и публичного session framework нет.
StreamDataResult теперь возвращает списанные `processedRecords`/`wireFields` согласно
принятому решению, без изменения wire или позиций.
