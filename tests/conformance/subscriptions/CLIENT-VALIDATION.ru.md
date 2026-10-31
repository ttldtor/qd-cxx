# Проверки локальной доставки STREAM

[English version](CLIENT-VALIDATION.md). Английская версия нормативная.

Native-тест `client_dispatch` проверяет [контракт диспетчера](../../../spec/subscriptions/CLIENT-DISPATCH.ru.md)
через настоящие события `LiveSession` и STREAM-коллектор. Для сборки теста включите
`ORG_TTLDTOR_QDCXX_BUILD_CLIENT`. Runtime и JSON могут оставаться выключенными.

Профиль окончательных исходников CLIENT ON / RUNTIME OFF / JSON OFF проходит
**42/42 CTests** на Windows x64 MSVC 19.42 Release и Linux x86-64 GCC 13.3 Debug
ASan/UBSan. LeakSanitizer основной матрицы выключен. На обеих платформах пройдены
27 клиентских сценариев / 2949 проверок и 21 сценарий коллектора / 21724 проверки.
В полном сохранённом выводе Linux-тестов нет сообщений санитайзеров. Это результаты
исполнения данных входов, а не доказательство безопасности всех возможных чередований.

Отдельная сборка GCC 13.3 Debug ThreadSanitizer проходит девять целевых сценариев
владения/межпоточного доступа, 1367 проверок, без сообщений санитайзера. Дополнительный
UBSan с явной остановкой при ошибке проходит четыре клиентских сценария / 559 проверок
и сценарий перемещения коллектора / 42 проверки. Эти запуски отделены от полной матрицы.

Production-only сборки и настоящие потребители перенесённого установленного пакета
проходят на обеих платформах. У каждого 29-файлового пакета все хеши сохранены после
переноса, включая нестандартные каталоги `lib64` и `public include`. Потребитель удерживает
батч, запрашивает graceful stop, наблюдает локальный terminal и освобождение callable,
затем освобождает независимые данные и завершённый ticket на другом потоке.
Runtime и JSON SDK ему не нужны.

## Проверяемое поведение

- Self-remove, remove/re-add и замена сохраняют исполняемый батч из нескольких записей,
  выводя из очереди последующие старые активации. Повторный вход в продвижение отклоняется.
- Graceful stop доставляет принятый допустимый префикс; abort учитывает отброшенный
  и неклассифицированный ввод. Исключение callback не повторяется и не заменяется остановкой.
- Последнее публичное освобождение с другого потока не завершает исполняющийся callback.
  Получение нового handle и последнее освобождение имеют один согласованный исход;
  копирование существующего закрывающегося handle позволяет наблюдать terminal.
- Принятые события переживают исчерпание работы staging/release. Их постановка в очередь
  предшествует обычным изменениям; повтор освобождения после нулевого выбора не возвращает
  записи, поступившие до add.
- Заполнение обычной очереди или слотов результатов не мешает зарезервированной остановке.
  Завершённые tickets переживают диспетчер без повторного использования занятых слотов
  и без удержания живых операций.
- Нулевые обычные ёмкости остаются конечными и сохраняют зарезервированную остановку;
  проверка произведения размеров допускает ноль без деления на него. Ошибочный live-кадр
  не выдаёт частично декодированный префикс записей в callback.
- Независимые батчи сохраняют точные context, membership, generation, nullable UTF-16,
  непрозрачные байты и исходные метаданные после уничтожения всех владельцев протокола.
  Последнее освобождение на другом потоке возвращает атомарную резервацию.
- Лимит удержанных батчей, отказы выделения памяти, недостаток арены или рабочего бюджета
  сообщаются явно. Неудачная копия не подтверждает более ранний префикс.
- Текст команды копируется до возврата отправителю, включая пустые значения, NUL и непарные
  UTF-16 units. Параллельные отправители не обращаются к изменяемому состоянию протокола
  или коллектора.
- Независимые контексты схем отклоняют взаимную подмену; пустая очередь не принимает
  чужую эпоху. Уничтожение последней библиотечной ссылки на callable может читать закрытый
  порт вне блокировок изменений.
- Java fixture полного стандартного Quote достигает callback и существующего prepared
  Quote mapping. Проверяется также явная передача исходящих подписок внешнему владельцу.

Отдельный тест коллектора `collectors_stream` покрывает атомарные подготовку и подтверждение
батча, чередование одиночных и пакетных offers, нулевой и выведенный префиксы, инвалидацию
при перемещении. Прежнее поведение перемещения одиночного offer сохранено.

## Границы квалификации

Диспетчер не владеет сокетом или сессией источника. Локальный terminal не подтверждает
вывод операций транспорта, исходящих данных или частичного кадра. Клиент поверх тестового
TCP peer, TLS, reconnect, общий MUX и полные подписки требуют отдельных проверок интеграции.
Память созданного приложением Consumer, захваченных объектов и независимых глубоких копий
относится к области приложения. Учёт запрошенной памяти и работы не измеряет RSS,
накладные расходы аллокатора или длительность callback.

## Собираемый пример владения

Этот фрагмент conformance-примера собирают и исполняют потребители перенесённого пакета
на обеих платформах. Он предполагает внешнюю готовую сессию, соответствующую схему
с локальной записью 0 и один полный STREAM_DATA-кадр ALPHA со значением 10. Ёмкости
и конечные границы циклов заданы для примера и не являются умолчаниями библиотеки.
Показаны локальные callbacks, удержание данных и остановка; сетевой клиент не создаётся.
Сессией и её исходящими сообщениями по-прежнему управляет вызывающая сторона. Возврат 0
означает завершение проверенного локального жизненного цикла; другие значения указывают
на неуспешный шаг.

```cpp
#include <qd/client/stream_dispatcher.hpp>
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <thread>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace d = org::ttldtor::qdcxx::client;

/** Supply explicit finite example work, never library defaults.
 * @return Owner turn shared by all example progress calls.
 */
d::DispatchWork ownerTurn() {
    return {128,
            64,
            16,
            100000,
            100000,
            100000,
            16 * 1024 * 1024,
            {64, 100000, 100000},
            {128, 100000, 100000, 16 * 1024 * 1024}};
}

/** Application-created callback state; library-held ownership ends before terminal publication. */
struct SampleConsumer final : d::Consumer {
    d::ClientHandle handle;
    d::OwnedBatch retained;
    std::size_t calls = 0;
    bool valid = true;
    bool requested = false;

    /** Retain independent immutable data and request graceful stop without waiting.
     * @param batch Callback-only borrowed values, retained explicitly for later thread-safe use.
     */
    void onBatch(const d::BorrowedBatch &batch) override {
        ++calls;
        retained = batch.retain();

        for (const auto &record : batch.records()) {
            const auto value = record.typedValues.empty() ? record.values[0] : record.typedValues[0].raw;
            valid = valid && record.symbol == u"ALPHA" && value == 10;
        }

        requested = handle.requestStop(d::StopMode::GRACEFUL).has_value();
    }
};

/** Run one local consumer over an actual externally owned ready protocol session.
 * The caller supplies exactly one complete STREAM_DATA envelope containing one ALPHA record.
 * Explicit example capacities are not library defaults. No callback/owner waits for itself.
 * @param session Actual serialized protocol driver; this function does not duplicate it.
 * @param schema Matching immutable local provenance.
 * @param bytes One whole controlled source frame.
 * @return Zero only after local terminal, callable retirement and foreign-thread detached release; nonzero on failure.
 */
int runClientExample(p::LiveSession &session, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    d::OwnedBatch retained;
    d::CommandTicket completed;
    std::weak_ptr<SampleConsumer> callable;
    const auto context = schema.recordContext(0);
    {
        auto consumer = std::make_shared<SampleConsumer>();
        callable = consumer;
        auto work = ownerTurn();
        const d::ClientLimits limits{2, 4, 4, 32, 4, 8, 64, 128, 128, 8192, 2, 32768, 131072};
        auto dispatcher = d::StreamDispatcher::create(schema, {1, 2, 16, 32, 4, 64, 4}, {4, 8, 131072, 262144}, limits,
                                                      consumer, work);

        if (!dispatcher) {
            return 1;
        }

        auto handle = dispatcher->handle();

        if (!handle) {
            return 2;
        }

        consumer->handle = *handle;
        const c::SubscriptionRequest request{{context, u"ALPHA"}, {}};
        d::SubmitWork submit{8, 128, 128};
        auto command = handle->add(std::span(&request, 1), submit);

        if (!command) {
            return 3;
        }

        for (unsigned step = 0; step < 16 && !command->result(); ++step) {
            if (!dispatcher->progress(work)) {
                return 4;
            }
        }

        if (!command->result() || command->result()->status != d::CommandStatus::APPLIED) {
            return 5;
        }

        // Applied is local desired state, never automatic wire emission or peer acknowledgement.
        p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
        auto event = session.feed(bytes, inputWork);

        if (event.status != p::LiveStatus::EVENT || event.accepted != bytes.size() || !event.event) {
            return 6;
        }

        // Hand the source event over before any later ordinary command progress.
        if (!dispatcher->admit(std::move(*event.event))) {
            return 7;
        }

        for (unsigned step = 0; step < 32 && !handle->terminal(); ++step) {
            if (!dispatcher->progress(work)) {
                return 8;
            }
        }

        const auto terminal = handle->terminal();

        if (!terminal || terminal->outcome != d::DispatchOutcome::DRAINED || terminal->counts.completedRecords != 1 ||
            !consumer->valid || !consumer->requested || consumer->calls != 1) {
            return 9;
        }

        retained = std::move(consumer->retained);
        completed = *command;
    }

    // Dispatcher and callable are gone. Completed tickets/batches retain no live operation or callback.
    if (!callable.expired()) {
        return 10;
    }

    std::atomic<bool> valid{false};
    std::jthread producer([batch = std::move(retained), ticket = std::move(completed), context, &valid]() mutable {
        valid = batch.records().size() == 1 && batch.records()[0].context.matches(context) &&
                batch.records()[0].symbol == u"ALPHA" && ticket.result().has_value() &&
                ticket.result()->status == d::CommandStatus::APPLIED;
        batch = {};
        ticket = {};
    });
    producer.join();

    return valid.load() ? 0 : 11;
}
```
