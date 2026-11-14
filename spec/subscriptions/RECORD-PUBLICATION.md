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
