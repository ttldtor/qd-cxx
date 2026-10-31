# Current capabilities and limits

[Русская версия](CURRENT-STATE.ru.md). English is authoritative.

The project implements selected bounded profiles against Java QD 3.355 at the revision in
[upstream.lock](../upstream.lock). Full QD, dxFeed, QTP or tape compatibility is not claimed.

## Implemented components

The [selected optional runtime profile](decisions/ADR-0006-live-runtime.md) has reproducible
[DNS/TCP/TLS feasibility tests](../tests/conformance/transport/VALIDATION.md).
The separate production TCP adapter implements the bounded plain, credential-free subset below.
TLS/credential adapters and a complete end-to-end subscription client remain unimplemented.
The bounded local HISTORY collector handles admitted source events without owning upstream demand,
a source session or callbacks. The pure bounded MUX component aggregates desired interests across
local clients; it does not own source sessions, wire reconciliation, callback delivery or slow-consumer queues.

| Component | Behavior and contract |
| --- | --- |
| Controlled STREAM client | Optional `qdcxx-connect` and a programmatic example compose the actual plain TCP runtime and dispatcher: [guide](SUBSCRIPTIONS.md). Exact limited CLI selectors, generic records and explicit Quote/Profile summaries; no TLS/reconnect/full-subscription claim. |
| Endpoint options | Typed immutable schema/settings snapshots in optional `qdcxx_client`, independent endpoints, finite budgets/timeouts and runtime/TLS/auth/schema handoff: [native guide](SUBSCRIPTIONS.md#endpoint-options), [configuration](CONFIGURATION.md), [validation](../tests/conformance/subscriptions/OPTIONS-VALIDATION.md). The current application remains credential-free numeric TCP STREAM; storing TLS/DNS/retry settings does not activate them. |
| Local STREAM dispatch | Serialized callbacks, bounded thread-safe commands, detached immutable batches and explicit local close: [contract](../spec/subscriptions/CLIENT-DISPATCH.md). Default OFF; source session/runtime and wire reconciliation remain caller-owned. Local completion does not certify transport quiescence. |
| STREAM collector | Every admitted subscribed record in source order, including identical values; bounded whole-event staging and local membership/revision barriers: [contract](../spec/subscriptions/STREAM-COLLECTOR.md). No callbacks, latest cache or upstream multiplexing. |
| TICKER collector | Shared bounded latest cache, independent local views, explicit retention, initial/live ordering and stale/reset evidence: [contract](../spec/subscriptions/TICKER-COLLECTOR.md). Pure caller-driven delivery; callback dispatch and upstream multiplexing remain separate. |
| HISTORY collector | Pure bounded per-view projection of whole admitted HISTORY events, with shared signed-raw-index storage and requested-range completeness: [contract](../spec/subscriptions/HISTORY-COLLECTOR.md), [validation](../tests/conformance/subscriptions/HISTORY-VALIDATION.md). No source session, callback or upstream-demand ownership. |
| Optional TCP runtime | Explicit SDK/bootstrap, caller-driven DNS/connect/read/write, owned buffers, bounded progress and quiescent close: [contract](../spec/transport/TCP-RUNTIME.md), [DNS resources](../spec/transport/DNS-RESOURCES.md), [validation](../tests/conformance/transport/TCP-VALIDATION.md). Default OFF; no TLS, application callbacks, collector or reconnect. |
| Pure live session | Whole-frame admission, immutable peer generations, protocol/heartbeat controls, readiness/auth gates and owned output: [session contract](../spec/subscriptions/LIVE-SESSION.md). No sockets, clocks, collectors or reconnecting client. |
| Subscription output | Six messages and explicit descriptions, exact symbols, owned partial-write suffixes and retained-output quotas: [live composer](../spec/subscriptions/WRITER.md). No socket/session/peer acknowledgement. |
| Desired subscriptions | Pure bounded per-client state and exact aggregate union, exact symbols/schema identity, atomic add/remove/replace/clear, revisions and non-reused membership tokens: [state API](../spec/subscriptions/STATE.md), [MUX interest contract](../spec/mux/INTERESTS.md), [MUX validation](../tests/conformance/subscriptions/MUX-VALIDATION.md). `qdcxx_mux` exposes the complete desired snapshot and client selection predicate; wire reconciliation, source sessions, callbacks, queues, reconnect, HISTORY collection, and full end-to-end delivery remain separate. |
| Encoding | Compact int/long acceptance, exact nullable UTF-16 strings, owned symbols, finite budgets and terminal errors: [primitives](../spec/encoding/PRIMITIVES.md), [UTFString](../spec/encoding/UTFSTRING.md), [symbols](../spec/encoding/SYMBOLS.md). |
| QTP | Incremental framing, raw descriptions, immutable ordinary local schemas, static binding and complete STREAM_DATA decoding: [framing](../spec/protocol/FRAMING.md), [descriptions](../spec/protocol/DESCRIBE-RECORDS.md), [binding](../spec/protocol/STATIC-BINDING.md), [stream data](../spec/protocol/STREAM-DATA.md). |
| Offline tape | Explicit uncompressed legacy STREAM and RAW_DATA/MESSAGE profiles, append-only descriptions, owned timing, finite work/ownership budgets and completed-prefix terminal errors: [tape reader](../spec/protocol/TAPE-READER.md), [RAW tape](../spec/protocol/RAW-TAPE.md). |
| In-memory tape writer | Bounded RAW_DATA/MESSAGE v1 writer for exact supported layouts and field types, with owned chunks, finite budgets and terminal errors: [writer contract](../spec/protocol/TAPE-WRITER.md), [writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md). |
| Strict tape admission | Whole-input `RAW_MESSAGE_V1` preflight with exact inventory binding, an owned move-only complete-source token, explicit EOF and finite independent work/logical scratch budgets: [admission validation](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.md). |
| Offline replay | Optional synchronous `qdcxx_replay` library over admitted immutable tapes, explicit `IMMEDIATE`/`RELATIVE` scheduling, finite per-session bounds and caller-owned callbacks: [contract](../spec/protocol/OFFLINE-REPLAY.md), [validation](../tests/conformance/replay/VALIDATION.md). |
| Field values | Explicit exact full IDs 2/8/10/40/56/72/88/120; nullable UTF-16 is owned, raw scalar representations are retained: [field types](../spec/encoding/FIELD-TYPES.md). Existing narrower profiles remain available. |
| Binary catalog | Explicit 19-identity finite field model, indexed/time schema state, raw message families, flags and removal metadata: [catalog coverage](../tests/conformance/protocol/real-tape/COVERAGE.md). |
| Finite tape sources | Explicit self-contained occurrence sequence, homogeneous time, optional bounded containers and coordinated parser/source/close/flush completion: [source contract](../spec/protocol/TAPE-SEQUENCE.md), [validation](../tests/conformance/protocol/real-tape/VALIDATION.md). |
| Events | Default Quote plus schema-bound prepared Quote/Profile mappings with explicit local IDs and preserved generic extra/custom fields: [default Quote](../spec/protocol/DEFAULT-QUOTE.md), [configured events](../spec/schema/CONFIGURED-EVENTS.md). |
| Native JSON | Optional separately pinned loader, owned validated schemas, typed errors before tape I/O and generic inspection: [native JSON](../spec/schema/NATIVE-JSON.md). Core, codecs, protocol and event mappings remain JSON-independent. |
| Binary JSON configuration | Separately selected `qdcxx-native-binary` v1, explicit pair/single indexed layouts, owned provenance, independent finite scopes and validate-only CLI: [contract](../spec/schema/NATIVE-BINARY-JSON.md). Existing HISTORY composition remains separate from CLI configuration validation. |
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

## Finite binary tape catalog and owning sources

The explicit `BINARY_CATALOG_V1` profile adds 19 finite field identities, supported
raw message families, indexed/time schema state, flags and removal metadata. It
preserves full identities, physical values and owned payloads rather than applying
business-event conversions. The explicit `SELF_CONTAINED_SEGMENTS_V1` source
profile adds a finite occurrence list, homogeneous NONE/MESSAGE/FIELD/LONG/TEXT
timing, optional bounded GZIP/ZIP and coordinated source closure. Core codecs and
protocol remain independent of files, clocks and transport.

The [catalog coverage](../tests/conformance/protocol/real-tape/COVERAGE.md) accounts
for all 71 rows and keeps unsupported alternatives explicit. The
[named validation](../tests/conformance/protocol/real-tape/VALIDATION.md) separates
production observations, native tests, complete ordinary-corpus comparison and
remaining platform/version limits. That comparison's output9 source profile is
limited to one uncompressed MESSAGE ordinary source; it does not replace the
FIELD/sidecar/container/sequence fixtures.

The new source CLI requires explicit source/time/schema/mode selection. It does
not discover companions or files, follow a live sequence or mix timing policies.
A complete marker requires all source layers, closes and final sink flushes plus
successful process exit. Earlier profiles and output formats 1–8 keep their
separate contracts. Logical resource counters are distinct from measured RSS.

Final native suites passed 74/74 on Windows MSVC Release and Linux GCC Debug
ASan/UBSan; six production-only profiles and dependency gates passed. The
Windows public Java workflow passed 120 checks after a separate fresh 93-check
legacy run. Complete ordinary-source comparisons passed against both exact
parser pins. [Validation](../tests/conformance/protocol/real-tape/VALIDATION.md)
records the source/artifact hashes and preserves the narrower time/OS scope.

Full QD/network/subscription/collector compatibility, additional event adapters,
object deserialization, general archives, live follow/cycle, macOS and a released
version remain outside this named result. Historical reports retain their exact
source/artifact qualification; final integration results are recorded separately.

## Known limits

- Native XML construction, general Java-property resolution and live schema replacement are absent.
  The Java bridge supports only its documented settings and controlled source closure.
- Indexed layouts, additional finite types, options/flags and compression are available only in their explicit catalog/source profiles. Recovery, unrestricted conversions, broader file-publication policies, real-clock integration, replay CLI and network transport remain outside the implemented boundary.
- No adapter beyond the documented Quote/Profile mappings is supplied. Raw timing and field bits
  do not imply business conversions, TICKER delivery or broad source integration. The bounded local HISTORY projection is described separately in the [collector contract](../spec/subscriptions/HISTORY-COLLECTOR.md).
- Complete private-source equality in [remaining gaps](REAL-TAPE-GAPS.md) is limited to its named ordinary MESSAGE profile; broader catalog cells use separate production fixtures.
- macOS, a separate standalone LeakSanitizer certification and hostile concurrent filesystem
  behavior remain unverified. Writer allocation-failure paths are covered by focused fault injection;
  XML preflight is not a general hostile-input sandbox.
- Priority between invalid configuration and simultaneous output failure remains unspecified;
  see [schema-export validation](../tests/conformance/schema-export/VALIDATION.md).

[Open upstream questions](upstream/OPEN-QUESTIONS.md), [configuration requirements](CONFIGURATION.md),
[compatibility policy](COMPATIBILITY.md) and [roadmap](NEXT-STEPS.md) define the remaining boundaries.
