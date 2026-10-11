# Ограниченные компоненты C++23

[English version](CPP-BUILD.md). Английская версия является нормативной.

## Ограниченный tape writer в памяти

Подключите `qdcxx_protocol` и `qd/protocol/tape_writer.hpp` для использования
`org::ttldtor::qdcxx::protocol::OutgoingLayouts` и `TapeWriter`.
[Контракт writer](../spec/protocol/TAPE-WRITER.ru.md) определяет явное несжатое подмножество
`RAW_MESSAGE_V1`, точные полные типы 2/8/10/40/56/72/88/120, различия descriptor,
конечные логические бюджеты и порядок отказов. API не выполняет файловый I/O.

Создайте проверенную `core::LocalSchema`, передайте полные объявления `OutgoingRecord`
с явными sender ID, local ID и lookup names и вызовите `OutgoingLayouts::create`
с `LayoutLimits`. Имена и порядок полей задаёт вызывающая сторона; каждое поле выбранной
локальной записи должно присутствовать ровно один раз. Layouts владеют ограниченными копиями
схемы/объявлений и открывают const views. Sender ID используют разреженный нативный lookup
и должны находиться в 0..INT32_MAX-1. Исключённая граница переполняет remapping ID закреплённого
Java parser; другие большие ID всё ещё могут превысить отдельные ограничения Java dense-array/heap.

`TapeWriter::create(layouts, descriptor, limits)` независимо владеет всей конфигурацией
до возврата READY. Задайте все поля `WriterLimits`: ноль означает реальную границу.
`begin()` выдаёт единственный descriptor, `describe(ids)` объявляет одну полную группу,
а `data(records)` выдаёт один полный RAW_DATA batch со значениями в локальном логическом порядке.
`heartbeat(value)` и `marker()` сохраняют явные границы frames. `finish()` не выдаёт байтов
и идемпотентен только после успешного finish. Время и marker не синтезируются.

Каждая операция возвращает `std::expected<WriterChunk, WriterDiagnostic>`. Успешные chunks
владеют одним полным envelope и остаются действительными после следующих операций или уничтожения
writer. Views записей заимствуют symbol/value storage только на время `data()`; входные layouts
и descriptor заимствуются только при построении. Неудачная операция не возвращает байтов,
не фиксирует объявления или output/frame counters и сохраняет первую terminal diagnostic.
Последующие вызовы возвращают ту же diagnostic без проверки входов или новых work charges.
Work/attempt counters сохраняют расходы неудачных попыток. Scratch limits ограничивают закрытое
логическое хранилище байтов/units, а не накладные расходы allocator или caller-held chunks.

`encoding::writeCharArray` из `qd/encoding/char_array.hpp` пишет nullable точные UTF-16 arrays
через прежние compact/UTFChar writers, сохраняя null, empty и isolated surrogate units.
Writer выдаёт raw symbols с FD и повторами FF внутри сообщения. Обещаний кратчайшего cipher
или побайтового совпадения целого Java tape нет. Writer и primitive checks являются обычными
CTest targets `protocol_tape_writer` и `encoding_char_array_writer`;
[валидация writer](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.ru.md) фиксирует
выполненные профили и отдельные наблюдения production parser.

Для генерации документированной справки C++ API выполните `doxygen tools/docs/Doxyfile`
из корня репозитория. Начальная HTML-страница — `.work/api/html/index.html`;
сгенерированная документация остаётся вне отслеживаемого дерева исходников.

## Офлайн-экспорт Java-схемы

[Необязательный Java-экспортёр](../tools/schema-export/README.ru.md) разрешает допустимые источники
settings/XML закреплённой Java и создаёт существующий native JSON версии 1. Java/JDK 8 и Maven
нужны только для явного экспорта/регенерации, а не обычных C++ configure/build/CTest и использования схемы.
Используйте `loadJsonSchema` и prepared API Quote/Profile ниже с явными экспортированными ID записей
и тем же контекстом загруженной схемы. Экспортёр сохраняет реальные lookup keys и проецирует Java
slots в логические поля; sidecar фиксирует отображение. Generic inspector принимает полученный
`schema.json` через `--schema`, сохраняя вывод версии 6 и оба существующих режима tape.

При включённых JSON loader, tape inspector и `BUILD_TESTING` тесты `schema_export_consumption` и
`schema_export_cli` читают [сохранённые production observations](../tests/fixtures/schema/export/expected)
без запуска Java, Maven, Python или PowerShell. Они охватывают настроенные события и пользовательские/
дополнительные XML-поля. [Контракт экспорта](../spec/schema/JAVA-EXPORT.ru.md) и
[авторская валидация](../tests/conformance/schema-export/VALIDATION.ru.md) разделяют поддержанную проекцию,
выполненные проверки и независимую приёмку. C++ по-прежнему не разрешает XML или Java properties.

## Нативный загрузчик JSON-схем

`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON` по умолчанию ON и добавляет `qdcxx_schema_json`.
Подключите `qd/schema/json_schema.hpp` и этот target для использования
`org::ttldtor::qdcxx::schema::loadJsonSchema(std::string_view, JsonSchemaLimits = {})`.
Функция возвращает `std::expected<core::LocalSchema, schema::JsonSchemaError>` и принимает переданные
UTF-8 байты без файлового I/O и неявных настроек. [Нативный контракт](../spec/schema/NATIVE-JSON.ru.md)
определяет полную грамматику, владение, ошибки и конечные значения по умолчанию.
`JsonSchemaLimits::documentBytes` и `depth` ограничивают разбор; `schema` задаёт прежние бюджеты
элементов/имён core. Это логические границы, а не точный предел heap или гарантия линейного времени
всей проверки.

CMake получает [исходники simdjson 4.6.4](https://github.com/simdjson/simdjson/releases/tag/v4.6.4)
из [закреплённого архива](https://codeload.github.com/simdjson/simdjson/tar.gz/refs/tags/v4.6.4),
SHA-256 `b091107844fe928158c5c2265c20360fff312889ddf7ebc4528a0f0f8f2ff9cd`.
Компилируется только amalgamation как закрытая зависимость; upstream tests/tools и компилятор C
не требуются. Сохранена [атрибуция MIT](../third_party/licenses/simdjson-MIT.txt).
Публичные заголовки содержат только типы проекта/STL. Core, codecs, protocol и event targets не
зависят от загрузчика или simdjson; закрытая зависимость линковки появляется только у потребителей,
подключающих необязательный загрузчик.

Для Windows-сборки с несколькими конфигурациями, используемой в примерах CLI:

```text
cmake -S . -B .work/schema-json-msvc -DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON
cmake --build .work/schema-json-msvc --config Release
```

Для Linux-сборки с одной конфигурацией используйте
`-B .work/schema-json-linux -DCMAKE_BUILD_TYPE=Debug` с теми же параметрами возможностей,
затем `cmake --build .work/schema-json-linux`.
Чтобы собрать только прежний путь инспекции со встроенными схемами, настройте отдельный новый
каталог сборки с `-DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF` и нужными параметрами
инспектора. OFF не получает и не линкует simdjson. Встроенные команды доступны; корректный запрос
`--schema` сообщает `LOADER_DISABLED` (выход 4) до файлового I/O. Обычный configure/build/CTest не
получает новых требований Python или PowerShell. Это инструкции использования, не отчёт о
выполненных проверках.

[Смешанный нативный пример](../spec/schema/examples/native/mixed.json) содержит пользовательскую
запись с локальным ID 0, переставленный Quote с ID 1 и переставленный Profile с ID 2, с optional и
дополнительными полями. На владеющей границе приложения прочитайте этот файл с конечным лимитом
байтов; следующая функция показывает загрузку и обе подготовки. Подключите `qdcxx_schema_json`,
`qdcxx_quote` и `qdcxx_profile`. Приложение выбирает конечные бюджеты отображения независимо от
бюджетов загрузки:

```cpp
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/schema/json_schema.hpp"

#include <string_view>

/** Prepare both event layouts from the mixed native example.
 * @param schemaBytes UTF-8 contents supplied by the application's bounded file reader.
 * @return True when loading and both preparations succeed; false on any typed failure.
 */
bool prepareExample(std::string_view schemaBytes) {
    using namespace org::ttldtor::qdcxx;

    auto loaded = schema::loadJsonSchema(schemaBytes);

    if (!loaded) {
        return false;
    }

    const events::MappingLimits limits{
        .fields = 64, .lookups = 128, .selections = 32, .keys = 128,
        .typeAlternatives = 64, .nameUnits = 1024,
        .totalNameUnits = 16384, .comparisonWork = 1048576};
    auto quote = events::PreparedQuote::prepare(*loaded, 1, limits);
    auto profile = events::PreparedProfile::prepare(*loaded, 2, limits);

    return quote.has_value() && profile.has_value();
}
```

Функция показывает только подготовку. Обрабатывающее приложение сохраняет успешно загруженную
схему и подготовленные отображения, передаёт тот же контекст схемы декодеру и отображает полные
значения `RecordValueView` с явными лимитами владения события, как описано ниже. Копии сохраняют
контекст; повторная загрузка даже идентичных байтов создаёт независимую identity и не может
повторно использовать первое подготовленное отображение. Уничтожение входа и парсера не нарушает
валидность загруженной схемы. Подготовка проверяет lookups, типы и обязательные поля; одного имени
записи недостаточно для совместимости. Общие записи и дополнительные поля доступны независимо
от проекции событий. [Внешний CLI](TAPE-INSPECT.ru.md) всегда использует общий вывод версии 6,
без выбора отображения события.

Принимаются только `qdcxx-native` версии 1, `hasTime:false` и точные полные ID
2/8/10/40/56/72/88/120. Прежний XML-captured JSON-профиль, XML/imports, разрешение Java properties,
URLs, автоматический выбор схемы, замена во время работы, новые wire-типы/преобразования и транспорт
остаются неподдерживаемыми.

## Актуальный переносимый процесс разработки

Нужны CMake 3.25+, компилятор C++23 и нативные средства сборки. [Инструкции средств разработки](../tools/dev/README.ru.md)
определяют автономный bootstrap, контракт native `qdcxx-run-utc` и команды Windows/Linux.
Bootstrap — исключение первой сборки: UTC-границы фаз, точные аргументы команд и отдельные исходные
потоки заменяют время каждой строки, пока logger ещё не собран. Обычный configure/build/CTest и новые
деревья команд logger/formatter не требуют Python или PowerShell; интерактивная оболочка находится
вне этого утверждения о зависимостях. `BUILD_TESTING=OFF` не собирает средства разработки и не загружает doctest.

Форматирование использует явно заданные native Uncrustify 0.83.0, затем clang-format 20.1.8;
корневые конфигурации и dependency locks неизменны. Выполните Format, затем Check с точными путями executable:

```text
cmake -DMODE=Format "-DUNCRUSTIFY=<native-uncrustify>" "-DCLANG_FORMAT=<native-clang-format>" -P tools/format/Format-Cpp.cmake
cmake -DMODE=Check "-DUNCRUSTIFY=<native-uncrustify>" "-DCLANG_FORMAT=<native-clang-format>" -P tools/format/Format-Cpp.cmake
```

[Инструкции форматтера](../tools/format/CPP.ru.md) описывают JSON-выбор файлов, область обработки,
подготовку native Linux и необязательные неизменённые Windows PowerShell downloader/wrappers.
Получение и исполнение разделены. Java tooling и оставшиеся ручные Python-проверки сохраняют свои требования.

QDCXX-028 U1–U5 реализованы и проверены автором на Windows/Linux, включая исправленные повторные запуски;
[новая авторская валидация](../tests/conformance/tooling/VALIDATION.ru.md) фиксирует фактические результаты
отдельно от исторических доказательств. Независимая приёмка открыта. macOS непроверена и не блокирует работу.
Ниже результаты эпохи отдельных компонентов сохраняют первоначальную область и не доказывают текущую матрицу.

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

Дополнительные команды sanitizer из корня репозитория в WSL после bootstrap Linux ниже (закреплённый doctest из кэша). ASan/UBSan и LSan отражайте отдельно; при ptrace-проверках с `detect_leaks=0` LSan не заявляется:

```sh
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-configure.log -- cmake -S . -B .work/utf-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer" "-DFETCHCONTENT_SOURCE_DIR_DOCTEST=/work/qd-cxx/.work/linux-debug/_deps/doctest-src"
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-build.log -- cmake --build .work/utf-asan -j 4
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-test.log -- ctest --test-dir .work/utf-asan --output-on-failure
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

Актуальные команды из корня репозитория (PowerShell допустим как интерактивная оболочка):

```text
cmake "-DBINARY=.work/dev" "-DLOG=.work/dev-bootstrap.log" "-DGENERATOR=Visual Studio 17 2022" -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-configure.log -- cmake -S . -B .work/msvc "-GVisual Studio 17 2022" -A x64
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-debug-build.log -- cmake --build .work/msvc --config Debug
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-debug-test.log -- ctest --test-dir .work/msvc -C Debug --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-release-build.log -- cmake --build .work/msvc --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-release-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-configure.log -- cmake -S . -B .work/clang "-GVisual Studio 17 2022" -A x64 -T ClangCL
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-debug-build.log -- cmake --build .work/clang --config Debug
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-debug-test.log -- ctest --test-dir .work/clang -C Debug --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-release-build.log -- cmake --build .work/clang --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-release-test.log -- ctest --test-dir .work/clang -C Release --output-on-failure
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

Следующий абзац сохраняет исходную валидацию compact-компонента. Актуальные результаты компиляторов
и sanitizers без Python записаны отдельно в [валидации QDCXX-027](../tests/conformance/python-migration/VALIDATION.ru.md).

Также проверено в WSL2 Ubuntu 24.04.4 LTS, x86_64, kernel 6.18.40.1-microsoft-standard-WSL2,
GCC 13.3.0 (libstdc++), CMake 4.4.2 и Ninja 1.11.1. Debug и Release проходят те же два случая doctest
(28 358 assertions) и неизменные 832 выбранных Java observations. Конфигурация/сборка только библиотеки
с `BUILD_TESTING=OFF` успешна без создания `_deps/`. Негативные fixture/parser/CTest проверки Linux
успешны; `--skip-format` явно пропускает проверки Windows-форматтеров, уже выполненные на Windows.
Sanitizers, другие дистрибутивы Linux, macOS и другие архитектуры не проверялись.

Актуальные команды из POSIX shell Ubuntu WSL (`wsl -d Ubuntu-24.04`), в смонтированном репозитории:

```sh
set -e
cd /work/qd-cxx
cmake -DBINARY=.work/dev-linux -DLOG=.work/dev-linux-bootstrap.log -DGENERATOR=Ninja -DCONFIG=Release -P tools/dev/Bootstrap.cmake
for config in Debug Release; do
    build=".work/linux-$config"
    .work/dev-linux/qdcxx-run-utc --log "$build-configure.log" -- cmake -S . -B "$build" -G Ninja "-DCMAKE_BUILD_TYPE=$config"
    .work/dev-linux/qdcxx-run-utc --log "$build-build.log" -- cmake --build "$build"
    .work/dev-linux/qdcxx-run-utc --log "$build-test.log" -- ctest --test-dir "$build" --output-on-failure
done
.work/dev-linux/qdcxx-run-utc --log .work/linux-library-configure.log -- cmake -S . -B .work/linux-library -G Ninja -DBUILD_TESTING=OFF
.work/dev-linux/qdcxx-run-utc --log .work/linux-library-build.log -- cmake --build .work/linux-library
```

Сохранённый необязательный [Run-Utc.ps1](../tools/Run-Utc.ps1) требует PowerShell 7.4+ и массив аргументов,
отмечает строки и запуск/завершение команды временем ISO 8601 UTC (`Z`), задаёт только
дочерний `TZ=UTC` и сохраняет код выхода. Это текстовый журнал UTF-8 с заменой неверных
последовательностей и конечным LF, включая последнюю незавершённую строку. CR/CRLF/LF
разделяют строки; порядок внутри каждого потока сохраняется, порядок между stdout/stderr
не гарантирован. Ошибки запуска, журнала и консоли возвращают 1; внутренний worker создаёт Windows job
или Unix process group до запуска команды, поэтому очистка работает и после её завершения. Ожидание
worker при очистке ограничено 5 секундами; намеренно отделённые Unix sessions вне этой границы.
Сырые CLI evidence сохраняются отдельно.
Исторические журналы и [старый logger](../tools/run_utc.py) не переписываются.

Ручная проверка ниже пока требует Python; она не перенесена и не заменена результатом CTest.
Остальные отложенные процессы перечислены в [реестре миграции](PYTHON-MIGRATION.ru.md).

```powershell
& ./tools/Run-Utc.ps1 -Log .work/linux-negative.log -Executable python3 -ArgumentList @('tests/conformance/check_compact_negative.py','.work/linux-release/qdcxx_compact_tests','--skip-format','--report','.work/linux-negative-proof.json')
```

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
[контракт](../spec/protocol/OFFLINE-INSPECTION.ru.md). При включённых тестах и инструменте CMake и тестовый C++ checker
обеспечивают process-level проверки CTest, doctest — переносимые инъекции ошибок I/O. Шесть
прежних suites остаются включены. Новых production-зависимостей и публичного session framework нет.
StreamDataResult теперь возвращает списанные `processedRecords`/`wireFields` согласно
[принятому решению](../spec/protocol/STREAM-DATA.ru.md#aggregate-accounting), без изменения wire или позиций.
