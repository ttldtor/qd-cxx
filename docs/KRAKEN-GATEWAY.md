# Run the Kraken Quote gateway

[Русская версия](KRAKEN-GATEWAY.ru.md).

The optional gateway reads the public Kraken Spot WebSocket v2 BTC/USD best bid and offer. It publishes the exact prices and sizes as QTP TICKER Quote records on IPv4 loopback. The public source requires no account or API key. [Kraken ticker API](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/ticker)

## Build and install

Use the platform settings from the [qualified Beast profile](../tools/deps/BEAST-WSS.md) and the [Feed build guide](CPP-BUILD.md#public-feed-api-build). Prepare the pinned Boost headers and qualified static OpenSSL SDK. Supply them as absolute paths. Enable the Feed runtime with standalone Asio and c-ares. Keep the QTP TLS option off for this process.

Add the following options to the Feed configuration.

```text
-DORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON
-DORG_TTLDTOR_QDCXX_BUILD_KRAKEN_SOURCE=ON
-DORG_TTLDTOR_QDCXX_BUILD_KRAKEN_GATEWAY=ON
-DORG_TTLDTOR_QDCXX_BUILD_TLS=OFF
-DORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE=<pinned Boost headers>
-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=<qualified OpenSSL SDK>
```

Build and install the configured tree.

```sh
cmake --build build/kraken
cmake --install build/kraken --prefix stage/kraken
```

The executable is installed in the configured runtime directory, typically `stage/kraken/bin`. The ordinary Feed package remains available to separate client applications through `find_package(qdcxx CONFIG REQUIRED)` and the `qdcxx::feed` target. See the [standalone Quote example](../examples/feed_quote/README.md).

## Receive live quotes

Build the Quote client from your source checkout using the installed package. Use the same toolchain and build profile as the producer.

~~~sh
cmake -S examples/feed_quote -B build/kraken-client -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/absolute/path/to/stage/kraken
cmake --build build/kraken-client
~~~

On Linux use the matching GCC 13 sanitizer configuration.

~~~sh
cmake -S examples/feed_quote -B build/kraken-client -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-13 \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -no-pie' \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/stage/kraken
cmake --build build/kraken-client
~~~

Save an explicitly selected trust anchor for the current Kraken certificate chain in PEM format. Use anchors approved for your deployment. Run the installed gateway from your checkout.

```sh
stage/kraken/bin/qdcxx-kraken-gateway --trust-pem /path/to/kraken-ca.pem --listen-port 7400 --duration-ms 30000
```

After `LISTEN 127.0.0.1 7400`, connect one or more Feed clients from separate terminals.

```sh
build/kraken-client/qdcxx-feed-demo --run --address 127.0.0.1:7400 --symbol BTC/USD --duration-ms 10000
```

Use `--listen-port 0` to request an ephemeral port. Pass the actual port from the `LISTEN` line to clients. Endpoints and trust are explicit. `--help` displays options without opening sockets. The source waits for BTC/USD demand before its first subscription. The gateway needs an accepted subscription and a fresh snapshot before publishing from each connection.

Each Quote carries the exact wide-decimal prices and sizes. Quote timestamps, sequence and exchange codes are zero. The optional `--trace-quotes` output includes the provider timestamp and admitted raw Quote values. TICKER can combine intermediate updates. The stats line reports local publication and transport metrics. To verify client delivery, inspect the quotes received at each client.

Use `--source-host`, `--source-identity` and `--source-port` to select a compatible local WSS replay peer and pass its trust PEM. The HTTP path is `/v2`. The gateway supports this one public source profile and plaintext QTP on IPv4 loopback.

See the [BBO and continuity contract](../spec/transport/KRAKEN-SUBSCRIPTION.md#bbo-values-and-continuity) for mapping, freshness and gap limits.
