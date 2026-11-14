# Run the distribution example

[Русская версия](README.ru.md).

The example runs as a publishing source, relay or subscribing application. All three roles use plain QTP over IPv4 loopback. The source publishes synthetic IBM events. The relay forwards combined client demand upstream, then forwards Quote, TimeAndSale and Order to the clients. A client creates one mixed subscription on one `qdcxx::feed` endpoint.

With an installed Feed SDK, build the example independently of the SDK source tree:

```sh
cmake -S /path/to/qd-cxx-install/share/qdcxx/examples/feed_distribution -B build/feed-distribution -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qd-cxx-install
cmake --build build/feed-distribution
ctest --test-dir build/feed-distribution --output-on-failure
```

The executable is `qdcxx-distribution` on Linux and `qdcxx-distribution.exe` on Windows. The root build also provides `qdcxx_distribution` when Feed and examples are enabled.

1. In the first terminal, run `qdcxx-distribution source`. Copy the port from `PORT`.
2. In the second terminal, run `qdcxx-distribution server 0 SOURCE_PORT`. Copy its `PORT`. `SOURCE` reports the negotiated upstream generation.
3. In a third terminal, run `qdcxx-distribution client SERVER_PORT`. Start a second client in another terminal if desired.
4. Wait for `ADD Quote`, `ADD TimeAndSale` and `ADD Order` in the source terminal. Enter `QUOTE`, then `EVENTS` and `REMOVE`. The clients print the bid price, each sale occurrence, and the order index and event flags.
5. Enter `CLEAR` on a client to withdraw its subscriptions. The source reports REMOVE when the last client withdraws a symbol. Enter `STOP` or send EOF in each terminal to close that role.

`SENT` means local publication admission. Event lines at the clients confirm actual callbacks. The sample shows DEFAULT-source Order. Feed can also derive Order events from Quote. Enter `STATS` in any role for a local observation of connections, demand and pending work. `sentFrames` counts complete DATA frames accepted by the local socket. The server also reports upstream connection state and its last diagnostic stage. Use `--help` or `--help-ru` for the full command and exit-code list. See the [publisher profile](https://github.com/ttldtor/qd-cxx/blob/master/spec/subscriptions/PUBLISHER.md) for supported layouts and bounds.
