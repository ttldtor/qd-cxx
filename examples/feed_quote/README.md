# Subscribe to Quote

[Русская версия](README.ru.md) · [Feed API guide](../../docs/FEED-API.md)

The [complete example](main.cpp) creates an endpoint, subscribes to Quote and prints bid and ask prices and sizes.

Build it against an installed qd-cxx package with a compatible compiler and C++23 support. On Windows, run these commands in the same qualified MSVC developer terminal used for the package.

```sh
cmake -S examples/feed_quote -B build/feed-quote -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build/feed-quote
```

### Installed-package presets {#sample-build-profiles}

For a Linux ASan/UBSan package, use the corresponding Debug sanitizer preset to match the package. The installed package can also be used with the qdcxx-sample-windows or qdcxx-sample-linux configure and build presets in this directory. Override CMAKE_PREFIX_PATH when the package is installed elsewhere. See [build profiles](../../docs/CPP-BUILD.md#portable-feed-profiles).

Run with the defaults or choose an endpoint, symbol and duration.

```sh
build/feed-quote/qdcxx-feed-demo --run
build/feed-quote/qdcxx-feed-demo --run --address demo.dxfeed.com:7300 --symbol AAPL --duration-ms 10000
```

Run with `--help` for the options and exit codes. The connection uses plain TCP. Availability and entitlement depend on the source. Demo data can be delayed. The application waits for the requested interval and closes the endpoint before exit. A retained endpoint diagnostic is printed even when the local drain succeeds. Ctrl+C uses the platform default process behavior.
