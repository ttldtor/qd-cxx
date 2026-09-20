# Открытые вопросы об upstream

[English version](OPEN-QUESTIONS.md). Английский документ является основным.

Записывать вопросы о поведении Java QD, на которые пока нет уверенного ответа. Связывать вопрос с исследующей задачей и закрывать его source или fixture evidence.

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
