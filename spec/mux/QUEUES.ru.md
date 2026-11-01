# Ограниченные локальные MUX delivery queues

[English version](QUEUES.md). Английская версия является нормативной.

Подключите [`qd/mux/queue.hpp`](../../include/qd/mux/queue.hpp) и свяжите необязательный `qdcxx_mux_queue`. `MuxQueue` компонует
один настоящий [MuxState](INTERESTS.ru.md), один настоящий family collector и конечные независимые
регистрации. Вызывающая сторона задаёт неизменяемую схему, ёмкости, work credits, consumers и полные
события, допущенные прежней [LiveSession](../subscriptions/LIVE-SESSION.ru.md). Очередь не выполняет
декодирование, I/O, background pumping, чтение часов, текстовый glob expansion и wire reconciliation.
Локальное завершение команды/доставки не подтверждает socket writes, применение peer и полный subscription facade.

## Композиция и атомарные границы

| Factory | Настоящий источник и маршрутизация |
| --- | --- |
| `createStream` | Один union [StreamCollector](../subscriptions/STREAM-COLLECTOR.ru.md); ограниченный source chunk атомарно публикуется всем здоровым выбранным клиентам |
| `createTicker` | Один общий [TickerCollector](../subscriptions/TICKER-COLLECTOR.ru.md) с настоящим независимым source view каждого клиента |
| `createHistory` | Один общий [HistoryCollector](../subscriptions/HISTORY-COLLECTOR.ru.md) с настоящим независимым source view каждого клиента |

Очередь повторно использует cache/snapshot/transaction semantics collectors, не создаёт копий их
алгоритмов и не интерпретирует business values. Detached records сохраняют точные context, symbol,
полные generic field identities, scalar bits, nullable UTF-16/bytes и family-produced flags.
`source` содержит необязательные настоящие raw metadata; синтезированные projections не получают
выдуманного source provenance. HISTORY `trigger` сохраняет настоящие trigger metadata, когда они
есть. Raw indexed bounds остаются signed 64-bit значениями без вывода календаря/единиц.

`admit` принимает полного совместимого владельца события и переносит владение только при успехе.
При отказе вход остаётся пригодным. `DATA` проходит staging до последующих desired commands;
`RELEASE_ONLY` controls освобождаются в сериализованном owner domain и не становятся consumer records.
Pending owner или обязательная фаза могут дать `BUSY`; перед новым admission явно продвиньте их.

У `open` нет initial desired payload. Она резервирует полные client maxima и готовит fixed backing,
port/result/command/control/mapping storage до публикации handle. Для TICKER/HISTORY `openView` —
последнее fallible component action. Отказ сохраняет прежних клиентов и desired state. Factory
заранее создаёт пустые внутренние native MUX clients, поэтому настоящая MUX revision может быть
ненулевой при пустых public generation count и aggregate demand. Внутренние clients не являются
публичными регистрациями.

## Команды, синхронизация и callbacks

`MuxHandle::add/remove/replace/clear` копируют ограниченный заимствованный вход до возврата.
Немедленные submission errors отличаются от pending `MuxCommandTicket`; `result()` пуст до
публикации неизменяемого результата. `APPLIED` означает authoritative desired mutation;
`REJECTED` сохраняет настоящий `MuxError`; `CANCELLED_CLOSING` обозначает отмену на локальном cutoff.
`APPLIED` не подтверждает source synchronization: `SYNC_PENDING` нужно продвинуть до подходящей
active/paused/reset фазы. `aggregateEntries()` заимствует настоящий полный desired union, а не
отправленный output; span истекает при effective mutation, retirement, move или destruction.

`progress(QueueWork&)` выполняет одно конечное fair owner action. Скрытого worker и неявного
пополнения credits нет. Успех сообщает настоящий `QueueProgressKind`, но не обязан завершать команду,
source synchronization, callback или shutdown.

`MuxConsumer::onBatch` получает `BorrowedMuxBatch`, действующий только до return/exception.
Копирование borrow не продлевает время жизни. `retain()` без аллокации создаёт `OwnedMuxBatch`,
владеющий уже подготовленным immutable backing. Record/field spans действуют при живом lease.
Обычные возвраты `CONTINUE` и `PAUSE` подтверждают весь batch; `PAUSE` запрещает последующие callbacks
до `requestResume()`. Поэлементного ACK и прерывания выполняющегося callback нет. Бросивший callback
учитывается и не повторяется: его побочные действия уже могли произойти.

## Контракт потоков и времени жизни

Owner methods, moves и destruction сериализуются извне. Owner может перемещаться между потоками;
привязки к factory thread нет. Независимые handles, tickets, control/result queries и immutable
leases допускают cross-thread use при обычных правилах времени жизни C++-объектов. Одновременные
move/изменение одного объекта handle по-прежнему требуют внешней синхронизации.

Callbacks и окончательные library-held consumer releases выполняются вне port/container locks.
Strong consumer reference и retirement reentrancy guard защищают выполнение и окончательный
callable release; рекурсивные owner mutation/progress дают `REENTRANT`. Execution owner обязан
пережить callback: уничтожение owner из собственного callback не поддерживается. Reserved handle
controls и `MuxQueue::requestStop` разрешены из callback согласно public header.

Detached ports, tickets, records, batches, epochs и их чистый reservation ledger не удерживают
живого protocol/callback execution owner. Они могут пережить очередь и освобождаться в другом
потоке. Настоящее backing освобождается до снятия его charge. Retired generation reservations
сохраняются до освобождения всего соответствующего backing. Перед reuse закрытие очищает
настоящую native desired map и закрывает source view; сохранённое старое backing может занимать
slot после terminal. Проверки port origin/generation не дают старым командам выбрать новую activation.

Последний public handle release записывает abort без ожидания. Он закрывает admission, но не
доказывает callback quiescence. Проверяйте настоящий `terminal()` result; один `TERMINAL` sentinel
state недействительного/moved объекта не является завершённым результатом.

## Явные reservations и работа

Все maxima задаёт вызывающая сторона; ноль буквален и не означает unlimited. Factory preflight
проверяет полное максимальное backing до queue allocation/component construction. Open preflight
включает сохранённые старые generations; candidates и old backing сосуществуют до настоящего release.

| Область | Лимиты |
| --- | --- |
| Aggregate registrations и event/chunk quanta | `QueueLimits::clients`, `sourceRecords`, положительные `routeRecords`, `batchRecords`, `cleanupRecords` |
| Aggregate detached delivery | `dataItems`, `recordBytes`, `batchOwners`, `batchBytes` |
| Aggregate commands/results/controls | `commandSlots`, `resultSlots`, `controlSlots` (два reserved control slots на generation) |
| Полная queue request scope | `residentBytes` |
| Per-client delivery | `ClientLimits::dataItems`, `recordBytes`, `batchOwners`, `batchBytes`, положительный `batchRecords` |
| Точный per-record payload | `recordFields`, `recordSymbolUnits`, `recordFieldUnits`, `recordByteUnits`, `recordArenaBytes` |
| Per-client command/result storage | `commandSlots`, `resultSlots`, `commandEntries` (включая duplicates), `commandSymbolUnits` |

Record bytes включают настоящие STL control/object requests и полные requested arena capacities.
Batch bytes включают настоящие control/object и roots/view-array requests. Fixed queue/generation
metadata также включают настоящие STL control/object/vector requests. Это квалифицированные размеры
запрошенного хранилища, а не RSS, physical headers аллокатора, total process heap и произвольные
consumer capture graphs. Числовое измерение control block использует ту же allocator request shape
до создания backing; поддерживаемая single-request STL shape проверяется. C++ ABI exception-transport/
runtime infrastructure может выделять память отдельно; отсутствие process allocations не заявляется.
Ownership/control blocks входят в admission, исключения для такого overhead нет.

Per-generation record/batch maxima один раз резервируют aggregate capacity; счётчики настоящего
live backing отделены от reservations. Сохранённые leases/results занимают конечные slots даже
после доставки. `resources()` отдельно сообщает queue, MUX, collector и event scopes. Events
перекрываются с независимо ограниченным session ledger; MUX/core/schema/source budgets и копии
вызывающей стороны сохраняют собственные области. Сумма счётчиков не даёт точный total heap.
Конкурентный detached release может дать соседние моменты в отдельно снятых counters; quiescent
значения точны для своих областей.

`QueueSubmitWork` учитывает operations, visits, text units и bytes. `QueueWork` отдельно учитывает
operations, metadata visits, fields, text units, bytes, command actions и callbacks, а также настоящие
`MuxWork`, `SubscriptionWork` и family collector work. Initialization, rollback, disposal и record
matching оплачиваются до пропорциональных действий. Batch preparation консервативно списывает
удвоенный client batch-byte ceiling. Потраченные credits не возвращаются и не увеличиваются скрыто.
`WORK_LIMIT` требует явно выбранного нового caller turn; постоянно недостаточная работа и callback,
который никогда не возвращается, не дают гарантий latency/termination.

## HISTORY range evidence

`rangeStatus` возвращает настоящую необязательную AS-OF source diagnostics, текущие registration
demand, membership, bound, mapping revision, source epoch и queued/executing counts. `mappingCurrent`
требует согласованности каждого нужного revision domain: client demand, synchronized mapping,
настоящей source-view revision, bound, activation и epoch. Глобальная MUX revision постороннего
клиента не обязана совпадать с revision этого клиента.

`completeDeliveredRange` истинен только для synchronized covered clean delivered state. Unstaged
DATA, `SYNC_PENDING`, reset, известный/не классифицированный gap и недоставленные records мешают
этому утверждению. Одни старые source diagnostics не доказывают completeness нового применённого
request. Query work включает точное key matching и queue visits. Это ограниченное native evidence,
а не эквивалентность Java HISTORY producer и гарантия upstream continuity.

## Поэтапный reset и локальная остановка

TICKER/HISTORY `resetSource` принимает/coalesce owner operation; STREAM требует нового owner.
Операция замораживает intake/callbacks, проверяет бюджет old pending disposal, исключает старого
pending owner, освобождает detached queues конечными quanta, вызывает настоящий source reset,
публикует reserved client notices и выполняет настоящий global ACK. `ResetStatus` показывает фазу,
настоящую source sequence, committed/acknowledged flags и сохранённую ошибку. Bookkeeping оплачен
до необратимого commit. TICKER global ACK может исчерпать work после source commit: сохраняются
sequence и `GLOBAL_ACK_PENDING`, ACK повторяется с настоящими новыми credits; reset не повторяется
и принятый reset не объявляется отклонённым. У настоящего HISTORY ACK нет work argument.

Global ACK и exact notice ACK каждого клиента разделены. `acknowledgeReset(sequence)` записывает
точный request для повторной owner-проверки; stale sequence не подтверждает новый interval. STOP,
RESUME и exact reset ACK используют reserved controls даже при полных data/ordinary commands.
Coalesced notices сохраняют interval evidence без повторного подсчёта одного source notice.

Graceful stop закрывает admission конечным локальным cutoff и по возможности доставляет его prefix.
Abort может усилить graceful. Paused/full client, который не может drain, обязан получить
`FAILED`/`ABORTED` с gap/discard evidence вместо `DRAINED`. Context/client terminals требуют
настоящей quiescence callbacks, commands и source-view cleanup. Destructor fallback выполняет
конечный abort traversal без запуска callbacks; произвольная работа consumer destructor и wall time
исключены. Это не закрывает sockets и не реализует whole-application stop protocol.

Diagnostic counts различают raw `sourceRecordsDiscarded` и per-client delivery projections.
TICKER reset `discarded` считает pending view items; HISTORY `discardedPending` — materialized FIFO
items. Ни один не суммирует cache evictions и retained points. `hadGap` охватывает также неизвестную
исключённую доставку и не равен `discarded > 0`. Counters насыщаются с явным flag. Первая
non-cancellation failure остаётся primary при stop/retirement; поздние причины — ограниченное secondary evidence.

## Ошибки и интеграция

Немедленные `QueueError` различают invalid state/configuration/client/event/context/epoch,
reentrancy, busy/closing, command/result/resource/work limits, allocation failure, identity exhaustion,
callback failure и source failure. Owner operations возвращают `QueueFailure` с необязательной
настоящей ошибкой `MuxError`, core subscription или family collector. Typed error не выдумывает
source exception message. Failed transaction не публикует частичный candidate; продолжающиеся
reset/closing phases сохраняют уже committed effects, не обещая межкомпонентного rollback.

[Сборка](../../docs/CPP-BUILD.ru.md#mux-delivery-queues) описывает CLIENT option и installed target.
[Валидация](../../tests/conformance/subscriptions/MUX-VALIDATION.ru.md#mux-queue-validation) отделяет
queue qualification от исторической pure-interest matrix. Прежние lower collector, desired-state
и wire contracts остаются нормативными в своих областях.

## Скомпилированный пример STREAM integration

Эта integration function собрана и выполнена в независимых installed/relocated приложениях Windows
и Linux. Это фрагмент полного приложения вызывающей стороны, а не самостоятельная программа:
вызывающая сторона передаёт готовую совпадающую `LiveSession`, её неизменяемую `LocalSchema` и один
полный STREAM_DATA envelope входных bytes. Квалифицированный preamble использует одну локальную
запись `Sample` с **тремя** compact-int32 полями полного type 8 `TimeHigh`, `TimeLow`, `Value`, явным
`IndexedTimePairInt32{0,1}`, sender ID 41 и точным symbol `ALPHA`. Envelope содержит две одинаковые
записи с raw time 1 и value 10. Этот локальный STREAM layout не объявляет standard Quote adapter
и Java HISTORY producer.

Сериализуйте source и сохраняйте её живой весь вызов; создайте её по прежнему
[контракту LiveSession](../subscriptions/LIVE-SESSION.ru.md). Возвращённый owning batch переживает
queue и callback owner этой функции. Проверенный caller затем читает/освобождает его в другом
потоке и завершает source session. `workForTurn` и все capacities ниже — явные варианты примера,
а не library defaults. Конечные loops в 4 096 turns явно завершаются ошибкой вместо обещания
завершить работу при произвольных caller work и source state.

Ниже — точный скомпилированный consumer prefix с английскими комментариями кода.

```cpp
#include <qd/encoding/compact.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>
#include <qd/mux/queue.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace m = org::ttldtor::qdcxx::mux;
namespace e = org::ttldtor::qdcxx::encoding;

/** Explicit independent caller turn; allocation/disposal credits are never silently replenished by the library.
 * @return Fresh finite caller-selected credits for one owner action.
 */
m::QueueWork workForTurn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Run one local STREAM callback/ownership/close example against a ready serialized actual source.
 * Preamble: Sample/ALPHA has three compact fields (TimeHigh,TimeLow,Value); bytes contains two identical
 * complete STREAM records, time1/value10. No external transport/output completion is implied.
 * The callback retains immutable backing, requests graceful stop, and returns its entire borrowed batch.
 * @param source Ready matching session, serialized through the entire call.
 * @param schema Actual complete immutable matching caller schema.
 * @param bytes One complete STREAM_DATA envelope with two identical requested records.
 * @return Detached immutable batch, valid after this function and its queue/callback owner are gone.
 * @throws std::runtime_error Explicit finite pumping/admission/close failure.
 */
m::OwnedMuxBatch runMuxQueueExample(p::LiveSession &source, const c::LocalSchema &schema,
                                    std::span<const std::byte> bytes) {
    struct Consumer final : m::MuxConsumer {
        m::MuxHandle control;      ///< Pure finite control port, retaining no callback/source owner.
        m::OwnedMuxBatch retained; ///< Immutable owned records only.

        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            retained = batch.retain();
            control.requestStop(m::StopMode::GRACEFUL);

            return m::BatchAction::CONTINUE;
        }
    };

    auto work = workForTurn();
    auto queue =
        m::MuxQueue::createStream(schema, {4, 2, 8, 8, 64, 512, 8, 1024 * 1024}, {4, 8, 64, 512, 16, 1024, 8},
                                  {4, 16, 512 * 1024, 2 * 1024 * 1024},
                                  {2, 16, 4, 4, 4, 16, 512 * 1024, 8, 128 * 1024, 8, 16, 4, 4 * 1024 * 1024}, work);

    if (!queue)
        throw std::runtime_error("queue factory failed");

    auto receiver = std::make_shared<Consumer>();
    work = workForTurn();
    auto port = queue->open({8, 256 * 1024, 4, 64 * 1024, 4, 16, 64, 128, 256, 4096, 4, 8, 8, 512}, receiver, work);

    if (!port)
        throw std::runtime_error("registration failed");

    receiver->control = *port;
    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    m::QueueSubmitWork submit{100, 100000, 100000, 100000};
    auto command = port->add(std::span(&request, 1), submit);

    if (!command)
        throw std::runtime_error("desired command admission failed");

    for (unsigned turn = 0; turn < 4096 && (!command->result() || port->state() != m::QueueClientState::ACTIVE);
         ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("desired progress failed");
    }

    if (!command->result() || command->result()->status != m::CommandStatus::APPLIED ||
        port->state() != m::QueueClientState::ACTIVE)
        throw std::runtime_error("desired synchronization incomplete");

    p::LiveWork sourceWork{10000, 10000000, 10000000, 10000000};
    auto admitted = source.feed(bytes, sourceWork);

    if (admitted.status != p::LiveStatus::EVENT || !admitted.event || admitted.accepted != bytes.size())
        throw std::runtime_error("whole source event failed");

    bool transferred = false;

    for (unsigned turn = 0; turn < 4096 && !transferred; ++turn) {
        work = workForTurn();
        auto result = queue->admit(std::move(*admitted.event), work);

        if (result)
            transferred = true;
        else if (result.error().error != m::QueueError::BUSY)
            throw std::runtime_error("queue source admission failed");
        else if (!queue->progress(work))
            throw std::runtime_error("admission progress failed");
    }

    if (!transferred)
        throw std::runtime_error("source event was not transferred");

    for (unsigned turn = 0; turn < 4096 && !port->terminal(); ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("callback close progress failed");
    }

    if (!port->terminal() || port->terminal()->outcome != m::TerminalOutcome::DRAINED ||
        receiver->retained.records().size() != 2)
        throw std::runtime_error("callback cutoff did not drain");

    queue->requestStop(m::StopMode::GRACEFUL);

    for (unsigned turn = 0; turn < 4096 && !queue->terminal(); ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("context close progress failed");
    }

    if (!queue->terminal() || queue->terminal()->outcome != m::TerminalOutcome::DRAINED)
        throw std::runtime_error("local context did not quiesce");

    return receiver->retained;
}

```

<a id="selector-profiles"></a>

## Opt-in canonical selectors

`MuxQueue::createStream` принимает неизменяемую [selector policy](../subscriptions/SELECTORS.ru.md).
Default и прежний скомпилированный пример остаются `EXACT_ONLY`; явный All использует
`CANONICAL_STREAM_ALL_V1`. TICKER/HISTORY могут выбрать canonical Exact через options без All.
Command/mapping copies сохраняют selector kind. Canonical-domain failure после queue admission
относится к уже принятому UNSTAGED event: обычные shared-source failure/disposal сохраняют nested
cause, counts и gaps. Принятая ownership barrier не отменяется. Reservations и local stop/lease rules
выше сохранены; пересобранные consumers должны учесть текущие metadata sizes.
