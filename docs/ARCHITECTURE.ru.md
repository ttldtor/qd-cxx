# Архитектура

[English version](ARCHITECTURE.md). Английский документ является основным.

## Цель

`qd-cxx` будет воспроизводить внешне наблюдаемые контракты Java QD без механического повторения иерархии классов или исторических решений реализации. Цель — модульная, проверяемая система на C++23.

## Предлагаемые границы

Это направления проектирования, а не обязательства по реализации:

- **qd-core**: records, field values, scheme semantics, symbols, subscriptions и core state transitions. Не должен зависеть от transport.
- **schema/model**: предлагаемые immutable resolved records, fields, полные type identities, names/lookups и Java slot projection; source operations и mapping/provider information сохраняются как provenance, а не executable Java mappings.
- **encoding**: байтовые примитивы и stateful codecs с явными правилами ошибок и владения.
- **protocol**: QTP messages, framing, negotiation и protocol state machines независимо от socket library.
- **transport adapters**: connections, I/O scheduling, backpressure, lifecycle и интеграция с будущим networking runtime.
- **tools**: importers, inspectors, fixture capture и compatibility diagnostics.

Зависимости направлены внутрь: transport может зависеть от protocol; protocol — от encoding и core contracts; core не должен зависеть от transport.

## Принципы проектирования

- Описывать наблюдаемое поведение до реализации.
- Явно определять ownership, lifetime, cancellation, threading и error semantics.
- Отделять детерминированные преобразования от I/O.
- Предпочитать заменяемые adapters на границах upstream, filesystem, timing и transport.
- Сохранять прослеживаемость поведения Java QD до исходников.
- Использовать ADR для существенных или труднообратимых решений.

## Пока не решено

Не выбраны networking library, build system layout, package manager, test framework, public API, threading model или binary compatibility policy. Эти решения требуют ограниченных задач и доказательств.

## Предложение QDCXX-007

[Требования normalized model](../spec/schema/NORMALIZED-MODEL.ru.md) и [ADR-0001](decisions/ADR-0001-normalized-model-boundaries.ru.md), статус **proposed**, разделяют source declarations/overlays как importer provenance, resolved local schema как самостоятельный core artifact и incoming sender-ID mappings вместе с requested/selected outgoing layouts как самостоятельный protocol state. Local schema identity не включает peer/session state. Logical values и Java-compatible slot observations не определяют физическое хранение C++.

Versioned JSON interchange рекомендован, но не принят; format/profile versions, immutable snapshots и scoped references предлагаются без выбора dependencies или public APIs. [Условия продолжения](NEXT-STEPS.ru.md) разделяют решения владельца, целевые пробы, importer contract и encoding work. Предложение не разрешает importer или C++ implementation.
