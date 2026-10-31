# Ограниченный STREAM-коллектор

[English version](STREAM-COLLECTOR.md). Английская версия нормативная.

Подключите `qd/collectors/stream.hpp` и свяжите `qdcxx_collectors`. Чистый C++23
компонент ставит точные STREAM interests в очередь поверх [desired state](STATE.ru.md)
и целиком принятых [событий live-сессии](LIVE-SESSION.ru.md). Каждая запись сохраняет
порядок, включая полностью одинаковые последовательные значения; latest-value cache
не используется. Callback dispatch, общий upstream multiplexing, wildcard, TICKER
и HISTORY — отдельные компоненты. Runtime, socket и JSON dependency не добавляются.

## Владение и вход

`StreamCollector::create` принимает usable STREAM `SubscriptionState`, явные
`StreamLimits` и `StreamWork`. Проверка не меняет desired revision; все fixed rings
выделяются до переноса владения. Отказ сохраняет identity, entries и ownership
входного состояния; оплаченная работа не возвращается. Коллектор move-only.
`desired()` — заимствованный const pointer, null после move; успешный перенос
владения инвалидирует прежние заимствованные представления коллектора.

Все связанные session/event/snapshot/collector access, moves и release должны
быть сериализованы и невложены. Фонового продвижения и прикладных callbacks нет.
Destruction/move assignment освобождают ограниченных владельцев вне caller-turn
credits; они не обещают bounded latency и не выполняют dispatch.

`stage` принимает только полный `LiveEvent` с DATA kind и message type 15.
Сессия уже проверила весь кадр: completed decoder prefix повреждённого кадра не
становится входом коллектора. Точные local record provenance и UTF-16 symbol
выбирают текущие desired entries; sender IDs не являются subscription identity.
Незапрошенные записи и pre-add input не кешируются для позднего replay.

Stage до публикации проверяет matching, ёмкость для всех selected records и полный
удерживаемый event backing. Успешный непустой admission переносит входное событие
и сохраняет membership/revision каждой записи. Первый selected frame закрепляет
непрозрачную эпоху даже после drain. Новые binding generations той же эпохи допустимы;
queued values сохраняют исходный snapshot. Другая эпоха отклоняется и требует нового
коллектора. Нулевая selection сохраняет входного владельца и прежнее offer. Отказ
не забирает вход и не публикует selected prefix.

## Барьеры ревизий и доставки

`add/remove/replace/clear` передают существующие атомарные desired-state commands
с `SubscriptionWork`. Effective mutation инвалидирует прежнее delivery offer;
failed/no-op сохраняют его. Неизменившиеся memberships остаются действительными.
Remove/re-add создаёт новую activation и не оживляет старые queued records.

`front(work)` предварительно оплачивает поиск retired entries и возможное освобождение
владельцев, удаляет retired activations и возвращает первую действительную borrowed
`StreamDelivery`. Admission revision — локальный факт, не peer acknowledgement.
Record pointer заимствует retained frame; successful nonempty stage/pop/discard или
effective desired change его инвалидирует. Failed preflight сохраняет queue/offer.
Повторный front не доставляет и не подтверждает следующую запись.

`pop(work)` требует текущее successful nonempty offer и подтверждает ровно эту
запись. Это не пользовательский callback; client dispatch отдельно соблюдает
[контракт callable/barriers](CONTRACTS.ru.md). `discard(work)` атомарно удаляет всю
очередь без заявления о доставке и сохраняет epoch pin/terminal cause. Счётчики
различают staged, delivered, withdrawn, discarded и ignored с явной saturation.

## Ресурсы и отказы

Все limits конечны, включая ноль: frame slots, selected record slots, retained
whole-event bytes и collector resident bytes. Fixed frame/item arrays не растут.
Retained event учитывает полный State/arena, включая незапрошенные записи, запас
и abandoned monotonic slots. `residentBytes()` добавляет fixed collector backing,
`stagedBytes()` считает только retained event backing.

Desired storage и shared snapshot/epoch reservations имеют прежние отдельные
границы. Верхний владелец должен агрегировать эти области; счётчик коллектора не
является полным transitive heap или process RSS. Borrowed views не создают owning
deliveries и не переносят квоту на вызывающую сторону.

`StreamWork` ограничивает операции, metadata/membership visits, просмотренные
UTF-16 units и byte work. Matching, публикация и освобождение предварительно
оплачиваются до изменения очереди; stage/front/pop не выделяют память. Возможное
уничтожение общего snapshot покрывается получением/оплатой `LiveEvent::releaseBytes()`
непосредственно перед release, с консервативным включением посторонних reservations
эпохи. Work failure сохраняет input/queue/offer и не terminal. Вызывающий код
выбирает новые turns; накопительной tape quota у продолжающегося feed нет.

Capacity overflow — видимый terminal `RESOURCE_LIMIT` с reserved inline
pending/incoming counts. Входной кадр отклоняется целиком, прежние ограниченные
владельцы удерживаются, доставка прекращается. Remove/clear и work-bounded discard
доступны; add/replace не перезапускают terminal collector. Уничтожение библиотеки
не заявляет, что удержанные записи были доставлены.

## Совместимость и проверки

Выбранные [production observations](../../tests/fixtures/subscriptions/CAPTURES.ru.md)
покрывают per-event/interleaved order, удаление интереса двух клиентов и фильтрацию
одинаковых значений без pre-add replay. Native tests сравнивают actual input,
subscription и returned output semantic coordinates/counts с этими наблюдениями.
Wire cipher/raw-symbol representation и pull-loop `more` вне этого collector-level
сравнения. Два независимых локальных коллектора не доказывают общий upstream MUX.

Усиленный local queued-withdrawal/re-add barrier следует проверенному локальному
контракту, а не выводится из Java remove-before-next-input capture. Проверяются также
whole-frame overflow, work retry, old snapshot release, binding generations, moved
owners, failed frames, необычный точный текст, независимая provenance, продолжительное
повторное использование rings и полные standard Quote values через prepared mapping.

См. [профили проверок и границы доказательств](../../tests/conformance/subscriptions/STREAM-VALIDATION.ru.md).

## Проверенный пример интеграции

Функция собирается и запускается installed consumer. Вызывающая сторона передаёт
ready schema-compatible session и ровно один полный STREAM_DATA envelope с одной
selected ALPHA record под local record zero. Limits — пример, не defaults.
Ноль возвращается только после явного pop выбранной записи; nonzero сообщает
локальный отказ. Callbacks и network I/O функция не выполняет.

```cpp
#include "qd/collectors/stream.hpp"
#include <cstddef>
#include <span>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;

int runStreamExample(p::LiveSession &session, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto desired = c::SubscriptionState::create(schema, c::SubscriptionFamily::STREAM, {1, 2, 16, 32, 4, 64, 4});

    if (!desired) {
        return 1;
    }

    s::StreamWork work{16, 1024, 1024, 16 * 1024 * 1024};
    auto collector = s::StreamCollector::create(std::move(*desired), {2, 4, 131072, 262144}, work);

    if (!collector) {
        return 2;
    }

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    c::SubscriptionWork command{4, 1024, 1024};

    if (!collector->add(std::span(&request, 1), command)) {
        return 3;
    }

    p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
    auto admitted = session.feed(bytes, inputWork);

    if (admitted.status != p::LiveStatus::EVENT || admitted.accepted != bytes.size() || !admitted.event) {
        return 4;
    }

    auto staged = collector->stage(std::move(*admitted.event), work);

    if (!staged) {
        return 5;
    }

    std::size_t delivered = 0;

    for (;;) {
        auto next = collector->front(work);

        if (!next) {
            return 6;
        }

        if (!*next) {
            break;
        }

        const auto &record = *(**next).record;

        // Read borrowed record fields only before the acknowledgement invalidates the view.
        if (record.symbol != u"ALPHA") {
            return 7;
        }

        if (!collector->pop(work)) {
            return 8;
        }

        ++delivered;
    }

    return delivered == staged->selected && delivered == 1 ? 0 : 9;
}
```
