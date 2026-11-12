# Indexed event model

[Русская версия](INDEXED-EVENT-MODEL.ru.md). English is authoritative.

`IndexedEventModel<Event>` retains typed indexed events after delivery. Each
partition has an exact logical UTF-16 symbol and signed source ID. A call to
`processEvents` accepts one partition and validates the key of every event.
The symbol includes any regional suffix or canonical Candle attributes present
on the typed event. A null Candle symbol is refused. Index comparisons use the
complete signed 64-bit public index.

- Ordinary events remain pending until an event clears `TX_PENDING`.
  Completed ordinary transactions within one call share an update group in
  arrival order. Raw event flags and removal records are preserved in that group.
- `SNAPSHOT_BEGIN` discards unfinished work for that partition. Its previous
  visible values remain available. The replacement becomes visible after
  `SNAPSHOT_END` or `SNAPSHOT_SNIP` and the end of the associated transaction.
  A boundary received before a begin is processed as an ordinary event.
- A completed replacement clears previous values in its partition, including
  for an empty snapshot. A snip is reported separately and does not prove full
  upstream coverage. Snapshot values retain first-insertion order, duplicate
  indices take the last value, and removal followed by reinsertion starts a new
  insertion position.
- The retained view keeps the latest surviving value for each index in ascending
  signed order. Indexed flags are cleared in retained values and snapshot
  notifications. Business flags and other payload fields keep their original
  values. Inputs are unchanged.

The model exposes completed changes only when the whole call succeeds. A
key mismatch, finite-limit refusal, or allocation failure preserves both the
prior visible view and unfinished input. `IndexedEventModelLimits` bounds exact
partition count, per-symbol UTF-16 units, events per call, and total unfinished
and retained event counts. Limits apply to the retained state after each complete
transaction and to unfinished events as they arrive. Scratch storage copies
only the selected partition, the input processed in this call and the changes
returned by it. Event counts describe owning values rather than allocator bytes.
The input owner supplies already bounded business payloads when a byte bound is
required. A partition with no retained or unfinished events releases its slot.

Callers serialize access and own subscription and reset policy. The model has
no callback or runtime reference. Returned results own their data and survive
`clear`, subsequent changes or model destruction. A retained view by itself
does not establish upstream history coverage, current connectivity or readiness
for a range query.

The reference state transitions are in the pinned Java QD
`com.dxfeed.model.TxEventProcessor` and
`com.dxfeed.model.AbstractIndexedEventModel.Source`. The detached C++ API returns
changes after the call instead of exposing Java listener or live list ownership.
