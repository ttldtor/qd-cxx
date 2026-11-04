# Collect live instrument-profile values

[Русская версия](IPF-COLLECTOR.ru.md).

Enable `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON` and link `qdcxx_ipf`. The collector is available
with Feed, runtime, TLS and IPF transfer disabled. A separate installed consumer can use:

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

Callbacks receive immutable owning batches; retain a batch or a revision shared reference
when data must outlive the callback. Supplied profiles are copied. Keep the input span
alive and unmodified until the call returns. Equal values refresh generation without a
notification; TYPE `REMOVED` deletes a symbol. `removeGenerations` deletes nonzero tagged
values and supplies minimal removal profiles.

Calls are thread-safe. Delivery is synchronous outside locks; nested/concurrent updates
can be delivered by the existing outer dispatcher after their updating call returns.
Removal/close prevent future callback starts and do not wait for an already started one.
A callback can remove itself, close or delete the collector. Synchronize ordinary concurrent
facade destruction externally. Handle returned admission errors and `std::bad_alloc`.
Callback exceptions are contained and visible through `callbackFailures()`.

Retaining old observations consumes the explicit finite revision/unit/batch allowances;
release them when no longer needed. These are counts/UTF-16 units, not exact RSS bytes.
See the complete [lifetime/resource contract](../spec/ipf/COLLECTOR.md).
