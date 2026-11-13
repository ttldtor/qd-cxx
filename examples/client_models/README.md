# Local client models and instrument helpers

[Русская версия](README.ru.md). This independent, offline consumer constructs a synthetic Candle and Order, applies them to the time-series and order-book models, and uses an IPF collector, CFI and option-chain builder with a synthetic stock profile. It does not claim that the values came from a feed. For real profile updates, the separate live IPF consumer is installed at `share/qdcxx/examples/ipf_live` when the optional IPF_LIVE component and its transfer prerequisites are enabled.

The installed qdcxx package must enable both Feed and IPF. Configure and run:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build
ctest --test-dir build --output-on-failure
./build/qdcxx-client-models --run
```

No arguments, `--help` and `--help-ru` print input-free help. `--run` makes no network or file access and creates no worker. Exit 0 indicates successful model operations, 1 a refusal or failure, and 2 invalid arguments. Model values and snapshots own their retained data; model mutation must be serialized by the application.
