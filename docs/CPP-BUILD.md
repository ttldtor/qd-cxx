# Bounded C++23 components

[Русская версия](CPP-BUILD.ru.md). English is authoritative.

`qd_encoding` is a dependency-free C++23 production library with provisional compact int/long
and UTFString APIs. QDCXX-010 introduced compact; QDCXX-011 adds
[utf_string.hpp](../include/qd/encoding/utf_string.hpp). The unchanged Java
[primitive evidence](../spec/encoding/PRIMITIVES.md) defines compatibility;
[UTFString policy and reuse decision](../spec/encoding/UTFSTRING.md) define explicit C++ bounds.
PentaCodec, importer, schema runtime, event payload decoding, frame writing, transport and packaging remain outside scope.

## DESCRIBE_RECORDS API and QDCXX-013 checks

Include `qd/protocol/describe_records.hpp` and link qd_protocol. Call
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

CTest adds protocol_java_descriptions; description_tests links only qd_protocol and test-only doctest.
QD_DESCRIPTION_FIXTURES defaults to the additive P08 descriptions.tsv and permits temporary negative
selection. All existing commands below include the new target. [Coverage/results](../tests/conformance/protocol/descriptions/VALIDATION.md)
record 81 Java observations, 50,841 new assertions, MSVC/ClangCL/GCC Debug/Release, GCC ASan+UBSan,
BUILD_TESTING=OFF with no acquisition, regressions and immutable negative checks. API is provisional;
event payloads, local binding, transport and writer remain outside scope.

## QTP frame reader API and QDCXX-012 checks

`qd_protocol` is a production component without external dependencies, linking only qd_encoding.
Include `qd/protocol/frame_reader.hpp` and link qd_protocol. Construct
`qd::protocol::FrameReader reader({1024, 1033});`: these are body and pending budgets, allowing
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

Commands below now build both libraries and also run frame_tests/protocol_java_frames. New tests
read historical P01 and separately added P02; QD_PROTOCOL_FIXTURES and QD_FRAME_PROJECTIONS permit
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
`compact_tests`/`compact_java_vectors` for compatibility with existing tooling; they execute four
cases, 43,375 assertions and all 1146 Java observations (314 UTFString, 832 compact), zero exclusions.
MSVC, ClangCL and WSL GCC pass Debug/Release; GCC ASan+UBSan also passes. Historical QDCXX-010 counts
below describe the earlier compact-only run. `BUILD_TESTING=OFF` still fetches nothing; no production
UTF dependency was selected. Reuse probes are optional, separate ignored projects.
[Current validation](../tests/conformance/encoding/VALIDATION.md) retains UTC evidence.

Additional WSL sanitizer commands from the repository root (cached pinned doctest):

```bash
python3 tools/run_utc.py --log .work/utf-asan-configure.log -- cmake -S . -B .work/utf-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' -DFETCHCONTENT_SOURCE_DIR_DOCTEST=/work/qd-cxx/.work/linux-debug/_deps/doctest-src
python3 tools/run_utc.py --log .work/utf-asan-build.log -- cmake --build .work/utf-asan -j 4
python3 tools/run_utc.py --log .work/utf-asan-test.log -- ctest --test-dir .work/utf-asan --output-on-failure
```

## Build and test on Windows

Executed with CMake 4.4.2, generator `Visual Studio 17 2022`, x64, Windows SDK 10.0.26100.0,
MSBuild 17.14.51, MSVC 19.42.34444.0 (tool directory 14.42.34433). The second compiler is the
installed ClangCL 19.1.5 using the same MSVC standard library/SDK. Tests use doctest 2.4.12 via
CMake FetchContent, pinned by archive URL and SHA-256 in [CMakeLists.txt](../CMakeLists.txt).
Only `compact_tests`, `frame_tests` and `description_tests` link doctest; `BUILD_TESTING=OFF` neither downloads nor links it. Downloads
are stored under the ignored build directory `_deps/`; the first test configure needs network access.
No package manager is needed. doctest 2.4.12 emits a non-fatal CMake deprecation warning for its
old minimum policy version; the downloaded source is unmodified. CMake requires 3.25+; each target inherits `cxx_std_23` from the library,
with extensions disabled. On these toolsets CMake selects `/std:c++latest` for C++23 support.

From the repository root in PowerShell:

```powershell
python tools/run_utc.py --log .work/msvc-configure.log -- cmake -S . -B .work/msvc -G "Visual Studio 17 2022" -A x64
python tools/run_utc.py --log .work/msvc-debug-build.log -- cmake --build .work/msvc --config Debug
python tools/run_utc.py --log .work/msvc-debug-test.log -- ctest --test-dir .work/msvc -C Debug --output-on-failure
python tools/run_utc.py --log .work/msvc-release-build.log -- cmake --build .work/msvc --config Release
python tools/run_utc.py --log .work/msvc-release-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
python tools/run_utc.py --log .work/clang-configure.log -- cmake -S . -B .work/clang -G "Visual Studio 17 2022" -A x64 -T ClangCL
python tools/run_utc.py --log .work/clang-debug-build.log -- cmake --build .work/clang --config Debug
python tools/run_utc.py --log .work/clang-debug-test.log -- ctest --test-dir .work/clang -C Debug --output-on-failure
python tools/run_utc.py --log .work/clang-release-build.log -- cmake --build .work/clang --config Release
python tools/run_utc.py --log .work/clang-release-test.log -- ctest --test-dir .work/clang -C Release --output-on-failure
```

Build outside the source root; `.work/` and `build/` are ignored. In-source configure is rejected.
`BUILD_TESTING=OFF` builds only qd_encoding and qd_protocol libraries. `QD_ENCODING_FIXTURES` may point at temporary fixture
copies for negative checks; it defaults to the checked-in expectations. Initial sandboxed MSBuild
SDK discovery was denied access to the local Microsoft SDKs directory; the authorized configure/build
runs above succeeded with sandbox elevation. This is an environment restriction, not a codec failure.

## API and ownership

All four functions are in `qd::encoding`, borrow caller-owned `std::span` storage only for the call,
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

Also executed in WSL2 Ubuntu 24.04.4 LTS, x86_64, kernel 6.18.40.1-microsoft-standard-WSL2,
GCC 13.3.0 (libstdc++), CMake 4.4.2 and Ninja 1.11.1. Debug and Release pass the same two doctest
cases (28,358 assertions) and unchanged 832 selected Java observations. Library-only configure/build
with `BUILD_TESTING=OFF` passes without creating `_deps/`. Linux negative fixture/parser/CTest checks
pass; `--skip-format` explicitly omits the Windows-only formatter checks already run on Windows.
Sanitizers, other Linux distributions, macOS and other architectures were not checked.

From Ubuntu WSL (`wsl -d Ubuntu-24.04`), at the mounted repository:

```bash
cd /work/qd-cxx
for config in Debug Release; do
  build=.work/linux-${config,,}
  python3 tools/run_utc.py --log "$build-configure.log" -- cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$config"
  python3 tools/run_utc.py --log "$build-build.log" -- cmake --build "$build"
  python3 tools/run_utc.py --log "$build-test.log" -- ctest --test-dir "$build" --output-on-failure
done
python3 tools/run_utc.py --log .work/linux-negative.log -- python3 tests/conformance/check_compact_negative.py .work/linux-release/compact_tests --skip-format --report .work/linux-negative-proof.json
cmake -S . -B .work/linux-library -G Ninja -DBUILD_TESTING=OFF
cmake --build .work/linux-library
```

[run_utc.py](../tools/run_utc.py) requires Python 3.11+, timestamps every retained output line and
command start/exit in ISO 8601 UTC (`Z`) and sets `TZ=UTC` for child processes. This also makes
CTest's generated LastTest.log use UTC. It preserves child output text and exit status; it does not
change the host timezone. Current C++ logs were freshly generated in UTC, not relabeled; historical
Java oracle captures keep their original bytes/timezones. New JSON negative reports include `executedAtUtc`.
Public functions and test utilities now document each parameter/template parameter and non-void result.
