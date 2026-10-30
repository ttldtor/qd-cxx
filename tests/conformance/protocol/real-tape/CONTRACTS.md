# Generic binary catalog verification contracts

[Русская версия](CONTRACTS.ru.md). English is authoritative.

Status: required checks for the [finite binary catalog](../../../../spec/protocol/BINARY-TAPE-COVERAGE.md).
This document specifies acceptance; it does not report those checks as executed.
The [matrix](../../../fixtures/protocol/real-tape/support-matrix.tsv) separates existing evidence,
pending implementation, selected exclusions and required production positive/negative observations.
The [decision](../../../../docs/decisions/ADR-0004-broad-binary-tape-reading.md) defines the boundary.

## Reference and result rules

Run each new applicable production case against QD3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Use actual FileWriter/BinaryQTPComposer and
FileReader/BinaryQTPParser with explicit schemas and settings. A standalone primitive probe or
diagnostic decoder cannot replace the production message-path observation. Source-identical code
can explain an observation but does not turn a pending test at the other pin into an executed one.

For each row, retain portable deterministic inputs, exact source/helper/runtime/classpath hashes,
canonical production observations and C++ expected status/value/order/flags/time. Preserve historical
captures and `upstream.lock`. Keep private recordings, symbols, paths and complete local logs outside
public fixtures. Distinguish immutable source facts, planned tests, author execution and independently
repeated checks. Do not fill an actual-evidence column with a future path that does not yet exist.

The [producer validation](producer/VALIDATION.md) is existing Windows Java evidence for its named
compact/Quote ordinary STREAM_DATA cases and truncation. It does not qualify C++ parity or the other
matrix cells. New expectations require review, promotion of unchanged bytes and a fresh Verify.

For any Java-valid alternative excluded by the selected profile, record both Java behavior and the
expected C++ UNSUPPORTED result. Reserve MALFORMED for bad grammar/state under the selected contract.
Unknown types/flags/messages must not become accepted through masked low bits or a skipped field.

## Common checks for every implemented row

1. Positive production fixture plus matching generic C++ canonical values, sender ID, source field
   order/full identities, symbol units/cipher, flags, family and supported time. Raw/stream and newly
   accepted family paths must agree on shared physical fields; no business conversion or collector
   transformation may hide a mismatch.
2. Negative production mutation at relevant prefix/body/field/frame boundaries, including every
   truncation point for each finite fixture. Record primitive examined position and production
   message-relative position separately. Confirm C++ typed cause/stage/offset and completed-prefix
   behavior; a production error and a selected unsupported-policy refusal are different observations.
3. Finite budget tests at zero and L-1/L/L+1 for input, counts, construction, output, units and owned
   storage as applicable. Admission must precede count-sized reserve, proportional copy/allocation,
   lookup/action/default construction and publication. Check overflow by subtraction. Rejected work
   is not admitted; admitted work is not refunded after skip, replacement or error.
4. Compare defaults, repeated assignments, unknown fields/records and a following sentinel field or
   record. Reject malformed skipped values instead of guessing the next boundary. Retained outputs
   survive input/source/schema/reader destruction and later successful generations.
5. Record final source/artifact hashes, actual mode/profile/toolchain and exact commands. After a
   source fix, rerun affected checks on the final source. Existing default profiles, admission tokens
   and serialized output remain byte/behavior compatible.

## QDCXX-041: missing scalar identities

Target matrix rows T00/T01/T03/T04/T24/T104/T152 and unchanged existing scalar rows.
Expected test locations: `tests/conformance/field_type_value_tests.cpp` and
`tests/conformance/field_type_tape_tests.cpp`; extend the production field-types oracle.

- BYTE/SHORT/INT: signed extrema, -1/0, big-endian exact widths, all incomplete widths and a following
  field sentinel. BYTE/SHORT factories return no default instance; use the production requested-wire
  descriptor path with a suitable consumer. Do not add a diagnostic-only field factory and call it
  a FileWriter observation.
- DECIMAL retains signed32 raw bits, including special encodings; LONG/TIME_MILLIS retain signed64.
  Compact-overlong/high32 inputs must distinguish physical read from int32 storage projection.
  WIDE_DECIMAL keeps all64 bits; no existing Quote/extended fixture changes.
- VOID consumes no value bytes. Only an explicit wire VOID may project raw zero; long storage's
  physical VOID tail is not an extra logical value. Verify descriptor writer elision and reader
  alignment separately. Settle the exact logical schema rule before implementation.
- Exact-ID negatives include reserved168, other identities sharing serialization bits, high unknown
  bits, negative and >255 descriptor IDs. Distinguish profile refusal from descriptor grammar failure.
- Existing profiles accept exactly their previous IDs. Introduce only the new explicit field profile;
  scalar progress does not claim a finished 19-type broader catalog.

## QDCXX-042: byte/string/opaque-object payloads

Target T09/T137/T233/T249 and unchanged T10. Expected tests:
`tests/conformance/object_field_value_tests.cpp`, `tests/conformance/field_type_tape_tests.cpp`.

- Verify nullable owned bytes and exact text tags: raw zero, null/empty/content distinction, scalar
  payload absence, equality/defaults/replacement/invalid manually constructed values. Old aggregates
  and profiles keep their meaning.
- Test four separate grammars: byte-array compact-long length; marshalled compact-int projected
  length; UTFString byte length; character-array unit count. For249 include wide/overlong prefixes
  whose low32 count differs from the full compact long. Compare actual consumed bytes and sentinel
  alignment. Do not share a length-validation rule merely because both results are byte vectors.
- Cover null, empty, NUL, FF bytes, large bounded contents; malformed negative/oversized lengths,
  truncated prefix/body and allocation failure. For137 cover BMP/supplementary/CESU/unpaired units,
  accepted overlong forms and malformed continuations through production consumers.
- Observe Marshalled bytes without calling getObject. Unknown class/tag contents remain opaque;
  malformed outer length/framing fails. No Java class loading/deserialization or PlainObjField
  semantic execution becomes a required capability.
- Check per-value/cumulative examined bytes, admitted payload work and completed/current/temporary
  retained bytes before every proportional allocation/copy. Skips, overwritten values, unknown
  records, failures and replacement do not refund work. Independently review allocation, exception
  translation and owned-result lifetime across all accepted data paths.

## QDCXX-043: indexed layout and immutable generations

Target S1–S8. Expected tests: `tests/conformance/indexed_schema_tests.cpp` and
`tests/conformance/tape_session_tests.cpp`; use production schema exporter/binding observations.

- Prove Java slot indices, widths, aliases and hasTime from explicit source schema. Exercise both
  PAIR_INT32 and SINGLE_INT64, ordinary versus indexed records, reordered/missing fields and explicit
  VOID. Never infer indexed layout from record names or wire-only descriptions. An uncharacterized
  layout is pending/unsupported, not an automatically accepted special case.
- Observe equal/new/redefined sender IDs; same ID with equal descriptor, reordered/subset/new fields,
  record-name/type changes and multiple replacements in a batch. Specify active table ordering and
  local-ID stability explicitly. Preserve source sender identity and exact types; do not apply Java
  conversion/fallback representation rewrite to make a mismatch fit.
- At failure halfway through parse/validation/admission/allocation, verify the prior descriptions,
  schema, context and borrowed views remain intact. On success, publish all components through
  nonthrowing operations without user callbacks between publications. Retained old records and
  RecordContext provenance remain valid; borrowed views expire only at documented boundaries.
- Bound current active state, old+candidate peak storage and cumulative construction. Repeated
  replacements/rebuilds hit exact limits without retaining an unbounded generation history or
  refunding work. Sender storage is sparse, including large valid sender IDs.
- For independent closed sources, new-reader construction is the reset mechanism. Old output survives;
  a new reader cannot use an old sender description. Characterize production reset separately. If
  an in-place reset is needed, review incomplete-frame/terminal-state handling and every resettable
  component; it must not evade source/sequence cumulative limits. No implicit header/footer reset.
- Old ordinary discovery keeps repeated-ID refusal. Independent critical state/lifetime/resource
  review is mandatory before this contract closes.

## QDCXX-044: options, flags and symbol state

Target O1–O3, F0/F1/F2/F4/F8/F16/F64/FU, SYM and MRU. Expected tests:
`tests/conformance/event_flags_tests.cpp`, `tests/conformance/symbol_tests.cpp`.

- Observe absent/empty/hs options and unknown/duplicate tokens at both pins. Capture composer masks
  independently of parser behavior: Penta reader ignores option set; normal writer filters flags.
  Broad decoding preserves known raw data flags even without hs advertisement; old modes still
  refuse nonzero data flags. Unknown features remain explicit profile refusals.
- Probe all64 subsets of0x5f in each RAW/TICKER/STREAM/HISTORY family, including parser mutations for
  combinations filtered by the writer. Keep flags as metadata; verify no collector semantics.
  Probe0x20/0x80/unknown high bits separately and distinguish writer masking, raw Java parser
  acceptance and selected C++ UNSUPPORTED results.
- Test F8 initialMRU1, F9 new flags, reuse, zero explicit flags, duplicate prefixes, flagged FF repeat,
  first-symbol repeat, message/frame reset, empty messages and truncation inside flags/symbol.
  Preserve exact eventFlagsBytes rules and symbol ownership/work accounting.
- REMOVE_EVENT reads builtin event fields then compact-long record time, with ordinary fields omitted.
  Test ordinary/indexed, zero-field and following-record sentinel cases; exact removalTime survives
  reader destruction. Configured local words match Java setTime. Unsupported FIELD metadata is
  refused until046 instead of being consumed as ordinary fields.
- Verify exact completed-prefix counters and limits across every family and flag combination that
  changes payload layout. No duplicate eventFlags member or snapshot/history scheduler is introduced.

## QDCXX-045: dispatch and descriptor policy

Target M0–MU, D1–D3, H1–H5, existing TIME0/TIME1 and C0. Future rows remain separate.
Expected tests: `tests/conformance/tape_message_family_tests.cpp`,
`tests/conformance/tape_reader_tests.cpp`.

- Production RAW/TICKER/STREAM/HISTORY tapes must match exact generic source family, record/value
  sequence, flags and supported time. Use RawDataConsumer observation rather than counting Java's
  fallback fan-out three times. Mixed families and controls preserve wire event order.
- Probe normal DXP3 headers, endpoint/no-endpoint tails, unknown properties/message descriptors,
  deltas/repeated headers, bounds and malformed/truncated extensions before implementing their exact
  bounded policy. Preserve existing mode acceptance/refusal and output.
- Headerless self-described binary requires an actual FileWriter positive with the writer's header/
  footer suppression setting recorded. Data IDs still require prior descriptions. Verify no-footer
  clean EOF versus truncated last frame. Keep headerless pending until this case exists.
- Explicitly reject schemeKnown/readAs/resync/ignore/continue/cycle, CSV/BLOB, PART reassembly,
  subscription and RMI payload operations. Probe each finite known message ID/group and representative
  unknown IDs. Valid excluded Java behavior is not relabelled malformed. Reject before proportional
  unsupported-feature work or emitting misleading records.
- Finish succeeds only for actual explicit EOF, complete frames and all validation required by the
  selected mode. A valid prefix, NEED_INPUT, resource stop, unread bytes or late failure is not full
  completion. Preserve source/frame/payload coordinates and the first terminal cause. No source,
  callback or transport framework is required by this packet.

## Future gates and final verification

FIELD046, LONG/TEXT047, GZIP/ZIP048, split sources049 and operational streaming050 remain pending.
Before048 select the optional pinned dependency/license/platform/OFF acquisition policy, members/
entries, CRC/trailer checks and source/inflated/output budgets; OFF must not fetch it. This contract
does not add that dependency. Future sinks need reviewed callback lifetime, stop/exception checkpoint,
source/sidecar/container close and layered completion rules. Unimplemented cells remain pending.

For changed C++/schema/protocol behavior run focused doctest/CTest and production-oracle comparisons,
then the required final Windows MSVC Release and Linux GCC Debug ASan/UBSan full CTest matrix.
Test applicable production-only optional profiles ON/OFF without unwanted dependency acquisition.
Producer Windows-only qualification does not waive Linux client verification. Run pinned C++/Java
formatters and affected oracle Verify after source changes; record final source/provenance hashes.

For documents/matrix, check EN/RU semantic parity, reciprocal and relative links, all changed/new
whitespace, row uniqueness/count, exact full-ID/message/flag dimensions, primary-source references
at both pins and actual-evidence path existence. Required independent final-source review covers
ownership, generation state, partial failure, EOF and resource bounds. Author rereading is not that
independent review. Publish only the checks actually executed and retain named unresolved limits.
