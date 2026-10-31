# Native binary schema JSON, версия 1

[English version](NATIVE-BINARY-JSON.md). Английская версия является нормативной.

Этот отдельно выбираемый формат задаёт владеющие объявления `LocalProfile::BINARY_FIELDS_V1`
с явной индексной/time-разметкой. Подключите `qd/schema/json_schema.hpp` и вызовите
`org::ttldtor::qdcxx::schema::loadBinaryJsonSchema(input, limits)` из необязательного `qdcxx_schema_json`.
[Обычный native JSON](NATIVE-JSON.ru.md), `loadJsonSchema`, `JsonSchemaLimits`, номера ошибок и
отклонение версии 2 сохранены. Ни один API не определяет и не преобразует профиль автоматически.

## Грамматика документа

Вход — ровно один UTF-8 JSON-документ с необязательными окружающими JSON-пробелами, без BOM.
Пустой вход, неправильные UTF-8/escape-последовательности, непарные surrogate escapes, комментарии,
завершающие запятые и последующие документы дают `MALFORMED_JSON`. Все члены ниже обязательны.
Объекты отклоняют неизвестные и повторные декодированные имена членов, включая разные escape-записи
одного ключа. Порядок членов несущественен; порядок массивов сохраняется.

| Объект | Точные члены |
| --- | --- |
| Корень | `formatVersion`: integer; `profile`: object; `records`: array |
| Профиль | `id`: string; `version`: integer |
| Запись | `id`: integer; `name`: string; `hasTime`: boolean; `fields`: array; `lookups`: array; `indexedTimeLayout`: null или object |
| Поле | `serialTypeId`: integer |
| Lookup | `name`: string; `fieldId`: integer |
| Парная разметка | `kind`: `"pair-int32"`; `highFieldId`: integer; `lowFieldId`: integer |
| Одиночная разметка | `kind`: `"single-int64"`; `fieldId`: integer |

`formatVersion` равен `1`; профиль — ровно `{"id":"qdcxx-native-binary","version":1}`.
Другие корректно типизированные версии дают `UNSUPPORTED_VERSION`, другие ID — `UNSUPPORTED_PROFILE`.
Идентичность проверяется до интерпретации записей. Числовым членам нужна целочисленная JSON-запись
в `0..2147483647`; `-0` равен нулю. Дробная/экспоненциальная запись, неверный тип и выход успешно
разобранного значения за диапазон дают `INVALID_SCHEMA`. Некорректное число и числовое переполнение
парсера дают `MALFORMED_JSON`.

ID записей равны их позициям с нуля независимо от wire sender ID. Имена записей непусты и уникальны;
имена lookup непусты и уникальны внутри записи. Имена становятся владеющим точным UTF-16 scalar text
без нормализации, обрезки, смены регистра и создания aliases. Escaped NUL и дополнительные Unicode
скаляры сохраняются. Неявные логические ID полей — позиции с нуля внутри записи. Каждому полю нужен
хотя бы один lookup; каждый lookup ссылается на существующее поле. Разные имена могут указывать на
одно поле. Пустые схемы и записи без полей допустимы при соблюдении этих инвариантов.
Нормативная проверка остаётся в `LocalSchema::create`, получающей исходные лимиты counts/names.

Допустимы полные ID **0/1/2/3/4/8/9/10/24/40/56/72/88/104/120/137/152/233/249** из
[FIELD-TYPES](../encoding/FIELD-TYPES.ru.md). Битовая маска не расширяет этот набор. JSON объявляет
идентичности, а не значения полей. Прежние codecs сохраняют точные nullable UTF-16, пустые/null bytes
и opaque payload. Java slots, синтетические VOID tails, единицы, преобразования и event mapping не выводятся.

## Явная индексная/time-разметка

| `hasTime` | `indexedTimeLayout` | Полные ID составляющих |
| --- | --- | --- |
| `false` | `null` | Без составляющих |
| `true` | Парный объект | Каждая из **4/8/24/40/56/72/88** |
| `true` | Одиночный объект | Одна из **104/120/152** |

ID пары различны и ссылаются на логические поля. `highFieldId` задаёт первую/high составляющую
core-разметки, `lowFieldId` — вторую/low независимо от wire-порядка, имён и Java slots. Одиночная
составляющая задаёт signed 64-bit разметку. Неполные, повторные, dangling и несогласованные объявления
дают `INVALID_SCHEMA`. Неизвестный kind и поддерживаемый тип в неподдерживаемой роли дают
`UNSUPPORTED_INDEXED`; полный ID поля вне каталога даёт `UNSUPPORTED_TYPE`. Имена не задают разметку.

Файлы [pair](examples/native-binary/pair.json), [single](examples/native-binary/single.json) и
[ordinary](examples/native-binary/ordinary.json) — полные JSON-объявления. Даже пустой или обычный документ
при успехе этого API использует `BINARY_FIELDS_V1`.

Парный пример — явно заданная локальная проекция `TimeAndSale`, выбирающая только `Time`, `Sequence` и `Price`. Квалификация полной стандартной схемы события и её адаптера относится к отдельному объёму поддержки.

## Владение и ошибки

Вход заимствуется неизменным только на время синхронного вызова и не требует padding. Хранилище
входа, DOM и парсера не покидает вызов. Успех возвращает полную владеющую неизменяемую `LocalSchema`
с независимым provenance. Копии разделяют прежнюю идентичность; независимые загрузки создают чужие
идентичности даже для одинакового входа. Ошибка не публикует частичный результат и не изменяет
схемы/идентичность вызывающей стороны. Сохранённые contexts и дальнейшие копии подчиняются прежним core-контрактам.

Сохранены токены `MALFORMED_JSON`, `INVALID_SCHEMA`, `UNSUPPORTED_PROFILE`, `UNSUPPORTED_VERSION`,
`UNSUPPORTED_TYPE`, `UNSUPPORTED_INDEXED`, `RESOURCE_LIMIT`, `ALLOCATION_FAILURE`. Диагностика не
повторяет вход/путь и сообщения парсера. Общий порядок независимых ошибок не обещан; ресурсные
ошибки и ошибки разбора могут препятствовать структурным проверкам.

## Независимые конечные лимиты

`BinaryJsonSchemaLimits::loading` сохраняет [обычные defaults](NATIVE-JSON.ru.md):
1 048 576 входных bytes, depth 16, 1 024 записи, 16 384 поля, 32 768 lookup, 1 024 UTF-16 units на
имя и 1 048 576 суммарных units имён. Дополнительно независимо пересекаются три лимита:

| Поле API | Параметр CLI | Default | Область |
| --- | --- | --- | --- |
| `parserBytes` | `--max-schema-parser-bytes` | 67 108 864 bytes | Обычные запросы аллокации свежего закреплённого парсера |
| `workUnits` | `--max-schema-work` | 67 108 864 units | Логические кредиты bytes/visits/comparisons |
| `schemaModelBytes` | `--max-schema-model-bytes` | 67 108 864 bytes | Логическая модель двух копий объявлений и controls |

Ноль буквален; равенство допустимо. Превышение лимита или проверяемой арифметики даёт `RESOURCE_LIMIT`.
Документ в прежних counts может превысить новый бюджет. Допуск использует фактический вход/counts
даже при огромных настроенных максимумах. Ошибка не запускает повтор с увеличенным бюджетом.

Для фактических bytes `B`, `C=max(B,32)`, `D=min(loading.depth,B)+1` и проверяемого округления вверх `round64`:

```text
parserRequests = (B+64) + 4*(round64(C)+9) + 8*round64(C+3)
    + round64(floor(5*C/3)+64) + 9*D + H
H = sizeof(simdjson::internal::dom_parser_implementation)
    + 6*sizeof(void*) + alignof(max_align_t)
```

Все промежуточные выражения, включая `5*C` и запас округления, проверяются до SDK allocation.
Четырёхбайтовые structural indexes, восьмибайтовые tape slots и девять bytes на depth следуют исходникам.
Все девять встроенных реализаций имеют одинаковые переменные выражения и пять слов derived state;
`H` добавляет консервативный запас слова/alignment. Одна аллокация свежего парсера и один parse
включают первые аллокации DOM и padded input без повторного роста capacity. Metadata аллокатора,
вход вызывающей стороны и посторонние аллокации процесса вне этой области запросов.

Для `R` записей, `F` полей, `K` lookup, суммы units имён `N`, самого длинного имени `U` и
чисел `Fi/Ki` в каждой записи полный preflight использует:

```text
T = R*sizeof(LocalRecord) + F*sizeof(LocalField) + K*sizeof(FieldLookup)
    + sizeof(char16_t)*(N+R+K)
fixed = 2*sizeof(vector<LocalRecord>) + sizeof(LocalSchema)
    + 2*sizeof(LocalRecord) + 2*sizeof(u16string) + 32*sizeof(void*)
schemaModel = fixed + 2*T
work = 32*B + 8*(R+F+K+N+1)
    + (U+1)*(R*(R-1)/2 + sum(Ki*(Ki-1)/2)) + sum(Fi*Ki)
```

Fixed term включает controls, временные объекты и логический запас provenance. Две копии — staging
и скопированные core объявления; имена учитывают terminators. Counts/names/model проверяются до
пропорциональных аллокаций объявлений. `schemaModelBytes` — логическая модель, а не фактические STL
capacities, сумма allocation requests, resident memory или точный heap cap. Наблюдения
`storageBytes()`/capacity после аллокаций не разрешают допуск задним числом. Parser, staging и result
перекрываются до возврата. Вход, сохранённые старые схемы и дальнейшие session/collector copies —
отдельные владельцы; их нельзя вычитать ради лимита. Сумма областей не равна точному расходу heap.

Начальные `32*B` начисляются до depth scan/parser/counting, включая отклонённый вход. На byte
учтены один depth scan, одна копия входа, четыре parser/UTF-8 visits, шестнадцать grammar/array/identity
visits, шесть name-count/conversion visits и четыре control/failed-prefix visits. Восемь линейных
кредитов покрывают validation/copy/cleanup объявлений; пары имён стоят `U+1`, coverage учитывает
каждую из `Fi*Ki` итераций. Треугольные выражения делятся пополам до умножения. Это логические
кредиты, а не bytes, CPU instructions или время; гарантий throughput/latency нет.

## Квалификация зависимости и использование

При JSON ON CMake проверяет SHA-256 полных amalgamation simdjson 4.6.4 до компиляции:

| Файл | SHA-256 |
| --- | --- |
| `singleheader/simdjson.h` | `fec0cb4559e25456789efb153b7e5eaa27b50ce2bc2c9d3b531ea2000928380a` |
| `singleheader/simdjson.cpp` | `826660ed60cf467641ab2f93a0e1a2dacf603c444f76d835358c814bc2caba3a` |

Квалификация охватывает встроенные реализации SDK и обычный выбор backend, который при первом
вызове может читать `SIMDJSON_FORCE_IMPLEMENTATION`. Загрузчик не разрешает application schema/
properties/sources, не выполняет I/O и не изменяет SDK globals. Чужая замена active implementation
на custom implementation и конкурентная чужая мутация SDK globals вне этого ресурсного контракта.
Изменения toolchain/backend требуют новых проверок requests/capacities; наблюдаемые размеры/requests
отделены от исходной формулы.

В квалифицированных x64 builds base size равен 56 bytes, размеры включённых backends — 96 bytes;
fixed allowance равен 112 bytes на MSVC 19.42 и 120 на GCC 13.3. Для входа в четыре bytes наблюдались
1 141 ordinary parser payload bytes requests/peak при envelopes 1 157/1 165 соответственно.
Из samples исключены headers наблюдателя, aligned/не перехваченные и process allocations. Они
подтверждают эти builds, но не заменяют source bound девяти backends и не доказывают total heap.

[Сборка и пакет](../../docs/CPP-BUILD.ru.md#installed-binary-json-loader) описывают условный экспорт
и закрытую зависимость. JSON OFF не ищет, не получает, не линкует, не читает исходники SDK и не
добавляет cache entries необязательной зависимости. Точные upstream
[исходники v4.6.4](https://github.com/simdjson/simdjson/tree/1bcf71bd85059ab6574ea1159de9298dcc1212c5),
[LICENSE](../../licenses/simdjson-4.6.4/LICENSE) и
[LICENSE-MIT](../../licenses/simdjson-4.6.4/LICENSE-MIT) сохраняют исходную юридическую атрибуцию.

[Режим проверки конфигурации](../../docs/TAPE-INSPECT.ru.md#binary-json-configuration-validation)
существующего inspector не добавляет чтение HISTORY tape, подписки, транспорт или новый facade.
Контракты TapeReader, formatter, CatalogSegment и subscriptions сохранены.
[Устанавливаемый consumer](../../tests/conformance/schema_json_consumer/main.cpp) и
[протокол квалификации](../../tests/conformance/schema-json/VALIDATION.ru.md#binary-json-profile-qualification)
проверяют композицию существующего native HISTORY; они не доказывают Java HISTORY production и полную wire equivalence.
