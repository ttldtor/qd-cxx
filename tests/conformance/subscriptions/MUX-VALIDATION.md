# MUX interest validation

[Русская версия](MUX-VALIDATION.ru.md). English is authoritative. This page records validation of the bounded pure desired-interest component. It does not qualify a network client or end-to-end subscription delivery.

## Native matrix

The final native matrix passed:

| Profile | Result |
| --- | --- |
| Windows MSVC 19.42, x64 Release, `CLIENT=ON`, runtime OFF, JSON loader OFF | 44/44 tests |
| GCC 13.3 Debug with ASan and UBSan, `CLIENT=ON`, runtime OFF, JSON loader OFF | 44/44 tests; `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` |
| Focused MUX cases | 16 cases, 537 assertions on each compiler profile |

The final formatting fixed point used Uncrustify 0.83, clang-format 20.1.8 and private fixed-point checks; the Doxygen 1.18 build reported zero warnings, and the four resulting API pages were inspected. These checks cover source/API documentation, not generated Markdown publication.

## Production-only package and consumer

Separate Windows and Linux production configurations used `BUILD_TESTING=OFF`, `CLIENT=OFF`, runtime OFF and JSON loader OFF. Each installed package contained 36 files; all 36 installed and relocated file hashes matched. The independent installed consumer configured, built and ran on both operating systems with exit code 0 and printed `MUX union=1; remaining client selected; revision=5`. Production configure traces had zero package-discovery and external-fetch calls; consumer traces used only `find_package(qdcxx REQUIRED CONFIG)`. No optional Threads or networking SDK discovery/includes occurred. The ordinary compiler and operating-system SDK remain toolchain requirements and are outside the optional-SDK claim.

## Behavioral scope exercised

The focused suite checks exact union across clients A and B, preserving a shared key after one client removes it and deleting it after the last removal. HISTORY uses the signed minimum raw bound across clients and an inclusive per-client selection bound.

Differential cases compare actual core desired-interest state after add, remove, replace, clear, no-op and contradictory-input operations, plus HISTORY-bound upserts. Identity/validation cases include foreign schema provenance, opaque tokens and slot reuse, moves, aliases, empty symbols and zero axes, work/byte limits, and allocation failure. Commands preserve atomic publication on failure.

The Java capture `history-raw-bound-change-aggregate-inclusive` records the aggregate HISTORY lower-bound sequence `4294967298 → 4294967297 → 4294967298 → 4294967299 → -1`, paired with an aggregate REMOVE record count of zero ([production capture profile](../../fixtures/subscriptions/CAPTURES.md#production-observations)). Separately, native cases reproduce these bounds and compare desired maps against actual `core::SubscriptionState`. This native comparison does not constitute a native wire observation or a Java-runtime parity claim.

## Limits of the evidence

`aggregateEntries()` is the complete desired snapshot, not a sent delta, written bytes, peer application or acknowledgment. `select` is a raw interest predicate; it is not HISTORY snapshot-control routing, actual callback fanout or slow-consumer isolation. The exercised state can change while a source is offline. Ownership is serialized; no public concurrency or TSan claim is made. No throughput, RSS, latency or bounded-destruction-time claim follows from these tests.

<a id="mux-queue-validation"></a>

## Local queue validation

Date: 2026-10-31 UTC. Scope: `qdcxx_mux_queue` and the [queue contract](../../../spec/mux/QUEUES.md).
The historical QDCXX-066 pure-interest results above retain their original source/profile scope;
they do not qualify callback queues. The following results concern the final queue implementation
and tests. Native execution and package checks were performed by the implementation author;
documentation verification checks links, exact source extraction and language/ownership claims.

### Executed native matrix

Both full suites used `CLIENT=ON`, runtime OFF, JSON loader OFF and CONNECT OFF. They cover the
complete configured module graph, not a runtime/TLS/JSON-enabled combined application profile.

| Profile | Full CTest | Focused queue suite |
| --- | --- | --- |
| Windows MSVC 19.42, x64 Release | 47/47; exit 0 | 42 cases, 86,529 assertions; exit 0 |
| Ubuntu 24.04 GCC 13.3, x64 Debug ASan+UBSan | 47/47; exit 0 | 42 cases, 88,035 assertions; exit 0 |

ASan used `detect_leaks=0:halt_on_error=1`; UBSan used `halt_on_error=1:print_stacktrace=1`.
LSan was excluded. Platform-dependent allocation-failure sweep counts explain the different
assertion totals. Isolated TSan passed 3 cases/2,851 assertions on the same production implementation
before the final two test-only reserved-control regressions were added. Its scope is producer
handles/controls/results, immutable leases and actual backing release, not concurrent queue-owner
calls or arbitrary application callbacks. No full-suite TSan claim is made.

The pinned Uncrustify 0.83/clang-format 20.1.8 pipeline reached a fixed point and checked all four
selected C++ files, including the installed consumer. Doxygen 1.18 reported zero
warnings; six actual queue API pages were inspected for ownership, serialization, limits, reset and
range evidence. This scoped Doxygen result is not an all-build warning-free claim. GCC Release
package builds retain an existing `wide_decimal.cpp` warning outside queue changes.

### Behavioral and resource evidence

[Queue tests](../mux_queue_tests.cpp) distinguish accepted commands, authoritative `APPLIED`, derived
source synchronization and callback completion. They exercise real STREAM/TICKER/HISTORY collectors,
healthy-client atomic routing, slow/full/paused clients, reserved STOP/RESUME/exact reset ACK,
phased reset, HISTORY mapping/epoch/range guards, stale generations, retained results/batches,
last-handle release and callback/retirement reentrancy. The range-query regression checks upfront
queued-symbol matching work, exact/one-short budgets and retry without silent credit increases.

The deterministic family workload ran 300 source frames per family with source-turn record quota 8,
two healthy clients and 11 churn operations per run. Actual pure-source comparison outputs contained
900 STREAM, 300 TICKER and 301 HISTORY records. These are finite observed workloads, not evidence
for larger proposed runs or a throughput/latency target.

Resource checks include STL object/control requests, fixed/old/candidate peak backing, nth-allocation
rollback, work axes and deallocation before uncharging. An allocation-pause probe checks that actual
record/batch/generation backing remains charged until released. Exception-transport/runtime
infrastructure, physical allocator headers, arbitrary consumer captures and independently enforced
source/MUX/core/session storage remain separate scopes. Requested byte observations do not establish RSS.

### Installed and relocated consumers

Fresh production-only Windows and Linux CLIENT ON builds used runtime/JSON/CONNECT OFF. Each moved
package had 42 files, all verified against its installation inventory. The independent consumer
configured, built and exited 0 on both operating systems using relocated imported targets.
The [documented integration fragment](../../../spec/mux/QUEUES.md#compiled-stream-integration-example)
is extracted verbatim from that compiled consumer. Its actual ready `Sample`/`ALPHA` source has
three full type-8 compact-int32 fields, pair layout 0/1 and two identical STREAM rows, time 1/value 10.
It retains immutable callback backing, requests graceful stop, reaches local terminal, then reads
and releases the retained batch on another thread after queue/callback-owner destruction.
It does not test a standard eight-field Quote schema or a Java HISTORY producer.

Fresh pure CLIENT OFF/runtime OFF/JSON OFF/CONNECT OFF production packages were also moved. Each
contained 37 verified files. Both installed consumers exited 0 after real `MuxState`
create/createClient/add/select/clear operations. Threads discovery was disabled; an unused bogus
C compiler setting was tolerated. Queue/client targets and headers were absent, and runtime,
OpenSSL and networking SDK dependencies were not acquired. The four moved prefixes contain
42+42+37+37 = 158 verified files; original installation copies are not counted again.

No errors were detected in these executed checks. Native constructed delivery is not a new Java
queue/HISTORY wire oracle, full subscription facade, transport-stop protocol, throughput, RSS,
latency or bounded wall-clock destruction proof. Final integration review, owner acceptance and
release remain separate from this scoped execution record.
