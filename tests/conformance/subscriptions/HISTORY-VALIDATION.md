# HISTORY collector validation

[Русская версия](HISTORY-VALIDATION.ru.md). English is authoritative.

This page records bounded HISTORY qualification in the combined revision containing the accepted pure MUX component. It does not establish a complete subscription client or wider QD compatibility.

## Java reference and comparison scope

The published [subscription capture profile](../../fixtures/subscriptions/CAPTURES.md) contains 87 cases and 10,051 observation rows, including 33 collector cases and 6,983 observations. Fifteen timed HISTORY delivery cases compare actual post-decoder input, local commands, projected outputs/counts and selected controls. Aggregate-demand `*-add`/`*-remove` phases, cipher/raw-symbol encoding and helper-driven extra sink draining are outside that comparison. Two wildcard/untimed cases are separate native refusal checks; they do not assert Java exception or API parity.

The final source-bound cross-platform comparison for the frozen four-file source set covers 2,082 coordinates across those 15 timed cases and 61 output phases; all 165,388 native output bytes matched between Windows and Linux. It does not establish parity for all 17 HISTORY cases.

The collector consumes whole type-20 events only after a matching [LiveSession](../../../spec/subscriptions/LIVE-SESSION.md) admits them. Session descriptors/control behavior and transport are separate protocol evidence; they are not counted as HISTORY collector delivery cases. The compiled example below exercises that caller-owned boundary with a controlled native frame, not a Java-produced HISTORY Quote/Profile stream.

## Native qualification

| Profile | Result |
| --- | --- |
| Windows MSVC 19.42 x64 Release, CLIENT ON, runtime/JSON/CONNECT OFF | Full CTest: 45/45 passed in 76.21 s; focused HISTORY tests: 30/30 cases and 27,576/27,576 assertions |
| GCC 13.3 Debug ASan+UBSan, CLIENT ON, runtime/JSON/CONNECT OFF | Full CTest: 45/45 passed in 111.94 s; focused HISTORY tests: 30/30 cases and 27,576/27,576 assertions. `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`; `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; LSan disabled. |
| Isolated token TSan case | 1 case, 33 assertions; four workers each copied/released 10,000 detached token handles while the owner retired. It covers token-marker lifetime only, not collector/protocol lease transfer or concurrent owner access. |

The production-only Windows and Linux packages were configured with `BUILD_TESTING`, CLIENT, runtime and JSON disabled. Each manifest lists 37 installed files under `lib64` and public includes. A consumer configured against each relocated install and exited zero; the pure package did not discover Threads. This verifies the installed `qdcxx_collectors` header/library closure for these two profiles, not arbitrary toolchains or an all-platform release.

Pinned Uncrustify 0.83.0 followed by clang-format 20.1.8 passed Format and Check; four C++ files reached byte-stable private-copy fixed points. Doxygen 1.18.0 completed with zero warnings, and five generated API pages were inspected. This Doxygen result concerns source API pages, not generated Markdown publication.

## Behavior exercised and evidence limits

Native cases compare per-view indexed projections, explicit requested-range completeness, and actual admitted session events. A covered range for one view can be complete while the partition's separate source snapshot remains `RECEIVING` for a lower bound. Shared admission and local view failure are tested separately: old-plus-incoming peaks are charged before retirement, a later shared failure cannot publish an earlier candidate view failure, and view failures commit atomically while healthy views remain isolated. Work/allocation failure preserves prior offers and caller input ownership. `forgetUnobserved` explicitly reclaims eligible empty metadata for churn; the native clock-free policy does not claim Java's opportunistic 60-second expiry.

The ordinary indexed-point MAX rejection and MAX REMOVE virtual boundary are source-backed native policies with boundary tests, not executed Java MAX-record parity. Standard untimed Quote/Profile schemas are rejected for HISTORY. Separate caller-indexed native envelopes extend complete standard schemas with time indexes: all 8 Quote or 16 Profile materialized fields (including missing-field defaults) are checked, and existing prepared mappings succeed. This does not claim a Java HISTORY Quote/Profile producer. Generic native checks cover null versus empty, exact NUL and unpaired UTF-16, float and raw scalar values, and opaque byte/custom/marshalled payloads after source destruction; arbitrary business/custom deserialization is outside scope.

No callback, source/business-ID decoding, live transport, end-to-end collector/MUX event fanout, throughput/RSS/latency measurement or bounded destruction-time claim follows from this validation. Requested logical bytes are not process RSS. The results cover the stated combined build and installed-package profiles; the pure desired-interest union has its separate [MUX qualification](MUX-VALIDATION.md).
