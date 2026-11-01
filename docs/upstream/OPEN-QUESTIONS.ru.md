# Открытые вопросы об upstream

[English version](OPEN-QUESTIONS.md). Английский документ является основным.

Записывать вопросы о поведении Java QD, на которые пока нет уверенного ответа. Связывать вопрос с исследующей задачей и закрывать его source или fixture evidence.

## Исходящие production-свидетельства QDCXX-031

Добавочный [production corpus](../../tests/fixtures/protocol/tape-write/CAPTURES.ru.md) разрешает
отдельные вопросы outgoing names/IDs/layout и raw values через закреплённый FileWriter → FileReader.
Исходящие поля используют property names, а не первый lookup alias; последний добавленный alias
может изменить имя. Исходные Quote/Profile ID 0/5 и 8/16 logical fields не изменяются. Сохранены
все прежние точные identities 2/8/10/40/56/72/88/120, nullable String/char-array units и допустимые
symbol forms. Full24/full137 остаются negative C++ admission witnesses, несмотря на принятие Java.

MESSAGE batch grouping отличается от singleton visits; FIELD добавляет built-in time descriptors,
NONE теряет event timing. Это не допускает дополнительные writing modes. Java close может
журналировать I/O errors, поэтому checked completion/publication требует отдельного outgoing
контракта. Два deterministic captures, строгий Verify и семь отвергнутых mutants приведены в
[проверках](../../tests/conformance/protocol/tape-write/VALIDATION.ru.md). Эта evidence не доказывает
C++ writer, strict rewrite, replay scheduler, новые adapters или transport support.

## Подмножество событий и конфигурации QDCXX-025

QDCXX-025 разрешает выбранные вопросы событий default/configured Quote и Profile через
[новые production-наблюдения](../../tests/fixtures/events/mapping/COVERAGE.ru.md) и
[подмножество реестра конфигурации](../../spec/schema/CONFIGURED-EVENTS.ru.md): обязательные/необязательные
выборы, wire omission против local absence, точные integer/wide альтернативы чисел, flags, UTF-16 строки,
упакованные Quote Sequence/TimeNanoPart и независимый eventTime. Full24/full137 сохранены как явно
неподдерживаемые случаи; положительным случаям не нужны новые identities или семантика контейнера.
[Контракты событий](../../spec/events/EVENT-MAPPING.ru.md) определяют ограниченное применение в C++.
Полная совместимость настроек/загрузчиков, полный dxlink, regional/indexed события, бизнес-представления
SHORT_STRING, дополнительные identities, ненулевые QTP flags и transport остаются отдельными вопросами.
Исторические утверждения QDCXX-024 ниже сохраняют исходные границы; Profile mapping заменён только здесь.

## Ограниченное разрешение вопросов полей QDCXX-024

[Production-доказательства Java](../../tests/fixtures/encoding/field-types/COVERAGE.ru.md) и
[FIELD-TYPES](../../spec/encoding/FIELD-TYPES.ru.md) устанавливают измеренное поведение count/UTFChar/
consumption типа 10 отдельно от UTFString, знаковую low-32-bit raw-проекцию 40/72/88, null defaults
object fields, смешанную проекцию int/object slots, повторные цели и повреждённые пропуски.
Production reader сообщения проверяет положительный character count по всем оставшимся байтам
сообщения до декодирования; у прямых примитивов другая точка обнаружения исчерпания входа.
Выбор границы C++ и конечная политика работы/владения явно фиксируют это различие.
Более широкое восстановление Java не разрешает C++ продолжать после терминальной ошибки.

Неизменённый стандартный Profile требует 16 логических полей, 24 Java int slots с десятью VOID tails
и двух object slots; библиотечная проекция полей сохраняет их все. Для этого примера не понадобились
исключённые identities или индексная схема. Примеры стандартных событий для SHORT_STRING/SEQUENCE
остаются открытыми; синтетическое покрытие не устанавливает общую поддержку стандартных схем
или бизнес-преобразований. [исходные проверки](../../tests/conformance/protocol/field-types/VALIDATION.ru.md)
отделены от ожидаемого независимого ревью. Полное отображение Profile, остальные события/типы,
импортер, индексные записи, сжатие, ненулевые flags и транспорт остаются открытыми.
Частный tape не использовался; исторические агрегированные пробелы и сгенерированные захваты
не доказывают поддержку всего файла.

## Ограниченное разрешение вопросов QDCXX-023

[Production-захваты Java](../../tests/fixtures/protocol/raw-tape/COVERAGE.ru.md) и
[контракт RAW-TAPE](../../spec/protocol/RAW-TAPE.ru.md) устанавливают production-доставку type-5,
наблюдения отсутствующего/пустого/null/непустого opt и распространение heartbeat до реализации C++.
Replay timestamp сохраняет знаковые int64 миллисекунды; доставленный event-time sequence использует
насыщенные знаковые секунды с миллисекундами независимо от бизнес-полей Quote. Политика C++ принимает
только отсутствующий/пустой/hs opt, время MESSAGE/message и описанное подмножество heartbeat; более широкая
приёмка Java не разрешает расширять C++. [исходные проверки](../../tests/conformance/protocol/raw-tape/VALIDATION.ru.md)
остаются отдельно от завершённой независимой приёмки.

[Оставшиеся пробелы частного образца](../REAL-TAPE-GAPS.ru.md) включают полные типы 10/40/72/88 и Profile;
сгенерированные тесты инвентаризации воспроизводимы без этого файла. Полная структурная инвентаризация
и Java replay с ограниченным бюджетом не определяют совместимость всего файла. Остальные поля/options heartbeat,
event flags, режимы времени, преобразования, сжатие, сессии, адаптеры и транспорт остаются открытыми.

## Историческое ограниченное разрешение вопросов QDCXX-022

[Runtime-доказательства Java](../../tests/fixtures/protocol/quote/COVERAGE.ru.md) устанавливают
выбранную default Quote-схему, production round trip, raw-zero отсутствующих полей, повторные цели,
позиции UTF_CHAR, отображение знаковых секунд и конечный набор decimal raw/text/binary64/потерь.
[Контракты](../../spec/protocol/DEFAULT-QUOTE.ru.md) и
[проверка C++](../../tests/conformance/protocol/quote/VALIDATION.ru.md) реализуют только
явный локальный профиль точного совпадения 2/8/56/120. Независимое ревью завершено; коммит реализации c26aed28f84db5c574ec5fbb9e4e5160e9dbbd8e.
Исключения TF-04, RAW_DATA/MESSAGE, политика отсутствующего opt, остальные события, преобразования,
региональное отображение, сжатие, импортер и транспорт остаются открытыми. Прежние ограничения на
Quote runtime/код ниже исторические и заменены только для этого этапа; исчерпывающего покрытия decimal/Unicode нет.

QDCXX-023 заменяет предшествующие вопросы RAW/MESSAGE и missing-opt
только в измеренном ограниченном контракте выше. QDCXX-024 отдельно разрешает своё точное подмножество полей;
прочие расширения полей и публикация частного образца не разрешены.

## Ограниченное разрешение вопросов QDCXX-021

TF-01 реализован только для явного constrained BINARY/NONE/EMPTY_SET/STREAM_DATA/type-8 профиля.
Для TF-02/03 получены [51 production FileReader-проба](../../tests/fixtures/protocol/tape-reader/COVERAGE.ru.md),
[политика TAPE-READER](../../spec/protocol/TAPE-READER.ru.md) и [ADR-0002](../decisions/ADR-0002-bounded-tape-reader.ru.md).
Неизвестные/повторные свойства, заголовки/ID, частичная установка Java, отсутствие описания,
неизвестные записи и повреждённый/усечённый EOF имеют ограниченные доказательства; строгая/атомарная/
терминальная политика C++ — разрешённые задачей отличия. Чистый EOF не подтверждает полноту записи.
Более широкие sessions, восстановление, крайние ресурсы TF-02/03 и все исключения TF-04 остаются открытыми.
Нет заявлений о default TapeConnector/RAW_DATA, Quote, сжатии, импортере и транспорте. исходная матрица
прошла; [независимое ревью ожидается](../../tests/conformance/protocol/tape/VALIDATION.ru.md). Прежние
условия ниже исторические и заменены только в пределах назначенной ограниченной реализации.

## Свидетельства tape QDCXX-019 и условия следующей задачи

Обновление QDCXX-020: [перечень](QDS-TOOLS.ru.md) и [стандартный Quote](../REAL-EVENT-BASELINE.ru.md)
устанавливают только отслеживаемый список команд и выбор схемы по исходникам. Runtime-справка/classpath,
полные транзитивные опции/defaults/ошибки и production-преобразование Quote (включая особые значения
wide decimal и потерю времени) не измерены. Исследование не закрывает прежние runtime-вопросы.
Решения владельца: CLI/конфигурация/вывод/коды каждой утилиты; ADR ограниченного tape descriptor,
добавления bindings и EOF; позднее адаптер/API Quote и преобразования типов; транспорт/потоки/
подписки. [Одна следующая реализация](../NEXT-STEPS.ru.md) сохраняет ограниченный type-8 профиль
QDCXX-019 как предварительное условие, не поддержку Quote. RMI/IPF/Launcher отложены;
до удаления активных Python-проверок нужна эквивалентность старых/новых отрицательных случаев.

[Инвентаризация/пробелы tape](../../spec/protocol/TAPE-FILES.ru.md) и
[проверка](../../tests/fixtures/protocol/tape/VALIDATION.ru.md) добавляют свидетельства
настоящих FileWriter/FileReader: обычный/ограниченный BINARY и TEXT сохраняют семь локальных записей;
обрезанный constrained доставляет четыре записи и одну stream error на EOF. Два чистых captures
совпали без нормализации.

| ID | Статус / оставшийся вопрос | Следующий необходимый шаг |
| --- | --- | --- |
| TF-01 | Измерено только для прямого writer: заголовок type 1 и поздние описания первого использования блокируют текущий инспектор даже при EMPTY_SET/time NONE. Default RAW_DATA у TapeConnector известен по исходникам, здесь не измерен. | Одобрить отдельную задачу несжатого BINARY/NONE/EMPTY_SET/STREAM_DATA; не заявлять общую поддержку tape/коннектора. |
| TF-02 | Открытая политика: неизвестные свойства дескриптора, пустой opt, отсутствие time, версия для диагностики, только добавляемые ID и владение планами. | ADR и Java-свидетельства дескриптора/переобъявлений/отсутствующих описаний до реализации C++. |
| TF-03 | Неполный EOF измерен; восстановление повреждений, follow, бюджеты и поведение на границах открыты. | Дополнительные конечные пробы обрезания заголовка/описания/данных и повреждённых кадров; явная терминальная политика/завершённый префикс без неявного resync. |
| TF-04 | Только исходники: CSV/BLOB, GZIP/ZIP, временные sidecar, split/cycle/retention и более широкие комбинации флагов/времени. Метаданные версии JAR и реальные схемы dxFeed не измерены. | Отдельные свидетельства до расширения профиля; новые runtime-зависимости не одобрены. |

QDCXX-019 — только исследование. Неизмеренные части прежних QT-вопросов сохраняются; возможности
C++/инспектора, importer и transport не добавлены. Новые инструменты эталонов — Java с запуском
через PowerShell, без новых Python-скриптов/зависимостей. Независимое ревью ожидается.

## Происхождение

- **QDCXX-002 — Статус GitHub repository**
  - **Статус:** неблокирующий вопрос о происхождении.
  - Upstream root POM закреплённого commit указывает внутренний Devexperts Stash repository (`https://stash.in.devexperts.com/scm/mdd/qd.git`) как SCM source, а `qd-cxx` использует публичный `https://github.com/devexperts/QD.git`.
  - Для закреплённой QD 3.355 baseline public tag, commit SHA, root POM version и release notes согласованы, поэтому source tree воспроизводим и однозначен.
  - `qd-cxx` считает pinned GitHub repository и immutable commit SHA **публичным reference source**, а это Java source tree — compatibility oracle.
  - Не проверено, является ли GitHub repository каноническим, официальным mirror или иной синхронизируемой публичной копией.
  - Неопределённость **не блокирует** specification extraction, conformance work или C++ implementation против pinned baseline.

## Начальные вопросы

- XML resources/loaders, rule layers, aliases, mappings, defaults и composition: начальные source answers зафиксированы QDCXX-003 ниже; QDCXX-005 добавляет 17 ограниченных captures. Более широкие captures и normative normalization остаются pending.
- QTP framing, directional schema exchange, codec state и candidate upstream tests: начальные source answers зафиксированы QDCXX-004 ниже; QDCXX-006 измеряет девять ограниченных P01/P03/P04/P05 cases; более широкое runtime coverage остаётся pending.

В каждом ответе различать явный upstream contract, наблюдаемое поведение и случайную особенность реализации.

## Исследование схемы QDCXX-003

Начальные вопросы loader/resource, composition, alias и mapping отвечены на уровне source inventory в [XML/DataScheme analysis](../../spec/schema/README.md) и [locked source citations](../../spec/schema/SOURCES.md). Это не окончательные нормативные правила или captured oracle results. XML syntax, source model и emitted runtime различаются; XML mappings сейчас игнорируются, runtime mappings поступают из Java services.

Оставшиеся вопросы связаны с [fixture proposals](../../tests/fixtures/schema/README.md):

| ID | Статус и точный вопрос | Доказательства / следующий шаг |
| --- | --- | --- |
| SC-01 | Открыт, parser edges: каковы наблюдаемые model/error для prefix-qualified elements, numeric booleans, whitespace, повторных include/exclude containers и XSD key constraints? | XmlSchemeModelReader child/attribute helpers и active XSD; S18. Фиксировать JDK/JAXP provider, не считать schema-valid равным semantically consumed. |
| SC-02 | Частично измерен; compatibility decision открыт: сохранять clone main-alias loss, ignored copy doc/regionals и main-alias removal pointer behavior? | QDCXX-005 S08-main-alias подтверждает сохранение copied explicit-main entry при fallback effective name к first alias; S08-before/after подтверждает snapshot timing. Doc/regionals/removal и normative decisions отложены. [Coverage](../../tests/fixtures/schema/COVERAGE.md). |
| SC-03 | Открыт, ordering/defaults: как normalized source operations сохранять defaulted XML overlay attributes, unordered overlay alias additions и отсутствие против пустых index values? | Active XSD, SchemeRecord.override, Field.override, SchemeRecordGenerator.override; S07/S09/S11. Не навязывать предполагаемое inheritance rule. |
| SC-04 | Частично измерен: BYTE/SHORT проходят model resolution, затем бросают NullPointerException; оба disabled indexes дают SchemeException на runtime, один disabled index даёт VOID. | QDCXX-005 S05/S15 [captures](../../tests/fixtures/schema/COVERAGE.md). Object/long/duplicate indexes, другие reserved types, generated-name и lookup collisions открыты. Нет универсального обещания SchemeException. |
| SC-05 | Открыт, bitfield boundary: фактическое поведение alias-to-flags, duplicate bitfields и offset+size beyond 64; определить metadata scope. | DefaultEmbeddedTypes.canHaveBitfields, reader addRecordField, Field.validateState/addBitfield; S19. В прослеженном пути runtime bitfield mapping generation не найдено. |
| SC-06 | Открыт, environment profile: какой service-provider set и property configuration определяют требуемый dxFeed parity? | RecordMappingFactoryHolder, DXFeedScheme, DXFeedPropertiesConverter и default-scheme tests; S13/S14/S16. Полная legacy property matrix, Nuam coverage и third-party provider order требуют продолжения. |
| SC-07 | Открыт, rebuild isolation: меняет ли последовательность withProperties shared base entities и влияет ли на позднейшие builds? | DXScheme.configureSchemeModel и SchemeModel.overrideEntityCollection разделяют references; S20. Source sharing наблюдается; sequence-dependent oracle capture ещё нужен. |
| SC-08 | Разрешён только для QDCXX-005: оригинальные tiny XML и Java harness; raw TSV сохранён, нормализуются только input-directory URL и CRLF. Upstream source/resources не распространяются. | [Harness/profile/provenance](../../tools/upstream/schema-oracle/README.md), [validation](../../tests/fixtures/schema/VALIDATION.md). Будущие copied resources требуют notice/provenance review; observation format не выбирает normalized model. |
| SC-09 | Предложение готово; решение владельца открыто: source/overlays и metadata как importer provenance, resolved local schema и directional protocol state как отдельные самостоятельные артефакты. | [Требования QDCXX-007](../../spec/schema/NORMALIZED-MODEL.ru.md), [ADR-0001](../decisions/ADR-0001-normalized-model-boundaries.ru.md), статус proposed. Это выбор дизайна qd-cxx, а не разрешённый вопрос upstream semantics. |

QDCXX-005 предоставляет начальные S01/S04/S05/S07/S08/S15 subcases; неперечисленные части открыты. Продолжение включает оставшуюся S01-S12/S15 matrix, parser/alias/rebuild probes S18-S20, normalized-model requirements и compatibility decisions (QDCXX-007), а также отдельный dxFeed provider/property parity profile S13-S17. QDCXX-006 позже добавляет ограниченные QTP captures ниже.

## Исследование QTP QDCXX-004

Начальные ответы — в [QTP boundaries](../../spec/protocol/README.md), [DataScheme/wire mapping](../../spec/protocol/SCHEMA-EXCHANGE.md) и [locked source index](../../spec/protocol/SOURCES.md). Source устанавливает sender-ID mapping по record name, ordered field descriptors, peer-requested outgoing adaptation, complete-frame buffering и message-local symbol state. QDCXX-006 добавляет [ограниченные captured observations](../../spec/protocol/CAPTURES.md), а не полные гарантии совместимости.

| ID | Статус и оставшийся вопрос | Доказательства / следующий шаг |
| --- | --- | --- |
| QT-01 | Открыт: точные primitive acceptance/canonicalization и limits, включая compact-int truncation, overlong encodings, malformed Unicode и huge counts/IDs. | BufferedInput/Output, IOUtil, PentaCodec; [encoding dependencies](../../spec/encoding/README.md); P01/P02/P08. Ограничивать probes для предотвращения опасных allocations. |
| QT-02 | Частично измерен: P03 directional IDs, P04 ordering/property-alias mapping и missing compact-int raw-zero slots, P05 requested layouts. Complete adaptation, application defaults, alias collisions, unconvertible и object/numeric mismatch matrix открыты. | BinaryRecordDesc повторяет попытку с local representation bits вместо единообразного rejection mismatches; FieldAdaptationTest покрывает выбранные numeric cases. P03–P06/P08. |
| QT-03 | Открыт: unknown local record с built-in event metadata, nonzero externally supplied event time или field replacers — какие null-cursor paths падают и как сообщается ошибка? | BinaryRecordDesc.readFields и AbstractQTPParser postprocessing могут разыменовать null. Normal skip path установлен по source; optional-path outcome требует P07. |
| QT-04 | Открыт: dynamic requested-layout changes и session reuse contract. Должна ли совместимость сохранять cached writer layout после resetSession? | BinaryQTPComposer.recordMap переживает inherited reset; ConnectionQTPParser.schemeMap сбрасывается. QDCXX-006 P05 измеряет только четыре fresh connections до first writer use; P12 и dynamic before/after writer use pending. |
| QT-05 | Открыт: PART reset/cancel lifetime, resource limits, excess fragment handling и error recovery policy. | BinaryQTPParser.resetSession не очищает partitionedMessages; assembly limit TODO; P10/P12. Не превращать возможную случайную implementation detail в нормативное правило. |
| QT-06 | Открыт: какое error/close behavior нужно для default/configured consumers, partial descriptor commits, final truncated streams и unchecked exceptions? | Parser callbacks, MessageAdapter skip flags, отсутствие final-EOF signal в базовом parse path. P01 фиксирует pending partial frames без errors и не использует final-EOF signal. P02/P08/P13 pending; file parser resync — отдельный профиль. |
| QT-07 | Открыт: полные negotiation/auth/legacy и live aggregation transition profiles, включая порядок first data после opt/version updates. | ProtocolDescriptor deltas и adapter paths прослежены; hook Javadoc противоречит source call order. P09 и отдельное auth/aggregation continuation, без заявления полной state machine. |
| QT-08 | Частично измерен: все 32 P01 schedules сохраняют record order/values, но callback grouping отличается. Полные event flag/time matrix, batching requirements и timing profiles открыты. | AbstractQTPParser reorder masks, EventFlag, HeartbeatPayload, MessageAdapterConnection. P01/P11/P13; разделять record order, callback grouping и clock values. |
| QT-09 | Разрешён для QDCXX-006: source-built harness, exact byte/callback captures и отдельные semantic expectations; нормализуется только CRLF; P05 использует настоящий connection path с документированным instrumentation. | [Coverage](../../tests/fixtures/protocol/COVERAGE.md), [validation/provenance](../../tests/fixtures/protocol/VALIDATION.md). Более широкие profiles и C++ conformance pending. |

Предлагаемое продолжение после QDCXX-006: P06 conversions/P09 negotiation и целевые error/reset/partition probes; затем normative primitive/wire rules и проверенный compatibility profile. QDCXX-006 не начинает QDCXX-007. Результат исследования не разрешает transport runtime или C++ implementation.

## QDCXX-007: условия решений и недостающие доказательства

QDCXX-007 завершена как документация; [ADR-0001](../decisions/ADR-0001-normalized-model-boundaries.ru.md) остаётся **proposed**. Прежние task-specific no-execution statements выше сохраняют историческую область действия. Model proposal не закрывает ни один SC/QT runtime unknown.

- **NM-01 — Решение владельца, условие importer contract:** утвердить three-layer ownership, source metadata retention и направление versioned JSON (SC-09). Альтернативы и последствия — в ADR.
- **NM-02 — Решение владельца, условие copy policy:** предлагается pinned-profile preservation измеренной main-alias loss (SC-02). Перед выбором corrected copying нужна целевая copy/known-mapping/property/wire probe. Copy doc/regionals/disabled, alias removal и collisions остаются unmeasured. Это не мешает представлять effective names независимо от declarations.
- **NM-03 — Решение владельца, условие error contract:** утвердить structured unsupported-local-type diagnostics вместо BYTE/SHORT NullPointerException (SC-04), сохранив successful model resolution и failed local construction. Другие unsupported types/index shapes и factory wrappers требуют отдельных probes; универсального exception parity нет.
- **NM-04 — Необходимые доказательства для расширения protocol support:** LONG/time-index wire omission, requested y,x order/types и P06 conversions до расширения static adaptation; before/after-first-writer requests и reset до dynamic layouts (QT-02/04). Существующие P05 captures покрывают только четыре fresh first-use connections.
- **NM-05 — Можно отложить вне initial profile:** SC-01/03/05/06/07 и QT-01/03/05/06/07/08 сохраняют unresolved portions. Primitive boundary probes — условие encoding contract; full dxFeed, mapping getters, generators, rebuilds, PART/auth/timing требуют отдельных задач. [Ограниченный backlog](../NEXT-STEPS.ru.md) фиксирует зависимости; исключение из scope не является заявлением совместимости.

## Принятое решение и обновление QDCXX-008

Владелец принял ADR-0001 2026-09-21. Это заменяет метки proposed/pending выше для решений SC-09 и NM-01–03: утверждены владение/JSON, закреплённое поведение копирования только для XML-импорта и структурированный отказ BYTE/SHORT. Неизмеренное поведение SC-02/04 остаётся открытым; утверждение не добавляет runtime-доказательств. QDCXX-008 теперь определяет [ограниченный контракт JSON](../../spec/schema/JSON-CONTRACT.ru.md) и [проверенные примеры](../../spec/schema/examples/VALIDATION.ru.md). NM-04/05 и семантика программного построения C++ остаются открытыми. Это обновление не запускает importer или QDCXX-009.

## Обновление доказательств примитивов QDCXX-009

QT-01 теперь частично измерен: [PRIMITIVES](../../spec/encoding/PRIMITIVES.ru.md) и [покрытие](../../tests/fixtures/encoding/COVERAGE.ru.md) устанавливают ограниченные переходы writer compact int/long, принимаемые избыточные формы reader, усечение между ширинами, кодовые единицы QTP UTFString, некорректные входы и consumed position прямого ByteArrayInput. Две независимые сборки и отрицательный/положительный Verify записаны в [валидации](../../tests/fixtures/encoding/VALIDATION.ru.md). Остаток QT-01: допустимые экстремальные длины/выделение ресурсов, другие подклассы BufferedInput и методы IOUtil, другие UTF/character codecs, PentaCodec, огромные протокольные counts/IDs и ошибки внешнего parser. Нельзя выводить rollback из EOF или переносить JSON Unicode policy на QTP. Нужны отдельные будущие задачи; эти доказательства не разрешают C++ или importer.

## Обновление QDCXX-012: framing

QT-01/QT-06 частично измерены новым [P02](../../tests/fixtures/protocol/p02/COVERAGE.ru.md): малые
границы длины/type, overlong, удержание неполных кадров, ошибки type/payload, unknown type и одно
сравнение закрытия стандартного adapter. Диагностика заголовка читает дополнительные байты, объясняя
зависимость позиции Java от разбиения. [C++ контракт](../../spec/protocol/FRAMING.ru.md) задаёт отдельные
terminal-first-error, ресурсы и finish; эквивалентность Java resetSession/EOF не заявляется. Полная
матрица error flags, огромные ресурсы, другие buffers, PART/session/transport остаются открытыми.

## Обновление descriptions QDCXX-013

QT-01/QT-06 теперь включают ограниченные [доказательства P08](../../tests/fixtures/protocol/p08/COVERAGE.ru.md):
проверки заголовков записи/поля после compact-проекции младших 32 бит, точные UTF-16, дубликаты,
отказы построения отдельно от повреждённых descriptions и сохранение предыдущих установок.
[Контракт raw parser](../../spec/protocol/DESCRIBE-RECORDS.ru.md) разделяет диагностические сырые наблюдения,
реальные Java mappings и ресурсную политику C++. Огромные ресурсы, local binding/conversions,
опциональные профили, session recovery и transport остаются открыты; всеобщее равенство callbacks/exceptions не заявлено.

## Обновление статического binding QDCXX-014

QT-02/QT-03 частично измерены [наблюдениями штатного parser](../../tests/fixtures/protocol/binding/COVERAGE.md):
точный type-8 remapping, field reorder/explicit lookup, итоговые duplicate values, отсутствующие поля,
пустые записи, sparse ID и пропуск неизвестных записей/полей с продолжением доставки. Undescribed ID
остаётся отдельной ошибкой сообщения. [Static binding](../../spec/protocol/STATIC-BINDING.md) — независимый
владеющий план, не payload decoding/session state. Промежуточные повторные записи подтверждены исходниками;
application defaults, conversion matrix, metadata/replacers/providers, огромные ресурсы, dynamic updates,
reset/PART и transport остаются открытыми. Валидация локального подмножества не реализует JSON/XML import.

## Уточнение символов QDCXX-015

QT-01/QT-06 теперь включают [ограниченные доказательства символов](../../tests/fixtures/encoding/symbols/COVERAGE.ru.md):
канонические/принимаемые формы, точные units/позиции, flags/MRU, repeat/reset и реальную production
интеграцию двух сообщений. Частичное состояние Java после ошибки наблюдается отдельно от
[terminal-политики C++](../../spec/encoding/SYMBOLS.ru.md). Огромные аллокации, resolver/другие буферы,
negotiation опций, интерпретация event/time, полное восстановление, PART и транспорт остаются открытыми.
Полный event decoder или совместимость протокола не подразумеваются.

## Обновление QDCXX-016 STREAM_DATA

QT-01/02/06/08 дополнены 35 ограниченными production-parser observations обычных type-8 payloads,
точных UTF-16, пропуска записей/полей, повторов символов между record ID, сброса сообщений, cross-width
values и повреждённых полных сообщений. [Доказательства](../../tests/fixtures/protocol/stream-data/COVERAGE.md).
[Контракт decoder](../../spec/protocol/STREAM-DATA.md) отделяет удаление Java batches повреждённого
сообщения от C++ completed-prefix retention и отклоняет любые ненулевые raw flags. Dynamic session,
conversions, metadata/replacers, event/time, огромные ресурсы, recovery и transport остаются открытыми.
