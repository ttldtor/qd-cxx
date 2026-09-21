# QDCXX-012 executed C++ validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 UTC after the [Java P02 evidence gate](../../fixtures/protocol/p02/VALIDATION.md).
[Coverage](COVERAGE.md) records exact selection and limitations. No production dependency was added;
qd_protocol links inward to qd_encoding, and doctest remains test-only.

## Results

| Configuration | Result and retained UTC evidence |
| --- | --- |
| MSVC 19.42.34444.0, x64, VS 2022 / SDK 10.0.26100.0, Debug | Both CTest suites pass; build, test. |
| Same MSVC, Release | Both pass; build, test. |
| ClangCL 19.1.5, same MSVC STL/SDK, Debug | Both pass; build, test. |
| Same ClangCL, Release | Both pass; build, test. |
| WSL Ubuntu 24.04, GCC 13.3.0 / libstdc++, Debug | Both pass; build, test. |
| Same GCC, Release | Both pass; build, test. |
| GCC Debug, ASan+UBSan, frame pointers | Both pass without diagnostics; build, test. |
| Fresh BUILD_TESTING=OFF, GCC | Both libraries build, no _deps directory/fetch; configure, build. |

[Compiler/cache settings](configurations.json) retain exact selected configurations. Existing builds
regenerated CMake when CMakeLists changed; cached pinned doctest 2.4.12 was reused. C++23 is inherited
from qd_encoding; extensions are disabled on production and test targets. All seven requested
compiler/configuration checks were available. Sandbox SDK/WSL access initially failed; the same
checks succeeded with authorized elevated access. No source fix was needed for those access errors.

The framing executable passes three cases / 246,166 assertions (summary);
primitive regressions pass four cases / 43,375 assertions / 1146 Java observations
(output). [Negative proof](negative-proof.json) records baseline
success and five deliberate fixture/projection failures, with expected exit codes and unchanged
hashes; log. No real golden was mutated.

C++ Format then Check passed the pinned Uncrustify 0.83.0 → clang-format 20.1.8 pipeline on eight
files. Java Format preceded the two final captures; Check passes four sources using the separate
JDK 25 formatter. Final checks record formatting, evidence audit,
relative-link validation and git diff --check. Doxygen, naming, arithmetic, span bounds, ownership,
EN/RU claims and dependency direction were reviewed separately from formatting.

## Reproduction

The standard [build instructions](../../../docs/CPP-BUILD.md) run both suites. CMake cache options
QD_PROTOCOL_FIXTURES and QD_FRAME_PROJECTIONS permit temporary negative inputs without editing
real expectations. For a direct negative check:

```powershell
python tools/run_utc.py --log .work/framing-negative.log -- python tests/conformance/check_frame_negative.py .work/msvc/Release/frame_tests.exe --report .work/framing-negative-proof.json
```

WSL sanitizer build uses `-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`,
Debug and the existing pinned doctest source cache, as in CPP-BUILD. Every retained command log is
created by tools/run_utc.py, without relabeling historical timestamps. Java metadata/builds remain
separate from C++ frame projections and explicit lifecycle/resource policies.

## Limits

API is provisional. No transport, payload parser, frame writer or new protocol/session component.
No huge allocations, allocation-failure injection, uint64 exhaustion, 32-bit/macOS or alternate
Java buffer/error-policy combinations were run. The exact output payload size is bounded by body
budget minus type; allocator/vector bookkeeping is not an exact heap-footprint promise. Reader
storage is fixed body+9 byte elements; retained caller-owned frames are outside its aggregate bound.
Historical oracle expectations/captures and upstream.lock remain byte-identical; new P02 is separate.
