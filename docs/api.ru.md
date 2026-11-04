# Справочник API {#api-reference-ru}

[English version](api.md) · [Главная](index.ru.md)

Сгенерированный справочник описывает объявления в `org::ttldtor::qdcxx`, параметры, ошибки и владение.
Для поиска отдельных объявлений используйте указатели пространств имён и типов рядом с этой страницей.
[Начало работы](getting-started.ru.md) показывает поиск установленного пакета и линковку.

| Компонент | Публичные заголовки | Цель линковки | Подробный контракт |
| --- | --- | --- | --- |
| Публичный типизированный feed | `qd/api/feed.hpp`, `qd/api/event_registry.hpp` | `qdcxx_feed` | [Endpoint, Feed и типизированные подписки](FEED-API.ru.md), [неизменяемый реестр](../spec/events/EVENT-REGISTRY.ru.md), [смешанные подписки](../spec/events/MIXED-SUBSCRIPTIONS.ru.md); `ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` (по умолчанию OFF), требует CLIENT/RUNTIME/RECOVERY |
| Кодировки | `qd/encoding/` | `qdcxx_encoding` | [Спецификации кодировок](../spec/README.ru.md#encoding-contracts-ru) |
| Схемы и идентичность записей | `qd/core/` | `qdcxx_core` | [Спецификации схем](../spec/README.ru.md#schema-contracts-ru) |
| Кадры, привязка и tape | `qd/protocol/` | `qdcxx_protocol` | [Спецификации протокола](../spec/README.ru.md#protocol-contracts-ru) |
| Стандартные события | `qd/events/` | `qdcxx_quote`, `qdcxx_profile`, `qdcxx_trade`, `qdcxx_event_mapping` | [Конфигурация](CONFIGURATION.ru.md), [Trade](../spec/events/TRADE.ru.md) |
| Периоды Candle | `qd/events/candle_period.hpp` | `qdcxx_candle_values` | [Типы, текст и оценки интервала](../spec/events/CANDLE-PERIOD.ru.md); без приёма Candle |
| Атрибуты Candle | `qd/events/candle_attributes.hpp` | `qdcxx_candle_values` | [Значения и операции с символами](../spec/events/CANDLE-ATTRIBUTES.ru.md); без агрегации |
| Коллекторы и мультиплексирование | `qd/collectors/`, `qd/mux/` | `qdcxx_collectors`, `qdcxx_mux`, `qdcxx_mux_queue` | [Контракты подписок](../spec/subscriptions/CONTRACTS.ru.md) |
| JSON-схемы | `qd/schema/json_schema.hpp` | `qdcxx_schema_json` | [Нативный JSON](../spec/schema/NATIVE-JSON.ru.md); устанавливается при `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` (по умолчанию ON) |
| Автономный replay | `qd/replay/tape_replay.hpp` | `qdcxx_replay` — только дерево сборки | [Контракт replay](../spec/protocol/OFFLINE-REPLAY.ru.md); `ORG_TTLDTOR_QDCXX_BUILD_REPLAY=ON` (по умолчанию OFF), без установки/экспорта пакета |
| Необязательный клиент | `qd/client/` | `qdcxx_client` | [Локальный STREAM dispatch](../spec/subscriptions/CLIENT-DISPATCH.ru.md) |
| Необязательный транспорт | `qd/transport/tcp_runtime.hpp`, `qd/transport/tls_runtime.hpp`, `qd/transport/tls_resources.hpp` | `qdcxx_transport`, `qdcxx_tls` | [TCP](../spec/transport/TCP-RUNTIME.ru.md), [TLS](../spec/transport/TLS-RUNTIME.ru.md) |
| Восстановление соединения | `qd/transport/recovery.hpp`, `qd/transport/tcp_recovery.hpp`, `qd/transport/tls_recovery.hpp` | `qdcxx_recovery`, `qdcxx_tls_recovery` | [Контракт восстановления и условия включения](../spec/transport/RECOVERY.ru.md); `ORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON` (по умолчанию OFF) |

Необязательные цели требуют соответствующих [параметров сборки](CPP-BUILD.ru.md). Контракты различают
заимствованные представления, владеющие значения, конечные бюджеты работы и локальное принятие байтов
от доставки другой стороне. Наличие объявления в справочнике не означает реализации всех запланированных семейств протокола и настроек.

Заголовок `qd/mux/queue.hpp` требует `qdcxx_mux_queue`; эта необязательная очередь включается вместе с `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`.

[Цепочки опционов](OPTION-CHAINS.ru.md): `qd/ipf/option_chains.hpp`, необязательная `qdcxx_ipf` при `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`.

- [CFI и дополнительные базовые активы](GLOSSARY-VALUES.ru.md): `qd/glossary/`, необязательная `qdcxx_ipf`.

- [Ценовые шаги инструмента](PRICE-INCREMENTS.ru.md): диапазоны, точность и округление цены через `qdcxx_ipf`.
