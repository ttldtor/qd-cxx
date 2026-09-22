# Exact field-type production oracle

[Русская версия](README.ru.md). English is authoritative.

This oracle runs unchanged locked QD through production `StringField`/`IOUtil`, integer field
methods, `BinaryQTPComposer`/`BinaryQTPParser`, and `FileWriter`/`FileReader`. It retains original
uncompressed BINARY files in STREAM/NONE and RAW/MESSAGE modes with EMPTY_SET options and zero
flags. It never reads the private tape. Full Profile event mapping is not exercised or implemented.

`FieldTypes` is a diagnostic ordinary record. Its logical order is Text(10), Short(40),
Sequence(72), Date(88), Count(8), Char(2), Seconds(56), Wide(120), Tail(10). Java stores Text/Tail
at object indices 0/1; the seven scalar fields occupy integer indices 0..6, followed by a VOID
tail at 7. `schema.tsv` records this projection and every physical slot/local/property name of
the unchanged default Profile. Profile has 24 physical int slots, two objects, no time indexing,
and 16 logical fields; no field or upstream scheme option is removed.

Three string-reader surfaces are measured separately: ordinary ByteArrayInput (`buffered`),
DataInputStream (`data-input`), and production message-boundary BufferedInputPart (`message-part`).
The last checks character count against remaining message bytes before allocation. Valid INT_MAX
count is tested only on that boundary; counts outside the Java int range are safe primitive probes.
The harness uses a 128 MiB heap and 30-second process limit. Timeout is failure. It does not allocate
an enormous array merely to demonstrate exhaustion.

Each launcher run requires a new output directory and first invokes the existing tape baseline
Verify: locked clean upstream validation, clean reactor build, classpath/provider/dependency hashes,
and byte-exact old tape expectations. PowerShell 7.4+, JDK 8, Maven and the old inspector are required.
The formatter uses a separate JDK 21+.

```powershell
$fieldArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-a
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-b
& tools/upstream/field-types-oracle/Compare-FieldTypes.ps1 -Actual .work/field-types-a/field-types -Expected .work/field-types-b/field-types
& tools/upstream/field-types-oracle/Test-Negatives.ps1 -RunDirectory .work/field-types-negatives
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -Mode Verify -RunDirectory .work/field-types-verify
```

The comparator enforces the finite artifact set, row grammar, canonical integers, UTF-16 units,
nullable/scalar invariants, full tags, ordering and completion, then compares all bytes. Capture
validates structure against itself; independent cross-capture and Verify establish equality.
Verify never replaces expectations. Negative checks cover bytes, tags, values, units, nullability,
order, identity, missing/duplicate/unknown rows, arity, completion and primitive consumption.

See [coverage](../../../tests/fixtures/encoding/field-types/COVERAGE.md),
[captures](../../../tests/fixtures/encoding/field-types/CAPTURES.md),
[validation](../../../tests/fixtures/encoding/field-types/VALIDATION.md) and the
[field contract](../../../spec/encoding/FIELD-TYPES.md). Metadata/log timestamps and paths are
run-specific provenance; observed artifact bytes are never normalized.
