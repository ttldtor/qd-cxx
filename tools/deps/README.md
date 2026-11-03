# Explicit runtime dependency provisioning

[Русская версия](README.ru.md). English is authoritative.

`ProvisionRuntime.cmake` prepares the existing qualified standalone Asio 1.38.2 and
c-ares 1.34.9+qdcxx2 source profiles in a caller-selected directory. It is a separate
step: ordinary qd-cxx configuration never calls it or downloads SDKs. It installs
nothing globally and does not prepare TLS, JSON, compilers or build tools.

## Prepare sources

Use CMake 3.25+ and an explicitly selected GNU patch executable. On Windows, GNU
patch is available in Git for Windows; on Linux supply your existing `/usr/bin/patch`.
Run from the repository root, adjusting the tool path:

```powershell
cmake '-DDESTINATION=.work/runtime-sdk' '-DARCHIVE_CACHE=.work/runtime-archives' `
  '-DPATCH_EXECUTABLE=C:/Program Files/Git/usr/bin/patch.exe' -P tools/deps/ProvisionRuntime.cmake
```

On Linux use the same arguments with `-DPATCH_EXECUTABLE=/usr/bin/patch`.
Only the exact [official Asio archive](https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz)
and [official c-ares archive](https://github.com/c-ares/c-ares/releases/download/v1.34.9/c-ares-1.34.9.tar.gz)
are downloaded. The existing [qualified receipts](../../cmake/runtime-profile/README.md)
supply archive/patch SHA256 and every qualified source hash. Asio extraction selects
only its 636 include files and original BSL license; the distribution's two convenience
symlinks are not extracted. The complete 532-file c-ares tree retains its MIT license.
The cumulative qdcxx2 patch applies to fresh stock c-ares with
`--batch --forward --fuzz=0 --binary`; offsets/fuzz and any final source mismatch fail.
No SDK script, configure program or source executable runs during provisioning.

Results under `DESTINATION`:

- `asio-asio-1-38-2/include` and adjacent `LICENSE_1_0.txt`;
- `c-ares-1.34.9-qdcxx2`, including `LICENSE.md`;
- `qdcxx-runtime-deps.json`, a relative-path completion receipt.

Supply these explicit paths to the ordinary library build:

```powershell
cmake -S . -B build/feed -G Ninja '-DCMAKE_BUILD_TYPE=Release' `
  '-DCMAKE_TRY_COMPILE_CONFIGURATION=Release' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' `
  '-DORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON' '-DORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON' `
  '-DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON' '-DORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON' `
  '-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=.work/runtime-sdk/asio-asio-1-38-2/include' `
  '-DORG_TTLDTOR_QDCXX_CARES_SOURCE=.work/runtime-sdk/c-ares-1.34.9-qdcxx2'
cmake --build build/feed --target qdcxx_feed --config Release --parallel 2
```

Use absolute SDK paths when configuring from another directory. On Linux use the
qualified GCC profile and omit MSVC settings; see [build instructions](../../docs/CPP-BUILD.md#public-feed-api-build).
Provisioning does not extend supported architectures or the existing compiler/ABI contract.

## Offline and cache behavior

`-DOFFLINE=ON` disables downloads. Prepopulate `ARCHIVE_CACHE` with the original
`asio-1.38.2.tar.gz` and `c-ares-1.34.9.tar.gz`; the helper verifies their SHA256 before
extraction. A complete existing destination is reverified against all 637/532 entries
without needing archives or GNU patch. Repeated calls never patch an already
qualified tree and never repair or overwrite corrupt inputs.

| Parameter | Default and contract |
| --- | --- |
| `HELP=ON` | Print help before required inputs, filesystem inspection or work. `LANGUAGE=en` or `ru` selects the language. |
| `ENABLED` | `ON` for this explicit helper; `OFF` returns without dependency work or input inspection. This is independent of root build options. |
| `DESTINATION` | Required; new source bundle, or an existing completed bundle to verify. Relative paths resolve from the invocation directory. |
| `ARCHIVE_CACHE` | Sibling `archives` directory; must not overlap destination or staging. Existing valid archives are reused. |
| `OFFLINE` | `OFF`; `ON` requires cached archives or a completed destination. |
| `PATCH_EXECUTABLE` | Explicit absolute GNU patch path for fresh provisioning. Not needed for completed-source readback. |
| `DOWNLOAD_TIMEOUT` | `120` seconds, integer 1..3600; both whole-download and inactivity timeout. No automatic retry. |

Exit 0 means success, help or disabled mode. Exit 1 means a CMake/input, cache,
download, patch, path or verification error; CMake launcher failures retain their
native status. Diagnostics distinguish `QDCXX_DEPS_CACHE_MISSING` (offline archive
absent), `QDCXX_DEPS_ARCHIVE_CORRUPT` (wrong bytes, never overwritten),
`QDCXX_DEPS_PARTIAL` (unfinished download/staging/destination),
`QDCXX_DEPS_PATCH_*`, `QDCXX_DEPS_PATH`, `QDCXX_DEPS_BUSY`, and source-manifest
missing/hash/file-set errors. A completion receipt alone is never trusted.

The helper creates parent/cache directories, lock files, downloads and a fresh
sibling `DESTINATION.partial`. A nonblocking destination/cache lock excludes
cooperating helper processes. Symlinks/junctions and redirected paths are rejected.
Archive paths are checked before extraction; only exact pinned archive bytes are
accepted. Both SDKs are source-verified before one directory rename publishes the
bundle. Directory publication uses an ordinary atomic rename after rechecking absence and path guards under the helper lock, including on DrvFS. It never deletes/replaces an existing destination under the cooperative locked/trusted-directory model. Protection against concurrent noncooperating path replacement is outside that model. Regular downloaded archives retain atomic `NO_REPLACE` publication. Any failed staging/download remains for inspection; a later call rejects
it. Inspect/preserve those owned artifacts, then choose fresh paths or manually
remove only the known failed artifacts. The helper never deletes directories.
Use trusted local directories without concurrent path replacement: advisory locks
do not protect against an unrelated process replacing filesystem paths.

## Check the helper

```text
cmake -DHELP=ON -P tools/deps/ProvisionRuntime.cmake
cmake -DHELP=ON -DLANGUAGE=ru -P tools/deps/ProvisionRuntime.cmake
cmake -DTEST_ROOT=.work/deps-cli-tests -P tools/deps/Tests.cmake
```

The standalone tests require a fresh output directory. They cover help/OFF with
absent inputs and no artifacts, competing-writer locks, empty/nonempty destinations and partials, offline missing/corrupt/partial caches, retained
partial staging, overlap, source corruption and symlink rejection when the host
permits creating a link (otherwise explicitly `NOT_CHECKED`). Add
`-DARCHIVE_CACHE=/existing/archives -DPATCH_EXECUTABLE=/gnu/patch` to test actual
archive extraction, cumulative patch and complete idempotent source readback too.
Tests retain their artifacts and per-case command/output receipts; nothing is
removed or downloaded. [Runtime qualification](../../cmake/runtime-profile/README.md)
remains the authority for SDK behavior; this helper provides the reproducible inputs.
