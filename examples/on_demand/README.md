# Local OnDemand replay

[Русская версия](README.ru.md).

This example replays two synthetic MOCK Quotes through the installed qdcxx::feed API. It creates an IPv4 loopback fixture peer using the same finite binary response and peer implementation as the OnDemand conformance tests. The endpoint starts paused at 2024-02-12 14:30:05 UTC and resumes at 2× after the source reports coverage. A playback listener requests the rate change and awaitState waits for finite completion without an application polling loop.

Build with a matching installed Feed SDK. The installed source directory contains this example, its fixture and its test peer.

~~~powershell
$prefix = 'C:/path/to/qdcxx-install'
$source = "$prefix/share/qdcxx/examples/on_demand"
cmake -S $source -B build/on-demand -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$prefix"
cmake --build build/on-demand
ctest --test-dir build/on-demand -R '^on_demand_example$' --output-on-failure
build/on-demand/qdcxx-on-demand --run
~~~

On Windows use a qualified compiler terminal and qdcxx-on-demand.exe. On Linux use the same compiler, runtime and build configuration as the installed SDK. A TLS-enabled SDK can require its qualified OpenSSL root at configure time.

The program prints MOCK bid=12.25 ask=12.5, followed by MOCK bid=13.25 ask=13.5, with the corresponding event times. No arguments or --help print English help without SDK or socket work. --help-ru prints Russian help. Exit code 0 requires two Quotes and a drained endpoint, 1 indicates a source or output failure, and 2 indicates invalid arguments. The 10-second wait and fixture lifetime bound the run. Fixture and SDK workers and their loopback sockets retire before process exit.

This is the finite synthetic IPv4 loopback profile. Provider service access, authentication, encryption and production replay windows have separate qualification. [Feed API guide](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.md) · [Synthetic fixture details](https://github.com/ttldtor/qd-cxx/blob/master/tools/upstream/ondemand-oracle/README.md)
