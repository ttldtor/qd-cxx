# Subscribe to live candles

[Русская версия](README.ru.md).

The [example](main.cpp) sets the HISTORY endpoint contract and subscribes to a canonical Candle symbol with an inclusive starting time. The same listener prints the initial candles and later updates. It prints time, open, high, low and close. The endpoint drains at the ten-second cutoff.

Build against the [installed Feed SDK](https://github.com/ttldtor/qd-cxx/blob/master/docs/CPP-BUILD.md#public-feed-api-build) with the same compiler and build profile. For the qualified MSVC Release profile, use a developer terminal and Ninja.

~~~powershell
cmake -S examples/feed_candle -B build/feed-candle -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx"
cmake --build build/feed-candle
build/feed-candle/qdcxx-feed-candle.exe 127.0.0.1:7400 'SPY{=m}' 1794652800000
~~~

Use a QTP source that offers HISTORY Candle for the chosen symbol and range. The example accepts plain TCP. Its source address and lower bound are command-line inputs. The [daily range query](https://github.com/ttldtor/qd-cxx/blob/master/examples/client_queries/README.md) uses a separate one-shot request.

Run the build commands from the checkout root. From the installed sample directory use `cmake -S .` with the same options. If copying only the source example directory, copy `examples/feed_common/session.hpp` into it as well.

The shared sample helper handles the receive window and waits for endpoint closure. `ApiRuntime` joins its worker on scope exit.

This sample accepts 0 to 2147483647999 Unix milliseconds.
