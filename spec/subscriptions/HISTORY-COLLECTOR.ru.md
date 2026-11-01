# Ограниченный HISTORY-коллектор

[English version](HISTORY-COLLECTOR.md). Английская версия нормативная.

Чистая C++23 граница HISTORY использует неизменяемую локальную схему, целиком
допущенные [события live-session](LIVE-SESSION.ru.md) и
[желаемое состояние](STATE.ru.md) каждого представления. Generic partition задаётся
точным контекстом записи и UTF-16 символом; точка определяется полным signed-64
raw indexed time. Это не декодер business source ID и не модель книги заявок.
Обычные untimed-записи отвергаются.

[Профиль Java-захватов](../../tests/fixtures/subscriptions/CAPTURES.ru.md) даёт
выбранные production-наблюдения. Факты исходников, исполненные сравнения и нативные
политики сохраняют разные границы. Protocol, runtime, upstream demand и прикладные
callbacks принадлежат отдельным владельцам. Публичный C++ API устанавливается в
[`qd/collectors/history.hpp`](../../include/qd/collectors/history.hpp) существующей
цели `qdcxx_collectors`; [страница валидации](../../tests/conformance/subscriptions/HISTORY-VALIDATION.ru.md)
фиксирует проверенное поведение и ограничения.

## API и владение

`HistoryCollector::create` принимает владеющую `core::LocalSchema`, явные
`HistoryLimits`, `HistoryOptions` и `HistoryWork`. Схема вызывающего кода перемещается
только при успехе. Коллектор перемещаемый; операции владельца сериализованы и
нереентерабельны. Фабрика проверяет запрошенное фиксированное хранилище до выделения
памяти и фактические capacity до публикации. Квоты core desired state и session
сохраняют свои независимо контролируемые области.

`openView` создаёт локальное представление с явными настройками проекции snapshot
флагов. `add`, `remove`, `replace` и `clear` используют отдельные кредиты core-команды
и коллектора. `closeView` завершает это представление. Токены определяют точного
владельца, слот и активацию; устаревшие и чужие handles явно отклоняются. Разные
отделённые копии токена можно копировать и освобождать в разных потоках: их
фиксированный скалярный marker не удерживает protocol lease, desired-state owner
или callback. Связанные владельцы коллектора и протокола остаются сериализованными.

`stage` потребляет успешно допущенное целое событие источника. `front` и `pop`
подготавливают и подтверждают локальную проекцию; `rangeStatus` наблюдает её полноту.
`reset` и `acknowledgeReset` реализуют явный reset-барьер владельца.
`forgetUnobserved` отдельно освобождает пустые метаданные по правилам ниже.
Исчерпание work сохраняет опубликованное состояние и владение событием вызывающего кода.

## Raw-прогресс и полнота

HISTORY может выдавать raw-прогресс незавершённой транзакции или snapshot.
Ожидающие записи не задерживаются молча до конца транзакции. Покрытие источника,
согласованность его транзакции и согласованность конкретного представления —
разные факты. Нулевые флаги и END выше запрошенной нижней границы не доказывают
полноту запрошенного диапазона.

| Ситуация | Значение |
| --- | --- |
| Legacy-данные без наблюдавшегося BEGIN/MODE | Raw-прогресс допустим; snapshot coverage неизвестно. |
| Включительная нижняя граница ещё не покрыта | Запрошенный диапазон неполон. |
| Источник покрывает диапазон, но явная/неявная транзакция ожидает завершения | Прогресс остаётся неполным. |
| Источник согласован, но initial/local-прогресс представления ещё ожидает | Это представление неполно. |
| Согласованная граница покрывает запрос и представление согласовано | Полнота относится только к точным epoch, membership и observation. |
| SNIP завершился выше запрошенной нижней границы | Snapshot завершён с усечением; полнота всего запроса не доказана. |
| Bound расширен ниже известной сохранённой границы | Прежняя полнота больше не действует. |
| Failed view, shared stale, ожидающий reset или разрыв эпохи | Свежая полнота недоступна. |

`rangeStatus` — независимое наблюдение с epoch, membership/revision и observation
serial. Запрашивайте его заново после продвижения владельца: копия не обновляется
сама и не удостоверяет текущую свежесть коллектора.

Полнота относится к запрошенному диапазону. Например, представления запрашивают
нижние границы 0 и 2. После убывающего BEGIN на 3 и следующей точки 2 верхний
диапазон покрыт. Когда его проекция выдана и транзакция источника не ожидает
завершения, `completeRequestedRange` истинно для этого наблюдения, хотя отдельный
факт `snapshot` источника остаётся `RECEIVING` для нижнего интереса. Диапазон от 0
остаётся неполным до покрытия. После нового изменения нужно новое наблюдение.

## Общее хранилище и выдача представлений

Общее хранилище содержит ограниченные indexed points и snapshot frontiers.
Представления имеют независимые desired bounds, initial cursors, transaction state
и конечные локальные FIFO обновлений. Initial history идёт по убыванию; измеренный
баланс initial/local использует quantum 100, отдельно от snapshot quantum 10000
для partition. Options задают их явно, без неявных name-based properties.
Conflation ограничен измеренным последним pending local record с тем же индексом.

Замена snapshot может генерировать removals для вытесненных точек. Поэтому
проекция HISTORY имеет отдельный тип: local record provenance, точный символ,
raw index, нормализованные флаги и typed field views. Синтетические обычные поля
получают точные schema defaults; indexed contributors содержат выданное время.
Nullable text и opaque bytes различаются, бизнес-десериализации нет.

Необязательный raw source record относится только к настоящей допущенной записи.
Сгенерированные границы/removals не выдумывают sender, cipher, raw flag или
`removalTime`. Метаданные настоящего trigger отделяются от выходной проекции.

`front` готовит одно заимствованное предложение без публикации cursor/queue и кредитов
выдачи/планирования. При этом явные кредиты `HistoryWork` вызывающего кода расходуются.
`pop` подтверждает именно его. Неудавшаяся подготовка сохраняет каждый
байт прежнего предложения; scratch у представлений раздельный. Успешный stage,
эффективная мутация, pop, close, reset, forget или перемещение владельца инвалидируют
затронутые ссылки до освобождения. Вызывающая сторона заканчивает использование
до этих операций. Доступ коллектора/protocol owners сериализован без reentrancy.

## Хранение и явное забывание

В измеренном false-retention профиле немедленный last-remove/re-add не повторяет
данные, а обычный live-ввод остаётся закрыт до нового BEGIN. Поэтому малые
snapshot-метаданные пустого partition отличаются от точек и владельцев payload.
True retention может повторить историю и выдать difference removals нового snapshot.

Нативный профиль без часов не переносит opportunistic 60-second expiry Java для
пустых метаданных. `forgetUnobserved` явно освобождает пустые метаданные без живого
интереса и pending reference. Конечный отчёт фиксирует забытое continuity
и неповторяемое поколение. Здоровые представления не сбрасываются, downstream ACK
не нужен. Новая активация начинает отдельную локальную область continuity.
Так отсутствует невосстанавливаемый накопительный лимит ключей без фонового
таймера, молчаливого LRU и неограниченной карты tombstones.

## Допуск и области отказов

Лимиты ограничивают partitions, points, символы, shared cache frames/bytes,
представления, их pending items/frames/bytes и requested resident storage.
Ноль означает нулевую ёмкость. Core desired-state и session snapshot/epoch backing
имеют отдельные контролируемые области; requested bytes не означают RSS или SLA.

План целого кадра проверяет context и source semantics до публикации. Shared
old-plus-incoming peaks проверяются без расчёта на освобождение после отказа view.
Общее исчерпание отвергает непотреблённый ввод и делает общие доказательства stale.
Если shared admission проходит, полный набор failed views определяется до одного
атомарного bound-trim/release commit. Failed views доступны для запросов до close
и перестают участвовать в retention/bounds. Изоляция здоровых views действует
внутри независимо ограниченной shared-области.

Work-отказ сохраняет прежние cache, views, offers и владение вводом. Кандидатные
ошибки и ссылки на caller input не выходят из отвергнутого плана. Work учитывает
инициализацию массивов, поиск, равенство, sweeps, view matching, очереди, scratch
и возможное последнее освобождение владельца. При замене учитываются старый
и входящий backing, даже если итоговый размер поместился бы.

Reset инвалидирует полноту, удаляет прежние cache/pending и сохраняет desired
interests. Резервное coalesced notice требует точного подтверждения владельца
до свежего допуска/выдачи. Это не подтверждение callbacks, транспорта или peer.
Disconnect/overflow внутри транзакции не превращается в полный snapshot через reset.
Cleanup деструктора освобождает ограниченных владельцев вне turn credits,
не обещая доставки или ограниченной задержки.

Обычный indexed point MAX отвергается зафиксированным исходником; MAX REMOVE —
виртуальная граница транзакции. Это source-backed нативные политики с отдельными
boundary tests, а не исполненное Java MAX-record сравнение. MAX для wire-bound
и MAX обычной точки данных относятся к разным входам квалификации.

## Пример функции для вызывающего кода

Установленный consumer компилирует и запускает эту функцию как часть драйвера вызывающей стороны. Это не самостоятельная программа: вызывающая сторона передаёт уже готовую сериализованную `LiveSession`, соответствующую неизменяемую схему и один полный envelope `HISTORY_DATA`. В примере `Sample`: индексированные `TimeHigh`/`TimeLow` находятся по индексам 0/1, обычное поле `Value` — по индексу 2. Контролируемый envelope содержит BEGIN с raw time 3 и value 30, затем END|REMOVE с raw time 0. Функция проверяет обе проекции, evidence полноты запрошенного диапазона от 0 и явное закрытие view; при успехе возвращает ноль. Готовностью/закрытием источника, transport, callbacks и upstream demand владеет вызывающая сторона. Это общий caller-indexed HISTORY profile, а не заявление о Java HISTORY Quote/Profile producer.

```cpp
#include <qd/collectors/history.hpp>
#include <qd/encoding/compact.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;
namespace e = org::ttldtor::qdcxx::encoding;

/** Run one bounded local HISTORY example against an already ready serialized source.
 * The caller provides Sample/ALPHA with explicit TimeHigh/TimeLow indexing: BEGIN at3/value30
 * followed by END|REMOVE at0. All callbacks, upstream demand, transport and source closure are external.
 * Borrowed projections are inspected before pop; only detached range evidence survives acknowledgment.
 * @param source Ready matching session, owner-serialized through this call.
 * @param schema Complete matching immutable caller schema.
 * @param bytes Exactly one complete controlled HISTORY_DATA envelope.
 * @return Zero after all two projections, complete range evidence and explicit view close; otherwise failure.
 */
int runHistoryExample(p::LiveSession &source, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto copy = schema;
    s::HistoryWork work{1000, 1000000, 1000000, 100000000};
    const s::HistoryLimits limits{
        2, 4, 16, 4, 512 * 1024, 1, 8, 4, 512 * 1024, 2 * 1024 * 1024, {4, 8, 32, 128, 16, 256, 16}};
    auto history = s::HistoryCollector::create(std::move(copy), limits, {false, 100, 10000}, work);

    if (!history)
        return 1;

    auto view = history->openView({true}, work);

    if (!view)
        return 2;

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, 0};
    c::SubscriptionWork desiredWork{1000, 100000, 100000};

    if (!history->add(*view, std::span(&request, 1), desiredWork, work))
        return 3;

    p::LiveWork inputWork{10000, 10000000, 10000000, 10000000};
    auto input = source.feed(bytes, inputWork);

    if (input.status != p::LiveStatus::EVENT || input.accepted != bytes.size() || !input.event)
        return 4;

    if (!history->stage(std::move(*input.event), work))
        return 5;

    std::size_t delivered = 0;
    constexpr std::size_t maximumProjections = 4;

    for (std::size_t n = 0; n <= maximumProjections; ++n) {
        auto next = history->front(*view, work);

        if (!next)
            return 6;

        if (!*next) {
            auto status = history->rangeStatus(*view, request.key, work);

            if (!status || !status->completeRequestedRange || delivered != 2)
                return 7;

            return history->closeView(*view, work) ? 0 : 8;
        }

        if (n == maximumProjections)
            return 9; // Explicit partial drain rejection, never a complete result.

        const auto &record = (**next).record;

        if ((delivered == 0 && (record.rawTime != 3 || record.eventFlags != 4 || record.fields[2].raw != 30)) ||
            (delivered == 1 && (record.rawTime != 0 || record.eventFlags != 10)))
            return 10;

        if (!history->pop(*view, work))
            return 11;

        ++delivered;
    }

    return 12;
}
```

<a id="selector-profiles"></a>

## Canonical Exact domain

`HistoryOptions::selectorPolicy` имеет default `EXACT_ONLY`; opt-in `CANONICAL_EXACT_V1` выбирает
[canonical incoming domain](SELECTORS.ru.md). HISTORY All и STREAM-All owner profile остаются
неподдерживаемыми. Whole-event domain/work/arithmetic guard выполняется до ownership, candidate,
epoch, stale и maintenance mutations; отказ сохраняет input/state/offers. Guard resource failure
не превращается в shared STALE. Прежние capacity-overflow, snapshot, AS-OF и reset semantics сохранены.
Выбранные parser/projection comparisons не доказывают full native Java HISTORY producer equivalence.
