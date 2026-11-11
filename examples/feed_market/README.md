# Mixed market events

[Русская версия](README.ru.md)

This standalone program uses the installed `qdcxx_feed` target. It subscribes to
Quote, Trade and Quote-derived Order, prints the selected events and final delivery
and connection counters, and waits without an application polling loop. It uses ordinary composite Quote and Trade for `AAPL`, with an explicit `COMPOSITE` Order source,
and an indexed `AAPL&Q` selector with `REGIONAL_BID` for Order. The latter requests regional Quote
and the bid side of regional Order delegates. The default Trade route retains the exact selector spelling.
Halfway through the interval it adds `MSFT` with the same composite source to the logical subscription.
Local acceptance of the change does not acknowledge peer receipt.

Build from the repository root with an installed qd-cxx package that includes the
feed component, using the matching compiler and runtime configuration.

```text
cmake -S examples/feed_market -B build/feed-market -G Ninja -DCMAKE_PREFIX_PATH=/path/to/qdcxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/feed-market
build/feed-market/qdcxx-feed-market --help
build/feed-market/qdcxx-feed-market --run --duration-ms 10000
```

On Windows use a qualified compiler terminal and `build/feed-market/qdcxx-feed-market.exe`.
An installed TLS-enabled package may require its qualified OpenSSL root at configure
time, as described in the [build guide](../../docs/CPP-BUILD.md). This program uses
plain TCP. `--address HOST:PORT` selects another compatible source. Demo data may be
delayed or unavailable because of service access, entitlements or market hours.
Regional Quote and Trade availability depends on the source. A successful connection
does not promise that every selected family or source will publish an event.

No arguments and `--help` print English help without connecting or creating an SDK
runtime. `--help-ru` prints Russian help. The application exits with 0 after receiving
at least one event and draining the local admitted prefix, 1 on failure or no events,
2 for invalid arguments, or 130 on interruption. The interval accepts 2–600000 ms,
with 10000 ms as the default. Ctrl+C requests shutdown. The process starts SDK and
signal-handling threads and a network connection. It does not modify input files.
`deliveredAvailability` and `readBytesAvailability` are the integer values of
`core::StatisticAvailability`; `desiredSelectors` is the local logical count, while
`readBytes` counts transport bytes. Counters describe the local SDK. The sample prints any retained endpoint diagnostic to stderr, including transport errors observed after a successful local drain.

The application calls `find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)` and
uses only public headers. It can be copied with its CMake file and signal bridge into
another project. The [feed API guide](../../docs/FEED-API.md) documents endpoint,
listener and statistics contracts.

## Verify without the public demo

In a repository build with Feed, examples and tests enabled, run the deterministic
loopback checks:

```text
cmake --build build --config Release --target qdcxx_feed_market qdcxx_feed_market_tests
ctest --test-dir build -C Release -R "^feed_market(_help)?$" --output-on-failure
```

The test starts a local QTP peer on a temporary loopback port, launches the sample,
and checks the exact subscription requests, the added MSFT interest, the seven
scripted business events and the final delivery counters. It uses no public data
service or DNS lookup. The sample supplies the runtime's resolver setting locally
for the numeric `127.0.0.1` address. The [test source](../../tests/conformance/feed_market_tests.cpp)
defines the fixed values and records.
