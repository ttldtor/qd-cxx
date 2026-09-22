# Tape evidence coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

QDCXX-019 uses the locked Java QD FileWriter open/write/close and FileReader production replay.
[Specification/source inventory](../../../../spec/protocol/TAPE-FILES.md),
[tooling](../../../../tools/upstream/tape-oracle/README.md), [validation](VALIDATION.md).
This is research evidence, not C++ tape conformance or a general file-format support claim.

## Inputs and cases

[inputs.tsv](inputs.tsv) contains seven records in three messages with explicit incoming heartbeat
times, exact UTF-16 symbols and zero flags. Sender R=0(x,y,z), S=1(v), E=2(); local S=0(v),
R=1(y,x,z), E=2(), all ordinary type 8, hasTime=false. The harness checks local identities and
the R permutation, both int32 endpoints, repeated ABC/U+2603 and empty symbol, and zero-field E.

| Case | Original file | Production replay | Diagnostic structural scan | Current inspector |
| --- | --- | --- | --- | --- |
| default | [165-byte BINARY](expected/default.qtp), no writer setters | [7 records, 3 callbacks](expected/default.tsv), three heartbeat times, zero flags | [11 envelopes](expected/default-frames.tsv), type 1 header, late descriptions, type 0 time and empty footer | [stdout](expected/default-inspect.stdout), [stderr](expected/default-inspect.stderr), [exit 4](expected/default-inspect.exit) |
| constrained | [137-byte BINARY](expected/constrained.qtp), explicit BINARY/NONE compression/NONE time/empty opt/STREAM_DATA | [7 records, 1 callback](expected/constrained.tsv), time sequence 0 | [8 envelopes](expected/constrained-frames.tsv), header and late descriptions remain | [stdout](expected/constrained-inspect.stdout), [stderr](expected/constrained-inspect.stderr), [exit 4](expected/constrained-inspect.exit) |
| text | [432-byte TEXT](expected/text.txt), format setter only | [7 records, 1 callback](expected/text.tsv), same values/symbols/time sequences as default; empty-heartbeat callback | Not interpreted by Java envelope scanner | [stdout](expected/text-inspect.stdout), [stderr](expected/text-inspect.stderr), [exit 4](expected/text-inspect.exit), initial text interpreted as type 61 |
| truncated | [135-byte derived file](expected/truncated.qtp), last two bytes removed from constrained (last data byte + footer) | [4 records, 1 callback, 1 stream error](expected/truncated.tsv); reader returns, logs 13 pending bytes | Derived malformed input, not a writer success case | [stdout](expected/truncated-inspect.stdout), [stderr](expected/truncated-inspect.stderr), [exit 4](expected/truncated-inspect.exit) at header before truncation |

The complete binary outputs are not repaired to fit the inspector. All inspector failures are
expected unsupported type exits, not successful semantic comparisons. The envelope scanner is
diagnostic; record correctness is checked through actual FileReader callbacks. Source-only gaps
are explicitly distinguished in the specification. There are 25 delivered record observations
across four replays; this count is not a count of distinct input records.

## Observation grammar

All observation TSV files use LF and nonempty cells. Hex strings are lowercase UTF-16 units,
four digits per unit; `-` means an empty sequence, absent value or null endpoint as specified by
the cell. Decimal integers have no leading zeros, plus sign or negative zero. No Unicode normalization.

| Row | Ordered cells, including row tag | Arity |
| --- | --- | --- |
| profile | profile, case, single-file, MAX_SPEED, auto | 5 |
| descriptor | descriptor, send type IDs or -, receive type IDs or -, endpoint units or - | 4 |
| property | property, key units, value units | 3 |
| heartbeat | heartbeat, time millis or -, isEmpty boolean | 3 |
| record | record, index, callback index, message type, local record ID, local name units, symbol units, local-order int32 list or -, flags, event-time sequence | 10 |
| error | error, stream/message/unknown, message type or - | 3 |
| end | end, record count, data callback count, error count, returned | 5 |
| complete | complete, case | 2 |
| frame | frame, index, start, exclusive end, envelope length, type (-1 for empty heartbeat) | 6 |
| complete (envelopes) | complete, envelopes, frame count, total bytes | 4 |

The closed verifier accepts only the captured stream error, one descriptor, expected property
keys/count, time/heartbeat shape, exact local field arities, complete/end order and counts, known
envelope sequence and inspector diagnostics. Exact byte comparison checks all remaining values
against the raw expectations. Missing or empty observations cannot count as success. Inspector
TSV has its own unchanged [grammar](../../../../spec/protocol/OFFLINE-INSPECTION.md).

## Limits and exclusions

No CSV/BLOB/compression/sidecar execution, connector replay, nonzero flags, arbitrary metadata,
dynamic redefinitions, converter/importer, follow, resync, huge resource probe or C++ expansion.
Runtime version is QDS-UNKNOWN from classes; actual commit/JDK/classpath provenance is separate.
No additional production dependency or Python script/dependency was introduced. The existing
UTC logger was used for build/format evidence only; oracle capture and Verify run through Java/PowerShell.
