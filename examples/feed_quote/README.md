# Typed Quote feed example

[Русская версия](README.ru.md) · [Public feed API guide](../../docs/FEED-API.md)

This finite console program receives full `Quote` events through the public
endpoint/feed/subscription API. The library owns network progress and its worker;
the main thread observes duration/interruption and awaits shutdown.
[main.cpp](main.cpp) contains the complete application.

## Build against an installed package

You need CMake 3.25+, a supported C++23 toolchain and a qd-cxx package built with
`ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` plus explicit CLIENT, RUNTIME and RECOVERY.
Follow the [package build instructions](../../docs/CPP-BUILD.md#public-feed-api-build),
including the qualified Asio/c-ares prerequisites. TLS and JSON are not required.
For the recommended TLS-OFF package, the consumer needs no SDK source roots or
SDK headers. If the installed package was built with TLS enabled, its package
configuration also requires the exact qualified OpenSSL installation, even when
this application links only the plain feed API; add
`-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified` to the configure command
below with your actual [qualified TLS SDK](../../docs/CPP-BUILD.md#tls-source-profile-cpp-build-en)
path. Asio/c-ares source roots are not required by the consumer.

From the qd-cxx source root, with your actual installed prefix:

```text
cmake -S examples/feed_quote -B build/feed-quote -G Ninja -DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/feed-quote --config Release --parallel 2
```

The [standalone CMake project](CMakeLists.txt) uses:

```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(qdcxx_feed_demo main.cpp)
target_link_libraries(qdcxx_feed_demo PRIVATE qdcxx_feed)
```

C++23 and native static-link dependencies are inherited from the package. Use the
same compatible compiler/standard-library/CRT profile as the installed package.
On Linux replace the prefix with your installation path. Windows/Ninja produces
`build/feed-quote/qdcxx-feed-demo.exe`; Linux produces `build/feed-quote/qdcxx-feed-demo`.
Visual Studio places the executable in the `Release` subdirectory.

The root project builds target `qdcxx_feed_demo` when the feed API and
`ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES` are enabled. Examples default to ON for a
top-level qd-cxx build and OFF when qd-cxx is included with `add_subdirectory`.
Set `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=OFF` to build the library without this
executable; this does not disable the feed API or its library tests. The
standalone project above builds the example directly against an installed
package, independently of the option used to build that package.

## Run

```powershell
./build/feed-quote/qdcxx-feed-demo.exe --help
./build/feed-quote/qdcxx-feed-demo.exe --help-ru
./build/feed-quote/qdcxx-feed-demo.exe --run --address demo.dxfeed.com:7300 --symbol AAPL --duration-ms 10000
```

On Linux use the executable without `.exe`. No arguments also print English help.
Help performs no runtime, DNS or connection work. `--run` is required to connect.
The demo address is an input example; external availability and event delivery are
not established by these commands. A source may supply delayed data.

| Option | Default and limit |
| --- | --- |
| `--address HOST:PORT` | `demo.dxfeed.com:7300`; at most 4096 bytes; plain TCP address |
| `--symbol SYMBOL` | `AAPL`; exact printable ASCII, at most 128 units, no wildcard/All |
| `--duration-ms N` | `10000`; integer 1..600000 milliseconds |
| `--help`, `--help-ru` | English/Russian help, standalone only |

Output lines contain the symbol and `bid`, `bidSize`, `ask`, `askSize`, using the
existing exact decimal formatter. System IPv4 DNS is discovered by `ApiRuntime`;
see the [DNS and lifecycle rules](../../docs/FEED-API.md#feed-api).
This program uses default TICKER delivery and plain TCP without credentials.

Exit 0 requires at least one actual Quote, actual local drain without deadline expiry
or an unresolved gap, and no subscription error. Retained endpoint diagnostics are
always printed as static/numeric fields with terminal flags, including when exit 0
meets that local-prefix criterion; earlier endpoint failures remain observable.
Exit 1 means failure, output error or no data; exit 2
means invalid arguments; exit 130 means interruption. Ctrl+C requests close, and the
program awaits owned retirement. A drain result concerns the admitted local prefix,
not peer acknowledgement or completeness of unread network data.

The run creates a worker and sockets, reads system DNS configuration, writes stdout
and diagnostics, and modifies no input files or global settings. Blocking output or
callbacks can delay shutdown; the duration and drain allowance are not hard process
completion deadlines.
