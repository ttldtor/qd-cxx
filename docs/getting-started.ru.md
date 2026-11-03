# Начало работы {#getting-started-ru}

[English version](getting-started.md) · [Главная](index.ru.md) · [Далее: практические руководства](guides.ru.md)

Для приложения с рыночными данными начните с [публичного feed API](FEED-API.ru.md)
и [полного примера Quote](../examples/feed_quote/README.ru.md). Необязательный пакет
сам продвигает соединение и требует SDK транспорта. Пример кодировок ниже остаётся
минимальным знакомством с пакетом без внешних зависимостей.

Пример использует установленную библиотеку кодировок, записывает compact int и читает его обратно.
Нужны CMake 3.25+, компилятор C++23 и штатные средства сборки. Сеть, SDK транспорта, JSON и тестовый
framework не нужны. Другие профили перечислены в [текущих возможностях](CURRENT-STATE.ru.md).

## 1. Собрать и установить библиотеку

Из корня исходников qd-cxx выберите отдельные каталоги сборки и установки:

```text
cmake -S . -B build/library -DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build/library --config Release --parallel 2
cmake --install build/library --config Release --prefix C:/sdk/qd-cxx
```

Замените `C:/sdk/qd-cxx` абсолютным путём установки; в Linux, например, `/home/me/sdk/qd-cxx`.
Для Visual Studio выберите генератор и x64 при configure; для Ninja используйте настроенный терминал
компилятора. Необязательные транспорт, клиент, JSON и CLI остаются выключенными.
[Подробные параметры сборки](CPP-BUILD.ru.md) находятся в разделе разработчика.

## 2. Создать приложение

В отдельном каталоге `consumer` создайте `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(qdcxx_example LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE qdcxx_encoding)
set_target_properties(example PROPERTIES CXX_EXTENSIONS OFF)
```

Создайте рядом `main.cpp`:

```cpp
#include "qd/encoding/compact.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <span>

int main() {
    using namespace org::ttldtor::qdcxx::encoding;
    std::array<std::byte, 5> storage{};
    const auto written = writeCompactInt(42, storage);
    if (written.status != Status::OK) {
        return 1;
    }
    const auto decoded = readCompactInt(std::span<const std::byte>(storage).first(written.written));
    if (decoded.status != Status::OK || decoded.value != 42) {
        return 1;
    }
    std::cout << decoded.value << '\n';
    return std::cout ? 0 : 1;
}
```

## 3. Собрать и запустить приложение

```text
cmake -S consumer -B build/consumer -DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/consumer --config Release
```

Запустите `build/consumer/example` в Linux или `build/consumer/example.exe` для Windows/Ninja.
У Visual Studio файл находится в `build/consumer/Release/example.exe`. Ожидаемый stdout — `42`
и перевод строки; код завершения — 0. Недостаточный выходной буфер даёт `INSUFFICIENT_OUTPUT`
без изменения буфера; пример проверяет оба результата. Буфер принадлежит приложению и заимствуется
только на время вызовов.

Если пакет не найден, проверьте установку и `CMAKE_PREFIX_PATH` либо задайте `qdcxx_DIR` прямо на
`lib/cmake/qdcxx` внутри установки; при другом каталоге библиотек используйте фактический путь.
Пример не меняет глобальные настройки и не создаёт фоновых процессов.
