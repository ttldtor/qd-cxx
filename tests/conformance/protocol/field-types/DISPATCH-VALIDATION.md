# Binary dispatch and descriptor validation

[Русская версия](DISPATCH-VALIDATION.ru.md). English is authoritative.

On2026-10-30 the explicit BINARY_CATALOG_V1 policy passed Windows x64 MSVC19.42 Release and
Linux GCC13.3 Debug ASan/UBSan full CTest:61/61 on each; leak detection was disabled on Linux.
All six production-only profiles passed. New JSON-OFF broad inspection of16 actual FileWriter
inputs per platform returned0, with byte-identical TSV across platforms for all16.
Pinned Uncrustify0.83.0 then clang-format20.1.8 Format/Check passed for authored C++.
Independent review exercised owned copies, schema/context mismatch refusal, failure rollback,
metadata peaks, read/write/flush/resource boundaries and final copied artifacts.

## Production evidence

The [capture manifest](../../../fixtures/encoding/field-types/dispatch-captures.json) records
separate QD3.355 `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19` executions, source/harness hashes and every raw artifact
hash. Both qualified14-root classpaths remained unchanged. A separate locked clean full-reactor
JDK8 governed Verify passed all609 artifacts. No current upstream target was rebuilt.

The maintained FieldTypesOracle retains all287 earlier artifacts byte-for-byte and adds322 dispatch
artifacts: normal305 (303 raw tapes,668-row observations,57-range UID map), headerless17 (16 raw
tapes and25-row observations). Every pin has independent capture/repeat; canonical rows match
across repeats/pins. Normal raw tapes retain the actual random synthetic endpoint UID. Only its57
source-identified complete/partial byte ranges are normalized in the in-memory comparison view;
all other bytes and map boundaries must match. Each raw file has its own original hash. Headerless
raw tapes repeat byte-for-byte; no capture is edited to obtain equality. Two lower-message case
labels keep case-sensitive time tests distinct on Windows filesystems.

Production FileWriter/FileReader covers all four families with NONE and MESSAGE, including actual
noHeaderAndFooter children. NONE headerless starts descriptions2, MESSAGE starts heartbeat0.
RawDataConsumer observes each wire record once. Controlled production observations cover full/late/
delta/repeated/absent descriptors, arbitrary version, unknown/duplicate properties, send/receive
alternatives, options, optional endpoint tails, malformed complete versus physical partial EOF,
known controls, unknown IDs and actual FileReader schemeKnown/readAs alternatives. Designed mutations
are labelled separately from original FileWriter tapes. Java mixed-family callbacks may group
15,20,15 into15,15,20; C++ preserves wire order and does not claim collector-order parity.

All50 oracle mutations were rejected:42 existing and8 dispatch cases for canonical value/family/time,
raw bytes, UID range offset/length, bytes outside UID and headerless bytes. Independent checks also
derived all57 ranges from raw protocol structure and tested additional boundary mutations.

## Native contracts and whole-file scope

Focused native cases cover four families, exact fields/owned bytes/UTF16, removal0 versus absence,
retired name/type refusal, constructor zero/one identity, huge valid UTFString length prefix with
tiny input, exact descriptor peak/work and no-op storms, allocation rollback, physical truncation,
v8 empty-batch generation, all19 formatter tags and authoritative schema/context/full-shape checks
before zero output budget. Accepted HUMAN labels preserve old modes and TSV. Old formats retain
payload/flags/removal refusal. Existing ordinary profiles and fixtures are unchanged.

The [real-tape acceptance](../real-tape/MILESTONE-1.md) completed93 checks:44 old checks,
47 explicitly designed bridge regressions and2 actual producer STREAM/MESSAGE cases. A separate
whole-file private ordinary corpus compared333607 records/3136236 values/48 generations, exact
source/order/symbol/raw timing, physical EOF/open/close/error/queue proof and literal FileReader
digest parity. Private bytes, names, paths and decoded rows were not promoted. That comparator
validates only the old eight ordinary types, zero flags and absent removal, including its strict
v8 comparison view; it is not an all19/new-flag business acceptance claim.

Native completion proves read/physical EOF/reader.finish/write/final flush and exit0. It does not
prove owner close-before-complete; Java instrumented close proof is separate. FIELD, sidecars,
compression, file chains, collectors, conversions and broader owned-source lifecycle remain excluded.
See [BINARY-MESSAGES](../../../../spec/protocol/BINARY-MESSAGES.md) for the exact selected policy.
