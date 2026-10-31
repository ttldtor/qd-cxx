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
