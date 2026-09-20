# Политика совместимости

[English version](COMPATIBILITY.md). Английский документ является основным.

## Oracle

Закреплённая в `upstream.lock` версия Java QD — начальный oracle совместимости. Заявление о совместимости должно называть upstream revision и сравниваемую наблюдаемую поверхность.

## Поверхности совместимости

Проект предполагает отдельно оценивать совместимость для:

- wire bytes и framing;
- message sequencing и negotiation;
- schema/DataScheme interpretation;
- symbol и field encodings;
- validation, defaults, aliases и mappings;
- error handling и malformed-input behavior;
- state transitions, видимых peers или API consumers.

Source/API compatibility с Java не является целью. Внутренняя структура классов и алгоритмы не являются поверхностью совместимости, если не влияют на наблюдаемое поведение.

## Доказательства

Каждое правило совместимости в итоге должно иметь:

1. нормативное или явно описательное утверждение в `spec/`;
2. прослеживаемую ссылку на upstream source, test или captured observation;
3. проверенную fixture, когда практически возможно представить байтовый или структурированный пример;
4. conformance test, исполняемый против C++ implementation.

Golden fixtures должны фиксировать способ получения, включая upstream commit и значимые options. Вручную написанные fixtures должны быть явно обозначены.

## Изменения и отклонения

Protocol или schema behavior нельзя менять только в коде. Specification, documentation и conformance fixtures обновляются вместе. Намеренные отклонения от Java QD требуют ADR с мотивацией, interoperability impact, migration strategy и test coverage.

Неизвестное поведение записывается как открытый вопрос, а не превращается в случайный контракт.

## Предлагаемый начальный профиль модели (QDCXX-007)

Политика выше не изменена. [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.ru.md) имеет статус **proposed**, а не approved deviation. [Матрица требований](../spec/schema/NORMALIZED-MODEL.ru.md) рекомендует сохранить точно измеренные schema/protocol subcases, effective names при copied-main-alias в pinned profile и представить VOID slots как compatibility projection без предписания физического хранения. Предлагается явная unsupported-local-type diagnostics вместо зафиксированного BYTE/SHORT NullPointerException с сохранением model recognition и local-construction rejection.

Original requests и selected outgoing layouts остаются раздельными; P05 evidence охватывает только свежие connections при первом использовании. Copy repair, более широкий error parity, dynamic layouts/reset, conversions, application defaults и dxFeed provider/property profiles остаются нерешёнными или неизмеренными. Установленное только по исходникам исключение VOID из wire не является captured LONG/time-index wire result. Java observations — неизменяемые доказательства; последующее утверждённое отклонение требует отдельных profile expectations и conformance work. Здесь не изменены wire behavior или существующие fixtures. [Validation](QDCXX-007-VALIDATION.ru.md) и [следующие шаги](NEXT-STEPS.ru.md) указывают ограничения и условия решений.
