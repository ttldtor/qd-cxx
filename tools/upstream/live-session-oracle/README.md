# Native live-session control verification

[Русская версия](README.ru.md).

This developer tool sends a complete **native-generated**, synthetic control capture through the
unchanged Java QD production `BinaryQTPParser`. It serializes actual `MessageConsumerAdapter`
callbacks immediately and compares them byte for byte with the native test's expected TSV. The
preflight is a resource and syntax gate; it is not the compatibility proof.

Requires CMake 3.25+, Git, JDK 8, the source revision in [upstream.lock](../../../upstream.lock),
unchanged prepared QD classes, their trusted preparation receipt, and native test outputs. This tool
does not prepare providers or build native tests. See [provider preparation](../../schema-export/README.md).
The locked source version and the observed runtime advertisement are separate provenance facts;
an advertisement such as `QDS-UNKNOWN` is recorded without replacing it with the source version.

```powershell
cmake -P tools/upstream/live-session-oracle/Verify.cmake
cmake -DHELP=ON -P tools/upstream/live-session-oracle/Verify.cmake
cmake '-DJAVA_HOME=C:/jdk8' '-DUPSTREAM=C:/locked-qd' `
  '-DRECEIPT=C:/prepared/classpath.json' '-DNATIVE=C:/native-controls' `
  '-DRUN=C:/new-control-check' -DMODE=Capture `
  -P tools/upstream/live-session-oracle/Verify.cmake
```

Replace example paths with existing local inputs. `NATIVE` must contain `native-controls.qtp` and
`expected.tsv` emitted by the native conformance test. `RUN` must not exist and must be outside
`UPSTREAM`, `NATIVE`, and `EXPECTED`. Required path settings have no defaults. `MODE` defaults to
`Capture`, which **always** compares callbacks with `NATIVE/expected.tsv`. `Verify` additionally
compares the exact four artifact files with `EXPECTED`, defaulting to
`tests/fixtures/live-session/expected`. To use an explicitly reviewed artifact directory:

```powershell
cmake '-DJAVA_HOME=C:/jdk8' '-DUPSTREAM=C:/locked-qd' `
  '-DRECEIPT=C:/prepared/classpath.json' '-DNATIVE=C:/native-controls' `
  '-DRUN=C:/new-repeat-check' -DMODE=Verify '-DEXPECTED=C:/reviewed-controls' `
  -P tools/upstream/live-session-oracle/Verify.cmake
```

Containment checks resolve the nearest existing parent and compare Windows paths without case
distinctions. They detect aliases resolved by CMake at check time. Concurrent replacement of parent
directories or reparse points, and aliases that the platform cannot resolve, remain outside this
preflight; it is not a filesystem security sandbox. Use stable local input and output directories.

No arguments and standalone `-DHELP=ON` print identical help without work. Exit 0 means help or a
successful exact comparison; exit 1 means invalid settings, drift, timeout, Java failure, or mismatch.
The launcher creates only fresh `RUN` classes, private logs, receipts, and `artifacts/`:

| Artifact | Meaning |
| --- | --- |
| `native-controls.qtp` | Exact native input copy |
| `actual.tsv` | Actual production callbacks |
| `provenance.tsv` | Source pin, source/helper/input/provider/runtime hashes and QD advertisement |
| `manifest.tsv` | Artifact byte counts and SHA-256 hashes, excluding the manifest itself |

Provider receipts and input/tool hashes must match before and after execution. No network, Maven,
upstream modification, native build, fixture publication, or global setting change occurs. `javac`
has a 60-second deadline; parsing has a 30-second deadline. Both use a 256 MiB Java heap; this is
not a bound on process RSS. Failed runs retain partial logs and artifacts and do not establish success.

Generate the native capture from the same sources checked by the run. Provenance binds the native
test producer, live driver and shared decoding/storage, schema, writer and encoding sources, plus
the root CMake compile profile. Native build logs establish which sources produced the executable;
the wrapper hashes the current source bytes and does not rebuild or infer that association from a
capture file alone. Quote/event-mapping source inputs are included because the producer test target
also links and tests the Quote projection.

Exact artifact verification includes provider and Java runtime hashes. Repeating `Verify` assumes
matching prepared providers and runtime bytes; a different runtime requires a separately reviewed
`Capture`. A provenance mismatch alone does not identify a wire-compatibility failure.

The standalone Java helper accepts `--input FILE --output NEW_FILE` in either order, with no
operational defaults. Its parent directory must exist; existing output paths are rejected using
`CREATE_NEW`. No arguments and standalone `--help` print identical help without file I/O or SDK
initialization, even without QD classes on the runtime classpath. Exit codes: 0 success/help,
1 operational, parser, resource, or help-output failure; 3 invalid, duplicate, or incomplete arguments.
Diagnostics never echo argument values or parser exceptions. A failed write can leave a partial file.

Only message 0 (Heartbeat), message 1 (DescribeProtocol), and empty envelopes are admitted. The entire
file passes preflight before SDK parsing. The input stream is bounded at 1 MiB plus one detection
byte, so a changing file cannot bypass the limit. Counts and text lengths are checked before the SDK
can allocate their objects or UTF buffers:

| Budget | Maximum |
| --- | --- |
| Frames | 4096 |
| Entries per property or message list | 256 |
| Total property and message entries | 4096 |
| Message ID | -1 through 65535 |
| Wire bytes per text | 16384 |
| UTF-16 units per text / entire input | 4096 / 65536 |
| Output rows (including header) / ASCII bytes | 65536 / 8388608 |

Unknown metadata is bounded opaque text. Malformed syntax, incomplete frames, unsupported message
types, EndpointId trailers, unknown heartbeat bits, and trailing payload bytes fail. Nonempty
`authorization` must be exactly the synthetic token `Basic c3ludGhldGljOnRva2Vu`; all other nonempty
values fail before production parsing, without echoing them. Nonempty authorization is always
`REDACTED` in callbacks; null and empty values remain distinct. Never use real credentials.

`actual.tsv` has six columns: `kind`, `index`, `section`, `message_id`, `key`, `value`. The index is
the global zero-based callback ordinal across both kinds. Text uses `N` for null and `U` followed by
four uppercase hex digits per UTF-16 code unit; empty text is `U`. Properties are sorted by Java
UTF-16 ordinal order with null first; messages by ID and then name. Protocol sections are `property`,
`send`, `receive`, `send-property`, and `receive-property`. Message properties include inherited
protocol properties. Property rows use the actual key and value; send/receive rows use the name in
`key` and `-` in `value`. Protocol property rows use `-` for the message ID.
Heartbeat `mask` rows use `-` for ID/key and the decimal mask for value; `field` rows use `-` for ID,
the field name (`timeMillis`, `timeMark`, `deltaMark`, `lagMark`) and its decimal value.
Only present fields are emitted. Empty envelopes and payload-free heartbeat frames produce no
callback; a heartbeat payload with mask zero produces its mask row. A descriptor with no properties
or messages produces no rows while still advancing the callback ordinal.

The observer does not retain mutable SDK callback objects. The production parser's accumulated
descriptor and inherited message properties are the observation; there is no invented wire reset
marker. A fresh helper process has fresh parser state.

This control-only check does not qualify real feeds, EndpointId, PART framing, authentication
integration, native clients, sockets, clocks, or subscription/data delivery. Java-generated
characterization inputs alone do not establish native-to-Java interoperability.

Primary contracts at the locked source: `ProtocolDescriptor.composeTo/parseFrom`,
`MessageDescriptor.getProperties/getMessageType`, `HeartbeatPayload.composeTo/parseFrom`,
`BinaryQTPParser.parseDescribeProtocol/parseHeartbeat`, and `BufferedInput.readUTFString/readProperties`.
Their hashes are captured in provenance. For the file-creation contract, see
[`StandardOpenOption.CREATE_NEW`](https://docs.oracle.com/javase/8/docs/api/java/nio/file/StandardOpenOption.html#CREATE_NEW).
