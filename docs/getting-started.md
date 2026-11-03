# Getting started {#getting-started}

[Русская версия](getting-started.ru.md) · [Documentation home](index.md) · [Next: practical guides](guides.md)

For a live market-data application, start with the [public feed API](FEED-API.md)
and its [complete Quote example](../examples/feed_quote/README.md). That optional
package owns connection progress and requires the transport SDK. The encoding-only
example below remains the smallest dependency-free package introduction.

This example uses the installed encoding library to write a compact int and read it back.
You need CMake 3.25+, a C++23 compiler and its native build tools. No network, transport SDK, JSON
or test framework is needed. See [current capabilities](CURRENT-STATE.md) for other profiles.

## 1. Build and install the library

From the qd-cxx source root, choose separate build and installation directories:

```text
cmake -S . -B build/library -DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build/library --config Release --parallel 2
cmake --install build/library --config Release --prefix C:/sdk/qd-cxx
```

Replace `C:/sdk/qd-cxx` with your absolute installation path; on Linux, for example,
`/home/me/sdk/qd-cxx`. For Visual Studio, select its generator and x64 at configure time; for Ninja,
use a terminal configured for the compiler. Optional transport, client, JSON and CLI components remain
OFF here. [Detailed build options](CPP-BUILD.md) are in the developer section.

## 2. Create an application

In a separate `consumer` directory, create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(qdcxx_example LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE qdcxx_encoding)
set_target_properties(example PROPERTIES CXX_EXTENSIONS OFF)
```

Create `main.cpp` beside it:

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

## 3. Build and run the application

```text
cmake -S consumer -B build/consumer -DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/consumer --config Release
```

Run `build/consumer/example` on Linux or `build/consumer/example.exe` with Windows/Ninja.
For Visual Studio, use `build/consumer/Release/example.exe`. Expected stdout is `42` and a newline,
with exit 0. A short output buffer returns `INSUFFICIENT_OUTPUT` without changing the buffer;
this example checks both operation results. The application owns the buffer, borrowed only for each call.

If CMake cannot find the package, check installation and `CMAKE_PREFIX_PATH`, or point `qdcxx_DIR`
directly to `lib/cmake/qdcxx` inside the installation; use the actual directory when installing
libraries elsewhere. The example changes no global settings and starts no background process.
