# Справочник API {#api-reference-ru}

[English version](api.md) · [Главная](index.ru.md)

Сгенерированный справочник описывает объявления в `org::ttldtor::qdcxx`, параметры, ошибки и владение.
Для поиска отдельных объявлений используйте указатели пространств имён и типов рядом с этой страницей.
[Начало работы](getting-started.ru.md) показывает поиск установленного пакета и линковку.

| Компонент | Публичные заголовки | Цель линковки | Подробный контракт |
| --- | --- | --- | --- |
| Кодировки | `qd/encoding/` | `qdcxx_encoding` | [Спецификации кодировок](../spec/README.ru.md#encoding-contracts-ru) |
| Схемы и идентичность записей | `qd/core/` | `qdcxx_core` | [Спецификации схем](../spec/README.ru.md#schema-contracts-ru) |
| Кадры, привязка и tape | `qd/protocol/` | `qdcxx_protocol` | [Спецификации протокола](../spec/README.ru.md#protocol-contracts-ru) |
| Стандартные события | `qd/events/` | `qdcxx_quote`, `qdcxx_profile`, `qdcxx_event_mapping` | [Конфигурация](CONFIGURATION.ru.md) |
| Коллекторы и мультиплексирование | `qd/collectors/`, `qd/mux/` | `qdcxx_collectors`, `qdcxx_mux`, `qdcxx_mux_queue` | [Контракты подписок](../spec/subscriptions/CONTRACTS.ru.md) |
| JSON-схемы | `qd/schema/json_schema.hpp` | `qdcxx_schema_json` | [Нативный JSON](../spec/schema/NATIVE-JSON.ru.md); устанавливается при `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` (по умолчанию ON) |
| Автономный replay | `qd/replay/tape_replay.hpp` | `qdcxx_replay` — только дерево сборки | [Контракт replay](../spec/protocol/OFFLINE-REPLAY.ru.md); `ORG_TTLDTOR_QDCXX_BUILD_REPLAY=ON` (по умолчанию OFF), без установки/экспорта пакета |
| Необязательный клиент | `qd/client/` | `qdcxx_client` | [Локальный STREAM dispatch](../spec/subscriptions/CLIENT-DISPATCH.ru.md) |
| Необязательный транспорт | `qd/transport/` | `qdcxx_transport`, `qdcxx_tls` | [TCP](../spec/transport/TCP-RUNTIME.ru.md), [TLS](../spec/transport/TLS-RUNTIME.ru.md) |

Необязательные цели требуют соответствующих [параметров сборки](CPP-BUILD.ru.md). Контракты различают
заимствованные представления, владеющие значения, конечные бюджеты работы и локальное принятие байтов
от доставки другой стороне. Наличие объявления в справочнике не означает реализации всех запланированных семейств протокола и настроек.

Заголовок `qd/mux/queue.hpp` требует `qdcxx_mux_queue`; эта необязательная очередь включается вместе с `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`.
