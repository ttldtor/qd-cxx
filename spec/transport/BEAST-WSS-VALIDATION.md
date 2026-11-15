# Composed Beast WSS validation

[Русская версия](BEAST-WSS-VALIDATION.ru.md). English is authoritative.

This qualification covers the [private WSS source session](BEAST-WSS-SESSION.md) built with Boost
1.92.0 and the project-qualified static OpenSSL 3.5.9 SDK. The session and the synthetic peers use
loopback. The package check uses a separate dependency consumer.

## Test profiles

| Check | Windows x64 | Linux x86-64 |
| --- | --- | --- |
| Private WSS build and CTest | 5/5 groups, MSVC 19.42 Release with dynamic CRT | 5/5 groups, GCC 13.3 Debug with ASan/UBSan |
| Relocated SDK consumer | Built, linked and ran | Built, linked and ran with ASan/UBSan |
| TCP, controlled TLS and recovery CTest | 23/23 groups, the same Windows profile with standalone Asio | 23/23 groups, the same Linux profile with standalone Asio |

The five WSS test groups cover dependency provisioning, verified TLS, the TLS allocation
observation, connection upgrade and bounded message I/O. An independent loopback peer verifies
the message boundary, fragmented UTF-8, pong, both directions of close, interrupted transport,
invalid frames, and cancellation and deadlines through read, write and close.

The composed fixture opens eight fresh attempts in a single warmed process. Each new attempt
starts after the previous owner and its peer are destroyed. It alternates canceling a pending
receive with canceling a write blocked by a peer that withholds reads. It checks fresh text on
the new sessions and checks that a retained value from the first session remains readable
after the last teardown. Across those eight attempts the observed Windows process handle count
was 144 before and after, and the Linux open descriptor count was 7 before and after, including
the enumeration descriptor. Missing trust is refused before network progress.
With explicit trust and a bound port without a listener, the attempt reports connection
refusal without reaching `READY`.

The separate QTP configuration covers TCP, controlled TLS, raw bytes, socket and resolver
ownership, recovery and its native peer, and subscription shutdown.
The QTP target uses standalone Asio. The installed WSS dependency target uses Boost.Asio.

The standalone WSS configuration skips compiler and SDK discovery with its option OFF, including
when given missing input paths. With the option ON it requires an explicit provisioned Boost path.
The relocated SDK exposes the private `qdcxx_private::beast_wss` dependency target and verifies
the supplied OpenSSL SDK. The installed consumer constructs the three transport layers
and exits successfully without opening a connection.

## Reproduce the checks

Use the [standalone provisioning and build guide](../../tools/deps/BEAST-WSS.md) to configure
`build-wss`. Select the qualified MSVC Release shell with `/MD` on Windows. For GCC 13.3 Debug on
Linux, use these arguments and test settings for both the WSS build and the standalone-Asio
transport build. Select `gcc-13` for the transport build's C compiler as well.

```bash
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
# Add these arguments to each Linux CMake configure command.
# -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-13
# '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie'
# '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -no-pie'
```

```text
cmake --build build-wss --parallel 2
ctest --test-dir build-wss --no-tests=error --output-on-failure
cmake --install build-wss --prefix wss-sdk-staging
```

Move `wss-sdk-staging` to a fresh path and configure the
[standalone consumer](../../tests/conformance/beast-wss-dependency/consumer/CMakeLists.txt)
using its absolute relocated `SDK_ROOT`, the same toolchain flags and the explicit qualified
`ORG_TTLDTOR_QDCXX_OPENSSL_ROOT`. Build and run `qdcxx_beast_wss_consumer` from the consumer
build directory. Exit 0 confirms its dependency probe.

For the root regression build, follow [Building controlled TLS](../../docs/CPP-BUILD.md#tls-source-profile-cpp-build-en)
with `BUILD_TESTING=ON` and `ORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON` in addition to the client,
runtime and TLS options. Set `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF` and
`ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=OFF`. For an offline build, supply the pinned doctest source
through `FETCHCONTENT_SOURCE_DIR_DOCTEST`. Use a separate root build directory on each platform.
Run the selected CTest groups after building the root configuration.

```text
cmake --build build-transport --parallel 3
ctest --test-dir build-transport --no-tests=error --output-on-failure -R "^(transport_(bootstrap_lease|raw_bytes|tcp|cares_ownership|tls|recovery).*|subscription_shutdown)$"
```

## Scope of the evidence

The Linux C++ compilation and executable linking use ASan/UBSan with LeakSanitizer disabled. OpenSSL is the
pinned prebuilt static SDK. The handle and descriptor observations cover this finite loopback
workload, and the [TLS allocation receipt](BEAST-WSS-TLS.md) counts requested OpenSSL bytes
in its own fixture. Process RSS, shared-host TLS, allocator-failure behavior and a hard wall-clock
limit for an in-flight system DNS call are outside this qualification. The installed surface
is the private dependency package. Provider subscription recovery, live venue availability and
gateway packaging require their own checks.
