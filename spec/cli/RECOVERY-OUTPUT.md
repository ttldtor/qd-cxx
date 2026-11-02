# Recovery subscription output, version 1

[Русская версия](RECOVERY-OUTPUT.ru.md). English is authoritative.

The explicitly selected recovery route writes `qdcxx-subscriptions<TAB>1` to stdout. Each mode keeps
all exact generic fields. Explicit `--output quote` or `--output profile` additionally requests the
existing prepared event projection; record-name inference is not performed. This format applies only
to recovery output. The legacy subscription output retains its existing format.

## Encoding and publication

- Every row ends with LF at the C++ stream boundary. Native text-mode stdout can translate LF to CRLF;
  captures must retain their actual native bytes. Columns are separated by one literal TAB, with no
  quoting, space padding or BOM. A trailing empty cell is preserved before the line ending.
- Integers are exact base-ten ASCII, with a leading minus only for negative signed values. No plus,
  grouping, hexadecimal prefix or floating conversion is used. Existing enum values use their underlying
  decimal codes. Booleans are `0` or `1`. Caller locale, boolalpha/base/width flags do not change cells.
- `null` is an absent optional value. An engaged empty text/byte value is an empty cell, not `null`.
  Numeric zero is `0`. No absent metadata is replaced with zero.
- `UTF16HEX` is four uppercase hexadecimal digits for every exact UTF-16 code unit, including unpaired
  surrogates and NUL. No Unicode normalization, surrogate replacement or character decoding occurs.
  `BYTEHEX` is two uppercase hexadecimal digits per opaque byte. Empty values have zero digits.
- Decimal/long/time raw values remain signed scalar bits. In particular, raw decimal zero remains zero;
  it is not reformatted as numeric zero, NaN, text or a host floating value.
- Streams are synchronous. A publication unit is a whole callback batch or one evidence-writer call.
  The unit is flushed and stream state checked before the function returns successfully. A blocked sink
  cannot be interrupted by the configured run duration. An exception uses static frontend text; arbitrary
  stream exception text is not forwarded.
- A failure can leave a partial row/unit and earlier external bytes. There is no rollback, retry,
  transactional file commit or downstream-consumption acknowledgement. A throwing callback remains a
  queue callback failure and its side effects are not replayed.

Stdout contains only selection and data rows below. Stderr contains the direct evidence rows. Each
stderr row is an observation of an existing public type; writers never poll an owner, submit controls,
advance recovery, derive completeness or decide an exit code. TLS-enabled builds additionally expose
the numeric `tls-diagnostic` row. Pre-activation validation errors and final application
cutoff/target/signal/exit reporting have separate runtime contracts outside this row specification.

## Stdout rows

Columns below follow the row-name cell, in the exact listed order. Names in backticks are fixed tokens;
other names identify public members or already validated frontend selections.

| Row | Columns after the row name |
| --- | --- |
| `qdcxx-subscriptions` | `1` |
| `selection` | family (`stream`, `ticker`, `history`), selectedLocalId, output (`generic`, `quote`, `profile`) |
| `record` | ordinal, family, selectedLocalId, epochOrdinal, revision, admissionRevision, symbolUTF16HEX, rawTime-or-null, producedEventFlags, fieldCount |
| `source` | ordinal, present, senderId, cipher, originalFlags, bindingGeneration, removalTime, fieldRawSeconds, fieldRawSequence, fieldSourceOrdinal |
| `trigger` | ordinal, present, senderId, cipher, originalFlags, bindingGeneration, rawTime, fieldRawSeconds, fieldRawSequence, fieldSourceOrdinal |
| `field` | ordinal, zeroBasedLocalFieldIndex, exactFullSerialType, rawSigned64, unitsUTF16HEX-or-null, bytesBYTEHEX-or-null |
| `quote` | ordinal, symbolUTF16HEX, bidTimeMs, bidExchangeCodeUnit, bidPriceRaw, bidSizeRaw, askTimeMs, askExchangeCodeUnit, askPriceRaw, askSizeRaw, timeMs, sequence, timeNanoPart, eventTimeOrigin, eventTimeMs-or-null |
| `profile` | ordinal, symbolUTF16HEX, betaRaw, earningsPerShareRaw, dividendFrequencyRaw, exDividendAmountRaw, exDividendDayId, high52WeekPriceRaw, low52WeekPriceRaw, sharesRaw, freeFloatRaw, highLimitPriceRaw, lowLimitPriceRaw, haltStartTimeMs, haltEndTimeMs, businessFlags, descriptionUTF16HEX-or-null, statusReasonUTF16HEX-or-null, eventTimeOrigin, eventTimeMs-or-null |

The first two rows occur once after complete preparation and before activation. For each callback
record the order is `record`, `source`, `trigger`, all `field` rows in local field order, then the optional
selected `quote`/`profile` row. An empty callback has no data rows. The sink always returns CONTINUE
after successful publication. It has no record target and cannot truncate a callback that crosses a
target; the runtime requests its cutoff only after the whole callback returns.

`ordinal` starts at one and increases for every record in this invocation. `completedRecords()` advances
only after a complete callback batch has flushed successfully. That counter describes frontend
publication, not subsequent queue ACK, native write acceptance or downstream reading. Earlier bytes can
exist when a later record/projection/flush causes the whole callback to fail.
`failed()` records a permanent publication refusal. The runtime inspects it after the owner turn
returns and requests its cutoff explicitly; the writer does not access or stop an owner.

Every context must match the known selected schema RecordContext with `matches`. The printed local ID
is that validated selection; it is not extracted from a token or inferred from a source sender ID.
Typed values preserve every actual full serial identity, scalar and nullable payload, including fields
not selected by a prepared mapper. The only legacy scalar fallback is an actually supplied complete
compact-only view matching an all-COMPACT_INT selected schema; its full type is exactly 8. Mixed,
incomplete or inconsistent typed/legacy shapes refuse output instead of manufacturing missing fields.

`source.present=0` means all following eight metadata cells are `null`. With present=1 the sender,
cipher, original flags and binding generation are actual source members; removal/FIELD values remain
individually optional. FIELD absence gives three null FIELD cells; a present FIELD can still have an
absent sequence. `trigger` uses the same presence rule, but its fifth metadata value is the actual
HISTORY trigger rawTime, not removalTime. A synthesized collector record can lack source while having a
real trigger. Produced record flags and original source/trigger flags are never substituted for each other.

Prepared rows call `batch.project(index, work)` explicitly. The result is owning and exists only during
that synchronous record write. One fixed supplied projection-work vector is copied at callback entry
and debited across every project call in that whole batch; it is not replenished per record. A failure,
generic monostate or wrong prepared variant is a static callback failure. All generic fields are still
present in successful prepared output. Quote exchange codes are the mapper's actual low UTF-16 units,
printed as unsigned decimal numbers. Other prepared numbers are actual mapper members; decimal values
are their raw bits. The generic rows retain any original source values wider than a prepared projection.

`eventTimeOrigin` is `field` exactly when SubscriptionProjection.eventTimeMs is present, otherwise
`unspecified`. The companion value is actual optional FIELD-derived milliseconds, not an invented
zero. Quote.timeMs is the mapper's business-time result. HISTORY rawTime, source FIELD seconds/sequence,
business times and optional eventTimeMs remain independent coordinates; no Unix/calendar conversion is
inferred from a field name.

## Run-local epoch labels

Epoch labels are one-based encounter ordinals, stable only within this invocation/table. They are not
wire generations, provider IDs, route IDs, global chronology or evidence of an upstream path. Opaque
tokens are compared only by `matches`; no pointer, private generation or membership ID is printed.
An invalid token produces `null`; delivered records with invalid tokens refuse output.

The one-family runtime reserves a complete table for maximumAttempts+1 entries before activation:
one initial queue epoch and at most one source reset per entered attempt. A checked independent byte
allowance bounds its requested token-array capacity; actual reserved vector capacity is checked too.
An unrepresentable/over-budget table refuses before activation. The table never grows past its initial
limit and retains only detached QueueIdentity tokens, with no queue, batch, receiver, callback, event,
runtime or credential owner. Its references are application serialized. Only actual observations from
the single selected queue enter this table; arbitrary other queues/tokens are outside this frontend.

## Direct stderr evidence rows

An optional queue cause occupies three adjacent cells: queueError, nestedKind, nestedError; all three
are null when absent. With a present outer error and absent nested cause only the latter two are null.
`nestedKind` is the existing QueueSourceError variant index: 0 MuxError, 1 SubscriptionError,
2 StreamError, 3 TickerError, 4 HistoryError. Error/enum codes otherwise come from the corresponding
current public enum declaration; this format does not reassign their meanings.

| Row | Columns after the row name |
| --- | --- |
| `recovery-status` | phase, attempts, demandedFamilies, convergedFamilies, stopped, terminalPresent |
| `recovery-diagnostic` | scope (`first`, `secondary`, `turn`, `history-query`), recoveryError, tcpError, nativeCode, protocolError, queueError-or-null, nestedKind-or-null, nestedError-or-null, endpointError-or-null, writerError-or-null |
| `tls-diagnostic` | scope (`first`, `secondary`, `turn`, `history-query`), tlsError, sdkCode, verifyCode, tcpError, nativeCode, protocolError; available only in a TLS-enabled recovery build |
| `command-accepted` | command (`initial-replace`, `remove-initial`), actualNonzeroTicketId |
| `command-result` | command, id, status, revision, changed, muxError-or-null |
| `reset` | sequence, fromEpochOrdinal-or-null, toEpochOrdinal-or-null, knownDiscardedQueueRecords, knownDiscardedSourceRecords-or-null, skippedResets, unknownEligibility, unknownUpstreamLoss, countsSaturated, sourceCommitted, globalAckPending, firstCause(3 cells), secondaryCause(3 cells) |
| `reset-ack` | exactAttemptedSequence, actualControlAdmission |
| `history` | selectedLocalId, queriedSymbolUTF16HEX, epochOrdinal-or-null, authoritativeRevision, clientDesiredRevision, mappingDesiredRevision, requestedFrom, mappingCurrent, acceptedUnstagedData, paused, resetPending, hadGap, queuedForKey, executingForKey, completeDeliveredRange, sourcePresent |
| `history-source` | selectedLocalId, queriedSymbolUTF16HEX, sourceRevision, observationSerial, partitionActivation, requestedFrom, sourceFrontier, knownFrontier, snipFrontier-or-null, snapshot, sourceTransactionPending, initialPending, viewTransactionDirty, completeRequestedRange |
| `history-unavailable` | selectedLocalId, attemptedSymbolUTF16HEX, actualRequestedFrom, actualRecoveryError |
| `recovery-terminal` | outcome, deadlineExpired, heldDataDiscarded, nativeOperationsAtCutoff-or-null, wirePresent |
| `wire-terminal` | primaryTcpError, primaryNativeCode, primaryProtocolError, secondaryTcpError, secondaryNativeCode, secondaryProtocolError, aborted, peerEnded, writeAcceptanceUnknown, reportedWriteBytes, discardedInputBytes, pendingOutputBytes |
| `queue-terminal` | family, epochOrdinal-or-null, outcome, authoritativeRevision, hadGap, unknownEligibility, unknownUpstreamLoss, failedRegistrationCount, primaryCause(3 cells), secondaryCause(3 cells) |
| `queue-counts` | family, acceptedFrames, acceptedRecords, collectorAcknowledged, queueAdmitted, withdrawn, discarded, sourceRecordsDiscarded, callbackStartedBatches, callbackStartedRecords, callbackCompletedBatches, callbackCompletedRecords, callbackFailedBatches, callbackFailedRecords, commandsApplied, commandsRejected, commandsCancelled, saturated |

Recovery phases are 0 CONFIGURED, 1 PREPARING, 2 STARTING, 3 NEGOTIATING, 4 REPLAYING, 5 ACTIVE,
6 RETIRING, 7 RESETTING, 8 BACKOFF, 9 STOPPING, 10 CLOSED, 11 EXHAUSTED, 12 FAILED.
Family mask bits are the existing RecoveryFamily positions (TICKER 0, STREAM 1, HISTORY 2).
Command status is 0 APPLIED, 1 REJECTED, 2 CANCELLED_CLOSING. Control admission is 0 RECORDED,
1 COALESCED, 2 STALE, 3 CLOSED. Terminal outcome is 0 DRAINED, 1 ABORTED, 2 FAILED.
HISTORY snapshot is 0 UNKNOWN, 1 RECEIVING, 2 COMPLETE, 3 TRUNCATED.

An accepted command is not yet applied, synchronized, composed, emitted or acknowledged upstream.
The runtime obtains actual ticket results and passes them to the writer. Status ACTIVE and convergence
bits are actual local observations and are not peer acknowledgements.

For reset, the runtime copies the actual notice, calls writeRecoveryReset and waits for successful
flush/check, then attempts acknowledgeReset with that exact sequence. Only that returned admission is
passed to writeRecoveryResetAcknowledgement. STALE means observe the newer notice on a later turn.
A failed publication must trigger abort and must not acknowledge a gap. No synthetic data record is
inserted to represent reset, and unknown losses are never printed as known zero.

Successful HISTORY observations emit adjacent `history` and `history-source` rows. When sourcePresent=0,
all twelve source cells after ID/symbol are null. When present, source facts are copied exactly:
RECEIVING can coexist with a complete higher requested range, while other views still need lower data.
CompleteRequestedRange and completeDeliveredRange are distinct actual members; pending delivery, pause,
reset, gaps and current mapping remain visible. The result is detached AS-OF evidence of an admitted
prefix. It does not certify future freshness, unread wire, full provider history or peer ACK.

An ineligible/refused query emits `history-unavailable`, optionally followed by its typed diagnostic.
It does not emit a cached former successful range, old epoch, complete=true or a manufactured
complete=false observation. The runtime must not treat an unavailable observation as evidence that
the range is incomplete or complete. Successful and unavailable rows are separate observations; no
previous success remains a current freshness claim.

Terminal rows require the actual optional public terminal to exist. Counters, duration expiry, zero
pending bytes, a stop request or an empty FIFO do not synthesize DRAINED. The wire row is emitted only
for actual optional retired-attempt evidence. Native reported-write bytes are local acceptance;
pending/unknown fields remain visible, and none proves network flush, peer receipt or TLS close-notify.
Queue counts describe their named local scopes and cannot be summed into a lost-event total.

## Runtime call order

The runtime owns every queue, receiver registration, ticket, retry/poll cadence and termination choice.
It prepares labels and the sink before activation, opens the existing receiver with the selected binding,
calls `sink.begin()`, then starts the existing owner. After a returning owner turn it can inspect
`completedRecords()` and choose a later cutoff. It passes actual changed status/ticket/reset/query/terminal
observations directly to free writers. HISTORY queries occur outside callbacks and use fixed explicit
caller work. Sink/label lifetimes extend through callback retirement and final `sink.flush()`.
No writer stores work references, owner pointers, secrets or previous successful range state.

See the [subscription receiver contract](../subscriptions/RECEIVER.md),
[queue contract](../mux/QUEUES.md) and [recovery contract](../transport/RECOVERY.md) for the underlying
delivery, control and lifetime guarantees.
