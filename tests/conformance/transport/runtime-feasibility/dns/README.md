# Qualified DNS feasibility test

[Русская версия](README.ru.md).

This optional internal test exercises a caller-driven c-ares DNS profile. It is a feasibility fixture, not a production transport adapter or an unmodified upstream c-ares distribution. `dependencies.lock.json` pins the original archive and qualified patch; `profile.json` records the restricted profile.

The build defaults to OFF. In that mode CMake enables no compiler language, finds no SDK and acquires no dependency. To run the test, verify the upstream archive SHA-256, extract it separately and apply `qdcxx1.patch` with `patch --batch --forward --fuzz=0 -p1` from the extracted source directory. Keep the upstream MIT notices; the exact license is included here. No system installation is required.

```sh
cmake -S . -B build -G Ninja -DORG_TTLDTOR_QDCXX_BUILD_DNS_FEASIBILITY=ON -DCARES_SOURCE_DIR=/absolute/path/to/patched-c-ares -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build -V --output-on-failure
```

For Linux GCC Debug validation, use `-DCMAKE_BUILD_TYPE=Debug -DORG_TTLDTOR_QDCXX_DNS_SANITIZERS=ON`. This instruments both the C SDK and C++ runner. Run CTest with child `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`; leak detection is outside this profile. Windows requires an x64 MSVC developer environment and permission for the owned loopback UDP/TCP sockets. The test contacts no external DNS service and changes no DNS/firewall setting.

The executable is an internal CTest runner: invoke it without arguments to perform eight configuration-rejection checks, 22 pending-query cancellation/destruction checks, and two UDP/TCP quota/resume cases. It writes status/timing records, exits 0 on success, 1 on a failed check or 2 on bootstrap failure. CTest applies a 20-second external deadline and cleans up that test process if necessary. The runner owns and closes only its own native sockets.

The qualified SDK rejects missing or excessive explicit IPv4 nameservers, skips OS/environment DNS configuration and hostname search fallback, and rejects reinitialization. Header and runtime version checks reject the wrong variant. Bootstrap must initialize c-ares before other application threads, with matching process-wide lifetime coordination and default allocators. One channel belongs to one independent upstream run; one wire query is allowed at a time, with A and AAAA queried serially. No event thread or monitor exists. Literal IP addresses bypass DNS in the intended adapter.

The socket callback bridge shares packet/byte/native-read quotas across the entire SDK progress call. Exhaustion returns would-block before consuming a UDP datagram; TCP fragments resume on a later turn. The test demonstrates cancellation of a genuinely pending `hold.invalid.` query, nonmatching UDP replies followed by NXDOMAIN, and a fragmented TCP NXDOMAIN response. The seven-byte TCP read chunk is test stress, not a production default. Input-buffer envelopes are source-derived; total SDK heap, parser/result metadata and allocator overhead still require an explicit production resource budget. A finite passing test is not a universal real-time guarantee or full NSS/split-DNS compatibility evidence.
