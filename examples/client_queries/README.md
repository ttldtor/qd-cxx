# Client snapshot queries

[Русская версия](README.ru.md). This independent consumer requests the latest Quote, daily Candle history and one DEFAULT Order snapshot. It builds a local order-book view from the detached result and prints coverage states, including TRUNCATED. A source must support the requested symbols, history and source; an empty result is not evidence of entitlement. Tape snapshot requests are refused by the SDK.

Build this directory against an installed qdcxx package with Feed enabled:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build
ctest --test-dir build --output-on-failure
./build/qdcxx-client-queries --run demo.dxfeed.com:7300
```

No arguments, `--help` and `--help-ru` print help without starting a runtime or opening a connection. `--run HOST:PORT [HISTORY_HOST:PORT [FROM_MS TO_MS]]` creates two SDK endpoints, uses plain TCP and a common ten-second deadline; HISTORY defaults to the first address; it prints source refusals and closes both endpoints. The sample reads the last 24 hours by default; optional signed FROM_MS and TO_MS are inclusive Unix-millisecond bounds and must be supplied together. Exit 0 means the owning query results were returned and both endpoints reached CLOSED or FAILED with final statistics and no active native connection, without deadline expiry; it does not promise lossless continuous-stream drain. A FAILED terminal state preserves its close diagnostic; exit 0 does not mean a clean transport close. The actual drain, gap and diagnostic are reported on stderr when non-clean. Exit 1 means a query or pre-close source failure, or a close deadline failure; 2 means invalid arguments. It writes no files and retains no credentials. Source availability and entitlements are external. NTV and other named native Order sources require an explicitly configured custom event registry.
