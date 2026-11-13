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
- [Контракты подписок](subscriptions/CONTRACTS.ru.md), [live session](subscriptions/LIVE-SESSION.ru.md), [входящие подписки](protocol/INCOMING-SUBSCRIPTIONS.ru.md), [запись DATA](protocol/LIVE-DATA-WRITE.ru.md)
  и [локальный STREAM dispatch](subscriptions/CLIENT-DISPATCH.ru.md)
- [TCP runtime](transport/TCP-RUNTIME.ru.md), [приём TCP-соединений](transport/TCP-LISTENER.ru.md), [TLS runtime](transport/TLS-RUNTIME.ru.md)
  и [ресурсные предпосылки TLS](transport/TLS-RESOURCES.ru.md)

Причины решений и открытые вопросы дизайна QD/native собраны в [базе знаний](../docs/knowledge/README.ru.md).
Точные результаты conformance-проверок находятся в [разделе разработчика](../docs/developer.ru.md).

[Группировка option chains](ipf/OPTION-CHAINS.ru.md) определяет порядок серий/strike, замену и допуск в конечных ресурсных лимитах.

- [Атрибуты Candle](events/CANDLE-ATTRIBUTES.ru.md): точные значения price/session/alignment/price-level/data-type/exchange и операции с символами.
- [Полные символы Candle](events/CANDLE-SYMBOL.ru.md): каноническая идентичность, явные getters и exact-selector bridge подписки.

- [Рыночные значения событий](events/EVENT-VALUES.ru.md): сохраняющие raw business enums/flags и точные преобразования nano, packed sequence, Gregorian day ID и SHORT_STRING.
- [Правила расписаний и часовые пояса](schedule/RULES-AND-TIMEZONES.ru.md): цели совместимости, владение провайдером/данными и границы контролируемого профиля; реализация Schedule ведётся отдельно.

- [Приём TradeETH](events/TRADE-ETH.ru.md): отдельные ETH fields, raw flags и пятнадцать mixed selections.

- [Приём Summary](events/SUMMARY.ru.md): цены/дни сессии, точный business OpenInterest, PriceType и доставка пяти типов.

- [Приём текстовых событий](events/TEXT-EVENTS.ru.md): nullable UTF-16, packed time и семантика версии TICKER.
- [Текст событий](events/EVENT-TEXT.ru.md): явный контекст, числа/даты, UTF-16/UTF-8 и ограниченные контракты владения/работы.

- [Сериализованные вложения](events/ATTACHMENTS.ru.md) и [получение Message/Configuration](../docs/MISC-EVENTS.ru.md): владеющие nullable bytes, явные локальные кодеки и version admission collector.

- [Конфигурация экземпляра endpoint](transport/ENDPOINT-CONFIGURATION.ru.md): pure validation, immutable precedence и explicit activation.

[Запрошенная серверная агрегация](transport/SERVER-AGGREGATION.ru.md): существующие QTP request/info properties, timing и finite ownership.

- [Идентичность time-series и логический диапазон](subscriptions/TIME-SERIES-IDENTITY.ru.md): явная capability, owning-обёртки и атомарные мутации, отдельно от HISTORY receiving.

- [Приём HISTORY с владением SDK](subscriptions/HISTORY-RECEIVING.ru.md): диапазоны worker, отделённые AS-OF наблюдения полноты/reset и явная пригодность бизнес-типов.

[Исполнение event callback](subscriptions/CALLBACK-EXECUTION.ru.md): owning packets, borrowed schedulers, конечные reentry и source retirement.

- [Живые диапазоны HISTORY](subscriptions/LIVE-HISTORY-RANGES.ru.md): транзакции fromTime/set, applied fences доставки и сходимость wire demand.
