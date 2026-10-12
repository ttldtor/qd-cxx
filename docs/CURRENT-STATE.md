# Current capabilities and limits

[Русская версия](CURRENT-STATE.ru.md). English is authoritative.

The project implements selected bounded profiles against Java QD 3.355 at the revision in
[upstream.lock](../upstream.lock). Full QD, dxFeed, QTP or tape compatibility is not claimed.

## Implemented components

| Component | Behavior and contract |
| --- | --- |
| Encoding | Compact int/long acceptance, exact nullable UTF-16 strings, owned symbols, finite budgets and terminal errors: [primitives](../spec/encoding/PRIMITIVES.md), [UTFString](../spec/encoding/UTFSTRING.md), [symbols](../spec/encoding/SYMBOLS.md). |
| QTP | Incremental framing, raw descriptions, immutable ordinary local schemas, static binding and complete STREAM_DATA decoding: [framing](../spec/protocol/FRAMING.md), [descriptions](../spec/protocol/DESCRIBE-RECORDS.md), [binding](../spec/protocol/STATIC-BINDING.md), [stream data](../spec/protocol/STREAM-DATA.md). |
| Offline tape | Explicit uncompressed legacy STREAM and RAW_DATA/MESSAGE profiles, append-only descriptions, owned timing, finite work/ownership budgets and completed-prefix terminal errors: [tape reader](../spec/protocol/TAPE-READER.md), [RAW tape](../spec/protocol/RAW-TAPE.md). |
| In-memory tape writer | Bounded RAW_DATA/MESSAGE v1 writer for exact supported layouts and field types, with owned chunks, finite budgets and terminal errors: [writer contract](../spec/protocol/TAPE-WRITER.md), [writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md). |
| Strict tape admission | Whole-input `RAW_MESSAGE_V1` preflight with exact inventory binding, an owned move-only complete-source token, explicit EOF and finite independent work/logical scratch budgets: [admission validation](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.md). |
| Offline replay | Optional synchronous `qdcxx_replay` library over admitted immutable tapes, explicit `IMMEDIATE`/`RELATIVE` scheduling, finite per-session bounds and caller-owned callbacks: [contract](../spec/protocol/OFFLINE-REPLAY.md), [validation](../tests/conformance/replay/VALIDATION.md). |
| Field values | Explicit exact full IDs 2/8/10/40/56/72/88/120; nullable UTF-16 is owned, raw scalar representations are retained: [field types](../spec/encoding/FIELD-TYPES.md). Existing narrower profiles remain available. |
| Events | Default Quote plus schema-bound prepared Quote/Profile mappings with explicit local IDs and preserved generic extra/custom fields: [default Quote](../spec/protocol/DEFAULT-QUOTE.md), [configured events](../spec/schema/CONFIGURED-EVENTS.md). |
| Native JSON | Optional separately pinned loader, owned validated schemas, typed errors before tape I/O and generic inspection: [native JSON](../spec/schema/NATIVE-JSON.md). Core, codecs, protocol and event mappings remain JSON-independent. |
| Java schema export | Offline resolved settings/XML sources, complete actual lookup keys, explicit logical storage-slot projection and failure-safe native JSON publication: [export contract](../spec/schema/JAVA-EXPORT.md), [usage](../tools/schema-export/README.md). Existing C++ consumers use saved outputs without Java. |
| Utilities | Optional `qdcxx-qtp-inspect`, `qdcxx-tape-inspect` and strict offline `qdcxx-tape-write`, finite budgets, explicit profiles and create-only output publication: [build/API](CPP-BUILD.md), [QTP inspection](QTP-INSPECT.md), [tape inspection](TAPE-INSPECT.md), [tape writing](TAPE-WRITE.md), [Profile inspection](PROFILE-INSPECT.md). |
| Developer tools | Native UTC logging and portable CMake formatting; ordinary configure/build/CTest does not require Python or PowerShell: [instructions](../tools/dev/README.md). Test-only dependencies and the JSON loader retain separate opt-in boundaries. |

## Verification evidence

[Schema-export validation](../tests/conformance/schema-export/VALIDATION.md) records the most recent
schema-export integration. The in-memory writer's final integration evidence is recorded in
[writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md): Windows MSVC
x64 Release passed 47/47 CTest tests; Linux GCC Debug ASan+UBSan passed the two affected writer and
char-array tests on the final source, while its 47/47 full-suite run used the initial snapshot
before the `textUnits` review correction. The final-source Linux production-only Release builds
passed with tools disabled and enabled (JSON disabled in both). Java 8 FileReader parsed all five
final native-emitted cases: 18 STREAM records, 54 callback projections, 152 rows and zero parser
errors. These results do not establish macOS, strict file publication or full tape support.

The report separates Java production observations from diagnostic projection and C++ conformance.
Earlier component reports retain their own executed profiles and limits. The formatter, capture
and source hashes describe their recorded revisions; they are not assertions about future inputs.

QDCXX-034 admission and CLI evidence is recorded separately in
[admission validation](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.md) and its
hash-indexed archive.
Windows MSVC x64 Release passed 50/50 full CTest tests; two Windows production-only profiles passed
with JSON OFF, including one with the optional tape-write and inspection tools enabled. Five final
native rewrites were read by the locked Java production FileReader (18 STREAM records, 54 callback
projections, 152 rows, no parse errors). Focused Linux GCC ASan+UBSan file-boundary tests passed
12/12; the final Linux Debug full suite passed 50/50 under Ubuntu 24.04 / WSL2 with GCC 13.3, ASan
and UBSan (CTest-reported 865.93 seconds). Both Linux production-only Release profiles built successfully with JSON disabled: minimal tools OFF, and the QTP inspection, tape inspection, and tape-write tools ON. The final checks confirmed that neither profile created `_deps`; see the dependency records in the admission archive. Both builds emitted the existing GCC `-Wstringop-overflow` warning at `src/core/wide_decimal.cpp:68` and exited successfully.
The C++ format check passed for 97 files. Doxygen generated the admission API page without warnings; its
`EXTRACT_ALL` setting does not establish complete API documentation. These results do not certify
macOS, hostile filesystem races or crash durability.

Replay validation records 9 cases / 1,303 assertions in the final focused Windows target run and a separate independent run. A standalone consumer compiled and ran with the documented macro guards. Full Windows and Linux sanitizer suites and production-only profiles passed. See [replay validation](../tests/conformance/replay/VALIDATION.md) for the recorded results and limits.

## Known limits

- Native XML construction, general Java-property resolution and live schema replacement are absent.
  The Java bridge supports only its documented settings and controlled source closure.
- Indexed schemas, additional full types/conversions, broader options/event flags, dynamic sessions,
  recovery/compression, broader file-publication policies, real-clock integration, replay CLI and network transport remain outside implemented profiles.
- No adapter beyond the documented Quote/Profile mappings is supplied. Raw timing and field bits
  do not imply business conversions or ticker/history delivery.
- Private aggregate tape observations in [remaining gaps](REAL-TAPE-GAPS.md) do not establish full-file support.
- macOS, a separate standalone LeakSanitizer certification and hostile concurrent filesystem
  behavior remain unverified. Writer allocation-failure paths are covered by focused fault injection;
  XML preflight is not a general hostile-input sandbox.
- Priority between invalid configuration and simultaneous output failure remains unspecified;
  see [schema-export validation](../tests/conformance/schema-export/VALIDATION.md).

[Open upstream questions](upstream/OPEN-QUESTIONS.md), [configuration requirements](CONFIGURATION.md),
[compatibility policy](COMPATIBILITY.md) and [roadmap](NEXT-STEPS.md) define the remaining boundaries.
