# ADR-0009 — Владение live IPF connection и retirement

[English version](ADR-0009-live-ipf-connection-ownership.md) является нормативной.

Статус: проект реализации для независимого lifetime/concurrency review; приёмка продукта
и универсальное source/API соответствие Java не заявляются.

## Контекст

Закреплённый QD запускает Handler thread и допускает callback-driven close. C++ facade
с joinable thread при удалении из callback попытался бы join-ить себя. Перенос process SDK
cleanup на worker нарушил бы существующий pre-thread bootstrap/creator-cleanup контракт.
После native cancellation необходим drain до освобождения owners.

QD присваивает Last-Modified после process, перед resource close. Поздняя ошибка close
может оставить committed metadata. Прежний C++ source reader сохраняет first error и
фиксирует metadata только после native retirement.

Источники: [закреплённый QD connection](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java),
[actual JDK8 provenance](../../tests/fixtures/ipf/live/connection-provenance.json),
[transfer contract](../../spec/ipf/TRANSFER.ru.md).

## Решение

Явный creator-owned InstrumentProfileConnectionRuntime владеет одним lazy worker и одним
непереиспользуемым lifetime-слотом. Connection facade хранит stable shared control/port без
thread/SDK cleanup. Callback close/delete запрашивает cutoff; worker operation живёт до drain.
Creator отдельно закрывает/join-ит runtime, затем освобождает bootstrap lease/owned SDK.
Borrowed профиль хранит стабильную явную host identity, без скрытого ApiRuntime bootstrap
и нового global initialization domain.

Callbacks, capture destruction и collector publication происходят вне owner/native locks.
State notifications запущенного подключения сериализованы на worker, не запущенного —
на creator при final retirement. Обычное конкурентное уничтожение facade и уничтожение
runtime на worker остаются запрещённым поведением caller.

Сохраняются first native/parser/HTTP/auth/admission cause и owning published prefix.
Metadata меняется после успешного native retirement. Sticky semantic completion отдельно
от active attempt, HTTP framing, container integrity, IPF EOF и native retirement.
Сохранены проверенные QD pending carryover, поколения/removal, capability,304/date и retry.

Work/backing/attempt/lifetime/history grants явны и конечны. Capacity ровно один lifetime
slot не обещает reusable pool. QD inf — signed64 numeric delay при независимых конечных
операционных lifetime/work/attempt limits.

## Альтернативы

Thread внутри facade небезопасен при callback deletion. Detached worker теряет joined
retirement/creator cleanup. Application polling переносит SDK delivery machinery в клиент.
Повторный whole-response snapshot load не даёт live prefix/carryover после ошибки.

## Совместимость и миграция

Java source/API и произвольная callback-thread identity не обещаются. Приложение использует
явный runtime и creator shutdown вместо join в connection destructor. Metadata намеренно
строже Java assignment-before-close: native/TLS close error сохраняет прежний Last-Modified.
Published revisions/semantic completion доступны. Следующий conditional check может снова
запросить источник, не теряя updates и не пропуская ошибочную передачу.

Wire extension нет: используются X-Live и прежние source-reader origin/auth/compression
правила. Pure collector/value не получает worker/SDK dependency. Проверяются доставка до
EOF, carryover, поколения,304/date, close/reentry/delete, first cause, finite admission,
moved bootstrap identity и отдельные installed/moved consumers.

Connection резервирует непереиспользуемый nonzero domain под collector mutex с конечным identity exhaustion. Публичные integer tags сохраняют domain0. Removal сравнивает domain и generation; равное значение обновляет оба без notification. Разные runtime не удаляют profiles друг друга или manual domain0. Внутренний model entry дополнен8-byte domain; collector limits остаются counts/UTF-16/work, а не точными allocator bytes/RSS. Резервирование domain списывает одну lifetime connection work unit до публикации.
