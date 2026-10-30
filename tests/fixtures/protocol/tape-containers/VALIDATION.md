# Container component validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Status: bounded component, optional build integration and scoped independent review verified in the
profiles below. Operational CLI composition and final catalogue acceptance remain separate; this
does not establish layered inspection COMPLETE or release acceptance. The [contract](../../../../spec/protocol/TAPE-CONTAINERS.md) and
[production captures](CAPTURES.md) define the bounded claim.

Author checks on 2026-10-30:

| Check | Actual profile/result |
| --- | --- |
| Native focused CTest | Windows MSVC 19.42 Release, static zlib 1.3.2: 14 cases, 130644 assertions, no failures detected |
| Native focused CTest | Linux GCC 13.3 Debug ASan/UBSan, including zlib: 14 cases, 130639 assertions, no failures detected |
| C++ formatting | Uncrustify 0.83.0 then clang-format 20.1.8 Format and Check on all three new sources |
| Documentation | Strict Doxygen on new header/cpp, zero warnings after documenting private ownership/state |
| Java production | Both pins: Capture, repeated Capture and Verify; unchanged 14-root inventories |
| Launcher | PowerShell syntax, explicit manual CMake help gate, no-argument/help byte equivalence, invalid-argument nonzero exit |
| Private dependency gate | Windows/Linux OFF and ON-without-consumer ignore poisoned dependency paths and do not enable C/create the target; ON validates archive/version/license and builds; bad hash/source override fail |
| Full configured project CTest | Windows MSVC Release63/63; Linux GCC Debug ASan/UBSan63/63, with the C compression backend instrumented |
| Production-only builds | Three profiles per OS: minimal, tools/replay with JSON OFF, tools/replay with JSON ON; compression OFF |
| Actual root dependency profiles | Fresh OFF/consumer, ON/no-consumer and ON/production-consumer builds on both OS; wrong archive and source override rejected for the expected reasons without invalid acquisition |
| Independent component review | Final native ownership/state/resource/frame contracts; copied Windows/Linux14-case binaries and separate EOCD classification/metadata-allocation probes |

The private gate checked the dependency block in isolation; subsequent fresh root-build profiles
checked the integrated block. On Windows CXX configure also enables the normal RC language; C remains
absent from OFF/no-consumer states. The header's C++23 requirement is PUBLIC and is consumed by the
native test target. The manual PowerShell help gate is outside normal configure/build/CTest.

Native cases cover production NONE/GZIP/ZIP equality, existing standard-Quote byte preservation,
one-byte physical/output chunks, empty/concatenated members, optional gzip headers/FHCRC, every
truncated prefix of small gzip/zip inputs, CRC/ISIZE, late source failures, descriptor/directory/EOCD
closure, multiple files, directories with payload, ZIP64/encryption/method refusals and trailing
bytes. Zero, exact/excess and SIZE_MAX configurations exercise physical/inflated/header/work/call/
entry/allocation/ratio bounds, including no maximum-count preallocation. Actual source-operation
counters verify no reads after terminal outcomes or destruction; byte counters alone would miss
zero-byte probes. `finish` and empty-span contracts are checked separately.

Physical read-ahead and logical progress remain distinct even when a caller requests one logical
byte. Actual source-call counters verify lifetime/stop behavior independently of returned byte counts.
No sanitizer errors were detected in the stated checks.

The developer catalog and separate manual oracle help gate are registered; native configure/build/
CTest retains its interpreter-free contract. Final CLI/parser/companion/split/close/flush composition
and full catalogue acceptance belong to their separate integration.
No installed runtime, private-market tape or universal ZIP acceptance is claimed.
