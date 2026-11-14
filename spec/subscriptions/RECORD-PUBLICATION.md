# Local record publication contract

[Русская версия](RECORD-PUBLICATION.ru.md).

A local publisher can be configured with a validated, immutable catalog and its ordered outbound field names. The endpoint owns that configuration. A publication identifies one catalog record, one collector contract and one exact symbol. Use the existing owning `RecordContext` provenance for record identity. Copies of the same validated catalog retain provenance. A context from an independently validated catalog fails even when its numeric record ID matches. Receivers can independently assign their wire IDs.

## Values and admission

A publication supplies exact tagged values in catalog logical order. It preserves raw numeric bits and the distinction between null and empty. On indexed HISTORY removals it supplies the signed removal index and no field values. A HISTORY data row uses the index layout declared by the record and supplies the required indexed values. Applicability and supported flags follow the selected collector and wire profile.

The synchronous call may borrow symbol and value storage. The publisher owns any admitted asynchronous work before returning. The caller may immediately change or destroy its input. Validation and resource reservation precede admission. Invalid catalog identity, record shape, metadata, symbol or limits, insufficient capacity and a closed publisher produce explicit refusals. A rejected single record has no effect on collector state, downstream queues or subscription demand.

Bound each row by field count and owned payload bytes, including symbol bytes, and bound aggregate pending rows and owned bytes. Charge work until applied or discarded, including a row the worker has taken from the FIFO. Distinguish oversized rows, exhausted pending capacity, and allocation failure. Construction or allocation failure before admission releases its reservation and exposes no partial work.

A detached producer handle and an owned record context remain safe after endpoint destruction. Publication and close share an admission fence. An attempt ordered after the fence returns a closed diagnostic. Work admitted before the fence may be discarded during shutdown. See the [publisher contract](PUBLISHER.md).

Admission means the publisher has accepted the work for processing. The STREAM collector preserves FIFO admission order, including across concurrent producers. TICKER may conflate values for the same key. HISTORY applies its bounded indexed, transaction and snapshot rules. Downstream delivery, peer shutdown and queue pressure follow the collector and publisher contracts. Admission provides no peer receipt guarantee. A caller-side multi-record batch API is outside this contract. Producers coordinate submissions for a multi-record HISTORY transaction on the same key.

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
