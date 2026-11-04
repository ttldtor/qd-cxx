# Bounded C++23 components

The independent [schedule-rules component](SCHEDULE-RULES.md) needs no runtime SDK. Configure with
`-DORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON` (default OFF) and link `qdcxx_schedule` or request the
installed `schedule` component. Its exact tracked calendar/ID manifest is checked at configure;
there is no acquisition step. Add `-DORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON` for
`qdcxx_schedule_example`; [example instructions](../examples/schedule_rules/README.md) include
input-free EN/RU help. The target also provides [owning Day/Session queries](SCHEDULE-QUERIES.md) and links the controlled
[private timezone backend](TIME-ZONE-DATA.md). No separate runtime SDK is needed. The always-exported
`qdcxx_time_zone_data` target supports fixed bindings with its optional backend OFF. Configure
`ORG_TTLDTOR_QDCXX_BUILD_TIME_ZONE_DATA=ON` for named bindings without Schedule; request the
installed `time_zone_data_backend` component when native availability is required. The query sample
target is `qdcxx_schedule_queries_example`; pass an explicit installed data directory.

Prepare the exact [runtime SDK sources](../cmake/runtime-profile/README.md) first. The explicit [dependency provisioning step](../tools/deps/README.md) supplies pinned Asio/c-ares with offline/cache support and no global installation; ordinary configure never downloads them.

## Portable Feed and minimal profiles {#portable-feed-profiles}

The [root presets](../CMakePresets.json) require CMake 3.25+ and Ninja. Choose a
qualified x64 MSVC 19.42 developer terminal on Windows or GCC 13.3 on Linux; the
presets select `cl` or `g++` from that terminal's `PATH`. They do not install tools.

| Configure/build preset | Result |
| --- | --- |
| `qdcxx-feed-windows` | Plain Feed and the console sample; Release, `/MD`, Release compiler probes. |
| `qdcxx-feed-linux` | The same targets; Debug with C++ ASan/UBSan and matching executable linker flags. |
| `qdcxx-minimal-windows` | SDK-free `qdcxx_core`; Release and `/MD`. |
| `qdcxx-minimal-linux` | SDK-free `qdcxx_core`; Debug with C++ ASan/UBSan. |

Only presets for the current host are listed. Feed profiles enable FEED_API,
CLIENT, RUNTIME, RECOVERY and the sample together: callers choose one profile
instead of repeating the three internal prerequisite switches. Tests, JSON, TLS,
replay and separate tools are OFF in these production profiles. Minimal profiles
also disable Feed/client/runtime/recovery/examples and neither inspect SDK roots
nor fetch dependencies. The normal raw CMake options below remain available.

Prepare the [qualified runtime sources](../tools/deps/README.md) as a separate,
explicit step. The default source bundle is `.work/runtime-sdk` from the checkout
root. Its `asio-asio-1-38-2/include` and `c-ares-1.34.9-qdcxx2` paths are set by the
Feed presets; an absent bundle gives the existing explicit SDK error, without a
download. To reuse sources elsewhere, override the two SDK variables on configure:

```powershell
cmake --preset qdcxx-feed-windows `
  '-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=C:/sdk/runtime/asio-asio-1-38-2/include' `
  '-DORG_TTLDTOR_QDCXX_CARES_SOURCE=C:/sdk/runtime/c-ares-1.34.9-qdcxx2'
```

With the default prepared bundle, run from the repository root:

```text
cmake --list-presets
cmake --preset qdcxx-feed-windows
cmake --build --preset qdcxx-feed-windows
cmake --install build/qdcxx-feed-windows
```

On Linux replace `windows` with `linux` in these preset/path names. Build presets
use two jobs by default; `--parallel N` overrides that limit. Binary directories
are `build/<preset>` and the Feed install prefix is `build/install/<preset>`.
`cmake --install` may override it with `--prefix`. Install after building the Feed
profile; the minimal build preset intentionally builds only core, rather than an
installable set of every pure-library archive.

For example, `cmake --preset qdcxx-minimal-windows` followed by
`cmake --build --preset qdcxx-minimal-windows` builds core without SDK preparation.
Each profile has its own binary directory; use a fresh directory when changing
compiler/toolchain. If configure uses a local `-B` override, build that directory with `cmake --build <directory>`; build presets continue to address their declared binary directories.
The [installed-sample presets](../examples/feed_quote/README.md#sample-build-profiles)
share these platform settings and default to the corresponding Feed install prefix;
no internal library switches or SDK source roots are needed by that consumer.

Linux profiles set `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` for configure/build children.
When launching a binary directly from your shell, supply the same environment
explicitly if needed; preset environment does not modify the parent shell.
LSan and c-ares C instrumentation are not promised by these C++ sanitizer flags.
## Independent instrument-profile values

`ORG_TTLDTOR_QDCXX_BUILD_IPF=ON` builds and installs `qdcxx_ipf` with
`qd/ipf/instrument_profile.hpp`; it defaults to OFF and has no feed/runtime/JSON prerequisites or
new dependencies. See the [instrument-profile guide](INSTRUMENT-PROFILES.md) and
[value contract](../spec/ipf/PROFILE.md).

## Public feed API {#public-feed-api-build}

`ORG_TTLDTOR_QDCXX_BUILD_FEED_API` defaults to `OFF`. It requires explicit
`ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`, `ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON` and
`ORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON`. It builds/installs `qdcxx_feed` and
`qd/api/feed.hpp`. The separate `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES` option controls
the `qdcxx_feed_demo` executable target (`qdcxx-feed-demo`); it defaults to ON for
a top-level build and OFF when qd-cxx is included with `add_subdirectory`.
Only examples whose public API is enabled are built. Turning examples OFF leaves
the feed library and its library tests available. The [standalone example](../examples/feed_quote/README.md)
can consume an installed package built without examples. TLS, JSON and the
separate connection CLI are not prerequisites.

Prepare the exact [runtime SDK sources](../cmake/runtime-profile/README.md) first.
Use a qualified x64 [recovery copy profile](../spec/transport/RECOVERY.md): selected
MSVC 19.42/standard-library ABI or GCC 13.3/libstdc++ ABI. An unsupported copy profile
is an explicit runtime refusal, not a generic C++23 compatibility promise.
For Windows/Ninja, use a compiler-configured terminal and actual SDK source paths:

```powershell
cmake -S . -B build/feed -G Ninja '-DCMAKE_BUILD_TYPE=Release' `
  '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' '-DCMAKE_INSTALL_LIBDIR=lib' `
  '-DBUILD_TESTING=OFF' '-DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF' `
  '-DORG_TTLDTOR_QDCXX_BUILD_TLS=OFF' `
  '-DORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON' '-DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON' `
  '-DORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON' '-DORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON' `
  '-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=C:/sdk/asio-1.38.2/include' `
  '-DORG_TTLDTOR_QDCXX_CARES_SOURCE=C:/sdk/c-ares-1.34.9-qdcxx2'
cmake --build build/feed --config Release --parallel 2
cmake --install build/feed --config Release --prefix C:/sdk/qd-cxx
```

On Linux use the qualified GCC profile, Linux SDK paths and installation prefix;
omit the MSVC runtime option. Existing SDK preparation/qualification requirements
still apply. Configuration does not acquire SDKs. The enabled package carries its
native static-link closure; an installed consumer needs no SDK source/include roots:

```cmake
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
add_executable(feed_app main.cpp)
target_link_libraries(feed_app PRIVATE qdcxx_feed)
```

`feed` is the supported package component. A required `feed` request fails early
with a rebuild hint when the installed package lacks it; an unsupported required
component also fails. `OPTIONAL_COMPONENTS feed` keeps the package usable when
Feed is absent and reports `qdcxx_feed_FOUND=FALSE`. Unsupported optional components
likewise report `qdcxx_<component>_FOUND=FALSE`. Each lookup refreshes the Feed flag
and package failure message, including repeated calls in the same directory.
Both local and installed Feed builds provide `qdcxx::feed` as an alias of
`qdcxx_feed`. Existing `find_package(qdcxx CONFIG REQUIRED)` calls and target names
remain supported. Other exported libraries do not have named package components.

The no-SDK-root consumer command above assumes the recommended TLS-OFF package.
A TLS-enabled installed package additionally validates its qualified OpenSSL
installation during package discovery, even for a plain feed application; supply
`-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified` with the actual
[qualified TLS SDK](#tls-source-profile-cpp-build-en) path. Asio/c-ares source roots
remain unnecessary for installed consumers.

Use compatible compiler/standard-library/CRT settings. See the [public API guide](FEED-API.md)
and [standalone installed example](../examples/feed_quote/README.md) for application
lifetime, system IPv4 DNS, typed subscriptions and EN/RU command help. This facade
uses plain TCP and TICKER/STREAM; enabling lower-level TLS does not add TLS support
to this high-level API.

<a id="installed-binary-json-loader"></a>

## Installed binary JSON loader

JSON ON installs/exports `qdcxx_schema_json` and private static dependency `qdcxx_simdjson`, the optional
schema header and exact upstream licenses. Consumers need no SDK headers/source root,
`find_package(simdjson)` or fetch step. SDK includes are build-only; relocated imported archives use
the package prefix. JSON OFF does not find/fetch/link simdjson, inspect its source or add optional
dependency cache entries. The [standalone consumer](../tests/conformance/schema_json_consumer/CMakeLists.txt)
uses these exact statements after its C++ project declaration:

```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(qdcxx_schema_json_consumer main.cpp)
target_link_libraries(qdcxx_schema_json_consumer PRIVATE qdcxx_schema_json qdcxx_collectors)
set_target_properties(qdcxx_schema_json_consumer PROPERTIES CXX_EXTENSIONS OFF)
```

C++23 is inherited from library usage requirements. Set `CMAKE_PREFIX_PATH` or `qdcxx_DIR` to the
installed/relocated package as described below. Include `qd/schema/json_schema.hpp` and select
`loadBinaryJsonSchema` with `BinaryJsonSchemaLimits` for the
[native binary profile](../spec/schema/NATIVE-BINARY-JSON.md). Ordinary `loadJsonSchema` retains v1.
New limits independently bound parser requests, logical work and logical two-copy schema bytes;
they do not cap total heap/RSS. Core/protocol/collectors remain JSON-independent. The
[qualification protocol](../tests/conformance/schema-json/VALIDATION.md#binary-json-profile-qualification)
defines the consumer lifetime checks and final-build evidence boundary.

## Explicit ordinary wire discovery

Include `qd/protocol/tape_reader.hpp`, link `qdcxx_protocol` and construct
`TapeReader(limits, TapeMode::LEGACY)` for the opt-in ordinary wire schema.
The existing `TapeReader(schema, limits, mode)` remains supplied-schema binding.
`schema()` exposes current immutable declarations, `descriptions()` exact accepted
source order, and completed records own their exact senderId. Borrowed views expire
on successful description updates; retain an explicit boundedCopy when old schema
declarations are needed. [WIRE-INVENTORY](../spec/schema/WIRE-INVENTORY.md) defines
provenance, first-seen IDs, full-union preflight, finite construction charges and
refusals. No new dependency or CMake option is required. With tape inspection
enabled, `wire_inventory_schema` and `wire_inventory_cli` add focused tests;
`--wire-schema ordinary-v1` selects self-describing output 7 independently of JSON.

[Русская версия](CPP-BUILD.ru.md). English is authoritative.

## Optional TCP runtime

`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME` defaults to `OFF`. To enable the credential-free,
caller-driven adapter, first prepare the exact [qualified SDK sources](../cmake/runtime-profile/README.md)
and use a supported x64 toolchain:

```powershell
cmake -S . -B build-runtime '-DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON' `
  '-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=C:/sdk/asio-1.38.2/include' `
  '-DORG_TTLDTOR_QDCXX_CARES_SOURCE=C:/sdk/c-ares-1.34.9-qdcxx2'
cmake --build build-runtime --config Release
```

Include `qd/transport/tcp_runtime.hpp` and link `qdcxx_transport`. The
[runtime contract](../spec/transport/TCP-RUNTIME.md) defines main-thread bootstrap,
owner-thread progress, deadlines, credits and explicit close. This is plain TCP;
TLS and a reconnecting client remain separate work. Application callbacks use the
optional local dispatcher below.
STREAM staging uses the separate collector component below.
OFF does not inspect those SDK roots or add C/SDK dependencies to the pure libraries.

`cmake --install build-runtime --prefix C:/sdk/qdcxx --config Release` installs a
relocatable `qdcxx` package. Consumers use `find_package(qdcxx CONFIG REQUIRED)` and
the target names `qdcxx_encoding`, `qdcxx_core`, `qdcxx_protocol`, `qdcxx_collectors`,
`qdcxx_event_mapping`, `qdcxx_quote`, `qdcxx_profile` and, when built,
`qdcxx_transport` and `qdcxx_client`. Installed event headers support generic-to-prepared
Quote/Profile mappings without a JSON dependency. JSON ON also exports the loader described above;
replay and tools are not additional exported library targets.
The enabled package includes the qualified static c-ares archive, native link
closure and dependency notices; consumers need neither SDK headers nor a C compiler.
Set `CMAKE_PREFIX_PATH` to the installed prefix for discovery. For a nonstandard
library directory such as `lib64` on Windows, point `qdcxx_DIR` directly to the
package directory inside the relocated prefix:

```powershell
cmake -S consumer -B build-consumer '-Dqdcxx_DIR=C:/sdk/qdcxx/lib64/cmake/qdcxx'
```

## Optional local STREAM dispatch

`ORG_TTLDTOR_QDCXX_BUILD_CLIENT` defaults to `OFF`. Enable it to build and install
`qdcxx_client`; include `qd/client/stream_dispatcher.hpp`. The
[client contract](../spec/subscriptions/CLIENT-DISPATCH.md) defines serialized owner
progress, a bounded thread-safe command port, immutable batches and local close.
The only additional dependency is standard `Threads::Threads`. Runtime and JSON
may remain disabled. `client_dispatch` is the focused CTest. The composing caller
owns its source session/runtime and explicitly routes events and wire output;
local terminal does not certify transport shutdown.

<a id="endpoint-options-build"></a>

## Native endpoint options without runtime or JSON

`qd/client/endpoint_options.hpp` is installed with CLIENT ON in `qdcxx_client`.
Runtime, CONNECT, JSON and BUILD_TESTING may stay OFF; the client facility retains
standard `Threads::Threads`. This profile needs no transport/TLS SDK or C compiler.
Use the compiler setup in this guide, then:

```powershell
cmake -S . -B build-options '-DORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON' '-DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=OFF' '-DORG_TTLDTOR_QDCXX_BUILD_CONNECT=OFF' '-DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF' '-DBUILD_TESTING=OFF' '-DCMAKE_INSTALL_LIBDIR=lib'
cmake --build build-options --config Release
cmake --install build-options --prefix C:/sdk/qdcxx-options --config Release
cmake -S tests/conformance/endpoint_options_consumer -B build-options-consumer '-Dqdcxx_DIR=C:/sdk/qdcxx-options/lib/cmake/qdcxx'
cmake --build build-options-consumer --config Release
ctest --test-dir build-options-consumer -C Release --output-on-failure
```

Choose your installation prefix; explicit `lib` keeps the package path fixed.
The complete [consumer source](../tests/conformance/endpoint_options_consumer/main.cpp)
contains headers and `main`; its [CMake project](../tests/conformance/endpoint_options_consumer/CMakeLists.txt)
uses `find_package(qdcxx CONFIG REQUIRED)` and links `qdcxx_client qdcxx_quote`.
It creates two independent snapshots without runtime/network activation and prints
`Independent endpoint snapshots: 7301, 7302` with exit zero.
[Native usage and driver restrictions](SUBSCRIPTIONS.md#endpoint-options) and
[executed validation](../tests/conformance/subscriptions/OPTIONS-VALIDATION.md)
cover lifetimes, budgets, the snapshot overload and optional dependency closure.

## Optional subscription tool

`ORG_TTLDTOR_QDCXX_BUILD_CONNECT` defaults to `OFF` and requires explicit CLIENT and
RUNTIME enablement with the qualified SDK sources above. Build target `qdcxx_connect`
for executable `qdcxx-connect`. The [usage and output guide](SUBSCRIPTIONS.md) covers
native EN/RU help, generic records and prepared Quote/Profile summaries. The default
mode preserves the controlled plain-TCP STREAM driver. Enable
`ORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON` for explicit `--mode recovery` with
STREAM/TICKER/HISTORY, finite retries, reset notices and drain outcomes. Enable
`ORG_TTLDTOR_QDCXX_BUILD_TLS=ON` and prepare the
[qualified TLS SDK](#tls-source-profile-cpp-build-en) for verified TLS and optional Basic
credentials. External schemas additionally require `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON`.
Disabled features reject before input files or network activation. The CLI selects
one family and one record per run; shared queues and multiple logical subscriptions
are available through the [public receiving API](SUBSCRIPTION-RECEIVER.md).

## Desired subscription state

Include `qd/collectors/stream.hpp` and link `qdcxx_collectors` for pure bounded STREAM
staging over admitted `LiveEvent` owners. The [collector contract](../spec/subscriptions/STREAM-COLLECTOR.md)
defines factory transfer, exact membership barriers, front/pop/discard, finite work and terminal overflow.
No runtime, JSON or additional SDK is needed. `collectors_stream` is the focused CTest;
the production-only installed package exports this target too.

For a shared bounded latest cache and independent local delivery views, include
`qd/collectors/ticker.hpp` and link the same `qdcxx_collectors` target. The
[TICKER contract](../spec/subscriptions/TICKER-COLLECTOR.md) defines explicit retention,
whole-frame conflation, front/pop and stale/reset barriers. `collectors_ticker` is the
focused CTest. This pure component adds no runtime or external SDK dependency.

## Bounded HISTORY collector (QDCXX-065)

Include `qd/collectors/history.hpp` and link the installed `qdcxx_collectors` target. The component consumes whole admitted HISTORY events and provides bounded shared indexed storage with independent local views and per-request range evidence. It does not own a source session, upstream demand, callbacks or transport. See the [collector contract](../spec/subscriptions/HISTORY-COLLECTOR.md) and [validation](../tests/conformance/subscriptions/HISTORY-VALIDATION.md). The build target is `qdcxx_history_collector_tests`; its CTest name is `collectors_history`. The installed production-only package includes the public header and `qdcxx_collectors`.

The separate [runtime feasibility projects](../tests/conformance/transport/runtime-feasibility/README.md)
test pinned optional SDK mechanisms with explicit local acquisition, default OFF and synthetic loopback
peers. They do not change this root build, provide production runtime targets or require a root CRT change.

For bounded live output, include `qd/protocol/subscription_writer.hpp` and link `qdcxx_protocol`.
The [composer contract and example](../spec/subscriptions/WRITER.md) cover six subscription types,
explicit descriptions, owned partial-write suffixes and retained-output budgets. The focused test
is `protocol_subscription_writer`; optional [production Java verification](../tools/upstream/subscription-writer-oracle/README.md)
does not become a native build dependency. Recording profiles are unchanged.

Include `qd/core/subscription.hpp` and link `qdcxx_core` for bounded synchronous desired maps.
The [API contract and complete example](../spec/subscriptions/STATE.md) cover exact symbols,
schema identity, atomic commands, revisions, membership tokens and explicit turn/peak budgets.
No runtime, transport, JSON or new dependency is needed. `core_subscription_state` is the focused
CTest; wire composition and STREAM staging use the separate components above.
The optional local dispatcher above adds the separate callback and command boundary.

## Bounded in-memory tape writer

Link `qdcxx_protocol` and include `qd/protocol/tape_writer.hpp` to use
`org::ttldtor::qdcxx::protocol::OutgoingLayouts` and `TapeWriter`. The
[writer contract](../spec/protocol/TAPE-WRITER.md) defines the explicit uncompressed
`RAW_MESSAGE_V1` subset, exact full types 2/8/10/40/56/72/88/120, descriptor distinctions,
finite logical budgets and failure precedence. This API performs no file I/O.

Build a validated `core::LocalSchema`, supply complete `OutgoingRecord` declarations with
explicit sender IDs, local IDs and lookup names, and call `OutgoingLayouts::create` with
`LayoutLimits`. Names and field order are supplied by the caller; every selected local field
appears exactly once. Layouts own bounded schema/declaration copies and expose const views.
Sender IDs use a sparse native lookup and must be in 0..INT32_MAX-1. The excluded endpoint
overflows the locked Java parser's ID remapping; other large IDs can still exceed Java's
separate dense-array/heap bounds.

`TapeWriter::create(layouts, descriptor, limits)` independently owns the full configuration
before returning READY. Supply all members of `WriterLimits`: zero is a real allowance.
`begin()` emits the sole descriptor, `describe(ids)` announces one complete group, and
`data(records)` emits one complete RAW_DATA batch with values in local logical order.
`heartbeat(value)` and `marker()` preserve explicit frame boundaries. `finish()` emits no
bytes and is idempotent only after successful finish. No time or marker is synthesized.

Each operation returns `std::expected<WriterChunk, WriterDiagnostic>`. Successful chunks own
one complete envelope and remain valid after later operations or writer destruction. Record
views borrow symbol/value storage only for `data()`; layouts and descriptor inputs are borrowed
only during construction. A failed operation returns no bytes, commits no announcements or
output/frame counters, and retains its first terminal diagnostic. Later calls return that
same diagnostic without examining inputs or charging work. Work/attempt counters retain
charges from failures. Scratch limits bound private logical byte/unit storage, not allocator
overhead or caller-held chunks.

`encoding::writeCharArray` in `qd/encoding/char_array.hpp` writes nullable exact UTF-16 arrays
through existing compact/UTFChar writers, preserving null, empty and isolated surrogate units.
The writer emits raw symbols with FD and message-local FF repeats. It makes no shortest-cipher
or byte-identical whole-Java-tape promise. Writer and primitive checks are normal CTest targets
`protocol_tape_writer` and `encoding_char_array_writer`; the
[writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md) records the
executed profiles and separate production-parser observations.

To build the user guides, architecture documents and C++ API reference from the repository root, run
`cmake -DDOXYGEN=/absolute/path/to/doxygen -DOUTPUT=/absolute/path/to/new-docs -P tools/docs/Build.cmake`.
Open `OUTPUT/html/index.html`. See the [documentation builder](../tools/docs/README.md) for help,
defaults and limits. Generated documentation remains outside the tracked source tree.

## Synchronous offline replay

The optional `qdcxx_replay` library is enabled with
`ORG_TTLDTOR_QDCXX_BUILD_REPLAY=ON` (default OFF). Link to `qdcxx_replay` (it exposes\n`qdcxx_protocol` as a public dependency) and include
`qd/replay/tape_replay.hpp` to call `org::ttldtor::qdcxx::replay::replayTape` on an immutable
`protocol::AdmittedTape`. It is a synchronous, in-process library API: there is no replay executable,
filesystem access, JSON dependency, transport, worker thread, background task or implicit clock. The
caller supplies finite `ReplayLimits`, an observer, cancellation poll, and monotonic clock/wait
boundary; all borrowed objects must outlive the call.

Choose `IMMEDIATE` to deliver in wire order without clock sampling or waiting, or `RELATIVE` to
schedule nonempty data frames from the first present raw timestamp. Relative waits are injected,
checked monotonic deadlines in bounded slices. Empty data frames do not affect scheduling anchors or
range calculations. Raw tape timing and business fields are preserved; business timestamps do not
control scheduling. Callback record references expire when `onRecord` returns, so copy retained
values in the callback. Failed frames deliver no completed prefix; earlier successful callbacks are
not rolled back. Clean parser EOF and the final cancellation poll precede the single observer
`finish()` call.

On Windows, the replay enumerator `ReplayPolicy::RELATIVE` can collide with a `RELATIVE` macro from
Windows headers, and existing core headers call `std::numeric_limits<T>::min()` / `max()`, which the Windows `min`/`max` macros also break. For the tested include sequence,
define `NOMINMAX` before Windows headers and undefine `RELATIVE` after all Windows headers but before
the replay header:

```cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef RELATIVE
#undef RELATIVE
#endif
#include "qd/replay/tape_replay.hpp"
```

This consumer-side workaround was verified with a standalone Windows consumer; it does not establish
compatibility for every header order. The [replay contract](../spec/protocol/OFFLINE-REPLAY.md)
defines ownership, counters, timing, cancellation and failure precedence. [Validation evidence](../tests/conformance/replay/VALIDATION.md) records checked profiles and remaining matrix limits.

## Offline Java schema export

The [optional Java exporter](../tools/schema-export/README.md) resolves permitted locked Java
settings/XML sources and produces the existing native JSON version 1. Java/JDK 8 and Maven belong
only to explicit export/regeneration, not ordinary C++ configure/build/CTest or schema consumption.
Use `loadJsonSchema` and prepared Quote/Profile APIs below with explicit exported record IDs and
the same loaded schema context. The exporter preserves actual lookup keys and projects Java slots
to logical fields; its sidecar records the mapping. The generic inspector accepts the generated
`schema.json` with `--schema`, using unchanged version-6 output and both existing tape modes.

With JSON loader, tape inspector and `BUILD_TESTING` enabled, `schema_export_consumption` and
`schema_export_cli` read [committed production observations](../tests/fixtures/schema/export/expected)
without invoking Java, Maven, Python or PowerShell. They cover configured events and XML custom/extra
fields. The [export contract](../spec/schema/JAVA-EXPORT.md) and
[validation](../tests/conformance/schema-export/VALIDATION.md) separate supported projection,
executed checks and independent acceptance. C++ still does not resolve XML or Java properties.

## Native JSON schema loader

`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON` defaults ON and adds `qdcxx_schema_json`. Include
`qd/schema/json_schema.hpp` and link that target to use
`org::ttldtor::qdcxx::schema::loadJsonSchema(std::string_view, JsonSchemaLimits = {})`.
It returns `std::expected<core::LocalSchema, schema::JsonSchemaError>` and consumes supplied UTF-8
bytes without file I/O or ambient settings. The [native contract](../spec/schema/NATIVE-JSON.md)
defines the complete grammar, ownership, errors and finite defaults. `JsonSchemaLimits::documentBytes`
and `depth` bound parsing; `schema` supplies the existing core element/name budgets. These are logical
bounds, not an exact heap cap or an end-to-end linear-time guarantee.

CMake acquires [simdjson 4.6.4 source](https://github.com/simdjson/simdjson/releases/tag/v4.6.4)
from the [pinned archive](https://codeload.github.com/simdjson/simdjson/tar.gz/refs/tags/v4.6.4),
SHA-256 `b091107844fe928158c5c2265c20360fff312889ddf7ebc4528a0f0f8f2ff9cd`.
Only its amalgamation is compiled as a private dependency; upstream tests/tools and a C compiler
are not required. [MIT attribution](../third_party/licenses/simdjson-MIT.txt) is retained.
Public headers expose project/STL types only. Core, codecs, protocol and event targets do not depend
on the loader or simdjson; only consumers linking the optional loader acquire its private link dependency.

For the Windows multi-configuration build used by the CLI examples:

```text
cmake -S . -B .work/schema-json-msvc -DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON
cmake --build .work/schema-json-msvc --config Release
```

For a Linux single-configuration build, use `-B .work/schema-json-linux -DCMAKE_BUILD_TYPE=Debug`
with the same feature options, then `cmake --build .work/schema-json-linux`.
To build only the existing built-in inspection path, configure a separate fresh build directory with
`-DBUILD_TESTING=OFF -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF` and the desired inspector options.
OFF neither acquires nor links simdjson. Built-in commands remain available; a valid `--schema`
request reports `LOADER_DISABLED` (exit 4) before file I/O. Normal configure/build/CTest gains no
Python or PowerShell requirement. These are usage instructions, not a report of executed checks.

The [mixed native example](../spec/schema/examples/native/mixed.json) contains a custom record at
local ID 0, reordered Quote at ID 1 and reordered Profile at ID 2, with optional and extra fields.
At the owning application boundary, read this file with a finite byte limit; then the following
function illustrates loading and both preparations. Link `qdcxx_schema_json`, `qdcxx_quote` and
`qdcxx_profile`. The application chooses finite mapping budgets independently of loading budgets:

```cpp
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/schema/json_schema.hpp"

#include <string_view>

/** Prepare both event layouts from the mixed native example.
 * @param schemaBytes UTF-8 contents supplied by the application's bounded file reader.
 * @return True when loading and both preparations succeed; false on any typed failure.
 */
bool prepareExample(std::string_view schemaBytes) {
    using namespace org::ttldtor::qdcxx;

    auto loaded = schema::loadJsonSchema(schemaBytes);

    if (!loaded) {
        return false;
    }

    const events::MappingLimits limits{
        .fields = 64, .lookups = 128, .selections = 32, .keys = 128,
        .typeAlternatives = 64, .nameUnits = 1024,
        .totalNameUnits = 16384, .comparisonWork = 1048576};
    auto quote = events::PreparedQuote::prepare(*loaded, 1, limits);
    auto profile = events::PreparedProfile::prepare(*loaded, 2, limits);

    return quote.has_value() && profile.has_value();
}
```

The function demonstrates preparation only. A processing application retains the successful schema
and preparations, gives the same schema context to decoding, and maps complete `RecordValueView`
values with explicit event ownership limits as described below. Copies retain context; a second
load, even of identical bytes, has an independent identity and cannot reuse the first preparation.
Input and parser destruction do not invalidate the loaded schema. Preparation validates lookups,
types and required fields; a record name alone cannot establish compatibility. Generic records and
extra fields remain accessible independently of event projection. The
[external CLI](TAPE-INSPECT.md#external-native-json-schemas-and-output-version-6) always uses generic
version-6 output rather than selecting an event mapping.

Only `qdcxx-native` version 1, `hasTime:false` and exact full IDs 2/8/10/40/56/72/88/120 are accepted.
The old XML-captured JSON profile, XML/imports, Java-property resolution, URLs, automatic schema
selection, live replacement, new wire types/conversions and transport remain unsupported.

## Current portable development workflow

Use CMake 3.25+, a C++23 compiler and native build tools. [Developer-tool instructions](../tools/dev/README.md)
define the standalone bootstrap, native `qdcxx-run-utc` contract and Windows/Linux commands.
Bootstrap is the first-build exception: UTC phase bounds, exact command arguments and separate raw
streams replace line timestamps until the logger exists. Normal configure/build/CTest and the new
logger/formatter command trees require neither Python nor PowerShell; an interactive shell is
outside that dependency claim. `BUILD_TESTING=OFF` neither builds developer tools nor fetches doctest.

Formatting uses explicit native Uncrustify 0.83.0 followed by clang-format 20.1.8; the root
configurations and dependency locks stay unchanged. Run Format and then Check, with exact executable paths:

```text
cmake -DMODE=Format "-DUNCRUSTIFY=<native-uncrustify>" "-DCLANG_FORMAT=<native-clang-format>" -P tools/format/Format-Cpp.cmake
cmake -DMODE=Check "-DUNCRUSTIFY=<native-uncrustify>" "-DCLANG_FORMAT=<native-clang-format>" -P tools/format/Format-Cpp.cmake
```

[Formatter instructions](../tools/format/CPP.md) describe JSON file selection, scope, native Linux
preparation and the optional unchanged Windows PowerShell downloader/wrappers. Acquisition and
execution are separate. Java tooling and remaining manual Python checks retain their requirements.

QDCXX-028 U1–U5 are implemented and verified on Windows/Linux, including corrected reruns;
[new validation](../tests/conformance/tooling/VALIDATION.md) records actual results separately
from historical evidence. Independent acceptance is open. macOS is unverified and nonblocking.
The component-era results below preserve their original scope and do not establish current matrix results.

## Prepared Quote and Profile mappings

Include `qd/events/quote.hpp` or `qd/events/profile.hpp` and link `qdcxx_quote` or `qdcxx_profile`.
Both depend on `qdcxx_event_mapping`, which depends only on `qdcxx_core`; source acquisition,
protocol and transport are outside these event libraries. No production dependency was added.
`defaultProfileSchema(limits)` is a convenience for all sixteen default fields, while
`PreparedQuote::prepare(schema, localId, mappingLimits)` and `PreparedProfile::prepare(...)`
accept compatible caller-provided schemas, including reordered, extra and optional fields.
The legacy `defaultQuoteSchema` and `mapQuote` contract stays unchanged.

Prepare once against the same `LocalSchema` used by the decoder. Supply
`{record.context, record.typedValues}` as `RecordValueView` to `map`, plus the symbol and explicit
ownership bounds. Profile takes `EventValueLimits{symbolUnits, stringUnits, totalUnits}`;
Quote takes `symbolUnits`. Both accept a separate optional `eventTime` argument in milliseconds.
For RAW data pass `result.timing->eventTimeMs()`; the default is zero. Each returned event owns its
symbol and strings. Callers retaining multiple events must bound their cumulative ownership/work.

`LocalSchema::recordContext(id)` and the decoded record's context identify the immutable schema
and record. Copies and `boundedCopy(limits)` retain identity; a fresh `create`, even with identical
declarations, creates an independent identity. Preparation/decoder input destruction is safe.
An untagged span or matching counts/types cannot authenticate a mapping. See the
[shared API](../spec/events/EVENT-MAPPING.md), [Quote](../spec/events/QUOTE.md),
[Profile](../spec/events/PROFILE.md) and [configuration evidence](../spec/schema/CONFIGURED-EVENTS.md).

The explicit tape preset `--profile default-profile-v1` emits version 5 in both existing modes;
[Profile inspection](PROFILE-INSPECT.md) defines CLI limits and output. Configured library schemas
do not imply a C++ XML/properties loader or full dxlink support. Earlier sections below describe
their historical component scope.

## Exact extended field values and bounded char arrays

`LocalProfile::EXTENDED_FIELDS_V1` accepts exactly full IDs 2/8/10/40/56/72/88/120. Old local profiles
retain their exact sets. `FieldValue::units` is owned optional UTF-16: type 10 has raw zero and
null/empty/content in units; scalars have no units payload. `validFieldValue` validates this shape and
tag/range invariants. Types 40/72/88 retain signed raw low-32-bit values, without text/calendar conversion.
[FIELD-TYPES](../spec/encoding/FIELD-TYPES.md) defines the exact contract.

Include `qd/encoding/char_array.hpp` and link `qdcxx_encoding`. `readCharArray(input, limits, boundary)`
returns owned units, consumed bytes, charged units and explicit status; it retains no input or partial
failed value. `CharArrayLimits` supplies encoded bytes including prefix and UTF-16 units.
The default `CharArrayBoundary::PRIMITIVE` matches direct primitive observations; `MESSAGE` selects
the production message remaining-byte check. This reader is separate from UTFString.

Set the trailing `StreamDataLimits::fieldString`, `fieldBytes`, `fieldUnits` and `ownedFieldUnits`
explicitly for string-bearing data. Defaults are zero allowance, preserving old aggregate call sites.
`StreamDataResult` exposes the three corresponding counters; `TapeReader::fieldBytes()`,
`fieldUnits()` and `ownedFieldUnits()` expose cumulative tape charges. Bytes/units count skipped,
repeated and failed attempts; owned units count completed output, while admission also bounds the
current record and temporary replacement. Symbols/names keep separate budgets. Read typed values
from `StreamDataRecord::typedValues` in both STREAM and RAW paths; legacy `values` remains unchanged.
All returned strings are owned independently of input/context/reader lifetime. Only complete records
are published; terminal failure keeps the completed prefix.

The tool-local `oracleFieldTypesSchema` supplies only the fixed synthetic schema for
`--profile oracle-field-types-v1`; [CLI usage and five limits](TAPE-INSPECT.md) describe version 4.
The unchanged standard Profile is tested with a complete caller-local field projection, without a
Profile event adapter. New suites are `field_type_java_values`, `field_type_fixture_negatives`, `field_type_java_tape`,
`field_type_inspection_io` and `field_type_inspection_cli`; the last two require the tape inspector.
Build targets/options and dependency policy are unchanged. [Coverage](../tests/conformance/protocol/field-types/COVERAGE.md)
and [validation](../tests/conformance/protocol/field-types/VALIDATION.md) record actual checks;
independent review remains pending. The earlier component sections below retain their historical scope.

## RAW/MESSAGE tape API

QDCXX-023 adds explicit `TapeMode::RAW_MESSAGE_V1` as the third TapeReader constructor argument.
Set the additional finite `TapeLimits::timingEvents` (default zero); the CLI default is 4096.
`TapeFeedResult::heartbeat` owns the accepted explicit heartbeat representation, and `timing`
owns raw timestamp/packed event-time sequence for all returned records of one RAW frame.
`TapeTiming::eventTimeMs()` exposes delivered milliseconds, which can differ from the raw timestamp
at Java's saturated boundaries. Retain the metadata along with records. No timing state is borrowed
from input. Existing constructor calls default to LEGACY and retain their behavior.

`decodeRawData` is the separate exact type-5 payload entry point; `decodeStreamData` remains type 15.
Timing and dispatch belong to TapeReader, while payload primitives remain shared internally.
`mapQuote` still takes a symbol-copy budget as its third argument, never a timestamp.
[Exact contract and ownership](../spec/protocol/RAW-TAPE.md), [tool invocation](TAPE-INSPECT.md).
Build dependencies and options are unchanged. New raw tape conformance, IO and CLI suites augment
the existing tests; the IO/CLI tests require the tape inspector option. Broader field types,
adapters, importer, transport and whole private-file compatibility remain excluded.

## Default composite Quote API

QDCXX-022 adds qd/encoding/utf_char.hpp to qdcxx_encoding, qd/core/wide_decimal.hpp to qdcxx_core,
and qd/events/quote.hpp in qdcxx_quote (core-only dependency). The Quote adapter returns a validated
owned LocalSchema for LocalProfile::QUOTE_FIELDS_V1. Supply it to TapeReader using the existing
finite TapeLimits; read StreamDataRecord::typedValues, then call
events::mapQuote(record.symbol.units, record.typedValues, maxSymbolUnits). The result owns its
symbol, millisecond times, exchange units and four raw WideDecimal values with toDouble/toString.
Legacy schemas default to COMPACT_INT_V1 and keep StreamDataRecord::values.

Build options are unchanged. Example after building both tools:

```powershell
.work/tape/Release/qdcxx-tape-inspect --profile default-quote-v1 --input tests/fixtures/protocol/quote/expected/default-quote.qtp --output-format human
```

[Contract](../spec/protocol/DEFAULT-QUOTE.md), [CLI](TAPE-INSPECT.md),
[validation and remaining limits](../tests/conformance/protocol/quote/VALIDATION.md).
New suites: quote_java_values, quote_java_tape, quote_inspection_io and quote_inspection_cli.
The IO/CLI suites require the tape tool option. Existing suites and production-only no-fetch/no-Python
configuration remain supported. No general tape, RAW_DATA, MESSAGE timing or schema loading is added.

[Русская версия](CPP-BUILD.ru.md). English is authoritative.

`qdcxx_encoding` is a dependency-free C++23 production library with provisional compact int/long
and UTFString APIs. QDCXX-010 introduced compact; QDCXX-011 adds
[utf_string.hpp](../include/qd/encoding/utf_string.hpp). The unchanged Java
[primitive evidence](../spec/encoding/PRIMITIVES.md) defines compatibility;
[UTFString policy and reuse decision](../spec/encoding/UTFSTRING.md) define explicit C++ bounds.
PentaCodec, importer, schema runtime, event payload decoding, frame writing, transport and packaging remain outside scope.

## Bounded tape API and optional utility

QDCXX-021 adds qd/protocol/tape_reader.hpp to qdcxx_protocol. Construct TapeReader(schema, limits)
with finite TapeLimits; the reader owns a bounded schema copy and immutable contexts. feed returns
at most one owning frame event plus completed data prefix; resubmit the unaccepted suffix. finish
requires actual EOF and one accepted initial header. No reset/replacement or IO is in the library.
The [contract](../spec/protocol/TAPE-READER.md) specifies descriptor grammar, atomic updates, construction
work accounting, positions and EOF. API is provisional. All older component contracts remain unchanged.

```powershell
cmake -S . -B .work/tape -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON
cmake --build .work/tape --config Release
ctest --test-dir .work/tape -C Release --output-on-failure
.work/tape/Release/qdcxx-tape-inspect --profile oracle-sre-v1 --input tests/fixtures/protocol/tape/expected/constrained.qtp
```

Targets qdcxx_tape_inspect_tool/qdcxx_tape_inspect keep IO/presentation outside protocol/core. Either
inspector can be enabled separately with BUILD_TESTING=OFF; all four option combinations were built
without doctest fetch or Python discovery. New tests are protocol_binary_tape, tape_inspection_io and
tape_inspection_cli (CMake process checks, no new Python); all ten old suites remain when the old tool
is enabled. [CLI contract](TAPE-INSPECT.md), [executed matrix and limits](../tests/conformance/protocol/tape/VALIDATION.md).

## DESCRIBE_RECORDS API and QDCXX-013 checks

Include `qd/protocol/describe_records.hpp` and link qdcxx_protocol. Call
`parseDescribeRecords(payload, {4096, 64, 128, 1024, {256, 256}, 4096})` only for a complete type-2
payload excluding the envelope. Budgets are payload bytes, descriptions, fields/description, total
fields, per-name {encoded body bytes, UTF-16 units}, and aggregate name units. Zero is valid.
The owning result provides descriptions, status, stage, consumed, errorOffset and primitiveStatus.
It retains ordered complete records before an error, never the partial current record. It preserves
exact UTF-16, duplicate IDs/names, VOID and structurally valid full types 0..255, including those
unsupported by Java descriptor construction. This does not promise serialization operations or local
field factory support. No schema lookup or map is built. Allocation errors preserve the completed
prefix; a throwing primitive commits no consumption beyond its start. See the
[contract](../spec/protocol/DESCRIBE-RECORDS.md) for precise precedence and positions.

CTest adds protocol_java_descriptions; qdcxx_description_tests links only qdcxx_protocol and test-only doctest.
ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES defaults to the additive P08 descriptions.tsv and permits temporary negative
selection. All existing commands below include the new target. [Coverage/results](../tests/conformance/protocol/descriptions/VALIDATION.md)
record 81 Java observations, 50,841 new assertions, MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan,
BUILD_TESTING=OFF with no acquisition, regressions and immutable negative checks. API is provisional;
event payloads, local binding, transport and writer remain outside scope.

## QTP frame reader API and QDCXX-012 checks

`qdcxx_protocol` is a production component without external dependencies, linking only qdcxx_encoding.
Include `qd/protocol/frame_reader.hpp` and link qdcxx_protocol. Construct
`org::ttldtor::qdcxx::protocol::FrameReader reader({1024, 1033});`: these are body and pending budgets, allowing
nine prefix bytes. Incompatible/overflowing budgets throw invalid_argument; constructor allocation
failure propagates. The API is single-threaded and non-reentrant.

`feed(input)` returns at most one owning frame, status and exact accepted count. Advance input by
accepted and resubmit the unaccepted suffix. FRAME_READY carries a frame; NEED_INPUT means no
complete frame, while FAILED/FINISHED accept nothing further. `pending()` borrows the exact retained
bytes until the next mutating operation. Returned frames own payload and survive feed/reset/reader
destruction. `finish()` distinguishes a clean boundary from truncated input; empty feed is not EOF.
`reset()` clears only the framer.

The first framing error is terminal; error()/errorOffset() give its cause and frame start,
totalAccepted() gives the accepted position. Invalid payload syntax and unknown in-range type
remain opaque output. EMPTY_HEARTBEAT has no type and differs from MESSAGE with type=0. Reader
storage is fixed body+9, delivered payload owns separate bounded storage; caller-retained frames
are outside the reader's aggregate budget. Payload allocation failure gives ALLOCATION_FAILURE.
The [EN/RU contract](../spec/protocol/FRAMING.md) defines exact wire/resource/lifecycle boundaries.

Commands below now build both libraries and also run qdcxx_frame_tests/protocol_java_frames. New tests
read historical P01 and separately added P02; ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES and ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS permit
temporary negative selections. MSVC/ClangCL/GCC Debug/Release and GCC ASan+UBSan pass;
BUILD_TESTING=OFF does not fetch doctest. [Coverage](../tests/conformance/protocol/COVERAGE.md),
[results and UTC logs](../tests/conformance/protocol/VALIDATION.md). Further payload/transport/writer
components require a separate approved task.

## UTFString API and current checks

`readUtfString(input, {bodyBytes, codeUnits})` returns owning optional UTF-16 units, status and
consumed bytes. Only OK permits interpreting value: absent means null, engaged empty means empty.
Errors have no partial value; allocation failures may throw. `writeUtfString(value, output, limits)`
borrows an optional UTF-16 view and writes atomically after preflight; input/output must not overlap.
Both budgets are mandatory, finite operational bounds. There is no normalization or surrogate repair.
Status adds INVALID_ENCODING, INVALID_LENGTH and RESOURCE_LIMIT without changing compact behavior.
See the contract for exact precedence, Java body overrun and allocation guarantees.

Existing commands below now build and test both codecs. The executable/CTest names remain
`qdcxx_compact_tests`/`compact_java_vectors` for compatibility with existing tooling; they execute four
cases, 43,375 assertions and all 1146 Java observations (314 UTFString, 832 compact), zero exclusions.
MSVC, ClangCL and WSL GCC pass Debug/Release; GCC ASan+UBSan also passes. Historical QDCXX-010 counts
below describe the earlier compact-only run. `BUILD_TESTING=OFF` still fetches nothing; no production
UTF dependency was selected. Reuse probes are optional, separate ignored projects.
[Current validation](../tests/conformance/encoding/VALIDATION.md) retains UTC evidence.

Additional WSL sanitizer commands from the repository root after the Linux bootstrap below (cached pinned doctest). Report ASan/UBSan and LSan separately; ptrace checks with `detect_leaks=0` do not establish LSan coverage:

```sh
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-configure.log -- cmake -S . -B .work/utf-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer" "-DFETCHCONTENT_SOURCE_DIR_DOCTEST=/work/qd-cxx/.work/linux-debug/_deps/doctest-src"
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-build.log -- cmake --build .work/utf-asan -j 4
.work/dev-linux/qdcxx-run-utc --log .work/utf-asan-test.log -- ctest --test-dir .work/utf-asan --output-on-failure
```

## Build and test on Windows

Executed with CMake 4.4.2, generator `Visual Studio 17 2022`, x64, Windows SDK 10.0.26100.0,
MSBuild 17.14.51, MSVC 19.42.34444.0 (tool directory 14.42.34433). The second compiler is the
installed ClangCL 19.1.5 using the same MSVC standard library/SDK. Tests use doctest 2.4.12 via
CMake FetchContent, pinned by archive URL and SHA-256 in [CMakeLists.txt](../CMakeLists.txt).
Only `qdcxx_compact_tests`, `qdcxx_frame_tests` and `qdcxx_description_tests` link doctest; `BUILD_TESTING=OFF` neither downloads nor links it. Downloads
are stored under the ignored build directory `_deps/`; the first test configure needs network access.
No package manager is needed. doctest 2.4.12 emits a non-fatal CMake deprecation warning for its
old minimum policy version; the downloaded source is unmodified. CMake requires 3.25+; each target inherits `cxx_std_23` from the library,
with extensions disabled. On these toolsets CMake selects `/std:c++latest` for C++23 support.

Current commands from the repository root (PowerShell may be the interactive shell):

```text
cmake "-DBINARY=.work/dev" "-DLOG=.work/dev-bootstrap.log" "-DGENERATOR=Visual Studio 17 2022" -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-configure.log -- cmake -S . -B .work/msvc "-GVisual Studio 17 2022" -A x64
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-debug-build.log -- cmake --build .work/msvc --config Debug
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-debug-test.log -- ctest --test-dir .work/msvc -C Debug --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-release-build.log -- cmake --build .work/msvc --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/msvc-release-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-configure.log -- cmake -S . -B .work/clang "-GVisual Studio 17 2022" -A x64 -T ClangCL
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-debug-build.log -- cmake --build .work/clang --config Debug
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-debug-test.log -- ctest --test-dir .work/clang -C Debug --output-on-failure
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-release-build.log -- cmake --build .work/clang --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/clang-release-test.log -- ctest --test-dir .work/clang -C Release --output-on-failure
```

Build outside the source root; `.work/` and `build/` are ignored. In-source configure is rejected.
`BUILD_TESTING=OFF` builds only qdcxx_encoding and qdcxx_protocol libraries. `ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES` may point at temporary fixture
copies for negative checks; it defaults to the checked-in expectations. Initial sandboxed MSBuild
SDK discovery was denied access to the local Microsoft SDKs directory; the authorized configure/build
runs above succeeded with sandbox elevation. This is an environment restriction, not a codec failure.

## API and ownership

All four functions are in `org::ttldtor::qdcxx::encoding`, borrow caller-owned `std::span` storage only for the call,
allocate nothing and retain no pointer. Results own their values/counts. The caller keeps the span
valid throughout the call. Byte order is explicitly big-endian; reads require no alignment.

- `readCompactInt` / `readCompactLong` return `ReadResult<int32_t/int64_t>`: `value`, `consumed`, `status`.
  Success consumes exactly one value and leaves trailing bytes available via `input.subspan(consumed)`.
  Overlong forms are accepted; int returns the signed low 32 bits even for a long encoding.
- Empty/incomplete input returns `INSUFFICIENT_INPUT`, zero value and the entire supplied length as
  `consumed`. This maps observed Java EOF category/position, not exception class/message strings.
  There is no rollback: retain original bytes and retry from the start when more bytes arrive.
  A partially consumed position is not resumable state; transactional framing belongs to a later layer.
- `writeCompactInt` / `writeCompactLong` return `WriteResult`: `written`, `status`. They produce canonical
  Java bytes (at most 5/9). Insufficient capacity, including zero capacity, returns
  `INSUFFICIENT_OUTPUT`, zero written and leaves all output unchanged. Success changes only the first
  `written` bytes. This bounded C++ writer policy is not a claim about Java stream output failure.

Unsigned accumulation/masking and fixed-width bit reconstruction preserve signs without signed
arithmetic overflow, negative shifts, type-width shifts or native-endian assumptions. Writer size
selection uses positive, representable powers of two. No strict-canonical reader validation is added.

## Verification and formatting

[Coverage](../tests/conformance/encoding/COVERAGE.md) and [validation](../tests/conformance/encoding/VALIDATION.md)
record selected/excluded cases, failures and limits. [Formatting instructions](../tools/format/CPP.md)
define the pinned two-tool pipeline; naming and meaningful Doxygen require separate review.

## Linux / WSL and UTC logs

The next paragraph preserves the original compact-component validation. Current Python-free
compiler/sanitizer results are recorded separately in [QDCXX-027 validation](../tests/conformance/python-migration/VALIDATION.md).

Also executed in WSL2 Ubuntu 24.04.4 LTS, x86_64, kernel 6.18.40.1-microsoft-standard-WSL2,
GCC 13.3.0 (libstdc++), CMake 4.4.2 and Ninja 1.11.1. Debug and Release pass the same two doctest
cases (28,358 assertions) and unchanged 832 selected Java observations. Library-only configure/build
with `BUILD_TESTING=OFF` passes without creating `_deps/`. Linux negative fixture/parser/CTest checks
pass; `--skip-format` explicitly omits the Windows-only formatter checks already run on Windows.
Sanitizers, other Linux distributions, macOS and other architectures were not checked.

Current commands from a POSIX shell in Ubuntu WSL (`wsl -d Ubuntu-24.04`), at the mounted repository:

```sh
set -e
cd /work/qd-cxx
cmake -DBINARY=.work/dev-linux -DLOG=.work/dev-linux-bootstrap.log -DGENERATOR=Ninja -DCONFIG=Release -P tools/dev/Bootstrap.cmake
for config in Debug Release; do
    build=".work/linux-$config"
    .work/dev-linux/qdcxx-run-utc --log "$build-configure.log" -- cmake -S . -B "$build" -G Ninja "-DCMAKE_BUILD_TYPE=$config"
    .work/dev-linux/qdcxx-run-utc --log "$build-build.log" -- cmake --build "$build"
    .work/dev-linux/qdcxx-run-utc --log "$build-test.log" -- ctest --test-dir "$build" --output-on-failure
done
.work/dev-linux/qdcxx-run-utc --log .work/linux-library-configure.log -- cmake -S . -B .work/linux-library -G Ninja -DBUILD_TESTING=OFF
.work/dev-linux/qdcxx-run-utc --log .work/linux-library-build.log -- cmake --build .work/linux-library
```

The retained optional [Run-Utc.ps1](../tools/Run-Utc.ps1) requires PowerShell 7.4+ and an argument array,
timestamps output and command start/exit in ISO 8601 UTC (`Z`), sets only child `TZ=UTC`
and preserves the exit code. It is a UTF-8 text logger with invalid-sequence replacement and
LF endings, including the final unterminated line. CR/CRLF/LF delimit lines; order within each
stream is preserved, while stdout/stderr race order is unspecified. Launcher, log and console
failures return 1; an internal worker owns a Windows job or Unix process group before launching
the command, so cleanup also works after the command exits. Cleanup waits at most 5 seconds for
the worker; deliberately detached Unix sessions are outside this boundary.
Raw CLI evidence is captured separately. Historical logs and the [old logger](../tools/run_utc.py)
are not rewritten.

The manual check below still requires Python; it is unmigrated and not replaced by CTest results.
Other deferred workflows remain in the [migration ledger](PYTHON-MIGRATION.md).

```powershell
& ./tools/Run-Utc.ps1 -Log .work/linux-negative.log -Executable python3 -ArgumentList @('tests/conformance/check_compact_negative.py','.work/linux-release/qdcxx_compact_tests','--skip-format','--report','.work/linux-negative-proof.json')
```

## Static type-8 binding API (QDCXX-014)

qdcxx_core owns the validated immutable local subset and has no protocol/transport dependency.
Include qd/core/local_schema.hpp; LocalSchema::create(records, {recordCount, fieldCount, lookupCount,
nameUnits, totalNameUnits}) returns std::expected with an owned schema or explicit error. Records
carry local ID/name/hasTime, full field types and explicit record lookup keys targeting field positions.
Only hasTime=false and exact type 8 are supported. Input files/provenance are not required.

Include qd/protocol/static_binding.hpp and link qdcxx_protocol. bindRecord(raw, schema,
{fieldCount, nameUnits, totalNameUnits}) returns an independent owning RecordBinding or explicit error.
A missing localRecordId denotes a described unknown record; missing targetFieldId denotes a type-8 skip.
No description creates no plan. No defaults, event decoder or session map is implied. All budgets are
mandatory, finite logical-element counts; allocation failures publish no partial result.
See [contract](../spec/protocol/STATIC-BINDING.md) and [validation](../tests/conformance/protocol/binding/VALIDATION.md).
CTest now adds qdcxx_binding_tests/protocol_java_binding; ORG_TTLDTOR_QDCXX_BINDING_FIXTURES selects reviewed plans.tsv.
All four targets pass the documented compiler matrix, GCC sanitizers and negative checks. BUILD_TESTING=OFF
builds qdcxx_core/qdcxx_encoding/qdcxx_protocol without fetching doctest. Existing command examples apply unchanged.

## QDCXX-015 symbol API

Include qd/encoding/symbol_reader.hpp and link qdcxx_encoding. Construct
`SymbolReader reader({4096, 1024, 1024}, 0);` with input-byte, returned-unit and retained-unit budgets
and EMPTY_SET mask zero. `beginMessage(0)` clears symbol/MRU/failure; call it between complete
messages. `read(remainingPayload)` returns one owned value with UTF-16, cipher and raw flags,
consumed count and explicit status. All read failures are terminal; nonzero option masks are
unsupported. [Contract](../spec/encoding/SYMBOLS.md) and [final validation](../tests/conformance/encoding/symbols/VALIDATION.md).
CTest adds encoding_java_symbols / qdcxx_symbol_tests; ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES selects immutable Java TSV.
No production dependency or event decoder is added. Earlier PentaCodec exclusions above are historical.

## STREAM_DATA API (QDCXX-016)

Include `qd/protocol/stream_data.hpp` and link qdcxx_protocol. First call
`StreamDataContext::create(schema, descriptions, limits)` with validated LocalSchema and unique raw
ordinary type-8 descriptions. The immutable result owns plans and local field counts. Then call
`decodeStreamData(payload, context, limits)` for one complete type-15 payload, excluding its envelope.
ContextLimits bounds schema storage and aggregate descriptions/actions/names. StreamDataLimits bounds
payload, processed records including skips, wire fields, emitted records/values/symbol units and
SymbolLimits. No unlimited defaults. Nonzero raw flags fail before sender ID; every call resets symbols.
Results own local IDs/symbols/values and retain the complete prefix on later failure. See the
[contract](../spec/protocol/STREAM-DATA.md) for exact precedence and positions. No Frame overload is supplied.

CTest adds `protocol_java_stream_data` / `qdcxx_stream_data_tests`; ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES permits only
intentional temporary fixture overrides. Existing build commands run all six suites. The full compiler/
sanitizer/production-only matrix and negative checks are in [validation](../tests/conformance/protocol/stream-data/VALIDATION.md).
No transport, dynamic session, nonzero flags, importer or production dependency is introduced.

## QDCXX-017 offline tool

Enable `-DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON` for the bounded `qdcxx-qtp-inspect` executable; default OFF
keeps the library-only build. `-DBUILD_TESTING=OFF` never acquires test dependencies, including
when the tool is enabled. [CLI/profile/limits](QTP-INSPECT.md),
[contract](../spec/protocol/OFFLINE-INSPECTION.md). With testing and tool enabled, CMake and a test-only C++ checker
provide process-level CTest checks and doctest covers portable injected I/O failures. The six
previous suites remain enabled. This adds no production dependency or public session framework.
StreamDataResult now exposes charged `processedRecords`/`wireFields` under the
[accepted accounting decision](../spec/protocol/STREAM-DATA.md#aggregate-accounting), without wire or position changes.

## Strict tape admission and offline rewriting

`qdcxx_protocol` includes `qd/protocol/tape_admission.hpp`. `admitTape(schema, bytes, limits)`
accepts only a complete supported `RAW_MESSAGE_V1` byte sequence that a fresh reader can finish
successfully. It checks every description, including unused records, binds an exact complete
outgoing inventory, and returns a move-only `AdmittedTape` only after explicit EOF. The token owns
the original bytes, an independent schema/layout inventory, descriptor and limits. Its `valid()`
state follows moves; accessors must not be used on a moved-from token. Failure publishes no token,
does not retain decoded history and reports the actual nonrefunding work and peak logical storage.

`AdmissionLimits` separately bounds input bytes, reader work/storage, layout/schema inventory,
additional nonrefunding copy/visit work and simultaneous additional logical byte/UTF-16 storage.
Scratch counters exclude allocator metadata/capacity and the reader's separately bounded private
buffers. The accepted descriptor must be exactly representable; unsupported or ambiguous mappings
fail closed. The Java-overflow sender endpoint is rejected without renumbering.

The optional `qdcxx-tape-write` executable is enabled with
`-DORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE=ON` and defaults OFF. It selects an explicit built-in schema
or `--schema` when the optional native JSON loader is enabled. Production core/protocol libraries
remain independent of JSON and file I/O. The tool reads and closes the complete source before
admission, performs a separate bounded fresh-reader/writer composition pass, and creates the final
destination atomically with create-only hard-link publication after checked output completion.
Existing destinations are refused; no overwrite or rename fallback is used. See the
[tape-write contract](TAPE-WRITE.md), [CLI reference](../tools/tape-write/README.md), and
[admission validation](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.md).

## Bounded MUX interest API (QDCXX-066)

The installed `qdcxx_mux` target links to `qdcxx_core`; include `qd/mux/interest.hpp`. It is a pure desired-interest component with no new SDK dependency. The build target and executable are named `qdcxx_mux_interest_tests`; the CTest name is `mux_interest`. See the [interest contract](../spec/mux/INTERESTS.md) and [validation](../tests/conformance/subscriptions/MUX-VALIDATION.md).

<a id="mux-delivery-queues"></a>

## Local MUX delivery queues

`ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` also builds/installs `qdcxx_mux_queue` and
`qd/mux/queue.hpp`. The target links `qdcxx_mux`, `qdcxx_collectors` and standard `Threads::Threads`;
core C++23 usage requirements propagate to consumers. Runtime and JSON may remain OFF. CLIENT OFF
omits this target/header and its Threads discovery; the existing pure `qdcxx_mux` stays available.
The queue adds no networking SDK or background worker.

Installed applications use `find_package(qdcxx CONFIG REQUIRED)` and link `qdcxx_mux_queue`.
Use the existing install/relocation and `CMAKE_PREFIX_PATH`/`qdcxx_DIR` instructions above. The CTest
name is `mux_queue`; its executable/target is `qdcxx_mux_queue_tests`. The
[queue contract](../spec/mux/QUEUES.md) explains family factories, caller-supplied source admission,
finite work and per-client reservations. The [validation section](../tests/conformance/subscriptions/MUX-VALIDATION.md#mux-queue-validation)
separates queue-specific execution from historical pure-interest results and runtime integration.
This component does not create a full wire subscription client or close external sockets.

For a complete caller-owned main.cpp, the qualified installed consumer uses these CMake declarations.
The [integration fragment](../spec/mux/QUEUES.md#compiled-stream-integration-example) requires the
ready source/schema/input preamble described there; it supplies no standalone main.

```cmake
cmake_minimum_required(VERSION 3.25)
project(mux_queue_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
add_executable(mux_queue_consumer main.cpp)
target_link_libraries(mux_queue_consumer PRIVATE qdcxx_mux_queue)
set_target_properties(mux_queue_consumer PROPERTIES CXX_EXTENSIONS OFF)
```

<a id="selector-profiles"></a>

## Canonical selector components

`qd/encoding/penta_symbol.hpp` is installed with the existing `qdcxx_encoding`; no new SDK, Threads
requirement or build option is added to the pure graph. Existing core/protocol/MUX/collectors expose
the immutable [selector profiles](../spec/subscriptions/SELECTORS.md). Optional `qdcxx_mux_queue`
still requires CLIENT ON; standalone dispatcher/CLI defaults remain legacy Exact. Consumers must
rebuild for changed C++ selector/membership metadata; source-compatible defaults do not promise binary ABI.
The focused CTest is `wildcard_subscriptions`, executable/target `qdcxx_wildcard_subscription_tests`.
[Qualified examples and packages](../tests/conformance/subscriptions/WILDCARD-VALIDATION.md) cover
both pure and CLIENT graphs with physically relocated installed prefixes and unchanged wire/schema layouts.

<a id="tls-source-profile-cpp-build-en"></a>

## Building controlled TLS

The pure `qd/transport/tls_resources.hpp` provides requirement calculation and atomic work admission.
The implemented `qd/transport/tls_runtime.hpp` belongs to the optional static `qdcxx_tls` target.
See [resources](../spec/transport/TLS-RESOURCES.md), [runtime](../spec/transport/TLS-RUNTIME.md)
and [executed checks and limitations](../tests/conformance/transport/TLS-VALIDATION.md).
Earlier runtime commands describe plain TCP; `runStream`, CLI and endpoint defaults are unchanged.

Enable `ORG_TTLDTOR_QDCXX_BUILD_CLIENT`, `ORG_TTLDTOR_QDCXX_BUILD_RUNTIME` and
`ORG_TTLDTOR_QDCXX_BUILD_TLS`. Set `ORG_TTLDTOR_QDCXX_OPENSSL_ROOT` to the installed qualified SDK
and `ORG_TTLDTOR_QDCXX_OPENSSL_SOURCE` to the exact OpenSSL 3.5.9 source tree.
Replace the illustrative paths below with absolute paths. Qualification checks source/SDK identities,
not only a version string. Runtime also needs explicit Asio/c-ares inputs as described above.

```powershell
cmake -S . -B build-tls -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
  -DORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON -DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON `
  -DORG_TTLDTOR_QDCXX_BUILD_TLS=ON `
  -DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/absolute/qualified-sdk `
  -DORG_TTLDTOR_QDCXX_OPENSSL_SOURCE=C:/absolute/openssl-3.5.9 `
  -DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=C:/absolute/asio/include `
  -DORG_TTLDTOR_QDCXX_CARES_SOURCE=C:/absolute/c-ares `
  -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=C:/absolute/install
cmake --build build-tls --parallel 2
cmake --install build-tls
```

Check the Asio/c-ares option names and toolchain setup against the runtime section above.
The qualified Windows profile is x64 MSVC 19.42 Release `/MD`; `/MT` is rejected.
The Linux profile is x86-64 GCC 13.3. The selected SDK is fixed by the
[profile](../cmake/tls-profile/openssl-3.5.9.json) and its source/SDK manifests;
arbitrary system OpenSSL, automatic acquisition, system roots or another CRT are not substitutes.
Consumer tests do not make the uninstrumented SDK ASan/UBSan-qualified.

An installed consumer calls `find_package(qdcxx CONFIG REQUIRED)` with the relocated prefix and
explicit qualified SDK, then `target_link_libraries(app PRIVATE qdcxx_tls)`.
Add `qdcxx_quote` only when using its prepared Quote schema. SDK/Asio headers and internal runtime
core are not consumer-code dependencies. Repeated `find_package` is checked separately.
The compiled independent consumer uses the complete eight-field Quote, public bootstrap/endpoint/
writer/options, actual TLS and finite retirement; short API declarations in the reference are not
presented as a standalone program.

| Internal executable | Purpose |
| --- | --- |
| `qdcxx_tls_test_peer` | One bounded loopback TLS child owned by the conformance helper; not installed. [Help, codec and lifetime](../tests/conformance/transport/TLS-VALIDATION.md#internal-test-peer). |

Control-v4 (v2/v3 reading is also supported) selects initial write → optional delay → exact read → reply;
each byte script is bounded by 4 MiB and both writes together by 8 MiB. Legacy/CBC/P-521/SHA-1
policies and TLS 1.3 ticket counts 0..2 belong only to the test server and do not widen the client profile.
Report-v2 contains 21 numeric words, including SNI flags and suite/group/hash/tickets/signature-scheme/
signature-size metadata; it emits no certificate/signature contents. On POSIX the parent preserves
waitable `SIGCHLD` without `SIG_IGN`/`SA_NOCLDWAIT` and exclusively reaps its child; known incompatible
pre-start state returns `PROCESS_START`.

## IPF source compression dependency

When ORG_TTLDTOR_QDCXX_BUILD_IPF_TRANSFER=ON, the producer privately builds the existing pinned
zlib1.3.2 profile through [cmake/Zlib.cmake](../cmake/Zlib.cmake), independently of tape tools.
ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE can name an exact local archive; its SHA256 must be
bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16.
The producer also verifies the version header and LICENSE SHA256
e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2.
The source-directory bypass is rejected. With no explicit archive, the selected ON profile uses
the same pinned upstream URL and hash. No system zlib discovery or new version is substituted.

The producer install includes a private qdcxx_ipf_zlib static archive and the upstream Zlib license.
Installed/relocated consumers use that closure without zlib headers, source checkout or a separate
compression SDK. The sealed HTTP/TLS SDK profile is unchanged. Pure IPF/feed with transfer OFF does
not invoke this dependency helper or fetch/discover zlib; the tape compression option retains its
independent meaning.

The [compression contract](../spec/ipf/COMPRESSION.md) specifies finite allocator/work accounting.
The [fixture oracle](../tests/fixtures/ipf/compression/README.md) documents actual JDK8 comparison,
intentional ZIP deviations and EN/RU no-work help. C++ tests replay checked fixtures without Java.

Enable ORG_TTLDTOR_QDCXX_BUILD_IPF_LIVE=ON only with explicit IPF_TRANSFER/TLS prerequisites; it exports qdcxx_ipf_live and the connection header. Pure IPF and default OFF do not acquire a worker or transfer dependency. See [live connection](IPF-CONNECTION.md).
