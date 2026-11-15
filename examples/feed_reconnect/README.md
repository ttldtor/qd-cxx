# Reconnect a Feed

[Русская версия](README.ru.md)

The [example](main.cpp) subscribes to Quote, requests a reconnect after ten seconds,
and continues using the same subscription. Desired symbols and listeners survive
the reconnect; there can be a data gap. Attempts are finite, and reconnect does not
replenish them. This example uses plain TCP. The main thread sleeps during both
receive windows while the Feed worker delivers events. `Endpoint::create()` uses
the library's default runtime. The sample waits for its endpoint to close, and
the library joins the worker at process teardown.

Build with an installed qd-cxx package and a compatible C++23 compiler:

```sh
cmake -S examples/feed_reconnect -B build/feed-reconnect -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build/feed-reconnect
build/feed-reconnect/qdcxx-feed-reconnect --help
build/feed-reconnect/qdcxx-feed-reconnect --run demo.dxfeed.com:7300 AAPL
```

From the installed sample directory, run `cmake -S . -B build -G Ninja
-DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx`.

The service may be unavailable, delayed or require entitlements. A successful
connection does not guarantee quotes, and this example does not verify delivery
in both connection epochs. Exit 0 means the endpoint was connected at shutdown;
it does not prove delivery or a gap-free stream. Any retained endpoint diagnostic
is printed even after a successful recovery. No arguments print help without connecting.
