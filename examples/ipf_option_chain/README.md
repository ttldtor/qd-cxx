# Build an option chain from an IPF file

[Русская версия](README.ru.md).

[main.cpp](main.cpp) reads a complete local UTF-8 IPF file with `readInstrumentProfilesFile`, builds chains with `OptionChainsBuilder::build`, and prints series for one underlying. The IPF package is sufficient for this application.

Build the package with `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`. The example is available from this directory or from `share/qdcxx/examples/ipf_option_chain` in the installed SDK. With that SDK as the prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qd-cxx
cmake --build build
ctest --test-dir build --output-on-failure
./build/qdcxx-ipf-option-chain example.ipf ABC
```

Use `.exe` on Windows. Multi-configuration generators place the binary in the selected configuration directory.

The bundled [example.ipf](example.ipf) contains the call and put at strike 10 from the [Java option-chain oracle inputs](https://github.com/ttldtor/qd-cxx/blob/master/tests/fixtures/option-chains/README.md). It is a deterministic example.

```text
ABC series=1
calls=1 puts=1 strikes: 10
```

Use your own uncompressed IPF file with a completion marker for other instruments. For updates over a connection, see the separate [live IPF example](https://github.com/ttldtor/qd-cxx/blob/master/examples/ipf_live/README.md).
