# Locked QTP source inventory

All links refer to Java QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Symbols narrow the cited evidence; labels in [the inventory](README.md) distinguish source, documentation/tests, inference and unknowns.

## Reproduction

On 2026-09-20 reused the external checkout from QDCXX-003, with no edits to upstream:

```powershell
git -C "$env:TEMP/qdcxx-003-qd" rev-parse HEAD
git -C "$env:TEMP/qdcxx-003-qd" rev-parse "refs/tags/qd-qds-3.355^{commit}"
git -C "$env:TEMP/qdcxx-003-qd" remote get-url origin
git -C "$env:TEMP/qdcxx-003-qd" status --short
```

Both hashes matched the lock, origin was https://github.com/devexperts/QD.git, status was clean. Checkout acquisition is documented in [UPSTREAM](../../docs/UPSTREAM.md). Read with Get-Content and locate symbols with rg -n. No Java execution, network capture or copied upstream source is part of this task. Inspected headers identify MPL-2.0; fixture redistribution remains subject to the fixture provenance policy.

## Framing and records

- [BinaryQTPParser.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java) — parseImpl, parseDescribeRecords, parseMessagePart, resetSession, recoverBuffer.
- [BinaryQTPComposer.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java) — getRecordDesc, describeRecord, writeMessageHeader, finishComposingMessage.
- [BinaryRecordDesc.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java) — constructors, field2Desc, getIntConverterType, readRecord, writeRecord.
- [AbstractQTPParser.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPParser.java) — parse, nextRecordsMessage, processPending.
- [AbstractQTPComposer.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPComposer.java) — beginRecord, resetSession, hasCapacity, beginMessage, endMessage.
- [QTPConstants.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/QTPConstants.java) — MAX_MESSAGE_SIZE, COMPOSER_THRESHOLD, READ_AGGREGATION_SIZE.
- [BuiltinFields.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BuiltinFields.java) — event metadata names.

## Messages and negotiation

- [MessageConstants.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageConstants.java) — message IDs and PART comment.
- [MessageType.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageType.java) — record flags, cannotReorderWithMask.
- [ProtocolDescriptor.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java) — composeTo, parseFrom, mergeFrom, deltaFrom.
- [MessageDescriptor.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageDescriptor.java) — composeTo, parseFrom, property lookup.
- [ProtocolOption.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolOption.java) — HISTORY_SNAPSHOT, parseProtocolOptions.
- [MessageAdapter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageAdapter.java) — retrieveDescribeProtocolMessage, processDescribeProtocol, error handlers.
- [AgentAdapter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AgentAdapter.java) — processDescribeProtocol, isProtocolDescriptorCompatible.
- [DistributorAdapter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/DistributorAdapter.java) — startImpl, processDescribeProtocol, isProtocolDescriptorCompatible.

## Connection and transport

- [ConnectionQTPParser.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ConnectionQTPParser.java) — remapRecord, schemeMap, resetSession, addChunks.
- [ConnectionQTPComposer.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/ConnectionQTPComposer.java) — getRequestedRecordDesc, hasCapacity.
- [MessageAdapterConnection.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageAdapterConnection.java) — constructor, processChunks, retrieveChunks, examine, processIncomingDescribeProtocol.
- [HeartbeatPayload.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/HeartbeatPayload.java) — composeTo, parseFrom.
- [socket/SocketReader.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/socket/SocketReader.java) — read loop and processChunks.
- [socket/SocketWriter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/socket/SocketWriter.java) — retrieveChunks and output writes.

- [ApplicationConnection.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/proto/src/main/java/com/devexperts/connector/proto/ApplicationConnection.java) — processChunks/retrieveChunks threading and readiness contract.
- [TransportConnection.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/proto/src/main/java/com/devexperts/connector/proto/TransportConnection.java) — transport notifications.

## Encoding and core

- [BufferedInput.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedInput.java) — compact numbers, UTFString, properties, byte arrays.
- [BufferedOutput.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedOutput.java) — corresponding writers.
- [IOUtil.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/IOUtil.java) — compact encoding documentation and primitive helpers.
- [PentaCodec.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java) — ReaderImpl/WriterImpl reset and symbol/event flag state.
- [SerialFieldType.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java) — serialization and representation bits.
- [RecordBuffer.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordBuffer.java) — add, clear, appendInternal, compactArrays.
- [EventFlag.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/EventFlag.java) — getSupportedEventFlags.

## Upstream tests

- [ComposeParseTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/test/ComposeParseTest.java) — testComposerAndParserWithDescribe, mixed variant; supplied chunks, not exhaustive byte-split coverage.
- [FieldAdaptationTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/test/FieldAdaptationTest.java) — parameters and testFieldConversion, numeric adaptation assertions.
- [MessageAdaptersTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/test/MessageAdaptersTest.java) — known-scheme parser override versus described binary mode.
- [PentaCodecTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/test/PentaCodecTest.java) — testNull and testPentas: symbol encoding, not complete QTP session coverage.
- [ProtocolDescriptorTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/test/ProtocolDescriptorTest.java) — round trip, accumulation, omission, empty override and property inheritance assertions.
- [DescriptorDeltaEmissionTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-sample/src/test/java/com/devexperts/qd/qtp/test/DescriptorDeltaEmissionTest.java) — testFirstOutgoingIsFullHandshake, testClientRequestEmitsOnlyRequestedAggregationPeriodDelta.

Tests above were inspected, not executed. ComposeParseTest transferBytes forwards available chunks despite its random-fraction comment; do not cite it as exhaustive fragmentation coverage. PART/reset/resource-limit captures are proposed independently.

## Source-to-capture index (QDCXX-006)

The no-execution statements above describe QDCXX-004. QDCXX-006 built the same locked source and executed the [protocol driver](../../tools/upstream/protocol-oracle/README.md); upstream tests listed above still were not run.

| Source symbols | Executed evidence |
| --- | --- |
| BinaryQTPParser.parseImpl; AbstractQTPParser.processPending; PentaCodec Reader/Writer | [P01](../../tests/fixtures/protocol/expected/P01.tsv): 32 schedules, complete-frame buffering and callback grouping. |
| BinaryQTPParser.remapRecord/parseSubscription; BinaryQTPComposer.describeRecord | [P03 A-B](../../tests/fixtures/protocol/expected/P03-A-B.tsv), [P03 B-A](../../tests/fixtures/protocol/expected/P03-B-A.tsv). |
| BinaryRecordDesc field lookup; [DefaultRecord.addFieldAlias](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java); [AbstractDataField.getDefaultPropertyName](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/AbstractDataField.java) | [P04 order](../../tests/fixtures/protocol/expected/P04-order.tsv), [P04 alias](../../tests/fixtures/protocol/expected/P04-alias.tsv). |
| MessageAdapterConnection processChunks/retrieveChunks/getRequestedRecordDesc; ConnectionQTPParser.remapRecord; ConnectionQTPComposer.getRequestedRecordDesc; BinaryQTPComposer.getRecordDesc; BinaryRecordDesc.isEmpty | [P05 none](../../tests/fixtures/protocol/expected/P05-none.tsv), [zero](../../tests/fixtures/protocol/expected/P05-zero.tsv), [x](../../tests/fixtures/protocol/expected/P05-x.tsv), [z](../../tests/fixtures/protocol/expected/P05-z.tsv). |

[Capture interpretation](CAPTURES.md) and [validation](../../tests/fixtures/protocol/VALIDATION.md) distinguish actual bytes/observations from remaining source-only assertions. No upstream source patch was made.
