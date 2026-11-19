# Local record publication contract

[Русская версия](RECORD-PUBLICATION.ru.md).

A local publisher can be configured with a validated, immutable catalog and its ordered outbound field names. The endpoint owns that configuration. A publication identifies one catalog record, one collector contract and one exact symbol. Use the existing owning `RecordContext` provenance for record identity. Copies of the same validated catalog retain provenance. A context from an independently validated catalog fails even when its numeric record ID matches. Receivers can independently assign their wire IDs.

## Values and admission

A publication supplies exact tagged values in catalog logical order. It preserves raw numeric bits and the distinction between null and empty. On indexed HISTORY removals it supplies the signed removal index and no field values. A HISTORY data row uses the index layout declared by the record and supplies the required indexed values. Applicability and supported flags follow the selected collector and wire profile.

The synchronous call may borrow symbol and value storage. The publisher owns any admitted asynchronous work before returning. The caller may immediately change or destroy its input. Validation and resource reservation precede admission. Invalid catalog identity, record shape, metadata, symbol or limits, insufficient capacity and a closed publisher produce explicit refusals. A rejected single record has no effect on collector state, downstream queues or subscription demand.

Bound each row by field count and owned payload bytes, including symbol bytes, and bound aggregate pending rows and owned bytes. Charge work until applied or discarded, including a row the worker has taken from the FIFO. Distinguish oversized rows, exhausted pending capacity, and allocation failure. Construction or allocation failure before admission releases its reservation and exposes no partial work.

A detached producer handle and an owned record context remain safe after endpoint destruction. Publication and close share an admission fence. An attempt ordered after the fence returns a closed diagnostic. Work admitted before the fence may be discarded during shutdown. See the [publisher contract](PUBLISHER.md).

Admission means the publisher has accepted the work for processing. The STREAM collector preserves FIFO admission order, including across concurrent producers. TICKER may conflate values for the same key. HISTORY applies its bounded indexed, transaction and snapshot rules. Downstream delivery, peer shutdown and queue pressure follow the collector and publisher contracts. Admission provides no peer receipt guarantee. Producers coordinate submissions for a multi-record HISTORY transaction on the same key.

## Compatibility

Use the existing collectors, subscriber filters and field encoders. Generic local publication does not require JSON or a provider-specific event type. Keep existing typed publish overloads and their supported profiles and preserve the compact and upstream relay modes.

Verify exact standard and custom records through TCP, including null and empty values, STREAM duplicates and order, TICKER conflation, HISTORY snapshots and signed removal. Exercise invalid identity, malformed fields, resource exhaustion and shutdown with rejected input.

## Local catalog profile

Set `PublisherOptions::catalog` to a binary schema and its ordered outbound names. Leave
`source` unset and `mixed` false. This endpoint advertises TICKER, STREAM, and HISTORY.
The HISTORY contract requires the existing negotiated QTP `hs` option. TICKER and STREAM
accept complete rows with default metadata. Typed `publish` overloads continue to use
the existing compact and mixed profiles.

Pass a `RecordContext` from the same schema used to configure the endpoint and one
`FieldValueView` for each logical field in catalog order. Only the caller's storage
for the duration of the call is required. The endpoint validates the outgoing
description before accepting clients. Each subscriber can assign its own record IDs.

The profile accepts up to 128 logical fields and a nonempty exact symbol of up to
256 UTF-16 units. A STRING field has the existing wire limit of 1,024 UTF-16 units.
`PublisherRecordLimits::rowBytes` defaults to 16 KiB and
`PublisherRecordLimits::pendingBytes` defaults to 256 KiB. Per row, the charge is the
array of field views, UTF-16 text including the symbol, and opaque payload bytes.
Fixed FIFO entries and allocator bookkeeping are outside that charge. The shared
`pendingQuotes` option caps the number of admitted rows, including the worker's
current unapplied row. A separate bounded source arena and the existing collector
limits cover work after copying. `PublisherStatistics::pendingPublications` includes
the current unapplied row in this profile.

### Byte-limited STREAM publication

Setting `PublisherOptions::streamBytes` enables independent STREAM publication and
output limits for a local catalog or mixed publisher. Local catalog admission uses
`recordLimits.rowBytes` for each row and `recordLimits.pendingBytes` for the sum
of retained row charges. Fixed queue entries remain separately bounded by
`pendingQuotes`. A multi-record call is admitted atomically: an invalid row,
exhausted capacity or allocation failure leaves the complete call unadmitted.
The mixed typed profile charges each fixed-format FIFO slot against the same
input byte budget and keeps worker-owned storage under a separate finite budget.

`targetPayloadBytes` is the soft threshold for encoded STREAM payload, defaulting
to 8,000 bytes. The publisher takes already available rows in order and sends
without waiting for more rows. It checks the threshold before appending the next
complete row. A single row may cross the threshold. `maxFrameBytes` separately
bounds the complete QTP frame including its length prefix, with a default of
65,536 bytes. A row that exceeds the hard frame limit is a resource failure.
The input API call and the outgoing frame need not contain the same rows.
In this profile `streamBatchRecords` does not limit publication or output.
The per-peer `deliveryWindowBytes` budget governs how much detached record
storage the next ordered selection retains. The default is 256 KiB and the
configured range is 1 byte through 8 MiB. The selection can take one larger
record so that the queue can advance. The full encoded frame must still fit
in `maxFrameBytes`. The publisher can combine records from several ready
input groups in one delivery window. It leaves promptly when there are no
ready records or the current work budget is exhausted.

The pending-row count, retained input bytes, delivery-window bytes and encoded
frame bytes measure separate resources. A work budget accounts for processing
steps within one turn. The number of records that fit within a byte budget
depends on their retained or encoded sizes; a record-count quota is a separate
setting. The body limit on the receiving framer excludes
the length prefix, unlike `maxFrameBytes`; see [FRAMING](../protocol/FRAMING.md).

Fixed queue slots and charged memory remain bounded independently. Per-peer
STREAM arenas are charged at their measured allocation size. The bytes of
complete rows and the available queue data determine frame size. A frame
composition failure retires the affected peer and increments STREAM
`failedFrames`.
When all subscribed peers lack room for the next source window, accepted
STREAM input stays in the publisher FIFO while sockets progress. Peers with
available capacity can keep receiving. Before collecting extra groups in the
same turn, the publisher gives sockets a chance to advance if a subscribed
peer lacks the next group's headroom. The existing finite per-peer overflow
and socket timeout policies still apply.

When this profile is absent, `streamBatchRecords` keeps its existing behavior.
For older receivers with a restrictive frame record limit, use this count-based
mode. Current Feed receivers can set `EndpointOptions::maximumReceiveMessageBytes`
for the expected maximum QTP message body.
Partial writes retain the frame and its source records until completion. Demand
replacement preserves the existing policy for a revoked record in a partially
sent frame.

### Indexed HISTORY

The catalog must declare a supported indexed time layout. The HISTORY subscription and
publication paths both exclude records with more than 128 logical fields or an outbound
layout whose first non-VOID field name starts with `Event`. The QTP writer cannot encode
removals for that layout, including removals generated by snapshot projection.
Such records can still be used in TICKER and STREAM within their normal limits.

Use the four-argument `publishRecord` for an ordinary update or pass
`PublisherHistoryMetadata` as the fifth argument for indexed controls. The supported
bits are `TX_PENDING`, `REMOVE_EVENT`, `SNAPSHOT_BEGIN`, `SNAPSHOT_END`,
`SNAPSHOT_SNIP`, and `SNAPSHOT_MODE` from `events::indexed_event_flags`. A data
row carries every logical field and takes its index from the catalog layout.
`INT64_MAX` is reserved for virtual snapshot boundaries and rejected for data rows.
Set `REMOVE_EVENT` with an empty field span and an exact signed 64-bit `removalIndex`.
The explicit index is preserved independently of how many index bits the data
layout represents. Invalid or mismatched metadata is rejected during admission.

An example sequence starts a snapshot with a full row and `SNAPSHOT_BEGIN`, then
ends it with an empty removal at the lower bound carrying `SNAPSHOT_END | REMOVE_EVENT`.
Later updates can carry `TX_PENDING` until a row clears that flag. The existing
HISTORY collector projects these controls for each subscriber's requested range and
can generate boundary removals. See the [HISTORY collector contract](HISTORY-COLLECTOR.md).
