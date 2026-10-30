# Ordinary wire inventory validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Scope: [WIRE-INVENTORY](../../../../spec/schema/WIRE-INVENTORY.md), explicit
ordinary discovery and [version-7 inspection](../../../../docs/TAPE-INSPECT.md).
This report distinguishes author checks from final independent/integration acceptance.

## Executed checks on 2026-10-30

| Check | Observed result |
| --- | --- |
| Supplied-schema characterization before production changes | PASS: unknown names skip; explicit generic field value retained; known-name type mismatch refused in STREAM/RAW |
| New API regression before implementation | RED: compiler rejected missing discovery constructor/views, senderId and inspectWire |
| Windows x64 MSVC 19.42 (toolset 14.42), Release, targeted rebuild | PASS |
| wire_inventory_schema | 10 cases, 7,724 assertions, all PASS |
| wire_inventory_cli | PASS, both current tape modes and one-byte/4096-byte schedules |
| Relevant tape/schema/Quote/Profile/raw/admission/writer/replay/stream CTests | 25/25 PASS, including the two wire tests |
| Pinned C++ Format then Check | PASS, 10 authored files, Uncrustify 0.83.0 then clang-format 20.1.8 |
| Doxygen, public include tree plus changed tooling headers | Exit 0, zero warning bytes; new constructor/schema/descriptions present in generated HTML |

The targeted CTest set includes protocol_binary_tape, tape_inspection_io/cli,
tape_argument_contract, field_type_java_tape and field_type_inspection_io/cli,
raw_tape_java_conformance and raw_tape_inspection_io/cli, quote_java_tape and
quote_inspection_io/cli, profile_mapping_contract and profile_inspection_io/cli,
protocol_tape_admission, protocol_tape_writer, offline_tape_replay,
schema_json_contract/integration/cli, protocol_java_stream_data and both wire tests.
Existing output fixtures remain unchanged; these checks exercise the old explicit
profiles and native external schema path as regressions.

## Source and failure evidence

The designed tests cover 48 records/all eight exact full types; sparse/reordered
sender IDs including INT_MAX; same-name union/subset/reordered fields; late and
empty descriptions; stable local IDs with fresh snapshot provenance; owned source
names/values and bounded schema copies surviving input/reader destruction.

Budget tests cover zero/exact/one-less/one-more construction bounds, source copy
and both local schema copies/context charges, final record/field/lookup/name
bounds before local materialization, a late field beyond the union limit, UTF-16
supplementary names, malformed keys and atomic partial-batch refusals.

The ordinary allocation checkpoint test advances failure injection from the first
allocation through every subsequent allocation until an append succeeds (within
200 checkpoints). Every injected failure must become ALLOCATION_FAILURE while
retaining old schema identity, context pointer, source storage and declarations;
more than ten failed checkpoints are asserted. This covers the exercised ordinary
new/new[] path, not every allocator/platform or exact allocation-capacity behavior.

Version-7 output is independently parsed to reconstruct original source field
names/order/presence from generation/sender bindings and local typed values.
Distinct senders with equal projected zero/null values remain distinguishable.
Rejected late batches emit no candidate generation; empty accepted batches do.
Logical output limits and short physical writes between metadata rows leave no
completed snapshot/completion. Read/write/flush failures, safe human names and
absent RAW timestamp versus zero cursor sequence/delivered time are checked.
CLI tests cover no-argument/help equivalence, pre-I/O source exclusions, missing/
duplicate/invalid source values, finite schema overrides and chunk-independent output.

Production evidence uses the unchanged mixed-stream.qtp and mixed-raw.qtp from
[the existing exact-field corpus](../field-types/VALIDATION.md). Discovery values
are compared with supplied-schema decoding by exact field name, full type, raw
bits and nullable string units, not by counts or accidental equal local order.
The old production-fixture conformance tests also pass independently in the
targeted regression set. Designed witnesses do not replace Java parser evidence.

## Artifact identity and remaining acceptance

Windows Release qdcxx-tape-inspect.exe SHA-256:
`66e0cc7a4cce01e4c26b09ef72947cc11db537e4bd5b602a999524f53a4490e4`.

Key final source SHA-256 values:

| File | SHA-256 |
| --- | --- |
| include/qd/protocol/tape_reader.hpp | 91c623562ee8327b4a9a92293b632adf72187dc0811c7dd8ce4e7e4fb247fdba |
| src/protocol/tape_reader.cpp | 729d2349b84b7f0d999af8746b8897f909cf9916e02ba86e79cdc50cc0320515 |
| src/protocol/stream_data.cpp | 06e6ac3f64376a60ba74fe026a68ca74202d29e17ed5a8a53912d38d184d0d9f |
| tools/qtp-inspect/formatter.cpp | 4dae9f56995d2f32212f7affe6f5be8c2d370e61b9fb0826b3d7291923f49015 |
| tests/conformance/wire_inventory_schema_tests.cpp | f9cfb64d009ce227dfaa476ace60b2322daa02634e6c97c2007814bc04ae2d56 |

Final independent ownership/lifetime review and its focused wire tests passed.
The final-source Windows x64 MSVC 19.42 Release suite passed 53/53 tests; Linux
x64 GCC 13.3 Debug with ASan/UBSan also passed 53/53. Leak detection was disabled,
so these results make no LeakSanitizer claim. Production BUILD_TESTING=OFF builds
passed on both platforms for minimal libraries, tools/replay with JSON OFF, and
tools/replay with JSON ON. The JSON-OFF wire CLI regression passed on both.
The production trees did not acquire doctest; the JSON-OFF trees did not acquire
optional dependencies. JSON ON used explicitly supplied existing simdjson sources.

No private tape, live endpoint, business-event mapping, arbitrary serial-type,
full Java DataScheme or general binary compatibility acceptance follows from
these results. Broader types, indexed layouts, flags and data families remain
separate contracts with their own pending implementation/acceptance.
