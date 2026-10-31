# Subscription writer validation

[Русская версия](WRITER-VALIDATION.ru.md). English is authoritative.

Executed on 2026-10-31 for the [live composer](../../../spec/subscriptions/WRITER.md).
Windows MSVC 19.42.34444 x64 Release and Ubuntu 24.04 GCC 13.3 Debug ASan+UBSan each passed
the affected minimal CTest profile **39/39**. Each focused writer executable passed
**15 cases / 4906 assertions**. LeakSanitizer was disabled under the existing recorded
environment exclusion; no LSan or ThreadSanitizer result is inferred.

Tests cover six exact message identities, symbol reset/repeats/UTF-16, raw HISTORY bounds,
indexed requirements on both history forms, sparse identities/layouts, limits/work/no-refund,
allocation and late-work rollback, retained consumed outputs, move/destruction/credit lifetime,
every suffix position, compact body/symbol-count 63/64 and source-schema lifetime.
Huge representational overflow branches remain source-reviewed, not forced with invalid spans.

The native fixture from both compilers was the same 317-byte stream. The locked Java 3.355
production parser returned 42 tuples and 7 actual HISTORY_ADD time reads; native input expectations
matched both TSV files exactly. Both five-file artifact trees (4844 bytes) were identical,
including provenance. Java ran on Windows JDK 8 Zulu 1.8.0_422; Linux Java execution is not claimed.
The pin and fixture boundaries are in the [capture ledger](../../fixtures/subscriptions/writer/README.md).
This is not a claim of arbitrary data-field/business conversion qualification.

A mutated expected raw bound caused the verification wrapper to fail on the actual production
time-read mismatch. The final Java helper separately passed 15 CLI/production controls after
recompilation: expected exit results 3×0/10×1/2×3. Its Java→Java characterization uses a distinct
214-byte/30-tuple/5-time-read fixture with local IDs; it does not substitute for native sparse remapping.
Noargs/help matched without QD providers; failed help output and existing destination were rejected.

The complete documented C++23 example compiled/linked/ran as separate Windows/Linux consumers
with BUILD_TESTING/JSON OFF and no fetched dependency tree. Native source/header formatting passed
Uncrustify 0.83.0 then clang-format 20.1.8 Format/Check; Java passed pinned google-java-format 1.36.1
Format/Check across 41 sources. Doxygen 1.18.0 generated the header reference with zero warnings;
composer/output/work/limits pages and parameter/result text were inspected.

Full 39-test profiles leave JSON/replay/inspector/writer tools/compression OFF; they include the
unchanged recording tape-writer regression. No all-enabled acceptance, sockets/TLS/DNS,
live negotiation/epochs/collectors, cross-thread output release, macOS or process RSS cap is claimed.
The [verification tool](../../../tools/upstream/subscription-writer-oracle/README.md) documents
reproduction, fixed receiver, finite limits, partial-output failure rules and exact comparison.
