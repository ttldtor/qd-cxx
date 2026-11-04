# Собирать живые значения профилей инструментов

[English](IPF-COLLECTOR.md).

Включите `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON` и линкуйте `qdcxx_ipf`. Feed, runtime,
TLS и IPF transfer можно отключить. Полный проверяемый пример и CMake внешнего consumer
приведены в [English](IPF-COLLECTOR.md); он создаёт collector, регистрирует listener,
передаёт AAPL, читает owning snapshot и закрывает collector без pump или фонового потока.

Callbacks получают неизменяемые владеющие batches. Сохраните shared reference на batch
или отдельную ревизию, если данные нужны после callback. Входные профили копируются,
но span должен жить и оставаться неизменным до возврата вызова. Равное значение обновляет
generation без уведомления; TYPE `REMOVED` удаляет символ. `removeGenerations` удаляет
значения выбранных ненулевых поколений и доставляет минимальные профили удаления.

Методы потокобезопасны. Доставка синхронная вне mutex; вложенное/конкурентное обновление
может быть доставлено внешним dispatcher уже после возврата обновляющего вызова.
Remove/close запрещают последующие старты callbacks и не ждут уже начатого. Callback
может удалить себя, закрыть или уничтожить collector. Обычное конкурентное уничтожение
фасада синхронизируйте снаружи. Обрабатывайте возвращённые ошибки admission и
`std::bad_alloc`. Исключения callback удерживаются и отражаются в `callbackFailures()`.

Удержание старых наблюдений расходует конечные revision/unit/batch allowances;
освобождайте их после использования. Это counts/UTF-16 units, не точные RSS bytes.
Полный [контракт ресурсов и lifetime](../spec/ipf/COLLECTOR.ru.md).

Полный пример:

```cpp
#include <qd/ipf/instrument_profile_collector.hpp>
#include <array>
#include <iostream>

int main() {
    using namespace org::ttldtor::qdcxx::ipf;
    auto collector = InstrumentProfileCollector::create();
    auto listener = collector->addUpdateListener([](auto batch) {
        for (const auto &revision : batch->profiles()) {
            std::cout << revision->revision << '\n';
        }
    });
    if (!listener) return 1;
    InstrumentProfile profile;
    if (!profile.setSymbol(u"AAPL") || !profile.setType(u"STOCK")) return 2;
    const std::array updates{profile};
    if (!collector->updateInstrumentProfiles(updates, 1)) return 3;
    auto snapshot = collector->snapshot();
    if (!snapshot || snapshot.value()->profiles().size() != 1) return 4;
    collector->close();
}
```

```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE qdcxx_ipf)
target_compile_features(consumer PRIVATE cxx_std_23)
```
