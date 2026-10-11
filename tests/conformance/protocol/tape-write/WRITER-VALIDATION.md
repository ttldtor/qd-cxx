# In-memory writer validation

[Русская версия](WRITER-VALIDATION.ru.md). English is authoritative.

The implementation follows [TAPE-WRITER](../../../../spec/protocol/TAPE-WRITER.md) and
[FIELD-TYPES](../../../../spec/encoding/FIELD-TYPES.md). It composes bounded owned chunks,
with no filesystem, clock, sink or transport ownership. This report distinguishes local
writer checks from the separate production Java parser and integration gates.

## Executed focused checks

### Initial snapshot

MSVC 19.42.34444.0, Visual Studio 17 2022, Windows SDK 10.0.26100.0, x64 Release built
`qdcxx_tape_writer_tests` against the ordinary protocol/core/encoding targets and existing
Quote/Profile targets. The 2026-10-11T22:17:40Z run passed 15 cases and 2365 assertions,
including interoperability-corpus emission. No errors were detected in these checks.

The tests consume the reviewed [contract fixtures](../../../fixtures/protocol/tape-write/contracts):
`schema.json` supplies local declarations; `writer-cases.json` supplies explicit outgoing
layouts, all ordered actions and 18 negative mutations; `raw-values.tsv` supplies 24 exact
tag/range witnesses; `budgets.tsv` supplies all four layout and seventeen writer allowance
families; `primitives.tsv` supplies five raw/repeat/new-frame symbol byte witnesses.
The finite test-only ASCII fixture reader bounds bytes, elements and depth and reads the
reviewed declaration/action grammar. It is not a general JSON validator: its integer reader
also accepts leading zeros, which are absent from these committed valid JSON fixtures.
It introduces no production JSON dependency and does not participate in production APIs.

Coverage includes explicit field permutation/aliases, sparse sender IDs, complete field
coverage, every supported tag, signed raw extremes, null versus empty type-10 values,
NUL/lone/pair surrogate units, first empty/wildcard/IBM/long/raw symbols, exact FF repeats
and reset at the next data frame. Every writer/layout allowance has zero, exact, one-below
and one-above witnesses; complete schema-copy bounds are checked separately. Fixed-input
logical charges are asserted independently of budget-success thresholds.

The existing `TapeReader` performs semantic round trips in `RAW_MESSAGE_V1` mode. Tests compare
the input action/record order, sender-to-local mapping, exact tags/raw/nullable units, symbol
units, descriptor absence/empty/spelling/send distinctions and signed raw/packed timing.
They cover absent/zero/timestamp heartbeats, negatives, repetition, decreases, signed64
saturation, empty batches/groups, explicit markers/trailing heartbeats and finish without
implicit bytes. Whole-tape byte identity with Java is not asserted.

Ordinary allocation is fault-injected only during actual owned construction/frame calls in
the test executable. Layout/schema copies, independent writer configuration, description
envelopes and retained long-symbol storage translate allocation failures without escaped
bytes or committed announcements/output/frame counters. Failed attempts retain charges;
every later operation returns the unchanged first diagnostic. Independent instances,
caller-owned chunks, source metadata mutation after copying and moved/finished states are checked.

The isolated pinned Uncrustify 0.83.0 → clang-format 20.1.8 Format and Check pipeline passed
for `tape_writer.hpp`, `tape_writer.cpp` and `tape_writer_tests.cpp`. Full local logs are
`.work/writer-033/writer-focused-build-final.log`, `writer-focused-test-final.log`,
`writer-format-final.log` and `writer-format-check.log`. Earlier SDK-registry sandbox refusal,
the missing member qualification found during a mechanical helper split, and the original
sender-endpoint mismatch remain recorded in the preceding local logs.

Two isolated source mutations verify test sensitivity. Removing the flags check caused two
cases/five assertions to fail, including W07 and the later-record failure-precedence checks.
Committing output bytes before allocation caused three cases/five assertions to fail, including
the exact output budget and actual allocation-failure atomicity checks. Their MSVC Release
builds succeeded; the expected failing test runs are in
`.work/writer-033/writer-flags-mutant-test.log` and `writer-atomic-mutant-test.log`.
The unchanged production binary then passed 15 cases/2258 assertions without emission at
2026-10-11T22:22:04Z (`writer-focused-restored-test.log`). These are post-implementation
mutation controls, not a claim of pre-implementation test-first execution.

### Descriptor-accounting review correction

Review found that descriptor policy comparisons charged logical work but omitted their
caller-text visits from `textUnits`. Each comparison now charges one caller unit before
charging two work-unit reads; the static policy spelling is work, not caller descriptor text.
The failed uppercase comparison for lowercase `message` also counts. A refusal before an
unadmitted comparison reports `descriptor-text-comparison`; if text admission succeeds but
work does not, it reports `construction-work`.

New exact/below/above tests cover uppercase `MESSAGE` and lowercase `message` with `hs`:
construction text charges are 27 and 28 respectively, with work charges 186 and 188.
The complete contract's text charge is now 199; its work charge remains 440.
The isolated corrected MSVC x64 Release run passed 16 cases/2396 assertions with corpus
emission at 2026-10-11T22:28:07Z and 16 cases/2289 assertions without emission at
22:29:21Z. Logs: `.work/writer-033/writer-review-fix-{configure,build,test}.log`,
`writer-review-fix-test-restored.log`, `writer-review-fix-format.log` and
`writer-review-fix-check.log`. Format/Check passed all three scoped C++ files.

An exact copy of the preceding frozen source built with these new tests then failed two
cases/three assertions: the B12 ledger/one-below checks and the new descriptor comparison
boundary. Evidence: `writer-review-old-{configure,build,test}.log`. The corrected source
and tests are the current snapshot; the earlier full/production observations below precede
this accounting correction and require the final integration rerun.

## Production-parser corpus and limits

The optional test invocation is:

```text
qdcxx_tape_writer_tests tests/fixtures/protocol/tape-write/contracts --emit-dir OUTPUT
```

It emits five cases: `contract`, `all-types`, `descriptor-opaque`, `standard-quote`, and
`standard-profile`. Each contains `tape.qtp`, exact `schema.tsv` declarations and input-derived
`expected.tsv` rows after asserted C++ round trips. Standard cases additionally contain
`schema-mode.txt` selecting the unchanged Java DXFeedScheme. They use the existing complete
default Quote/Profile APIs and every captured non-VOID property field, without a reduced schema.
The separate production parser harness is [TapeWriterInteropOracle.java](../../../../tools/upstream/tape-write-oracle/TapeWriterInteropOracle.java).

The separately executed 2026-10-11T22:21:30Z–22:21:49Z production run passed all five cases
through locked Java FileReader/BinaryQTPParser: 18 wire-record projections, zero parser errors,
54 RAW fanout callback records and 152 compared rows. Comparison selects the STREAM_DATA
projection once; it does not mistake the triple RAW fanout for three wire records.
The verifier checks complete unchanged Java Quote/Profile field coverage and retains hashes
for the C++ producer, source/input bytes and actual Java provider trees before and after the run.
Evidence: `.work/writer-033/interop-final-stable.log` and its `interop-final-stable/`
stdout/stderr, `bounds.tsv`, `provenance.tsv` and `classpath-receipt.json`.

The separate Doxygen run `doxygen tools/docs/Doxyfile` completed with exit 0 at
2026-10-11T22:20:14Z; `.work/api/warnings.log` is empty and the generated entry point is
`.work/api/html/index.html`. The pinned Java formatting Format/Check passed 26 files with
google-java-format 1.36.1 using the formatter JDK 23.0.2 and a clean PowerShell invocation.
Logs: `.work/writer-033/doxygen.log`, `java-format-no-profile.log` and
`java-format-check-no-profile.log`.

The final Doxygen configuration uses `EXTRACT_ALL=YES` to include documented free functions
whose legacy file groups are absent. Parameter/markup warnings remain enabled; missing-member
warnings are suppressed. An empty warning log therefore does not prove that every legacy
member is documented.

The final integration section below records the executed writer gates. Earlier focused results
above remain tied to their recorded snapshots.

The original INT32_MAX sender witness exposed signed `id + 1` overflow in the locked production
Java remapping path. The outgoing contract now refuses that endpoint at `record-id`; a native
MAX-1 construction witness still uses two sparse entries and is deliberately not sent to Java.
Other very large IDs remain subject to Java's dense-array length and heap limits. Existing reader
IDs and historical fixtures remain unchanged.

No fabricated oversized view tests the INT32_MAX character-array count or multi-gigabyte
UTFString domain. Those representability guards are source-checked; finite actual backed
inputs, all exact ordinary boundaries and real allocation failures are tested. Logical scratch
counts do not certify exact heap usage, allocator capacity or caller I/O success.

## Final integrated verification

The final Windows source passed the complete MSVC 19.42 x64 Release suite: 47/47 CTest tests,
including 16 writer cases/2289 assertions without emission and 4 char-array cases/323 assertions.
The full CTest run took 213.61 seconds. Logs: build
and CTest.

On Ubuntu 24.04 under WSL2 with GCC 13.3, the final source passed an incremental Debug
ASan+UBSan build and both affected CTest entries (writer and char-array, 2/2). Both Release
production configurations also built successfully with JSON disabled: one with tools disabled,
one with tools enabled. The 47/47 full Linux CTest result was from the initial snapshot before the
accepted `textUnits` accounting correction; there was no full Linux CTest rerun after that fix.
The final-source Linux logs are indexed in the writer evidence archive.
The earlier full-suite logs remain regression evidence for that earlier snapshot only.

The separate final native emission run passed 16 test cases/2396 assertions and produced five tapes. Java 8 FileReader then
parsed all five cases (`contract`, `all-types`, `descriptor-opaque`, `standard-quote`, and
`standard-profile`): 18 STREAM records, 54 callback projections, 152 compared rows and zero
parser errors. Quote/Profile coverage retained all 8/16 unchanged fields per record. Evidence
inputs and observed output are in `inputs/`,
`oracle/observed/`, and the archived
`provenance.tsv`,
`classpath-receipt.json` and
`bounds.tsv`; the complete interop log is
`interop-reviewed.log`.

The independent critical recheck closed the required descriptor-text accounting finding and
found no remaining required findings in its assigned scope. Its fresh MSVC Release run passed
2/2 targeted CTest entries: writer 16 cases/2289 assertions and char-array 4 cases/323 assertions.
This targeted recheck does not expand the platform matrix: Clang-cl and macOS were not run for
this writer change. No standalone LeakSanitizer certification is claimed.

The Linux sanitizer commands set no explicit `ASAN_OPTIONS` or `UBSAN_OPTIONS`; GNU defaults
were used, including default leak detection. No sanitizer diagnostics were detected in those
runs. Both platforms' production builds used `BUILD_TESTING=OFF` and JSON OFF; `_deps` was absent
with tools OFF and with both inspection tools ON. GCC Release emitted a nonfatal
`-Wstringop-overflow` warning at the unchanged `src/core/wide_decimal.cpp:68`; both builds exited 0.

The complete pinned C++ format check passed all 88 selected repository files at
2026-10-11T22:44:46Z. Java Format and Check passed 26 files at 22:31:13Z and 22:31:17Z.
The direct hex conversion matched the former formatter for all 65536 UTF-16 units in both
String and char-array representations, with null/empty and oversized-input refusal also checked.
See the archive overview for positive/negative parser probes and limitations.
