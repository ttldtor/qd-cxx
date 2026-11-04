# Архитектура и контракты протокола {#architecture-protocols-ru}

[English version](README.md) · [Главная](../docs/index.ru.md)

[Архитектура](../docs/ARCHITECTURE.ru.md) описывает ответственность компонентов,
[политика совместимости](../docs/COMPATIBILITY.ru.md) — границу поведения Java QD. Спецификации задают
наблюдаемые контракты; [текущие возможности](../docs/CURRENT-STATE.ru.md) определяют реализованные профили.

## Контракты кодировок {#encoding-contracts-ru}

См. [спецификацию wire-кодирования](encoding/PRIMITIVES.ru.md): compact integers, текст и символы.

## Контракты схем {#schema-contracts-ru}

[Конфигурация](../docs/CONFIGURATION.ru.md) описывает явные схемы вызывающей стороны и приоритет источников.
[Модель схем](schema/NORMALIZED-MODEL.ru.md), [нативный JSON](schema/NATIVE-JSON.ru.md)
и [обычная схема с провода](schema/WIRE-INVENTORY.ru.md) задают соответствующие границы.

[Schema-bound mappings событий](events/EVENT-MAPPING.ru.md) и
[неизменяемый реестр принимаемых событий](events/EVENT-REGISTRY.ru.md) определяют
явные типизированные проекции полных пользовательских схем.
[Смешанные зарегистрированные подписки](events/MIXED-SUBSCRIPTIONS.ru.md) выбирают
несколько поддержанных типов через одну логическую подписку на владеющие variant values.

- [Приём Trade](events/TRADE.ru.md): полные owning fields, business numeric views и явные composite/regional Feed routes.
- [Значения периодов Candle](events/CANDLE-PERIOD.ru.md): чистые типы агрегации и текст периодов, отдельно от приёма Candle.

## Контракты протокола {#protocol-contracts-ru}

- [Кадры QTP](protocol/FRAMING.ru.md) и [чтение tape](protocol/TAPE-READER.ru.md)
- [Контракты подписок](subscriptions/CONTRACTS.ru.md), [live session](subscriptions/LIVE-SESSION.ru.md)
  и [локальный STREAM dispatch](subscriptions/CLIENT-DISPATCH.ru.md)
- [TCP runtime](transport/TCP-RUNTIME.ru.md), [TLS runtime](transport/TLS-RUNTIME.ru.md)
  и [ресурсные предпосылки TLS](transport/TLS-RESOURCES.ru.md)

Причины решений и открытые вопросы дизайна QD/native собраны в [базе знаний](../docs/knowledge/README.ru.md).
Точные результаты conformance-проверок находятся в [разделе разработчика](../docs/developer.ru.md).

[Группировка option chains](ipf/OPTION-CHAINS.ru.md) определяет порядок серий/strike, замену и допуск в конечных ресурсных лимитах.

- [Атрибуты Candle](events/CANDLE-ATTRIBUTES.ru.md): точные значения price/session/alignment/price-level/data-type/exchange и операции с символами.
