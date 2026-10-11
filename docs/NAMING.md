# Project identity and CMake migration

[Русская версия](NAMING.ru.md). English is authoritative.

The owner selected `qdcxx`, scoped to `ttldtor.org`. The naming policy covers C++ namespaces,
CMake project/targets and project-owned cache variables/options. Public include paths remain
`qd/...`; the executable is now `qdcxx-qtp-inspect`, with unchanged TSV/human identifiers and bytes.
No forwarding namespace aliases or duplicate legacy targets/options are introduced.

| Surface | Current name |
| --- | --- |
| Namespace root | `org::ttldtor::qdcxx` |
| Components | `org::ttldtor::qdcxx::core`, `::encoding`, `::protocol`, `::inspect` |
| CMake project | `qdcxx` |
| Libraries | `qdcxx_core`, `qdcxx_encoding`, `qdcxx_protocol` |
| Tool library / executable target | `qdcxx_inspect_tool` / `qdcxx_qtp_inspect` |
| Executable filename | `qdcxx-qtp-inspect` (selected through OUTPUT_NAME) |
| Test executable targets | `qdcxx_compact_tests`, `qdcxx_frame_tests`, `qdcxx_description_tests`, `qdcxx_binding_tests`, `qdcxx_symbol_tests`, `qdcxx_stream_data_tests`, `qdcxx_offline_io_tests`, `qdcxx_human_io_tests` |
| Tool option | `ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT` (default OFF) |
| Fixture cache variables | `ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES`, `ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS`, `ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES`, `ORG_TTLDTOR_QDCXX_BINDING_FIXTURES`, `ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES` |

CTest case names and source filenames stay unchanged. Standard/dependency identifiers such as
`BUILD_TESTING`, `CMAKE_*`, `Python3_*` and `doctest::doctest` are not project prefixes.
Future project-owned targets use `qdcxx_`; public configuration/macros use `ORG_TTLDTOR_QDCXX_`.
The [C++ style contract](CPP-STYLE.md) also documents this rule.

This is a source/build interface break: update namespace references and target/cache names in
consumers, then rebuild. Old QD_* cache entries do not control the new options; use a fresh build
directory or remove obsolete entries, and pass the new option explicitly. No source/binary ABI
compatibility is claimed for the renamed C++ symbols. Header include paths need no edits.

```cpp
#include "qd/core/local_schema.hpp"
// The header path is intentionally unchanged.
using org::ttldtor::qdcxx::core::LocalSchema;
```

```powershell
cmake -S . -B .work/qdcxx -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON
cmake --build .work/qdcxx --config Release
ctest --test-dir .work/qdcxx -C Release --output-on-failure
```

Historical task text, captures, validation logs and source-hash snapshots retain their original
names and commands; they describe earlier revisions. Current build examples use the new names.
Java QD/package names, protocol/schema semantics and fixtures remain untouched. This change does
not rename the repository directory or add tape/transport support. [New validation](../tests/conformance/naming/VALIDATION.md)
records checks against the renamed sources separately from historical evidence.

## Utility executable filenames

The owner extended the migration to utility filenames: `qdcxx-qtp-inspect` replaces `qd-qtp-inspect`.
Use `qdcxx-` and hyphen-separated words for future user-facing utility binaries; keep CMake targets
as `qdcxx_*` and set OUTPUT_NAME. Internal test binaries retain their qdcxx_ target names.
Update shell commands/shortcuts and rebuild. No legacy alias binary is built. Old binaries may remain
in reused build directories; their presence does not mean they were rebuilt. Version 1 TSV/human
headers still contain `qd-qtp-inspect` as the fixed format identifier, preserving exact output bytes.
Help advertises the new executable name. [Validation](../tests/conformance/executable-naming/VALIDATION.md).
