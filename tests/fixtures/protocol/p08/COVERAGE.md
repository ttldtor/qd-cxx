# P08 description evidence coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Inputs](inputs.tsv), [Java observations](expected/descriptions.tsv), [validation](VALIDATION.md),
[driver](../../../../tools/upstream/description-oracle/README.md),
[layer contract](../../../../spec/protocol/DESCRIBE-RECORDS.md).

All 81 selected inputs are captured: 35 base hand-authored stimuli, 34 strict prefixes of four tiny
payloads, and 12 historical P01/P03/P04/P05 type-2 payload slices. Each prefix is inside its own complete
frame with a recomputed length; empty and description-boundary prefixes may succeed. This is payload
truncation, not transport fragmentation. Invalid inputs are explicitly hand-authored; outputs are Java execution.

Base cases cover empty payload; zero/one/multiple records; duplicate IDs/names; negative ID/count;
null/empty record/field names; malformed/truncated names; negative/256 type versus in-range 5/255;
BYTE/SHORT/VOID; valid then malformed/truncated/unsupported then valid; overlong/cross-width ID/count/type;
surrogates and NUL. Prefix families are one, multiple, surrogates and utf-overrun.

Raw diagnostic descriptors preserve full types, UTF-16 and order. Actual Java callbacks report only
corrupted messages; remapRecord and reflected final mappings separately show construction and prior
installation effects. Type 5/255 is structurally accepted but not installed. VOID is removed by Java
construction; BYTE/SHORT skip mappings succeed in the empty local scheme. An unsupported replacement
preserves the previous mapping and parsing continues. A later malformed record preserves earlier installation.

Numeric/name validation order comes from inspected parseDescribeRecords and the Java diagnostic walk,
not an instrumented raw callback in upstream. Primitive positions are direct ByteArrayInput observations;
actual parser positions are complete-frame positions. C++ comparison deliberately uses the raw-layer
projection and preserves unsupported/VOID descriptions. Local factory support is a third independent
question, with historical S15 evidence, not a new local-construction experiment.

No huge counts/IDs/allocations, local binding/conversions, event payloads, optional parser profiles,
recovery policy, transport or complete protocol compatibility is measured here.
