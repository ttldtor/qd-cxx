# Desired-state validation

[Русская версия](STATE-VALIDATION.ru.md). English is authoritative.

Executed on 2026-10-31 against the [bounded desired-state API](../../../spec/subscriptions/STATE.md).

| Profile | Executed result |
| --- | --- |
| Windows MSVC 19.42.34444 x64 Release | Build passed; affected minimal CTest profile 38/38. |
| Ubuntu 24.04 GCC 13.3 Debug, ASan+UBSan | Build passed; affected minimal CTest profile 38/38; detect_leaks=0, halt_on_error=1. |
| Focused core_subscription_state | Both profiles: 15 doctest cases, 484 assertions passed; no sanitizer error detected in these checks. |
| Separate source consumer | Complete documented example compiled/linked/ran on both profiles using only qdcxx_core, BUILD_TESTING=OFF and JSON=OFF; no dependency source tree fetched. |
| Formatting | Uncrustify 0.83.0 then clang-format 20.1.8, Format and Check on changed C++ sources. |
| API reference | Doxygen 1.18.0 generated header reference with zero warning-log bytes; state/limits/key/work/membership pages inspected. |

The minimal profiles leave JSON, replay, inspector/writer tools and compression OFF; this is
not the full enabled-feature acceptance matrix. Existing doctest 2.4.12 sources were reused
without acquisition. Native UTC logging and isolated build directories were used.
LeakSanitizer was disabled under the previously recorded Linux environment exclusion.
No macOS, ThreadSanitizer, transport, callback dispatch or collector qualification is claimed.

Scenarios cover non-null empty/null/reserved/exact UTF-16, duplicates/no-op, atomic replace/clear,
absent removes, HISTORY raw bounds/indexed requirements, schema copies/foreign identities,
membership ABA/move, resident/peak capacities, work exhaustion/no refund/new turns, aliases
into current storage and actual allocation-failure rollback. A dedicated test executable scopes
its global-new failpoint to API calls and disables it before assertions.

Independent contract review found that HISTORY removal incorrectly accepted ordinary records
without indexed metadata. Both absent-key and mixed valid/invalid removal regressions were
observed failing on the previous code before the fix. The final implementation validates every
HISTORY key before publishing any removal; regressions and all focused/full checks then passed.
The clear documentation was narrowed to no candidate allocation on successful clear, without an
allocation-free error/control-path promise.

Natural uint64 revision/activation exhaustion is guarded and source-reviewed, not practically
exhausted in runtime tests. Array/UTF-16 capacities are logical requested storage, not allocator/
exception overhead, an operating-system memory cap or a measured RSS guarantee. Fixed retained
token/schema identity and later aggregate runtime quotas remain distinct.

The first Windows sandbox compiler-ABI probe failed with C1902 before project compilation;
the same SDK/configuration succeeded with the necessary process access, without installation
or global changes. An initial direct clang probe lacked SDK include setup and is not behavioral
regression evidence. These environment failures were kept separate from the observed code regression.
