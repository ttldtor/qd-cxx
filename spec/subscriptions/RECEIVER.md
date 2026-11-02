# Generic and prepared subscription receiver

[Русская версия](RECEIVER.ru.md). English is authoritative.

Include `qd/client/subscription_receiver.hpp` and link `qdcxx_client` with
`ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON`. The receiver composes the existing
[queue](../mux/QUEUES.md) and [prepared event mappings](../events/EVENT-MAPPING.md).
It introduces no endpoint, scheduler, wire message, schema-name inference or business-event catalog.

## Registration and membership

`openSubscriptionReceiver` checks the exact schema origin, family and immutable selector
policy before preparing typed routes or opening a registration. Equal local IDs and
equal declarations from an independently created schema do not match. All requested
Quote/Profile mappings must prepare successfully. Invalid input, missing required fields,
incompatible types, work exhaustion or allocation failure leaves existing registrations
and desired memberships unchanged. Attempted work can be consumed on refusal.

Success returns an ordinary `MuxHandle` with an empty desired set. Submit the initial set
with its atomic `replace`, then inspect the returned `MuxCommandTicket`. The existing
`add`, `remove`, `replace`, `clear`, pause/resume and stop operations remain available.
Each logical application subscription has its own handle. Removing one handle's interest
does not remove another handle's interest in the same record/symbol.

A returned command ticket acknowledges local acceptance. Its later result records local
application or rejection and the actual revision. Wire emission, peer acceptance and
upstream acknowledgements are separate concerns. A refused initial `replace` does not
undo the already opened empty registration: retry with fresh explicit work, or request stop.

`MuxQueue::validateReceiverBinding` checks the immutable binding of an open queue even
after source admission, staging or reset. It performs no allocation, callback or mutation.
The existing `validateBinding` retains its stricter pristine-source precondition for
transferring a queue into recovery. Both queries and receiver creation require serialized
owner access and reject reentrancy/closing. Existing `open` refusal during other queue
phases is propagated unchanged.

## One callback per batch

The adapter invokes `SubscriptionConsumer::onBatch` exactly once for each underlying
queue batch and returns its existing whole-batch `BatchAction`. It performs no internal
per-record callbacks. `SubscriptionBatch::records()` directly exposes every generic
field, full type/raw bits, null-versus-empty payload, membership, revision, flags, optional
signed indexed `rawTime` and actual source/trigger metadata.

`project(index, work)` synchronously returns an owning `SubscriptionProjection`:

- A prepared `Quote` or `Profile` for an explicitly selected exact record context.
- `monostate` for an unlisted generic record, preserving access to its complete generic values.
- A typed index/work/resource/mapping error without changing delivery or acknowledging another batch.

Input fields are borrowed directly through `core::FieldValueView` and the prepared
`mapView` methods. No intermediate owning field array or payload copy is constructed.
The final Quote/Profile owns its symbol and projected strings under explicit limits.
Generic extra/custom fields remain available in the original record.

The queue's executing-callback guard keeps the adapter, its immutable plan and application
callable alive through the invocation. Existing supported public recovery-owner stop,
destruction and move replacement inside the callback keep their current semantics.
The application controls any iteration or additional `project` calls inside its already
running callback. The adapter returns its action directly; later batches remain subject to
the existing queue stop/pause barrier.
Application exceptions retain the queue's existing per-client failure isolation.

## Lifetimes and event time

`SubscriptionBatch` and its projection service expire when the callback returns or throws.
Copy/move is disabled; saving a pointer/reference does not extend this lifetime.
`retain()` returns the existing independent `OwnedMuxBatch` without allocating. It preserves
only generic backing, not the batch facade, prepared plan, callback, queue or transport.
Already returned projections are owning values and can outlive all of those objects.
For later projection of retained generic records, retain an explicit existing prepared
mapping and use its `mapView` with `BorrowedRecordValueView`.

`UNSPECIFIED` leaves `SubscriptionProjection::eventTimeMs` absent. `FIELD_METADATA` uses
only actual original `record.source.fieldTime`, if present. The legacy business event's
`eventTime` member receives zero when metadata is absent; the optional projection member
retains that distinction. Indexed `rawTime`, removal time and HISTORY trigger time are
never substituted for event time. Synthesized projections do not manufacture source metadata.

## Finite bounds and failures

`ReceiverLimits` bounds route count, summed complete shapes and summed declared selection
allowances. Immutable receiver plans have separate storage from queue reservations;
application captures and retained owning projections remain application-owned memory.
Per-binding `MappingLimits` and `EventValueLimits` keep their existing units. Quote uses
`min(symbolUnits, totalUnits)`; Profile also applies `stringUnits` to each projected string.

`ReceiverWork` debits input/duplicate inspections and a conservative checked sum of each
mapper's declared preparation allowances. `ReceiverProjectionWork` debits index/route/shape
inspections, the declared selection allowance and one timestamp inspection, the selected route's
conservative owned text allowance and logical result/text
copy bytes. Text copy work includes a possible terminator for each owned string. These are
logical work/capacity bounds, not measured allocator overhead, RSS, elapsed time or latency.
Zero is a real limit. Repeated calls debit the same supplied work again; nothing replenishes
credits implicitly. Refused attempts can consume already performed inspection work.

Factory errors preserve `ReceiverError`, existing `MappingError` or existing `QueueFailure`,
plus the binding index where applicable. Projection errors preserve a receiver or mapping
cause. No factory refusal starts a callback, native factory or network operation.

## Field-view compatibility

`core::FieldValue` and its owning validation API remain unchanged. The new `FieldValueView`
and `validFieldValueView` preserve the same canonical tag/range/nullability rules without
owning payloads. `BorrowedRecordValueView`, `validateView` and `mapView` are separate names;
existing owning calls, empty aggregate arguments and member-function addresses stay unambiguous.

`mux::MuxFieldValueView` is an alias of the shared core view with the same member order/types.
Ordinary aggregate initialization, member access and spans retain their source form. Rebuild
consumers when updating: C++ type identity/mangled names change. Code forward-declaring the
old standalone `struct mux::MuxFieldValueView` must include the public declaration and use
its alias instead. Wire encoding and decoded values are unchanged.
