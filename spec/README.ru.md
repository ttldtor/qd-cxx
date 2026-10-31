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
- [Подписки](subscriptions/CONTRACTS.ru.md) — проектные контракты native desired state, delivery ownership, resource limits и lifecycle; implementation и peer qualification ещё не выполнены.
