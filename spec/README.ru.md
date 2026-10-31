# Спецификации

[English version](README.md). Английская версия нормативная.

Здесь находятся независимые от реализации контракты, полученные из upstream evidence и намеренных решений qd-cxx.

Спецификации различают:

- **нормативные** требования qd-cxx;
- **описательное** поведение Java QD на закреплённой ревизии;
- **открытые** ещё не проверенные вопросы;
- **намеренные отклонения**, обоснованные ADR.

Каждое нетривиальное upstream-derived утверждение должно иметь источник. Предпочтительны примеры, пригодные для conformance fixtures. C++ class design не используется как язык спецификации, если контракт не относится именно к C++ API.

Разделы:

- `schema/` — records, fields, types, aliases, mappings и построение схемы.
- `protocol/` — QTP messages, framing, negotiation и state machines.
- `encoding/` — byte-level primitives и stateful codecs.
- [TCP transport](transport/TCP-RUNTIME.ru.md) — владение, продвижение и close runtime под управлением вызывающего кода; [граница ресурсов DNS](transport/DNS-RESOURCES.ru.md).
- [STREAM-коллектор](subscriptions/STREAM-COLLECTOR.ru.md) — очередь каждой записи, membership/revision barriers, whole-frame overflow и оплата освобождения.
- [TICKER-коллектор](subscriptions/TICKER-COLLECTOR.ru.md) — общий latest-кеш, ограниченные локальные представления, initial/live порядок, явные retention и stale/reset барьеры.
- [Агрегация интересов](mux/INTERESTS.ru.md) — точный upstream union, независимые локальные клиенты, raw HISTORY bounds и атомарные ограниченные изменения.
- [Локальная доставка STREAM](subscriptions/CLIENT-DISPATCH.ru.md) — сериализованные callbacks, ограниченные межпоточные команды, независимые батчи и явное локальное завершение; источником и транспортом владеет вызывающий код.
- [Контролируемые STREAM-подписки](../docs/SUBSCRIPTIONS.ru.md) — опциональные plain-TCP CLI и программный пример, точный выбор, общие записи и prepared summaries с отдельными локальным и транспортным исходами.
- [Подписки](subscriptions/CONTRACTS.ru.md) — контракты delivery ownership, resource limits и lifecycle; [целевое состояние](subscriptions/STATE.ru.md), [исходящие сообщения](subscriptions/WRITER.ru.md), [чистая live-сессия](subscriptions/LIVE-SESSION.ru.md), STREAM staging и ограниченный TICKER cache/views реализованы с [выбранными Java-доказательствами](../tests/fixtures/subscriptions/CAPTURES.ru.md). Сквозной клиент и HISTORY-коллектор ещё не реализованы.
