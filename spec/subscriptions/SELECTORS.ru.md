# Явные subscription selectors и canonical symbols

[English version](SELECTORS.md). Английская версия является нормативной.

[`SubscriptionSelectorPolicy`](../../include/qd/core/subscription.hpp) — неизменяемый локальный и
controlled-peer profile, выбираемый до activation. Это явное утверждение вызывающей стороны;
descriptors, имена записей, входной текст и remote acknowledgments не согласуют и не включают его.
Default сохраняет legacy Exact semantics и wire bytes. Glob engine, перечисление symbols и глобальный registry не добавлены.

## Профили и ключи

| Профиль | Fixed desired/MUX/collector owners | Subscription writer |
| --- | --- | --- |
| `EXACT_ONLY` | Прежние Exact keys в STREAM/TICKER/HISTORY; без All | Прежние raw FD Exact symbols и message-local FF repeats |
| `CANONICAL_EXACT_V1` | Canonical Exact domain всех трёх families; без All | Canonical Exact всех шести subscription messages |
| `CANONICAL_STREAM_ALL_V1` | Только STREAM; canonical Exact и явный All; создание TICKER/HISTORY owner отклоняет профиль | Canonical Exact всех шести messages; All только STREAM_ADD 16 / STREAM_REMOVE 17 |

Exact key сохраняет record provenance и точный UTF-16 text. Null text недопустим; non-null empty
text, встроенный NUL и непарные UTF-16 units сохраняют прежнюю семантику. Текст с начальным `*`,
включая одиночный `*`, остаётся зарезервированным и недопустимым Exact. `AB*` и `A?` — обычные Exact
strings без интерпретации шаблонов. Нового escape-синтаксиса для буквальной начальной звёздочки нет.

`core::allSymbols(recordContext)` создаёт `SubscriptionSelectorKind::ALL` одной известной записи.
Он не использует desired symbol text и сам не даёт permission. Принимающий STREAM owner уже должен
выбрать `CANONICAL_STREAM_ALL_V1`. All — отдельные key/activation, отличные от Exact-empty и любого
конкретного DATA symbol. Чужие schema contexts недопустимы. Неизвестные записи не выбираются.
All в других profiles/families даёт typed `UNSUPPORTED_SELECTOR`; прежний недопустимый Exact text
сохраняет `INVALID_SELECTOR`. Новые ошибки добавлены в конец без перенумерации прежних.

All занимает одну desired entry и одну activation при нуле desired symbol-text units. Incoming
symbol, source payload, cache, queue, per-record arena, new-symbol и work limits сохраняются.
Профиль не разрешает неограниченную materialization и не резервирует storage всех возможных symbols.

## Canonical wire и incoming domain

Stateless [`encodePentaSymbol`](../../include/qd/encoding/penta_symbol.hpp) в `qdcxx_encoding`
кодирует закреплённый Java Penta domain без allocation и сохранения входа. Исследует не более семи
UTF-16 units и возвращает ненулевой signed32 cipher и от одного до пяти symbol bytes либо отсутствие
encoding. Empty text имеет canonical FE representation. Non-ASCII/unsupported/unrepresentable text
использует явно выбранный raw-string path; точные NUL/непарные units там сохраняются. Вызовы независимы
и thread-safe; SDK, reader fork, mutable dictionary и Threads dependency не добавлены.

Canonical writer profiles используют Penta для encodable Exact text и raw FD/cipher-zero для
unencodable text с message-local repeat state. All компонуется как canonical Penta star в STREAM 16/17.
Legacy `EXACT_ONLY` сохраняет каждый прежний FD/repeat byte. Tuple order, duplicates, sparse sender IDs,
HISTORY signed bounds, output leases и partial-write rules сохраняют [контракт writer](WRITER.ru.md).

Canonical collectors допускают decoded source symbol только при соответствии настоящей cipher/text
пары тому же domain: encodable text требует точного Penta cipher; unencodable text — cipher zero.
Literal FD для encodable text, включая FD star, вне этого incoming profile. Это явное native
ограничение, а не утверждение, что Java отклоняет такие bytes. Canonical Penta `*` в DATA — конкретная
запись: подходящий All получает её один раз. Subscription All и входящая конкретная star — разные понятия.

Default legacy FD writer и canonical Penta keys настоящего Java collector могут принадлежать разным
mapping domains. Равенство decoded text на уровне parser не доказывает collector-key interoperability.
Canonical profile квалифицирован для выбранного controlled-peer path; произвольные provider adapters
могут нормализовать иначе. Совместимость всех providers и protocol permission response не подразумеваются.

Первичные source facts закреплены в [upstream.lock](../../upstream.lock):
[PentaCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java),
[BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java),
[BinaryQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java)
и Matrix [Stream.setEnableWildcards](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/Stream.java).
Эти source/production observations не доказывают полный network subscription facade.

## Whole-event guard и владение

Настоящие STREAM/TICKER/HISTORY collectors предварительно оплачивают canonical-domain visits/text
work и arithmetic всего совместимого события до ownership transfer, candidate construction, epoch
pinning, stale transitions и maintenance. Прямой domain/work/arithmetic refusal сохраняет input,
published state и существующие offers. `UNSUPPORTED_SYMBOL_DOMAIN` отличается от capacity overflow.
В HISTORY guard resource refusal не превращается в shared `STALE`. Остальные прежние collector
overflow/reset rules сохраняются после допуска события guard в соответствующий алгоритм.

Граница admission [MuxQueue](../mux/QUEUES.ru.md) наступает раньше: принятый UNSTAGED owner уже
принадлежит очереди. Поздний collector-domain failure следует прежним shared-source failure/disposal
rules, сохраняет typed nested cause и обычное gap/discard evidence. Он не превращает успешный queue
admission задним числом в caller-owned rejection. Нового cache/snapshot/AS-OF алгоритма нет.

## Сохранённая eligibility и mutation

Одна конкретная STREAM record может совпасть не более чем с первоначальной Exact activation и
первоначальной All activation клиента. Она предлагается один раз с сохранением source order и
одинаковых последовательных records. Настоящие collectors сохраняют matched pair вместо позднего
пересчёта текущего desire. Удаление одной исходной activation сохраняет eligibility через другую;
удаление обеих отзывает её. Remove/re-add и reset создают новые scopes и не оживляют старую eligibility.

`SubscriptionMembership::anyOf` — structural OR same-origin singleton tokens. Это не liveness и
record-matching oracle. Invalid, foreign и уже compound inputs отклоняются до duplicate handling.
Настоящий collector сначала обязан сопоставить обе entries с этой конкретной record. Tokens
сохраняют только прежнее pure identity backing, а не source/callback owners. MUX аналогично
сохраняет два первоначальных client-key choices. Прежние serialized-owner/cross-thread immutable-token
rules и конечные retained-backing reservations остаются в силе.

## Точки выбора и совместимость

Выбирайте policy в `SubscriptionState::create`, `SubscriptionWriter::create` или `MuxState::create`.
`StreamCollector` получает policy с desired-state owner. TICKER/HISTORY выбирают
`TickerOptions::selectorPolicy` / `HistoryOptions::selectorPolicy`; поддерживаются только legacy
и canonical Exact. `MuxQueue::createStream` принимает явную STREAM policy; TICKER/HISTORY factories
передают canonical Exact через соответствующие options. Связанные writer, desired state и incoming
source должны использовать совместимые domains, выбранные владельцами; runtime auto-detection
не исправляет несовпадение.

Standalone dispatcher, `runStream` и прежний CLI остаются legacy `EXACT_ONLY`. Queued All доступен
через `MuxQueue`; копирование selector-kind сохраняет All вместо превращения в Exact-empty. Полный
subscription facade, socket-stop protocol и network demo не добавлены.

Прежняя двухполевая aggregate initialization `SubscriptionKey` по-прежнему означает Exact:
добавленный selector имеет default `EXACT`. Schema/wire record layouts не изменены. C++ object sizes,
private membership fields и metadata footprints вложенных объектов меняются: consumers должны
пересобраться и настроить budgets для текущих headers. Это source compatibility описанных случаев,
а не обещание stable binary ABI; общая [ABI policy](../../docs/ARCHITECTURE.ru.md) ещё не определена.

[Валидация](../../tests/conformance/subscriptions/WILDCARD-VALIDATION.ru.md) разделяет настоящий parser
DATA, выбранные native/Java projections, полные writer bytes, package checks и исключения.
Следующие примеры используют настоящие квалифицированные installed consumers, а не предполагаемый API sketch.

## Проверенный STREAM queue fragment

Код ниже дословно извлечён из installed consumer, собранного/выполненного на Windows и Linux.
Это integration fragment без `main`. Caller передаёт готовую совпадающую сериализованную
`LiveSession`, неизменяемую schema и один полный canonical STREAM frame. Проверенный preamble —
одна известная локальная запись `Sample`, три compact-int32 поля полного type 8 `TimeHigh`, `TimeLow`,
`Value`, явный pair layout 0/1, sender 41 и две одинаковые Penta `ALPHA` rows с raw time 1/value 10.
Это не standard Quote schema. Source/schema provenance и controlled-peer profile должны совпадать.

Для этого conditional example prefix задайте `WILDCARD_CLIENT=1` и линкуйте `qdcxx_mux_queue` из CLIENT ON
installation. Example macro только включает queue code; permission по-прежнему даёт явная factory policy.
Сериализуйте source access весь вызов. Callback сохраняет immutable batch, запрашивает graceful stop
и возвращает весь borrow. Проверенный caller позднее читает/освобождает возвращённый batch в другом
потоке после уничтожения queue/callback owner, затем завершает source. Все capacities/work и границы
4 096 turns — явные варианты примера, а не defaults и обещания latency.

```cpp
#include <qd/collectors/stream.hpp>
#include <qd/encoding/compact.hpp>
#include <qd/encoding/penta_symbol.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>
#include <qd/protocol/live_session.hpp>
#ifdef WILDCARD_CLIENT
#    include <qd/mux/queue.hpp>
#endif

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

namespace e = org::ttldtor::qdcxx::encoding;
#ifdef WILDCARD_CLIENT
namespace m = org::ttldtor::qdcxx::mux;

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
m::OwnedMuxBatch runWildcardQueueExample(p::LiveSession &source, const c::LocalSchema &schema,
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
    auto queue = m::MuxQueue::createStream(schema, {4, 2, 8, 8, 64, 512, 8, 1024 * 1024}, {4, 8, 64, 512, 16, 1024, 8},
                                           {4, 16, 512 * 1024, 2 * 1024 * 1024},
                                           {2, 16, 4, 4, 4, 16, 512 * 1024, 8, 128 * 1024, 8, 16, 4, 4 * 1024 * 1024},
                                           work, c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);

    if (!queue)
        throw std::runtime_error("queue factory failed");

    auto receiver = std::make_shared<Consumer>();
    work = workForTurn();
    auto port = queue->open({8, 256 * 1024, 4, 64 * 1024, 4, 16, 64, 128, 256, 4096, 4, 8, 8, 512}, receiver, work);

    if (!port)
        throw std::runtime_error("registration failed");

    receiver->control = *port;
    const c::SubscriptionRequest requests[]{{c::allSymbols(schema.recordContext(0)), {}},
                                            {{schema.recordContext(0), u"ALPHA"}, {}}};
    m::QueueSubmitWork submit{100, 100000, 100000, 100000};
    auto command = port->add(requests, submit);

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

#endif

```

## Проверенный pure collector fragment

Вторая точная функция использует header/namespace prefix выше без queue helper. Линкуйте
`qdcxx_collectors`, `qdcxx_protocol` и `qdcxx_encoding`; CLIENT может быть OFF, conditional queue block
можно исключить. Она получает целый уже допущенный canonical STREAM `LiveEvent` с тем же schema/Penta
`ALPHA` preamble. Source event и collector остаются сериализованными; прямой staging refusal
сохраняет input ownership. Перекрытие Exact и All даёт две доставленные records для двух входных,
а не по одной копии на selector. Borrowed offers читаются до `pop`. Функция явно проверяет bounded
completion и бросает исключение при отказе; wire/callback ACK она не предоставляет.

```cpp
/** Deliver one complete canonical STREAM event using a known-record explicit All plus Exact.
 * Preamble: the caller has qualified the controlled peer canonical profile, and Sample/ALPHA is a known record.
 * The whole LiveEvent and collector stay serialized; front is borrowed only until mutation/pop/destruction.
 * Fixed finite limits and one caller turn bound this example to at most sixteen admitted records.
 * @param schema Immutable actual source schema, preserving record provenance.
 * @param event Whole actual canonical STREAM source owner, consumed only on successful selection.
 * @return Actual consumed record count, or throws before claiming successful delivery.
 * @throws std::runtime_error Explicit desired/staging/offer/ack failure; no callback or wire ACK is inferred.
 */
std::size_t runWildcardCollectorExample(const c::LocalSchema &schema, p::LiveEvent &&event) {
    auto desired = c::SubscriptionState::create(schema, c::SubscriptionFamily::STREAM, {4, 8, 64, 512, 16, 1024, 8},
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);

    if (!desired)
        throw std::runtime_error("desired factory failed");

    const c::SubscriptionRequest requests[]{{c::allSymbols(schema.recordContext(0)), {}},
                                            {{schema.recordContext(0), u"ALPHA"}, {}}};
    c::SubscriptionWork command{1000, 1000000, 1000000};

    if (!desired->add(requests, command))
        throw std::runtime_error("desired add failed");

    org::ttldtor::qdcxx::collectors::StreamWork work{100000, 1000000000, 1000000000, 1000000000};
    auto collector = org::ttldtor::qdcxx::collectors::StreamCollector::create(
        std::move(*desired), {4, 16, 512 * 1024, 2 * 1024 * 1024}, work);

    if (!collector || !collector->stage(std::move(event), work))
        throw std::runtime_error("staging failed");

    std::size_t consumed = 0;

    for (unsigned index = 0; index <= 16; ++index) {
        auto front = collector->front(work);

        if (!front)
            throw std::runtime_error("offer failed");

        if (!*front)
            return consumed;

        const auto &record = *(**front).record;

        if (record.symbol != u"ALPHA" || record.typedValues.size() != 3 || record.typedValues[2].raw != 10)
            throw std::runtime_error("unexpected materialized input");

        if (!collector->pop(work))
            throw std::runtime_error("acknowledgment failed");

        ++consumed;
    }

    throw std::runtime_error("finite delivery bound exceeded");
}

```
