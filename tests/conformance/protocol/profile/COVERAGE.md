# Configured events and Profile inspection coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The QDCXX-025 implements only the exact ordinary-record subset in
[EVENT-MAPPING](../../../../spec/events/EVENT-MAPPING.md), [QUOTE](../../../../spec/events/QUOTE.md)
and [PROFILE](../../../../spec/events/PROFILE.md). [Java coverage](../../../fixtures/events/mapping/COVERAGE.md)
separates production delegate/file observations, direct boundary probes and source-resolution evidence.
[validation](VALIDATION.md) records actual runs; independent review is pending.

| Requirement | Executable coverage |
| --- | --- |
| R1–R3 explicit schemas, keys and types | `event_mapping_contract`: aliases, reordered fields, nonzero local ID, missing/ambiguous/incompatible requests, optional absence and malformed keys/types. `configured_java_events` constructs complete selected records from captured local/property keys and exact identities. |
| R2/R5 configuration and XML | 21 fresh-JVM Java scenarios; 18 accepted C++ variants, with unchanged full default and selected dxlink Quote/Profile records, extension, conflicting sources, optional combinations and numeric-int. Rejected, full24 and full137 cases remain explicit; full dxlink is not claimed. |
| R4 context and ownership | Equal-shape independent schemas cannot exchange mappings; copies retain identity; source destruction/move, STREAM/RAW publication and TapeReader snapshot rebuild preserve valid contexts. Context owns no declarations. |
| R6 event semantics | Direct Java cursor/event pairs and 108 original/omitted tape files, each fed whole and bytewise. All raw values, exact UTF-16 symbols/strings, delivered properties and timing are compared. Sequence boundaries, optional defaults, integer versus decimal defaults, negative times and business flags are distinct. |
| R7 compatibility | All 25 earlier CTest suites, 41 historical Quote files, unchanged legacy mapping and profiles; 18 separately captured legacy process outputs cover three profiles, two formats, both modes and representative failures. Help changes document new options; no-argument/help equivalence is tested. |
| R8 generic records | Shared helper and decoder tests retain extra fields and custom records; prepared projections leave generic values unchanged. Same event names alone do not establish compatibility; incompatible CLI schema/display pairs reject before IO. |
| R9 bounds and failures | Zero/exact/exceeded mapping counts/name/comparison budgets and per-event string/symbol/aggregate budgets. Profile IO tests cover cumulative copies, encoded output pair atomicity, input bytes, field units, records/fields, truncation, read/write failure and distinct first/final flush failure. |
| R10 strict evidence | Java comparator rejects 14 mutations; configured C++ comparator rejects 13 mutations; Profile CLI comparator rejects 84 mutations and compares six complete outputs independently from Java cells and original frame boundaries. |

Six new CTest suites are `event_mapping_contract`, `profile_mapping_contract`, `configured_java_events`,
`configured_event_fixture_negatives`, `profile_inspection_io` and `profile_inspection_cli`.
Prepared Quote checks extend `quote_java_tape`. The CLI comparator does not call event mappings,
TapeReader or Formatter; primitive envelope reads and independently enumerated decimal/quoting rules
construct complete expectations. Every event cell and raw field is mutated; unknown, trailing,
duplicate and missing rows, null/empty, flags, time, decimals and unsafe UTF-16 are rejected.
Both output formats also cover a locally present but wire-omitted default Profile.

Finite tests do not exhaust allocation failure schedules or all Unicode/numeric values. No general
source loader, complete settings compatibility, regional/indexed events, new full type, wire coercion,
compression, nonzero QTP flags, replay scheduler or transport is added. No private tape or live endpoint
is used. Retaining multiple library events requires caller-owned cumulative budgets. Self-review and
recorded checks do not close independent review.
