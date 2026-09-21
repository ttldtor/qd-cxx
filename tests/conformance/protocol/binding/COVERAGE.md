# Static binding conformance

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Contract](../../../../spec/protocol/STATIC-BINDING.md), [tests](../../binding_tests.cpp),
[Java observations](../../../fixtures/protocol/binding/COVERAGE.md), [validation](VALIDATION.md).

Three doctest cases execute 1357 assertions. All 12 described Java cases / 15 descriptions are
mandatory, with strict names/order, payload hex and plan grammar. No empty, missing or malformed
coverage can pass. Java values independently support designed IDs/order/skip decisions; source-only
intermediate duplicate assignments are explicitly labeled in the Java coverage.

API-policy tests cover all 256 raw types against known and unknown records (only 8 succeeds),
negative/out-of-range types, empty names/negative sender IDs, empty schema/records, sparse INT_MAX IDs,
repeated targets, exact keys without invented prefixes, duplicate/ambiguous keys, invalid references,
missing key coverage, unsupported local type/time shapes, local malformed UTF-16 rejection and raw
isolated UTF-16 unknown lookup. Every schema/planner budget is tested at zero/below/equal/above its
small successful boundary. size_t maximum budgets are finite, with no huge allocation. Owned schema
survives input destruction; plans survive schema and description destruction. Calls retain no session state.

[Negative runner](../../check_binding_negative.py) detects 11 temporary corruptions: local target ID,
order, skip, intermediate duplicate target, missing case, empty coverage/file, malformed hex, unknown
case, trailing row and missing file. Baseline and temporary files remain unchanged. Existing compact,
UTFString, framing and descriptions targets run as regressions. No C++ event decoder, production
library dependency, importer or transport is introduced. Allocation-failure injection, huge allocations,
32-bit/macOS and full schema/protocol profiles are unexecuted.
