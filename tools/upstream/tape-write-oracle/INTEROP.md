# C++ writer production interoperability

[Русская версия](INTEROP.ru.md). English is authoritative.

`Verify-Writer.cmake` checks tapes emitted by the native writer against the locked Java QD
production `FileReader`. It compares the observed `STREAM_DATA` projection, including exact record
names, field order, raw scalar values, nullable UTF-16 units, symbols, flags and cursor sequence.
RAW callback fanout is counted separately; it does not multiply the expected wire records.
This is a semantic comparison, not a requirement for byte identity with Java output.

## Inputs and execution

The native `qdcxx_tape_writer_tests` executable accepts the contract fixture directory and
`--emit-dir PATH`. Use a fresh output directory. Each emitted case contains `tape.qtp`,
`schema.tsv` and `expected.tsv`. Standard Quote and Profile cases additionally contain
`schema-mode.txt` with `standard`; this selects the unchanged production `DXFeedScheme`.
Every non-VOID integer field and every object field must be represented exactly once in a
standard case. Custom cases use an explicit diagnostic scheme, with no inferred conversions.

Both TSV files have six tab-separated columns and no header. Names and strings are exact
lowercase UTF-16 hex; `-` denotes an empty string and `null` a null field value.

| File / row | Columns |
|---|---|
| schema / record | `record`, record name, `-`, `-`, `-`, `-` |
| schema / field | `field`, record name, local logical ID, local field name, wire field name, full type ID |
| expected / record | `record`, record index, record name, symbol, flags, Java cursor sequence |
| expected / value | `value`, record index, local logical ID, full type ID, raw scalar, units or `null` |

From the repository root, pass explicit prepared-provider paths and the actual producer binary:

```powershell
cmake '-DJAVA_HOME=C:/path/to/jdk8' '-DUPSTREAM=C:/path/to/locked-QD' '-DRECEIPT=C:/path/to/classpath-receipt.json' '-DINPUT=C:/path/to/emitted-cases' '-DPRODUCER=C:/path/to/qdcxx_tape_writer_tests.exe' '-DRUN=C:/path/to/fresh-verification' -P tools/upstream/tape-write-oracle/Verify-Writer.cmake
```

The receipt is produced by the [provider preparation workflow](README.md). The run directory must
not already exist. No existing capture or expected output is rewritten. The formatter JDK remains
separate from oracle JDK 8; follow the [Java formatting instructions](../../format/README.md).

## Bounds and evidence

The harness bounds each input file to 16 MiB, schema records to 256, fields to 2048, each decoded
name/string to 1,048,576 UTF-16 units, callback records to 8192 and output rows to 131072. The launcher accepts
1–64 cases, gives each compilation/parser invocation 30 seconds and uses a 256 MiB Java heap.
These diagnostic limits do not replace the native writer's explicit budgets.

A successful run retains observed rows, separate stdout/stderr, UTC phase bounds, provider class
hashes and input/tool/native-source/producer hashes. Input bytes, native source snapshots, producer
binary and provider classes are checked for changes during verification. Producer hashes identify
the supplied artifact; the caller must build that artifact from the corresponding source snapshot
before emitting cases. A hash alone is not proof of that build relationship.

Sender ID `INT32_MAX` is rejected by the native outgoing contract because the locked Java parser's
`id + 1` overflows during its dense record-map growth. Smaller large IDs still depend on Java array
and heap limits. Native-only sparse boundary tests do not claim successful production Java parsing
at those resource extremes. See the [writer contract](../../../spec/protocol/TAPE-WRITER.md).
