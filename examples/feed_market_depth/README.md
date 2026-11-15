# Order books for several symbols

[Русская версия](README.ru.md)

Subscribe to NTV Order events for AAPL and MSFT and print each symbol's
committed bid and ask book. The feed must provide orders for both symbols. NTV data depends on provider
availability and entitlement. Use `--source DEFAULT` with a compatible local
publisher, or `--source COMPOSITE` for quote-derived top-of-book data.

```text
cmake -S examples/feed_market_depth -B build/market-depth -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build/market-depth --config Release
build/market-depth/qdcxx-feed-market-depth --run
```

On Windows use an MSVC developer shell matching the installed SDK.
Use `--help` for options. The example uses plain TCP and permits one connection
attempt; it does not reconnect or reconstruct a book after a gap.
`OrderBookModel` retains unfinished transactions internally, so a printed view
can repeat an earlier committed view until the current transaction completes.
The event listener runs serially; its captured model outlives endpoint shutdown.
Success requires a nonempty committed book for each symbol and an active connection
before a complete local drain. A diagnostic retained during close is printed separately.

Use `-S .` when building from the installed example directory.
