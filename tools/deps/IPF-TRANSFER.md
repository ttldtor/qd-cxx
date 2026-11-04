# Explicit IPF transfer dependency preparation

[Русская версия](IPF-TRANSFER.ru.md). English is authoritative.

These commands prepare only the private [Boost 1.92.0 dependency profile](../../cmake/ipf-transfer-profile/README.md).
They add no production HTTP/file facade. No global installation, package manager, ordinary-configure
fetch, SDK bootstrap script or compiler execution by a `cmake -P` provisioner is involved.
Use trusted local directories without concurrent noncooperating path replacement.

## Source acquisition

From the repository root with CMake 3.25+, place the exact official archive in the selected cache first, then use the explicit verified-archive/offline path:

```powershell
cmake '-DDESTINATION=.work/ipf-boost-source' '-DARCHIVE_CACHE=.work/ipf-archives' `
  -DOFFLINE=ON -P tools/deps/ProvisionIpfTransfer.cmake
```

Only the exact [official archive](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz)
with [published metadata](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz.json)
is accepted. Its SHA256 and all selected source/license hashes are pinned in the profile manifest.
`-DOFFLINE=ON` requires that archive already in `ARCHIVE_CACHE`, or an existing completed destination.
To request optional HTTPS acquisition, omit `-DOFFLINE=ON`. Download and inactivity timeouts are finite; network errors or a slow connection can fail the attempt while retaining the partial download. For a slow network, obtain the exact archive separately and use the offline path.
The output is the selected source directory and adjacent `DESTINATION.receipt.json`; a completed
source tree is always rehashed before reuse. This helper does not compile Boost.

| Parameter | Default and contract |
| --- | --- |
| `HELP=ON` | Help before required inputs, path inspection, network or writes. `LANGUAGE=en` (default) or `ru`. |
| `ENABLED` | `ON`; `OFF` returns without inputs, discovery, network or writes. |
| `DESTINATION` | Required fresh source directory, or completed directory to reverify; relative to invocation directory. |
| `ARCHIVE_CACHE` | Sibling `archives`; must not overlap destination/staging. |
| `OFFLINE` | `OFF`; `ON` forbids download. |
| `DOWNLOAD_TIMEOUT` | `120` seconds, integer 1..3600, total and inactivity timeout; no automatic retry. |

Exit 0: success/help/OFF. Exit 1: CMake/input/path/cache/download/source error. Diagnostics use
`QDCXX_IPF_DEPS_INPUT`, `PATH`, `BUSY`, `PARTIAL`, `CACHE_MISSING`, `ARCHIVE_CORRUPT`, `DOWNLOAD`,
`SOURCE_CORRUPT`, `PUBLISH` and `QDCXX_IPF_SOURCE`. Corrupt archives/sources are never repaired or
overwritten. Stale `.part`, `.partial` and incomplete destinations are retained and rejected.
Inspect a failed attempt and choose fresh paths. Symlinks/junctions and redirected paths are rejected.
The helper creates parent/cache directories, locks, downloaded archive and fresh staging. Publication
uses ordinary atomic rename under cooperative destination/cache locks on trusted Windows/Linux local
filesystems, including DrvFS. It does not protect against adversarial concurrent path replacement.
A crash between source rename and adjacent receipt publication leaves an incomplete destination,
which a retry rejects rather than repairs. Successful empty owned staging is removed after publication.

## Explicit static build, test and install

Use a developer shell where the selected compiler is already available. On Windows select
MSVC 19.42 x64 and release dynamic CRT; no script guesses or modifies the shell toolchain:

```powershell
cmake -S cmake/ipf-transfer-profile -B .work/ipf-sdk-build -G Ninja `
  -DORG_TTLDTOR_QDCXX_IPF_TRANSFER_DEPENDENCY=ON `
  '-DORG_TTLDTOR_QDCXX_IPF_TRANSFER_SOURCE=C:/absolute/project/.work/ipf-boost-source' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
  '-DCMAKE_INSTALL_PREFIX=C:/absolute/project/.work/ipf-sdk'
cmake --build .work/ipf-sdk-build --parallel 1
ctest --test-dir .work/ipf-sdk-build --output-on-failure
cmake --install .work/ipf-sdk-build
cmake '-DSDK_ROOT=.work/ipf-sdk' -P tools/deps/SealIpfTransferSdk.cmake
```

Linux uses GCC 13.3.0 x86-64 with the same source/install arguments, `-DCMAKE_BUILD_TYPE=Debug` and
`'-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'`; omit the MSVC options.
Keep each platform in a separate fresh build/install directory. The standalone option defaults OFF:
OFF enables no compiler and performs no SDK discovery, download or link.

The installed package contains `source/` with all 417 files/license, `lib/` with the static URL
archive and relative package files, and `share/qdcxx-ipf-transfer/` with pinned manifest/build metadata.
`SealIpfTransferSdk.cmake` requires explicit `SDK_ROOT` and no working-path default. It verifies pinned
sources and metadata and writes only `share/qdcxx-ipf-transfer/seal.json` plus a cooperative lock.
Existing seals are reverified and never overwritten. EN/RU `HELP=ON` needs no inputs and creates no
files. Exit 0 means success/help; exit 1 means invalid source/profile/artifact/path/lock.
The seal hashes every source/archive/package file used by consumers; the receipt and lock are excluded
from their own inventory. It binds the actual local static archive and flags to source identity.
It neither executes tests nor authenticates an untrusted package; run CTest before sealing.
Different build paths may yield different archive bytes. A matching source profile is reproducible
preparation, not one universal artifact hash or binary/ODR isolation guarantee.

## Installed and relocated consumer

Move or copy the complete sealed SDK, then configure the standalone consumer against that location:

```powershell
cmake -S tests/conformance/ipf-transfer-dependency/consumer -B .work/ipf-consumer -G Ninja `
  '-DCMAKE_PREFIX_PATH=C:/relocated/ipf-sdk' -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/ipf-consumer --parallel 1
ctest --test-dir .work/ipf-consumer --output-on-failure
```

Use the same GCC Debug/sanitizer flags on Linux. `find_package(qdcxx_ipf_transfer_sdk CONFIG REQUIRED)`
imports `qdcxx_private::url` only after host/source/artifact/inventory verification. The package has
no original source/build path dependency. Missing SDK, wrong host/CRT, modified sources/archive/package
files or extra SDK files fail configuration. This is an internal dependency consumer; public IPF
interfaces remain unchanged and must not expose Boost types.

## Helper checks

```powershell
cmake -DHELP=ON -P tools/deps/ProvisionIpfTransfer.cmake
cmake -DHELP=ON -DLANGUAGE=ru -P tools/deps/SealIpfTransferSdk.cmake
cmake '-DTEST_ROOT=.work/ipf-deps-tests' '-DARCHIVE_CACHE=.work/ipf-archives' `
  -P tools/deps/TestsIpfTransfer.cmake
```

`TestsIpfTransfer.cmake` requires a fresh `TEST_ROOT`, has EN/RU `HELP=ON`, runs no compiler or network,
and preserves per-case logs/fixtures. With no optional cache it runs help/OFF and negative cases;
with cache it also provisions/rechecks offline and detects source corruption. Exit 0 means executed
cases passed; exit 1 means invalid inputs or unexpected behavior. A denied symlink-creation case is
reported `NOT_CHECKED`, never as a pass. Full transfer/authentication/TLS/cancellation qualification
belongs to the production facade and is not established by these dependency checks.
