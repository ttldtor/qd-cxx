# Quote option strikes

[Русская версия](README.ru.md)

[main.cpp](main.cpp) reads a complete local IPF, selects up to four strikes around a supplied price in the first option series, and subscribes to the available calls and puts. It prints each symbol with its bid and ask. The first series is ordered by its metadata, beginning with expiration; this example does not choose an expiration dynamically.

Build an installed qd-cxx package with `ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` and `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`. Then build this example with a compatible C++23 compiler:

```sh
cmake -S examples/feed_option_quotes -B build/option-quotes -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build/option-quotes
ctest --test-dir build/option-quotes --output-on-failure
```

Run `qdcxx-option-quotes FILE UNDERLYING CENTER [ADDRESS]`. For example, use your own complete, uncompressed IPF and an address that supplies its option symbols:

```sh
build/option-quotes/qdcxx-option-quotes options.ipf SPY 500 demo.dxfeed.com:7300
```

The reader accepts up to 64 MiB and 100,000 profiles. The address defaults to `demo.dxfeed.com:7300`; the TCP subscription lasts ten seconds. Data availability and entitlement depend on the source. The bundled [example.ipf](example.ipf) has synthetic symbols and is for testing selection, not live market data:

```sh
build/option-quotes/qdcxx-option-quotes examples/feed_option_quotes/example.ipf ABC 11 --list
```

`--list` prints the selected symbols without a connection. See `--help` for input limits and exit codes. On Windows use `.exe`; multi-configuration builds place it in the selected configuration directory.

Use `-S .` when building from the installed example directory.
