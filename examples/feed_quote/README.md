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

### Presets for an installed package {#sample-build-profiles}

The [Feed build profiles](../../docs/CPP-BUILD.md#portable-feed-profiles) prepare
an installable plain package without repeating internal library switches.
After `cmake --install build/qdcxx-feed-windows` from the repository root, run
these commands from `examples/feed_quote`:

```text
cmake --list-presets
cmake --preset qdcxx-sample-windows
cmake --build --preset qdcxx-sample-windows
```

On Linux use `qdcxx-sample-linux`. Only the host's sample preset is visible;
[shared hidden profiles](../../cmake/BuildProfiles.json) supply the same Release/MD
or Debug ASan/UBSan settings as the library. CMake 3.25+, Ninja and the qualified
compiler terminal are prerequisites. The sample never configures or downloads the
library or SDKs. It calls `find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)`.

Default package prefixes are `build/install/qdcxx-feed-windows` and
`build/install/qdcxx-feed-linux` from the checkout root. For a different installed
or relocated package use `cmake --preset qdcxx-sample-windows -DCMAKE_PREFIX_PATH=/your/prefix`
(or the Linux name). Compiler/CRT/sanitizer settings must match that package.
Binary directories are `build/qdcxx-sample-windows` or `build/qdcxx-sample-linux`
at the checkout root; build presets default to two jobs, overridden by `--parallel N`.
The executable is `qdcxx-feed-demo.exe` on Windows or `qdcxx-feed-demo` on Linux.
Pass `--help` for offline help. Direct Linux execution should use
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`
when retaining the qualified sanitizer environment; presets do not change your shell.

The manual configure/build form below remains supported.
From the qd-cxx source root, with your actual installed prefix:

```text
cmake -S examples/feed_quote -B build/feed-quote -G Ninja -DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/feed-quote --config Release --parallel 2
```

The [standalone CMake project](CMakeLists.txt) uses:

```cmake
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
add_executable(qdcxx_feed_demo main.cpp)
target_link_libraries(qdcxx_feed_demo PRIVATE qdcxx_feed)
```

The required `feed` component gives an early, actionable package error if Feed
was not installed. `qdcxx::feed` is an equivalent alias for `qdcxx_feed`; the
existing target name and component-free package lookup remain supported.
For optional discovery use `find_package(qdcxx CONFIG REQUIRED OPTIONAL_COMPONENTS feed)`
and check `qdcxx_feed_FOUND` before linking. Only `feed` is a supported named component.

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

Output lines contain the symbol and `bid`, `bidSize`, `ask`, `askSize`, using Java-like
business getters with binary64 `max_digits10` precision. This preserves direct LONG conversion;
the separate WideDecimal fields remain available as raw decimal views. See
[field conversions](../../spec/events/FIELD-CONVERSIONS.md). System IPv4 DNS is discovered by `ApiRuntime`;
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

The receiving interval uses `awaitState` with a steady-clock deadline and stop token;
there is no state/sleep polling loop. Subscription failure requests that same token
and remains exit 1. The SDK installs no process signal handlers. This CLI owns a
scoped signal bridge: POSIX `sigwait` for SIGINT/SIGTERM with complete saved/restored
signal dispositions and creator mask, or a Windows Ctrl+C/Break event reader.
Only the ordinary reader calls `request_stop`; SIGPIPE is scoped while POSIX output
and native work remain live. SDK callbacks quiesce before bridge restoration on
normal and exceptional exits. Windows console close/logoff/shutdown retain prior
OS behavior. Pending real POSIX signals racing restoration retain prior OS semantics.
See the [complete wait and signal contract](../../spec/subscriptions/FEED-WAITS.md).
The CLI creates one application signal-reader thread in addition to the SDK worker;
both retire before runtime bootstrap destruction.

For owning Trade, use the same endpoint and createSubscription<events::Trade>() with getPrice()/getSizeAsDouble(); the [short example](../../docs/FEED-API.md) uses SDK progress without application polling. Built-in Trade is composite; a regional record requires an explicit registry, as specified in [Trade](../../spec/events/TRADE.md). This console executable retains its Quote output; a public live connection is distinct from deterministic installed-consumer checks.

For the distinct extended-hours snapshot use createSubscription<events::TradeETH>() with the same
business getters. The console remains a Quote example; [TradeETH](../../spec/events/TRADE-ETH.md)
describes its separate required flags and regional registration.

For session snapshots use createSubscription<events::Summary>() with getDayOpenPrice(),
getPrevDayVolume() and getOpenInterest(); [Summary](../../spec/events/SUMMARY.md) specifies its exact
business-int conversion and regional OpenInterest absence. This executable retains its Quote output.
