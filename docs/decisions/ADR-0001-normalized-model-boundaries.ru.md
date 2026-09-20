# ADR-0001 — Разделить source provenance, local schema и directional protocol state

[English version](ADR-0001-normalized-model-boundaries.md). Английский документ является основным.

Статус: **proposed**. Дата: 2026-09-21. Задача: QDCXX-007. Завершение задачи не означает утверждения решения.

## Контекст

[Требования и матрица совместимости](../../spec/schema/NORMALIZED-MODEL.ru.md) различают captured behavior, source facts и design proposals. QDCXX-005 измеряет 17 подслучаев; QDCXX-006 — девять cases, включая 32 P01 schedules. Их [schema coverage](../../tests/fixtures/schema/COVERAGE.md) и [protocol coverage](../../tests/fixtures/protocol/COVERAGE.md) не охватывают целые семейства. Java observations не являются целевым interchange format.

Момент копирования и effective aliases зависят от истории построения. Local IDs/slots отличаются от directional wire IDs/order. Originally empty requested description наблюдаемо отличается от empty selected layout. Единая плоская модель не может безопасно выражать все эти значения.

## Предлагаемое решение

1. Сделать immutable resolved local schema и directional protocol descriptors/state самостоятельными раздельными артефактами. Source declarations и ordered overlay operations сохранить как importer provenance с оригиналами inputs и profile identity. Не предоставлять исполняемую XML model в qd-core. Core остаётся независимым от importer execution и transport.
2. Сохранять semantic/full/local/property names и scoped lookups, полные serial type IDs, emitted record order/local IDs, hasTime/index roles и явную Java slot projection. Logical fields отделены от физического C++ storage; synthetic VOID positions остаются сравнимыми без требования Java arrays. Templates, disabled entities, docs, enums, tags, bitfields и custom types сохраняются в provenance; сохранение не обещает исполнения.
3. Сохранить измеренный effective-name result потери copied main alias в initial pinned profile. Это обосновано наблюдаемыми различиями full/local names и collapsed lookup в [S08-main-alias](../../tests/fixtures/schema/expected/S08-main-alias.tsv), а не обязанностью копировать Java bugs. Explicit-main metadata хранить отдельно. Не «исправлять» молча существующие артефакты. Corrected profile требует явного review и mapping/property/wire probes; wire impact этого XML copy не измерен.
4. Предложить одно намеренное отклонение диагностики: отклонять BYTE/SHORT local construction со структурированной unsupported-type diagnostic вместо воспроизведения NullPointerException. [S15-byte](../../tests/fixtures/schema/expected/S15-byte.tsv) и [S15-short](../../tests/fixtures/schema/expected/S15-short.tsv) проходят model resolution, но падают на runtime; SerialFieldType factory/DefaultRecord объясняют null dereference ([sources](../../spec/schema/SOURCES.md#types)). Сохранить эту acceptance boundary и отличие от [S05-disabled-both](../../tests/fixtures/schema/expected/S05-disabled-both.tsv). Не обещать универсального error-stage/class parity, не принимать эти local types и не запрещать их wire codes на основании construction failure.
5. Хранить incoming sender-ID bindings, original requests и selected outgoing layouts в protocol state с привязкой к peer/direction и local schema. Сохранять P03/P04 и first-use P05 outcomes только в измеренных границах [CAPTURES](../../spec/protocol/CAPTURES.md). Absent, originally empty и nonempty requests остаются разными, даже если selected list пуст. Не принимать Java writer-cache/reset behavior без проб.
6. Предпочесть versioned data-only JSON interchange с ordered arrays и отдельными format/profile versions. Final schema, canonical serialization/hash, C++ API, storage и dependencies оставить открытыми. Java TSV/digest остаются доказательствами, а не canonical normalized identity.

## Альтернативы

| Альтернатива | Преимущество | Почему не рекомендуется вначале |
| --- | --- | --- |
| Только resolved runtime descriptors | Минимальная поверхность реализации | Теряются source order, disabled metadata и объяснение copy/overlay outcomes. |
| Единый source/runtime/session object graph | Вся информация в одном графе | Связывает source/session lifetimes с core и размывает границы ID/name/order. |
| Публичные lossless XML AST и overlay engine | Редактирование и round trip | Расширяет начальный контракт до непроверенных defaults, parser quirks и overlay semantics. Сохранённых originals достаточно для provenance. |
| Немедленно исправить main alias | Интуитивная семантика копирования | Меняет наблюдаемые full/local names и lookups; downstream wire/provider impact не измерен. Нужен отдельный утверждённый behavior profile. |
| Эмулировать Java exception classes/messages | Проще буквальное сравнение с oracle | Переносит случайную implementation failure в не-Java API. Сохранение rejection со структурированной диагностикой полезнее; отклонение всё равно требует утверждения. |
| Полностью убрать VOID или скопировать Java physical arrays | Более простая logical model либо прямое совпадение oracle storage | Удаление теряет наблюдаемые slots/lookups; обязательные arrays излишне ограничивают C++ storage. Projection сохраняет доказательства без этих обязательств. |
| Принять TSV, XML interchange или binary serialization | Готовые captures, повторное использование source syntax или компактность | TSV смешивает задачи наблюдения; XML плохо выражает три границы; binary optimization преждевременна. JSON — направление, а не выбранная dependency. |

## Последствия и влияние на совместимость

Нужны более явные metadata и references, зато importer diagnostics, local semantics и peer adaptation можно проверять независимо. Mapping/provider/property provenance обязательна для воспроизводимости; Java class names не реализуют C++ mappings. Полные dxFeed profiles, conversions, dynamic requests и malformed/reset/PART behavior остаются вне initial profile.

ADR не меняет принятые wire rules или существующие goldens. Slot projection и directional separation нацелены на сохранение наблюдаемого поведения. Сохранение main alias — ограниченный выбор совместимости, а не нормализация предполагаемого clone contract. Предложение об ошибках намеренно меняет диагностику invalid local inputs; в двух captures нет успешного local schema/wire output. Это не доказывает отсутствия влияния на вызывающий код, классифицирующий ошибки по Java exception type.

Предлагаемая миграция: оставить original captures неизменными; после утверждения ввести отдельно именованные profile/version и comparison expectations для structured errors. Если позже будет выбрано corrected copying, сохранять различимость pinned-profile artifacts и требовать явного re-resolution/diff names, lookups, properties и затронутого wire output; никогда не переинтерпретировать молча существующий артефакт. Автоматическая миграция или corrected profile по умолчанию не разрешены.

## Проверка и условия принятия

Это документационный результат; [QDCXX-007 validation](../QDCXX-007-VALIDATION.ru.md) перечисляет действительно выполненные проверки. Java, C++ и новые semantic probes здесь не исполнялись. Будущие importer tests должны сравнивать все 17 точных schema subcases, сохраняя rejection stages и Java observations рядом с утверждёнными diagnostic differences. Будущие protocol tests должны сравнивать девять exact/semantic cases и сохранять различие P01 между ordered values и callback grouping; целевые LONG/time-index, alias-copy и request-update probes — дополнительная работа, а не существующие доказательства.

Нужен выбор владельца: утвердить три границы артефактов и provenance retention; выбрать pinned либо corrected/deferred copy semantics; принять или отклонить structured-error deviation; поддержать JSON как направление interchange либо выбрать альтернативу. [Backlog](../NEXT-STEPS.ru.md) перечисляет зависимые работы и необходимые доказательства. До review этих решений ADR остаётся **proposed** и не разрешает реализацию.
