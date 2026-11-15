# Offline Kraken gateway qualification

[Русская версия](README.ru.md). English is authoritative.

The stand runs four processes over IPv4 loopback. A synthetic WSS peer supplies the ticker,
the gateway publishes TICKER Quote, and two independent applications subscribe over QTP.
One receiver uses the installed C++ Feed SDK. The other uses Java QD from the revision
in [upstream.lock](../../../upstream.lock). The WSS peer uses
[synthetic credentials](../../fixtures/transport/tls/README.md). The stand needs no external connection.

The run verifies a first snapshot at the native receiver, its replay to the newly joined
Java receiver and an update at both receivers, followed by a forced
source disconnect, resubscription with a new request identifier and a fresh snapshot.
Before that snapshot, a distinctive update must not appear in the gateway trace or either
receiver. The native receiver continues after the Java receiver closes. Bid, ask and both
sizes use exactly representable test decimals. The native wire values must also match the
gateway's admitted raw values. Both receivers check zero time, sequence, nanosecond remainder
and exchange defaults. The gateway must drain pending publications, retire both peers and
finish with a successful process exit. No claim is made about upstream timestamps surviving
as Quote timestamps or about live Kraken.

Build the gateway using the [Kraken tool instructions](../../../tools/kraken/README.md).
Set `FEED_SDK` and `BEAST_SDK` to the installed package prefixes,
`OPENSSL_SDK` to the qualified static OpenSSL prefix, and `QD_CLASSPATH` to the
built pinned Java QD modules and their runtime dependencies. Use a JDK 21 or later.
Set `BUILD`, `CLASSES` and `LOGS` to dedicated working locations. Set `GATEWAY`,
`PEER` and `CLIENT` to the built executables. Run the commands from the repository root.

## Build the test programs

On Windows, use PowerShell initialized with the x64 MSVC 19.42 developer environment
and Ninja.

```powershell
cmake -S tests/conformance/kraken -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
  "-Dqdcxx_DIR=$FEED_SDK/lib/cmake/qdcxx" `
  "-Dqdcxx_beast_wss_sdk_DIR=$BEAST_SDK/lib/cmake/qdcxx_beast_wss_sdk" `
  "-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=$OPENSSL_SDK"
cmake --build "$BUILD"
```

On Linux, use GCC 13 and Ninja with ASan and UBSan.

```sh
cmake -S tests/conformance/kraken -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++-13 \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -no-pie' \
  "-Dqdcxx_DIR=$FEED_SDK/lib/cmake/qdcxx" \
  "-Dqdcxx_beast_wss_sdk_DIR=$BEAST_SDK/lib/cmake/qdcxx_beast_wss_sdk" \
  "-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=$OPENSSL_SDK"
cmake --build "$BUILD"
```

## Run the stand

On Linux, run with the same sanitizer settings as the gateway qualification.

```sh
javac -proc:none --release 21 \
  -cp "$QD_CLASSPATH" -d "$CLASSES" tests/conformance/distribution/ProcessSupervisor.java \
  tests/conformance/kraken/KrakenJavaQuoteClient.java tests/conformance/kraken/KrakenOfflineHarness.java
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
java -cp "$CLASSES" KrakenOfflineHarness "$GATEWAY" "$PEER" "$CLIENT" java \
  "$CLASSES:$QD_CLASSPATH" tests/fixtures/transport/tls "$LOGS"
```

On Windows, use `;` as the classpath separator.

```powershell
javac -proc:none --release 21 `
  -cp "$QD_CLASSPATH" -d "$CLASSES" tests/conformance/distribution/ProcessSupervisor.java `
  tests/conformance/kraken/KrakenJavaQuoteClient.java tests/conformance/kraken/KrakenOfflineHarness.java
java -cp "$CLASSES" KrakenOfflineHarness "$GATEWAY" "$PEER" "$CLIENT" java `
  "$CLASSES;$QD_CLASSPATH" tests/fixtures/transport/tls "$LOGS"
```

For a source build, the tested classpath
is `target/classes` from `dxlib`, `mars`, `dxfeed-promise`, `dxfeed-api`,
`dxlib-qd-sandbox`, `auth`, `proto`, `qd-core`, `proto-ssl`, `qd-rmi`,
`dxfeed-impl`, `dxfeed-scheme`, `dxfeed-ipf-filter` and `qds-file`. Leave out
`qd-tools`; its optional time synchronization plugin opens a multicast socket.
The harness runs the gateway for 12 seconds and bounds each
observation to 15 seconds. It initializes Java before starting the gateway so JVM cold
start does not consume the gateway interval. On WSL use a classpath on the Linux filesystem.
The harness owns the cleanup of its four children. It requires a fresh
`LOGS` directory, keeps each child's standard output and error there, and prints `PASS`
on success. An assertion, timeout, or unsuccessful child exit makes the harness fail.

For a separate finite receiver, build only the Feed client with
`-DORG_TTLDTOR_QDCXX_KRAKEN_TEST_PEER=OFF`. Set `ADDRESS` to the gateway's printed
loopback address and port. Two invocations create two independent QTP connections:

```sh
"$CLIENT" "$ADDRESS" 5000
```

The client prints `QUOTE` rows with symbol, four raw wide decimals, four decimal projections
and eight metadata values. It exits successfully after the interval only when it has received
a Quote and closed cleanly. Without an interval it waits for the harness's `STOP` command.
