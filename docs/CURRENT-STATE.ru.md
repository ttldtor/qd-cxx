# Текущее состояние

[English version](CURRENT-STATE.md). Английский документ является основным.

## Утверждение ADR

Утверждено владельцем проекта 2026-09-21. Три границы определяют логическое владение и не требуют трёх форматов файлов или трёх подсистем сериализации. Core может использовать разрешённую локальную схему без загрузки XML и его истории. Потеря copied main alias сохраняется только для XML-импорта в закреплённом профиле QD 3.355; это не требование к будущему программному созданию схем в C++. Java slot projection сохраняется без предписания физического хранения C++. Утверждена структурированная диагностика unsupported-local-type для BYTE/SHORT с сохранением отклонения локального построения; это не запрещает их wire-коды. Исходные requests и selected layouts остаются раздельными. JSON принят для обмена описаниями схем; библиотеки, канонические хеши и сериализация всей сессии остаются открытыми. Существующие oracle-доказательства неизменны.

## Фаза

**Specification baseline / repository bootstrap.**

Репозиторий определяет правила работы и участия, области документации и спецификаций, закрепление upstream и первые исследовательские задачи. Production C++ implementation, build configuration, dependency selection и опубликованного API нет.

Завершены: QDCXX-008 — контракт JSON и примеры; QDCXX-009 — векторы и контракт примитивных кодировок. Дальнейшие C++/importer требуют отдельной утверждённой задачи. Старые записи ниже описывают состояние при завершении соответствующих задач.

## История задач

- Завершена: `QDCXX-001`, создание репозитория и рабочего процесса.
- Завершена: `QDCXX-002`, проверка происхождения Java QD 3.355 и документирование воспроизводимого upstream inspection.
- Завершена: `QDCXX-003`, начальное исследование XML schema/DataScheme со ссылками на исходники; runtime oracle captures и normalized-model decisions оставлены последующим задачам.
- Завершена: `QDCXX-004`, inventory QTP framing/protocol и local DataScheme/wire-schema mapping со ссылками на исходники; исполняемая oracle validation оставлена продолжению.
- Завершена: `QDCXX-005`, Java schema oracle tooling и 17 captured subcases с независимой воспроизводимостью и positive/negative verify.
- Завершена: `QDCXX-006`, девять in-memory QTP P01/P03/P04/P05 captures, настоящий connection requested-layout path и reproducibility/verify evidence.
- Завершена: `QDCXX-007`, только документация: normalized-model requirements, proposed ADR и ограниченный follow-up backlog. ADR-0001 остаётся **proposed**, ожидая решений владельца.

## Следующий рубеж: исполняемая oracle baseline

Schema-часть теперь имеет [исполняемую oracle baseline](../tools/upstream/schema-oracle/README.md), ограниченную [точным покрытием](../tests/fixtures/schema/COVERAGE.md). Разрешены Java tools в рамках задач; production C++ и dependency selection отложены.

QDCXX-006 предоставляет protocol bytes/observations. QDCXX-007 предоставляет проверяемые model proposals, а не принятые compatibility rules. Source findings сами по себе не устанавливают executable compatibility. Оставшиеся edge cases, negotiation, conversion matrices, reset/PART и dxFeed provider profiles открыты для дальнейшей целевой работы.

## Ближайшие критерии завершения фазы

Фаза завершена, когда рабочий процесс воспроизводим, происхождение upstream проверено, начальные schema/QTP inventories доступны для review и связаны с исходниками, а следующие implementation tasks можно ограничить письменными контрактами вместо предположений.

## Текущие нецели

- C++ source code или generated code;
- CMake или dependency-manager configuration;
- выбор Asio или другого transport runtime;
- заявления о производительности;
- стабильный публичный C++ API.

## Проверка bootstrap

Проверено для `QDCXX-001` 2026-09-20:

- [x] Входные документы (`README.md`, `AGENTS.md`, `CONTRIBUTING.md`, `tasks/README.md`) существуют и объясняют поиск текущих работ и правил.
- [x] Описанные области под `docs/`, `spec/`, `tasks/`, `tests/`, `tools/upstream/` существуют.
- [x] Относительные Markdown links в отслеживаемой документации разрешаются в файлы репозитория.
- [x] Проверка только документации и правила игнорирования локальных файлов зафиксированы без production code, build configuration или dependencies.
- [x] Отложенные architecture/dependency choices явно не решены в `docs/ARCHITECTURE.md`; upstream semantic unknowns перечислены в `docs/upstream/OPEN-QUESTIONS.md`, а исследовательские задачи ограничены в `tasks/`.

## Проверка upstream baseline

Проверено для `QDCXX-002` 2026-09-20:

- [x] `https://github.com/devexperts/QD.git` доступен для fetch и содержит locked commit.
- [x] Annotated tag `qd-qds-3.355` разрешается в `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
- [x] Root POM и release notes этого commit указывают версию 3.355.
- [x] Точные процедуры detached checkout и review обновления lock описаны в `docs/UPSTREAM.md`.
- [x] Значимые top-level modules инвентаризированы без schema/QTP semantic analysis, назначенного последующим задачам.
- [x] Неоднозначность public-GitHub/internal-SCM metadata зафиксирована в `docs/upstream/OPEN-QUESTIONS.md`.

Обновлять этот файл при изменении фазы проекта, активной работы или существенных известных ограничений.

## Исполняемая schema baseline

Зафиксировано для `QDCXX-005` 2026-09-20:

- [Java harness и Windows launcher](../tools/upstream/schema-oracle/README.md) проверяют locked origin/commit/tag/version и чистоту исходников, пересобирают внешний source с JDK 8/Maven и используют свежие JVM с отключёнными optional property overlays.
- [17 подслучаев](../tests/fixtures/schema/COVERAGE.md) покрывают только части S01/S04/S05/S07/S08/S15: empty/no input, field ordering/disabled entities, index/VOID variants, aliases/lookups, copy timing/main-alias loss, storage slots и BYTE/SHORT rejection.
- [Validation evidence](../tests/fixtures/schema/VALIDATION.md) сохраняет original inputs, raw observations, normalized expectations, точные commands и provenance. Два независимых captures воспроизводят все expectations; verify отклоняет временную мутацию count 0-to-999 и проходит неизменную baseline.
- Runtime подтверждает предсказанную исходниками copy-main-alias quirk и различные exception families. В этих подслучаях нет необъяснённого semantic discrepancy. Observation format не является normalized-model decision.
- Установленный Zulu JDK 8 удовлетворяет upstream enforcer. Maven cache location и JAXB temporary working directory — явные адаптации окружения; нет upstream source/POM patch или подмены QD binary.
- Upstream tests пропущены; full schema/provider/property parity не заявляется. QTP captures позже добавлены QDCXX-006 ниже; более широкие parser/rebuild edges, C++ implementation и C++ conformance не выполнялись.

## Schema investigation baseline

Зафиксировано для `QDCXX-003` 2026-09-20:

- [XML/DataScheme inventory](../spec/schema/README.md) прослеживает locked Java source от загрузки ресурсов до runtime construction и разделяет XML syntax, model semantics и dxFeed conventions.
- [Source inventory](../spec/schema/SOURCES.md) содержит immutable citations, значимые symbols и checkout verification.
- [Schema fixture proposals](../tests/fixtures/schema/README.md) задают 21 capture case; expected output и Java execution не заявляются.
- [Schema open questions](upstream/OPEN-QUESTIONS.md#schema-investigation-qdcxx-003) охватывают parser/default/alias quirks, runtime failures, property/provider profiles, rebuild isolation и normalization scope.
- Все пять acceptance criteria начального исследования выполнены. C++ code, build dependencies, normalized format или wire semantics не добавлены.

## QTP investigation baseline

Зафиксировано для `QDCXX-004` 2026-09-20:

- [QTP boundary inventory](../spec/protocol/README.md) разделяет stream/frame/message/session/transport responsibilities, partial input, batching, negotiation, heartbeat, PART и error handling.
- [Local DataScheme и wire descriptions](../spec/protocol/SCHEMA-EXCHANGE.md) объясняет sender-ID to receiver-local-ID mapping по record name, ordered field names/types, requested outgoing layouts и различающиеся схемы.
- [Locked source index](../spec/protocol/SOURCES.md), [encoding dependencies](../spec/encoding/README.md), [13 предложенных capture families](../tests/fixtures/protocol/README.md) и [QT-01–QT-09](upstream/OPEN-QUESTIONS.md#qtp-investigation-qdcxx-004) различают source facts, documentation/test assertions, inferences и unknowns.
- Source review выявил неатомарные record-description errors, message-local symbol state, сохранение writer cache/PART state после resetSession и расхождение protocol-hook documentation/source ordering. Нужны captures и compatibility review вместо молчаливой нормализации.
- Проверены locked checkout, documentation links/diff; Java build/test и runtime capture не выполнялись. Все пять inventory acceptance criteria выполнены. Production C++, dependencies, wire change или networking choice не добавлены; byte-level compatibility ещё не продемонстрирована.

## Исполняемая QTP baseline

Зафиксировано для `QDCXX-006` 2026-09-20, reasoning High:

- [Protocol harness](../tools/upstream/protocol-oracle/README.md) повторно использует locked-source clean builds, provenance и capture/verify conventions. Все builds общего checkout выполнялись последовательно.
- [Девять cases](../tests/fixtures/protocol/COVERAGE.md) охватывают P01 (32 schedules), P03 в обоих направлениях ID remapping с data/subscription, P04 reordered/absent/alias-property fields и P05 none/zero/x/absent-z requests через настоящие MessageAdapterConnection parser/composer paths.
- [Exact captures и отдельные semantic expectations](../tests/fixtures/protocol/expected/) сохраняют bytes, callbacks, raw values, local slots и chunk schedules. Fragmentation сохраняет ordered records, но callback batching различается. Zero original fields выбирает все поля; nonempty absent-z выбирает zero outgoing fields, но запись доставляется.
- [Validation](../tests/fixtures/protocol/VALIDATION.md) фиксирует независимую clean-build reproduction и negative/positive verify с неизменными expectations. Metadata идентифицирует tool/input source hashes, upstream, commands, JDK и providers.
- [Measured specification](../spec/protocol/CAPTURES.md) ограничивает наблюдения. P06 conversion, P09 negotiation, malformed/reset/PART, auth, sockets и timing остаются pending; P05 metadata instrumentation явно описано. В рамках этой задачи QDCXX-007 или C++ не выполнялись.

## Предложение normalized model

Зафиксировано для `QDCXX-007` 2026-09-21, reasoning High:

- [Требования](../spec/schema/NORMALIZED-MODEL.ru.md) разделяют source/overlay provenance, resolved local schema и directional wire descriptions/state. Примеры используют реальные подслучаи; source-derived VOID wire omission помечено отдельно.
- [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.ru.md) имеет статус **proposed**: сохранить измеренные copy-name results в pinned profile, Java slot projection без фиксации C++ storage и заменить случайные BYTE/SHORT null dereferences явной диагностикой только после утверждения. Versioned JSON — рекомендация, а не принятый format/dependency.
- [Backlog](NEXT-STEPS.ru.md) задаёт decision gates и недостающие probes для importer/encoding work. Dynamic requested layouts, provider/property parity и оставшиеся SC/QT questions открыты.
- [Documentation validation](QDCXX-007-VALIDATION.ru.md) фиксирует evidence/link/status checks и bilingual review. Не добавлены Java rerun, runtime capture, C++ implementation, importer или dependency; существующие fixtures неизменны. Завершение документационной задачи не принимает ADR.

## Baseline контракта JSON

QDCXX-008 завершена 2026-09-21 с reasoning High. [Контракт версии 1](../spec/schema/JSON-CONTRACT.ru.md) определяет разрешённые локальные схемы, scoped lookups, логические поля, принадлежность Java slots, упорядоченные ссылки provenance и структурированную диагностику построения в рамках принятого ADR-0001. [Покрытие](../spec/schema/examples/COVERAGE.ru.md) сопоставляет все 17 подслучаев спроектированным JSON-проекциям; [валидация](../spec/schema/examples/VALIDATION.ru.md) фиксирует синтаксис, инварианты, сравнения с TSV и отрицательные проверки. Документы EN/RU синхронизированы. Исходные schema/protocol goldens неизменны. Importer, production C++, зависимости, повторный запуск Java и работа над QDCXX-009 не добавлялись. Программное построение, расширенные профили и сериализация сессии остаются открытыми.

## Базовые примитивные кодировки

QDCXX-009 завершена 2026-09-21 с High reasoning. [Контракт](../spec/encoding/PRIMITIVES.ru.md) разделяет канонический вывод writer, дополнительные формы reader и измеренные ошибки/consumed position для compact int/long и QTP UTFString. [Покрытие](../tests/fixtures/encoding/COVERAGE.ru.md): 109 наблюдений writer и 1037 reader, включая 858 строгих префиксов. [Валидация](../tests/fixtures/encoding/VALIDATION.ru.md) фиксирует две независимые чистые сборки, отрицательный/положительный Verify, неизменность настоящих/временных эталонов и прежних schema/protocol goldens. Результаты синхронизированы EN/RU. Точные методы QTP прослежены; JSON Unicode policy, другие UTF codecs и PentaCodec остаются отдельными. Огромные допустимые длины, другие буферы и полный протокол не измерены. C++, importer и зависимости не добавлены.

## Стиль Java-инструментов

После QDCXX-009 владелец принял Google Java Style. AGENTS.md, [.editorconfig](../.editorconfig) и [wrapper закреплённого форматтера](../tools/format/README.ru.md) задают постоянный процесс. Все три Java harness проходят google-java-format 1.36.1; форматтер использует JDK 21+, oracle остаётся на JDK 8. [Валидация](../tools/format/VALIDATION.ru.md) фиксирует отрицательные/положительные проверки форматирования без записи, успешные Verify schema/protocol и обновлённые независимые capture/Verify encoding. Все байты эталонов неизменны; исторический provenance schema/protocol сохранён отдельно от доказательств текущего кода.

## Будущий стиль C++

Владелец принял [контракт стиля C++](CPP-STYLE.ru.md) по образцу ttldtor/dxfcxx_event_test на commit d96ef76f3c4c6649437ac438bd0fa30f91c12f48. Правила имён, документации и организации кода дополняют скопированные корневые конфигурации форматтеров: типы UpperCamelCase, функции/публичные поля lowerCamelCase, инкапсулированные поля lowerCamelCase_, значения enum/именованные константы UPPER_SNAKE_CASE. Будущий полный процесс — Uncrustify 0.83.0, затем clang-format 20.1.8; сами конфигурации не проверяют именование. Сохранены provenance образца/настроек и лицензия копий. Обновление документации/конфигураций не начинает реализацию C++ и не выбирает build/test dependencies; C++-форматтеры не запускались.
