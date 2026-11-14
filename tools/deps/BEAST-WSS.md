# Prepare the private Beast WSS dependency

[Русская версия](BEAST-WSS.ru.md). English is authoritative.

## Provision from the local archive

Use CMake 3.25 or newer. Place the exact Boost 1.92.0 archive in a local cache and provide its path. Provisioning extracts only the pinned [source inventory](../../cmake/beast-wss-profile/boost-1.92.0-sources.sha256.tsv) and checks every hash. It uses trusted local directories and coordinated writes. [Official Boost archive](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz)

```powershell
cmake '-DARCHIVE=.work/archives/boost_1_92_0.tar.gz' '-DDESTINATION=.work/wss-headers' `
  -P tools/deps/ProvisionBeastWss.cmake
```

| Parameter | Behavior |
| --- | --- |
| `HELP=ON` | Prints help before checking provision inputs or writing files. `LANGUAGE=en` by default, or `ru`. |
| `ENABLED=OFF` | Skips provisioning. The default is ON. |
| `ARCHIVE` | Required for a new destination. Local archive path relative to the current directory or absolute. |
| `DESTINATION` | Required output directory. A completed tree is verified again without the archive. |

The helper creates a sibling lock and staging directory, then publishes the verified tree by rename. It rejects redirected paths and keeps an incomplete staging directory or corrupt input for inspection. Provisioning uses no network, compiler or global settings. Exit 0 reports successful verification, help or OFF. An input, file or verification failure exits nonzero.

## Build and install

Use a Windows MSVC 19.42 x64 Release developer shell with the dynamic CRT, or Linux GCC 13.3.0 x86-64 Debug with ASan/UBSan. The OpenSSL installation must match the project profile. [Qualified OpenSSL SDK](../../docs/CPP-BUILD.md#tls-source-profile-cpp-build-en)

```powershell
cmake -S cmake/beast-wss-profile -B .work/wss-build -G Ninja `
  -DORG_TTLDTOR_QDCXX_BEAST_WSS_DEPENDENCY=ON `
  '-DORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE=.work/wss-headers' `
  '-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/wss-build
ctest --test-dir .work/wss-build --output-on-failure
cmake --install .work/wss-build --prefix .work/wss-sdk
```

On Linux supply the Linux OpenSSL root and use `-DCMAKE_BUILD_TYPE=Debug`, `-DCMAKE_CXX_COMPILER=g++-13` and `'-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'` instead of the Windows build flags. Keep the two platforms in separate build and install directories. The standalone profile defaults to OFF, which returns before enabling C++ or inspecting dependencies. Ordinary qd-cxx configuration does not invoke it.

The installation contains the verified headers and license under `source/` and the relative package configuration under `lib/cmake/qdcxx_beast_wss_sdk/`. OpenSSL remains an explicit input to the consumer. Link `qdcxx_private::beast_wss` privately from an adapter target.

## Verify the installed package

Move or copy the install directory to a fresh location. Configure the [consumer probe](../../tests/conformance/beast-wss-dependency/consumer/CMakeLists.txt) in a fresh build directory with `-DSDK_ROOT` set to the absolute path of the copy, the same compiler and flags, and an explicit OpenSSL root. Build it to verify import, compilation and linking.

```powershell
cmake -S tests/conformance/beast-wss-dependency/consumer -B .work/wss-consumer -G Ninja `
  '-DSDK_ROOT=C:/absolute/project/.work/wss-sdk-relocated' `
  '-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/wss-consumer
```

The producer and consumer probes do not connect to a network peer. This verification covers the recorded [dependency profile](../../cmake/beast-wss-profile/README.md).
