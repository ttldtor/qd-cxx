# Ограниченное объединение интересов подписок

[English version](INTERESTS.md). Английская версия нормативная.

`MuxState` владеет одной schema/family областью и конечным числом локальных
клиентов. На каждой revision публикуется точное объединение их desired interests.
Чистый компонент `qdcxx_mux` зависит от `qdcxx_core`; protocol output, sessions,
коллекторы, callbacks и очереди медленных потребителей имеют отдельных владельцев.

[Контракт состояния](../subscriptions/STATE.ru.md) задаёт сопоставимые desired-key
правила. [Профиль захватов](../../tests/fixtures/subscriptions/CAPTURES.ru.md)
содержит выбранные Java-наблюдения aggregate bounds.
[Решение о владении](../../docs/decisions/ADR-0005-live-subscriptions.ru.md)
разделяет локальный intent, protocol output и completion. [Валидация реализации](../../tests/conformance/subscriptions/MUX-VALIDATION.ru.md) фиксирует нативную матрицу и границы доказательств.

## Идентичность и смысл объединения

Ключ состоит из неизменяемой record-context provenance и точного UTF-16 символа.
Равные числовые ID и одинаково выглядящие независимые схемы не совпадают.
Семейство контракта принадлежит владельцу. HISTORY дополнительно задаёт signed-64
raw lower bound — атрибут ключа, не Unix milliseconds и не business source ID.

Для STREAM/TICKER aggregate key существует, пока его желает хотя бы один клиент.
Для HISTORY aggregate bound равен signed-min границ заинтересованных клиентов.
Удаление одного клиента не удаляет чужой ключ. Повышение минимальной границы
может изменить aggregate без удаления самого ключа.

Полный aggregate snapshot описывает желаемый upstream demand. Он не означает
wire delta, записанные байты, применение или подтверждение peer. Desired state
может меняться при отключённом источнике.

## Атомарные изменения клиента

Явные create/remove client используют opaque owner/slot/activation токены.
Разрушение токена не удаляет клиента. Membership различаются между клиентами;
remove/re-add и изменение HISTORY bound отменяют старую активацию. Неизменённые
записи сохраняют membership. Счётчики не переполняются и не переиспользуют ID.

`add/remove/replace/clear` проверяют целиком заимствованный ввод до публикации.
Равные дубликаты сворачиваются, противоречивые атрибуты дают атомарный отказ.
Replace задаёт точное множество, включая эффективную замену пустым.
Отказ и операция без изменения сохраняют revision, membership и ссылки.

Каждое эффективное изменение lifecycle или desired state повышает global revision
один раз. Создание/удаление пустого клиента может повысить её без изменения
aggregate. Кандидат и объединение публикуются одной owner-транзакцией;
ошибочная синхронизация после отдельной публикации клиентского state исключена.

## Выбор и заимствованные наблюдения

`clientEntries` показывает desired entries только выбранного клиента;
`aggregateEntries` — полное объединение для существующего writer. Они заимствуют
published bank и инвалидируются эффективной мутацией, перемещением или разрушением
владельца. Aliased input читается до удаления опубликованного backing.

`select` возвращает только текущую membership выбранного клиента. Для HISTORY
raw time должен включительно достигать его lower bound. Это raw interest predicate;
snapshot/transaction control markers по-прежнему требуют семантики коллектора.
Метод не распределяет событие и не вызывает callback.

Ошибка запроса отличается от отсутствия/false, включая work-отказ.
`clientEntries` проверяет owner/slot/generation за константное время; прочие
ограниченные поиски оплачиваются work. Методы сериализуются; конкурентный доступ
и фоновое продвижение к этому компоненту не относятся.

## Ресурсы и сроки жизни

Два фиксированных bank с создания резервируют client entries, aggregate slots
и символы. Aggregate symbols используют собственные units того же bank. Old
и candidate bank учитываются всегда, включая scratch. Команды не выделяют память
и не меняют ёмкости контейнеров.

Владелец захватывает record contexts и indexed-layout metadata, удерживая fixed
core identity без schema declarations. Однократный резерв `schema.storageBytes()`
консервативно покрывает эту импортированную identity с намеренным завышением.
Локальные метаданные и оба bank также учитываются. Requested backing не включает
allocator bookkeeping и не является RSS.

Лимиты конечны; ноль означает нулевую ёмкость. Ненулевой по указателю пустой символ
допустим при нулевой text-arena capacity благодаря фиксированному empty sentinel.
Null view недопустим. Embedded zero и raw UTF-16 surrogates сохраняются точно;
зарезервированный leading star подчиняется отдельному wildcard-контракту.

Work покрывает инициализацию, input/deduplication/context проверки, client/entry
сканирования, aggregate сравнения, копирование текста и retirement. Кредит
списывается до изменения scratch и не возвращается при отказе. Пожизненной квоты
сообщений нет. Деструктор освобождает ограниченных владельцев вне turn credits
без гарантии ограниченной задержки.

Перемещение передаёт source tokens, отменяет старые destination tokens
и инвалидирует заимствованные spans; self-move сохраняет владельца. Удержанный
токен сохраняет только fixed local identity без banks, schema declarations и callbacks.

## Компилируемый пример потребителя

Ниже приведён полный исходный файл потребителя, который был скомпилирован. Он создаёт двух локальных клиентов с одинаковым STREAM interest, удаляет одного клиента, затем проверяет оставшееся агрегированное состояние и выбор interest второго клиента. Пример показывает только локальное желаемое состояние: соединения с источником и доставки событий нет. Проверенный вывод: `MUX union=1; remaining client selected; revision=5`, код завершения 0.

Соберите исходник через CMake с установленным пакетом (замените `C:/sdk/qdcxx` на префикс установки):
```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(MuxExample MuxExample.cpp)
target_compile_features(MuxExample PRIVATE cxx_std_23)
target_link_libraries(MuxExample PRIVATE qdcxx_mux)
```

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/sdk/qdcxx
cmake --build build --config Release
.\build\Release\MuxExample.exe
```

```cpp
#include "qd/mux/interest.hpp"

#include <array>
#include <iostream>

/** Exercise the installed pure MUX API without client, runtime, JSON or ambient configuration.
 * Link this example with qdcxx_mux; nonzero exit identifies a failed contract check.
 * @return Zero for the exact union after isolated client removal, otherwise a deterministic failure code.
 */
int runMuxExample() {
    namespace c = org::ttldtor::qdcxx::core;
    namespace m = org::ttldtor::qdcxx::mux;
    const std::array records{c::LocalRecord{0, u"Custom", false, {{8}}, {{u"Value", 0}}, {}}};
    auto schema = c::LocalSchema::create(records, {1, 1, 1, 16, 32});

    if (!schema) {
        return 1;
    }

    m::MuxWork work{100, 1000000, 1000000, 1000000};
    auto state = m::MuxState::create(*schema, c::SubscriptionFamily::STREAM, {1, 2, 4, 4, 16, 64, 4, 65536}, work);

    if (!state) {
        return 2;
    }

    const auto a = state->createClient(work);
    const auto b = state->createClient(work);

    if (!a || !b) {
        return 3;
    }

    const std::array requests{c::SubscriptionRequest{{schema->recordContext(0), u"AAPL"}, {}}};

    if (!state->add(*a, requests, work) || !state->add(*b, requests, work)) {
        return 4;
    }

    if (!state->removeClient(*a, work)) {
        return 5;
    }

    if (state->aggregateEntries().size() != 1 || state->aggregateEntries()[0].key.symbol != u"AAPL") {
        return 6;
    }

    const auto selected = state->select(*b, requests[0].key, {}, work);

    if (!selected || !selected->has_value()) {
        return 7;
    }

    std::cout << "MUX union=1; remaining client selected; revision=" << state->revision() << '\n';

    return 0;
}

/** Run the documented installed-consumer example.
 * @return The deterministic example contract result.
 */
int main() {
    return runMuxExample();
}
```

