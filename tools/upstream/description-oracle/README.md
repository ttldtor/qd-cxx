# Description oracle

[Русская версия](README.ru.md). English is authoritative.

QDCXX-013 adds an independent bounded driver over source-built QD 3.355. Existing Java harnesses,
launchers, expectations and captures are unchanged. [Coverage](../../../tests/fixtures/protocol/p08/COVERAGE.md)
and [validation](../../../tests/fixtures/protocol/p08/VALIDATION.md) define the measured scope.

Run sequentially against the shared locked checkout; JDK 8 is the oracle and JDK 21+ is only the formatter.
Use the same Upstream, JavaHome and Maven paths as the [protocol launcher](../protocol-oracle/README.md).
`Invoke-Oracle.ps1 -Mode Capture -Upstream ... -JavaHome ... -Maven ... -RunDirectory <new-directory>`
performs an independent clean reactor build. Repeat for a second capture, compare normalized outputs,
review them, then explicitly promote descriptions.tsv. Capture never writes expectations.
`Test-Verify.ps1 -Upstream ... -JavaHome ... -Maven ... -TestDirectory <new-directory>` mutates only
copied raw descriptor type 8 to 9, requires exact mismatch, then verifies the original and both hashes.
`Invoke-Oracle.ps1 -Mode Verify` also supports a separate ExpectedDirectory. Use tools/run_utc.py for logs.

The driver consumes [inputs.tsv](../../../tests/fixtures/protocol/p08/inputs.tsv): 81 payloads,
maximum 256 bytes, at most 16 fields and sender IDs at most 1024 for the bounded diagnostic profile.
Inputs are reviewed trusted probes, not a public parser for arbitrary adversarial count/length stimuli.
The real parser uses an empty DefaultScheme/PentaCodec and maxMessageSize=1024; one fresh parser per input,
returning MessageConsumerAdapter error handlers, no events, metadata, overrides, transport or EOF signal.
Each payload is reframed with Java ByteArrayOutput, including strict payload prefixes. Existing positive
inputs are slices at Java-captured frame offsets, not rewritten historical expectations.

A separate diagnostic walk calls real readCompactInt/readUTFString and applies source-checked structural
checks in parseDescribeRecords order. It records primitive positions, stage, cause and ordered completed
raw descriptions. This is instrumentation, not the actual parser's callback list. remapRecord calls super
and records successful installation IDs; reflection reads recordMap and names/types/nDesc after parsing.
Those observations distinguish omitted VOID, unsupported construction, replacement and prior-map retention.
No upstream source is patched or copied. Local schema factories are not executed by this new driver;
BYTE/SHORT local rejection remains historical S15 evidence. stderr preserves Java diagnostics.

Output rows have 14 columns: `case, id, payloadHex, completeFrameHex, diagnosticStatus, stage,
consumed, errorOffset, rawDescriptions, primitiveExceptionClass, installedIds, constructedMap,
parserErrors, frameInputPosition`. Dash means absent/empty; descriptors are `id/UTF16hex/name:type,...`
with records separated by semicolons. Names use four hex digits per UTF-16 unit. The final row is
`complete<TAB>descriptions`. Normalization changes CRLF to LF only; exception text is not a C++ contract.
Metadata records current sources/input/environment hashes, classpath outputs, dependencies and commands.
