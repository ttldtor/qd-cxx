# Offline QTP inspection

[Русская версия](QTP-INSPECT.ru.md). English is authoritative.

`qd-qtp-inspect` is an optional bounded diagnostic executable, not a tape reader or client.
The [version 1 contract](../spec/protocol/OFFLINE-INSPECTION.md) defines sequencing, exact TSV,
offsets, exits and resource accounting. No importer, transport, dynamic session, nonzero flags,
conversions or general schema loader is included. Raw QTP is not a tape container.

## Build and run

```powershell
cmake -S . -B .work/inspect -DQD_BUILD_QTP_INSPECT=ON -DBUILD_TESTING=OFF
cmake --build .work/inspect --config Release
.work/inspect/Release/qd-qtp-inspect.exe --profile oracle-sre-v1 --input tests/fixtures/protocol/offline-inspection/expected/composed.qtp
.work/inspect/Release/qd-qtp-inspect.exe --help
```

With Ninja/Makefiles use `.work/inspect/qd-qtp-inspect` (or `.exe` on Windows).
`ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT` defaults OFF; library-only builds remain available. BUILD_TESTING=OFF
does not fetch doctest or require Python. With tests and tool enabled, CTest includes executable
integration and portable injected I/O failures; Python 3 is test-only. Production links only
the existing qdcxx_protocol/qdcxx_core/qdcxx_encoding and standard library.

## Arguments and finite defaults

`--profile oracle-sre-v1 --input PATH` are mandatory, in either order. No positional arguments,
`--key=value`, option abbreviations, duplicate options or stdin `-`. `--help` must be the only
argument. Every numeric operand is ASCII decimal digits only; zero is allowed except chunk-size.
Unknown options/profile, missing operands and values outside size_t/uint64_t/streamsize where
applicable fail usage. Frame+9 must fit, and frame must fit signed 64 bits. Per-message limits
greater than totals are valid: the decoder receives the minimum. Defaults are decimal counts:

| Option | Default | Unit/scope |
| --- | --- | --- |
| --chunk-size | 4096 | Input bytes/read, positive |
| --max-input | 67108864 | Accepted file bytes |
| --max-frame | 1048576 | Type+payload bytes/frame; also payload and symbol input-span bound |
| --max-frames | 4096 | All frames including heartbeat |
| --max-descriptions | 1024 | Initial descriptions across frames |
| --max-actions | 65536 | Initial wire field actions across descriptions |
| --max-name-units | 1024 | UTF-16 units/description name |
| --max-total-name-units | 1048576 | All description record/field name units |
| --max-records | 1048576 | Charged records across data messages, including skips |
| --max-fields | 4194304 | Charged wire fields across data messages |
| --max-values | 4194304 | Completed local values across data messages |
| --max-symbol-units | 4194304 | Owned completed output symbol units, counting repeats |
| --max-output | 67108864 | All stdout bytes including format/complete |
| --message-records | 65536 | Charged records/data message |
| --message-fields | 262144 | Charged fields/data message |
| --message-values | 262144 | Completed values/data message |
| --message-symbol-units | 262144 | Owned completed symbol units/data message |
| --symbol-units | 4096 | Units/current symbol and retained message-local symbol |

Exact limits succeed, excess fails; empty inputs can succeed with zero data budgets. A zero
output budget cannot accommodate even the format row. These are logical bounds, not RSS promises.

## Explicit diagnostic local schema

`oracle-sre-v1` uses the existing [STREAM_DATA oracle scheme](../tests/fixtures/protocol/stream-data/COVERAGE.md).
All fields are ordinary full type 8, hasTime=false. IDs are positions, with these ordered fields
and explicit lookup keys (no inferred aliases):

| ID/name | Fields in local order | Keys → field ID |
| --- | --- | --- |
| 0 / S | 0:v | v, S.v → 0 |
| 1 / R | 0:y, 1:x, 2:z | y, R.y → 0; x, R.x, xdot → 1; z, R.z → 2 |
| 2 / E | none | none |

This is a temporary fixture adapter, not the production schema model. Tool-local `inspect` accepts
an independently supplied validated LocalSchema with fixed inspection limits documented in the
contract. Later loaders need not change binding or decoding. Sender descriptions never define the
local schema. Output uses exact unit hex; for example R is `0052`, ABC is `004100420043`.

## Completion and failures

Success is exit 0 and exactly one final complete row. Empty/description-only files may succeed
with no records. Exit 2 usage, 3 I/O, 4 unsupported profile/sequence, 5 malformed data, 6 resources.
Stderr marks output incomplete and supplies available frame/payload positions. Earlier complete
records may remain after failure. A failed physical write/flush can leave partial bytes, even a
completion marker; always check exit status. This differs from Java malformed-message callback
batching. See the contract for strict row arities and field meanings.

## Readable output

Add `--output-format human` to any invocation; omit it or use `--output-format tsv` for the
unchanged version 1 TSV. Unknown, repeated or missing format operands are usage errors.
There is no automatic TTY selection.

```powershell
.work/inspect/Release/qd-qtp-inspect.exe --profile oracle-sre-v1 --input tests/fixtures/protocol/offline-inspection/expected/composed.qtp --output-format human
```

Human version 1 shows a format/profile header, frame names/numeric IDs and actual byte boundaries,
local record names/IDs, quoted symbols, local-order labeled values and a final summary. Labels are
explicit `S.v`, `R.y`, `R.x`, `R.z`; independent tool callers without display metadata get
`field[index]`. Lookup aliases are not canonical display names. Displayed identities are local,
not reconstructed sender identities. Empty symbols (`""`) differ from zero fields (`fields={}`).

Both modes are deterministic, BOM-free and LF-terminated. Human uses UTF-8, combines valid surrogate
pairs, escapes lone surrogates losslessly and escapes quotes/backslashes, controls, line separators
and bidi controls. The exact table and grammar are in the [contract](../spec/protocol/OFFLINE-INSPECTION.md).
No Unicode normalization or silent replacement. `--max-output` counts actual encoded bytes including
header/escaping/summary; format-specific costs differ. Rows are counted before allocating their
expanded representation, so insufficient budgets never intentionally publish partial rows.
Stderr and exit codes are unchanged; completed prefixes may remain on failure, and even a delivered
summary does not supersede a failed write/final flush exit.

This reads raw QTP, not QD tape: no tape containers, compression, time files, protocol descriptors,
broader fields, importer, transport or dynamic sessions. [Human coverage and author validation](../tests/conformance/protocol/human-inspection/VALIDATION.md)
reuse historical Java evidence; the presentation itself is a designed C++ policy.
