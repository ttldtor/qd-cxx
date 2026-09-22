# Remaining real-tape gaps

[Русская версия](REAL-TAPE-GAPS.ru.md). English is authoritative.

QDCXX-023 separates bounded structural inventory from payload compatibility. A complete inventory
does not establish that the C++ reader or locked Java reader can decode the whole private sample.
The task authorizes RAW/MESSAGE for existing exact types 2/8/56/120;
new field identities and broader standard-event adapters remain outside its scope.

## Diagnostic method and bounds

[RawTapeInventory.java](../tools/upstream/raw-tape-oracle/RawTapeInventory.java) uses locked Java
`ByteArrayInput.readCompactInt`, `readCompactLong` and `readUTFString`. It walks complete frame
boundaries and only structurally parses type-2 DESCRIBE_RECORDS, following the
[raw-description grammar](../spec/protocol/DESCRIBE-RECORDS.md). Other payloads remain opaque;
unknown message kinds and structurally valid full type IDs 0..255 are counted without guessing
field widths. This is diagnostic instrumentation, not a production parser or decoder.

Finite limits are: file 8 MiB, frame body 1 MiB, 100,000 frames, 10,000 descriptions, 100,000 fields,
4,096 encoded bytes per name, and 1,048,576 aggregate UTF-16 name units. Name-length limits are
checked before the Java string allocation. The file size is checked before opening and bytes are
also charged during reading. The launcher gives each Java command 30 seconds and the helper JVM
128 MiB maximum heap. Production replay has a 1,000,000-record budget and stops on the first excess
delivery attempt; the reported observation counter includes that rejected attempt (at most 1,000,001).

Inventory stops at the first invalid/truncated primitive, framing/description failure or exhausted
limit, without resynchronization. `boundary` is the absolute start of the failing frame or current
description; `examined` is the absolute primitive examination position. On complete coverage both
equal file length. A successfully framed type-2 envelope is counted even when its description body
fails; only fully valid descriptions publish field/type counts. Earlier complete descriptions remain.
An empty envelope has diagnostic kind -1 and is distinct from type-zero heartbeat. Repeated
description names/IDs count as occurrences: inventory does not install or validate a schema session.

## Reproducible generated checks

[Check-Inventory.ps1](../tools/upstream/raw-tape-oracle/Check-Inventory.ps1) validates upstream HEAD
against the lock and requires clean tracked upstream sources, JDK 8 and already-built locked
`qds-file` dependencies. It compiles the helper and runs generated cases without needing the private
sample. Run from the repository root, using local dependency paths:

```powershell
pwsh -NoProfile -File tools/upstream/raw-tape-oracle/Check-Inventory.ps1 `
  -UpstreamRoot $lockedUpstream -JavaHome $oracleJdk
```

Optional `-PrivateFile $privateTape` adds inventory and bounded production replay; all output is
restricted to the ignored repository `.work` directory. Keep that output private. The launcher
does not build or modify upstream and must run after an oracle build, not concurrently with cleaning.

Generated checks cover complete and partial inventory, opaque unknown message kinds, unsupported
full type 255, late descriptions, malformed record/field headers, invalid framing/type/UTF,
every nonempty truncated frame prefix and every nonempty truncated description prefix in a complete
envelope, empty-marker/heartbeat distinction, zero/exact/excess frame/description/field limits and
file/frame/name/aggregate-name limits. These checks validate the diagnostic walk's stopping and
counting behavior; the separate RAW oracle and C++ conformance suites prove the supported task profile.

No private tape, symbols, event values or private captures are published. The aggregate gap inventory
does not claim full private-tape support, and follow-up implementation is not started here.
