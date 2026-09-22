# Additional field types: conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The [FIELD-TYPES contract](../../../../spec/encoding/FIELD-TYPES.md) defines the exact opt-in union,
owned values, primitive/message boundary and charging policy. These author tests consume the
[unchanged Java expectations](../../../fixtures/encoding/field-types/expected) directly.
[Java coverage](../../../fixtures/encoding/field-types/COVERAGE.md) distinguishes original production
FileWriter/FileReader delivery, production parser probes and direct primitives. A diagnostic walk
is not used as proof of event delivery. Executed configurations and limits are in [VALIDATION](VALIDATION.md).

| Requirement | Executable evidence |
| --- | --- |
| R1 exact policy/binding | `field_type_java_values` tests every identity -1..256 against both old profiles and the new union; `field_type_java_tape` checks exact mismatches and unknown supported fields/records. |
| R2 nullable owned UTF-16 | Value tests consume all 9 writer, 219 reader and 324 integer observations; null, empty, surrogates, noncanonical encodings, prefixes, status and consumption are compared exactly. Direct results survive input/context destruction. |
| R3 raw scalar identity | Signed low-int32 projection of 40/72/88, including wider compact-long forms, retains full tags. Invalid manually constructed values and new tags are rejected by every Quote slot. |
| R4 production delivery | Original mixed and unchanged full Profile tapes pass STREAM and RAW paths under whole-file, bytewise and mixed schedules. All 16 logical Profile fields remain present despite physical Java VOID tails. 27 successful diagnostic tapes additionally check defaults, repeats, skips, wider values and following sentinel alignment. |
| R5 bounded work/ownership | Zero, exact and short byte/unit limits; malformed admitted strings; skipped/replaced/failed attempts; simultaneous old/new replacement storage; cumulative tape budgets; work versus retained counters. |
| R6 atomic prefixes and output | Malformed third-record strings preserve two completed records with exact offsets/counters. A rejected appended description retains the previous context. Terminal feed/finish, timing separation, reader isolation, every output-byte cutoff, short writes, thrown I/O and both flush failures are checked. |
| R7 field-only inspection | Fixed diagnostic schema, explicit version 4, independent Java STREAM projection, separate field SEQUENCE and heartbeat eventTimeSequence; no Profile business-event mapping. |
| R8 regressions | All 20 previous CTest suites remain enabled with both inspectors, alongside five new suites. Existing profiles/formats and exit behavior are exercised; only documented tape help is extended. |

`field_type_inspection_cli` compares complete TSV and human stdout for both modes with independently
constructed expectations. Original Java field rows and original tape envelopes determine identity,
order, frame association and timing; the actual inspector output never supplies grouping. Three
singleton wire records occupy three original data frames, including equal-time RAW frames. Java's
callback batching/fanout is validated separately; only its STREAM projection represents wire records.
The comparator checks all descriptor/frame/record/timing/marker/completion bytes, not just values.

Twelve CLI mutations cover changed units, null/empty, tag, raw value, omitted field/record,
duplicate/unknown row, column count, early/duplicate completion and movement between equal-time
frames. Twelve primitive-fixture mutations cover bytes, units, nullability, raw, tag, consumption,
unknown rows, columns, duplicates, omission and completion. Each mutation must actually change the
input, must fail comparison, and is followed by an unchanged baseline. Separate Java negatives cover
17 mutations and a full negative-then-baseline Verify.

The seven I/O cases exercise both formats and both modes. Whole-row preflight prevents resource
failure from writing part of a row; a physical I/O failure may leave a prefix. A final-flush failure
can follow an already written completion row, so exit 0 is required. Escaping tests include controls,
surrogate pairs and lone surrogates. Allocation failure translation is implemented and reviewed;
allocator fault injection is not performed.

Java can recover at a later frame or convert mismatched fields; the bounded C++ policy intentionally
rejects mismatches and becomes terminal. Those cases assert the documented refusal rather than
equating Java recovery with C++ acceptance. The tests do not cover additional serial identities,
indexed schemas, compression, flags, transport, full Profile mapping or the private tape, which was
not read. SHORT_STRING/SEQUENCE standard-event witnesses remain outside the measured real-schema
coverage; their exact field evidence is synthetic.
