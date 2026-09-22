# Bounded RAW/MESSAGE implementation coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

This is the QDCXX-023 implementation map, not a claim of general tape support.
[RAW-TAPE](../../../../spec/protocol/RAW-TAPE.md) defines the measured policy and output;
[Java coverage](../../../fixtures/protocol/raw-tape/COVERAGE.md) separates production tapes from
diagnostic-wire probes. The default composite Quote schema is unchanged. RAW callback fan-out
10/15/20 in Java is compared using callback 15 as the single-wire-record projection, without
rewriting input message IDs or dropping replay/sequence timing.

## Requirements and executable evidence

| Requirements | Coverage |
| --- | --- |
| R1, R2 | 94 Java cases / 284 artifacts; six producer cases and separately labelled diagnostic-wire inputs. C++ compares unchanged production S/R/E and default Quote tapes; exact types 2/8/56/120 only. Descriptor matrix distinguishes absent, empty, null, hs and unknown opt; time and send-list checks precede data acceptance. |
| R3 | Absent timestamp versus explicit zero; empty envelope versus absent/zero-content heartbeat; repeated/decreasing times, EOF heartbeat, time across descriptions and data, int64 boundary packing/saturation, exact sequence and mapped event time versus Quote business times. |
| R4, R6 | Local profile and tape mode remain independent. Both profiles, modes, formats and diagnostic versions; strict version-3 TSV parser, exact old-mode byte/exit comparisons, default/explicit legacy equivalence, QTP regressions, help/argument/no-input-IO checks. |
| R5 | Chunk independence, owned values after input/reader destruction, no cross-reader state, terminal errors, precise primitive checkpoints, atomic heartbeat/description publication, completed prefixes, unknown-record/field skips with known widths only, finite zero/exact/excess metadata/data/reconstruction/output limits. |
| R7 | Generated bounded inventory self-tests and private aggregate-only report in [REAL-TAPE-GAPS](../../../../docs/REAL-TAPE-GAPS.md); unsupported data remains opaque and malformed framing/descriptions stop enumeration without resynchronization. |

## C++ suites

- [raw_tape_tests.cpp](../../raw_tape_tests.cpp): ten doctest cases compare all selected Java record
  values, field order, symbols, flags, callback-projected timing and default Quote mapping. Whole,
  bytewise and mixed input schedules, exact boundaries, failed append-only descriptions, symbol
  resets, reader lifetime and budget counters are checked. Intentional field/time/order mutations
  must fail semantic comparison.
- [raw_tape_io_tests.cpp](../../raw_tape_io_tests.cpp): three doctest cases exercise both profiles,
  TSV/human, diagnostic versions, timing rows, saturated/absent/zero metadata, exact/short/zero output
  budgets, injected read/write/flush errors and invalid-mode rejection before file IO.
- [raw_tape_cli.cmake](../../raw_tape_cli.cmake) with
  [raw_tape_cli_tests.cpp](../../raw_tape_cli_tests.cpp): 29 accepted Java scenarios undergo strict
  complete TSV parsing and ordered value/time/envelope comparison. Process-level chunk, profile,
  mode, argument, missing/empty/malformed/unsupported file and output-limit checks supplement IO tests.
  The standalone [negative runner](Test-Negatives.ps1) rejects 14 mutations and accepts the unchanged
  baselines; the CMake suite also injects eight malformed output variants. These 29 selected fixtures
  contain exactly one record per RAW frame by `RawTapeOracle.production/connector/focused` construction.
  The comparator requires that restriction independently of actual output and rejects moving a record
  between equal-timestamp frames. Multi-record reader behavior is tested separately in the reader suite.

Production targets remain independent of doctest, file IO and presentation. Existing type-15
`decodeStreamData` retains its contract; the explicit type-5 `decodeRawData` shares the measured
payload implementation. Fixed owned timing state is bounded separately from cumulative
`timingEvents`, which charges every attempted explicit heartbeat or RAW data message, even if a
later primitive fails. Default limit zero is meaningful; the tool supplies a finite limit.

## Limits and handoff

Java's broader acceptance of descriptor options/heartbeat extensions is retained as evidence and
does not broaden C++ policy. Nonzero event flags, other full types, Profile mapping, compression,
FIELD/NONE timing in the new mode, session replacement, transport and replay scheduling remain
excluded. Complete private-file structural inventory is not complete semantic replay; its Java
replay stopped at the delivery budget, and types 10/40/72/88 remain unsupported by C++.

[Author validation](VALIDATION.md) records executed configurations, counts, commands and limitations.
Independent acceptance is complete within this scope; its fresh checks remain
separate from the author runs and do not establish general tape compatibility.
