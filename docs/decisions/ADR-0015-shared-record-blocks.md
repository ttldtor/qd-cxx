# ADR-0015 — Shared schema-bound record storage

[Русская версия](ADR-0015-shared-record-blocks.ru.md). English is authoritative.

Status: internal packed storage and typed receive ownership implemented; public RecordFeed and remaining production-path migration are separate work.

## Context

The current local STREAM path already shares immutable `SharedRecordPayload::Batch` storage through
local event ownership, the STREAM collector, MUX runs and retained delivery batches. Replacing that
mechanism with another payload owner would add complexity. Its row aliases preserve symbols and
fields, while complete schema declarations and source metadata need an additional immutable owner.
The [record ownership contract](../../spec/schema/RECORD-OWNERSHIP.md) requires both declarations and
payloads to survive source replacement and final release without an active network worker.

## Decision

Extend the existing private Batch through `BoundRecordBatch`. Allocate the shared control for the
derived type; base row aliases retain that same control. The derived destructor releases the backing,
then the pure binding and reservations. No virtual dispatch, protocol owner, callback, executor or
arbitrary application `shared_ptr` is stored. The existing unbound Batch factories stay available.

Create `RecordBlockBinding` once for a schema binding. It moves an already owned `LocalSchema` without
copying its declarations, preserves schema provenance, and stores caller-supplied numeric source origin
and generation labels. These labels are retained exactly; this factory neither authenticates them nor
establishes global uniqueness. Blocks and independent range copies share the immutable binding.
Creating or provenance-preserving copying of the input schema is a separate binding-stage operation.

One block makes two allocation requests: its derived shared control and one packed byte array. The
array contains existing row/field descriptors, exact symbols and engaged nullable payloads, followed by
aligned local IDs and complete source metadata. Null differs from engaged empty. Raw scalar bits,
exact UTF-16 units, opaque bytes, sender/cipher/flags, removal time, raw FIELD words and ordinal, and
tape MESSAGE time are retained. Mutable collector state and delivery projections remain in their
current owners. Schema origin and exact field count/type/value are validated before block allocation.

A single compare/exchange reservation admits the complete block request before either allocation.
Permanent oversize, temporary capacity pressure, invalid input and allocation failure have separate
results. Rejection leaves the original input and all prior owners valid and publishes no partial block.
A binding likewise reserves its whole control/schema-capacity request before allocating and moving the
schema; failure preserves the caller's schema. Final releases use only pure atomic capacity state.
Control requests remain charged until allocator deallocation, including outstanding weak controls.
Backing is freed before its reservation is returned.

Explicit range retention takes a separate consumer reservation for the whole physical block and its
associated binding. The candidate lease is move-only; its transfer and destruction preserve one quota
charge. A second retain reserves a second consumer charge, while physical storage remains shared.
Retention allocates and copies no payload. Independent contiguous copying creates a smaller backing
with the same binding, copies only the selected records and uses no temporary descriptor array.
There is no record-count ceiling: byte capacity and representable sizes bound admission.

## Copy and capacity accounting

| Boundary | Payload copying | Metadata and allocation accounting |
| --- | --- | --- |
| Binding creation | No record copy; moves an already owned schema | One exact shared-control request; schema capacity model reported separately |
| Input to bound block | Each symbol and nonempty engaged text/byte payload once | One packed array and one derived shared control; row/field/source descriptor and alignment bytes reported separately |
| Range retention | None | Move-only consumer reservation; no allocation or staging array |
| Independent range copy | Selected payloads once | Two new block requests; binding reused, no schema copy |
| Existing local event handoff | Shared payload retained; field descriptors reconstructed | Existing event arena, record/field descriptors and shared-row references remain charged by the session |
| STREAM collector to MUX | Shared payload retained | Existing `StreamRun`, row-source metadata, payload-reference arrays, one shared ledger wrapper and one lease per client/input batch |
| MUX delivery and callback retain | Shared payload retained | Existing `QueueBatch` view/range arrays and detached quota controls remain authoritative |

`payloadCopyBytes` counts symbol and engaged payload bytes; scalar/tag/nullable descriptor storage is
reported as `fieldDescriptorBytes`. `rowDescriptorBytes`, `recordMetadataBytes` and `paddingBytes`
complete the packed-array equation. `backingAllocationBytes`, `controlAllocationBytes` and the observed
allocation count describe actual allocator requests for the block. Control-layout measurement is cached per concrete type and constructor signature; argument values do not change this allocator/control layout. Cold probes allocate no memory and are counted separately from runtime allocations. Binding control and byte-domain
control requests are reported separately; they are not repeated per block. Instrumented allocation
checks compare the two block requests to these counters on the qualified implementations.

`schemaStorageBytes` is the existing `LocalSchema::storageBytes()` capacity model. It includes fixed
objects, container capacities, conservative inline string capacity and shared provenance attribution;
it is not an observation of schema allocator requests. Its fixed LocalSchema object is subtracted
when the object already resides in the binding-control request. Schema acquisition/copy allocations
are outside block-copy counters. The physical domain charges a binding once; a consumer charges its
whole associated binding per retained block conservatively. Existing component ledgers can attribute
the same retained backing independently. These figures do not establish a unified pipeline ledger,
allocator overhead, process RSS or a throughput improvement.

## Direct receive ownership

Typed live DATA decodes once into a permanent `ReceiveRecordBatch` with a pure PMR arena. The
existing decoder moves its owning buffers; adopting them adds no symbol, text or byte copy after
decode. The row facade supports separately located field descriptors and symbols, including inline
short symbols in frozen decoded records. Core and live field-view types keep separate descriptor
arrays. Final decoded vectors stop moving before either array or a row alias is published.

An immutable `ReceiveCatalog` owns the original `LocalSchema`, moved once during successful session
creation. Its existing dynamic-capacity reservation transfers from the operational base without a
second physical charge. Every earlier allocation failure preserves the caller's schema and composer.
Each frame retains that catalog and the exact selected committed source generation; it retains no
decoder plan, `LiveSnapshot`, `LiveIdentity`, session, callback or executor. The origin label refers
to the retained pure byte-domain identity and cannot be reused while that identity remains alive.
It remains an internal numeric label without global authentication.

The epoch uses one atomic physical byte domain. Its own measured shared-control request, operational
objects, fixed arenas, receive arena blocks, catalog and data controls are charged there. Operational
owner slots remain serialized and retire with their wrappers. Fixed session backing retires with the
framer/composer, even if a pure data owner or serialized identity remains. A detached data destructor
destroys PMR objects before their resource, frees each backing before returning its charge, and uses
no operational release callback. The original allocator retains pure capacity state through final
weak-control deallocation. Final detached release may overlap continuing receive admission.

One existing `eventBytes` or `dataEventBytes` allowance covers the receive control, initial/growth
backing and live/core/row/reference descriptors together. Monotonic abandoned and spare bytes remain
charged. A whole frame publishes only after decode and descriptor construction succeed; malformed
suffixes and late allocation/resource failures publish no prefix and restore candidate reservations.
The packed two-request equation does not apply to arena storage: its shared control, initial array,
actual growth requests, used descriptors/payload and unused capacity are distinct costs. Binding and
physical-domain controls are separate requests. Changed internal descriptors and controls can change
the exact capacity required by a frame while the configured limits retain their meaning.

This existing homogeneous range path requires `compactRecordArena` and empty queues for all active
clients. Otherwise all selected receive rows use the existing copied per-row path.

For homogeneous typed STREAM ranges, MUX chooses independently for each selected client. It retains
the whole frame and catalog only when that capacity, accounting caches/controls and ordinary selected
record controls fit the remaining client and aggregate record domains. Preflight conservatively
allows cold caches so concurrent weak-cache retirement cannot invalidate it; actual reservations
charge only owners and requests really created. Records exceeding an enabled per-batch record-byte
window use the copied path. Shared receive rows use ordinary `QueueRecord` controls, preserving
existing callback assembly across frames. Existing local packed runs keep their implementation.

If whole-owner retention cannot fit, MUX independently copies the selected rows through its existing
bounded record path. Heterogeneous/per-row receive routing also uses that path. A large unselected
row or a client with a smaller quota therefore does not force whole-frame retention. Another client
may retain the original frame. Each accepted alias continues to charge its actual whole owner until
final release; it cannot be compacted retroactively when later input arrives. Simultaneous occupancy
can therefore differ from independent copied records. Fallback copies and their temporary overlap
must be counted separately; the eligible shared handoff alone avoids post-decode payload copying.

The validated `COMPACT_INT_V1` profile selects existing event ownership before decode and retains its
legacy `values` span through copied delivery, including split and heterogeneous frames. Typed
TICKER/HISTORY receive uses the pure frame owner but keeps existing collector projections and copied
delivery; no fanout benefit is claimed for those families. Existing copied public MUX owners retain
their current values, metadata and provenance contract, without a newly exposed full schema catalog.

## Verification and limits

The focused [storage tests](../../tests/conformance/shared_record_payload_tests.cpp) exercise every
supported exact field identity, null/empty/content, schema provenance and foreign rejection, source
generation preservation, selected copying, observed allocation requests, atomic failure rollback,
competing byte reservations, weak-control lifetime and final release on another thread.
The [MUX queue test](../../tests/conformance/mux_queue_tests.cpp) injects the bound owner through the
actual local event, STREAM collector and MUX route, checks identical field addresses for two clients,
and releases the last detached owner after queue destruction. The private binding/source metadata
survive that route; the current public MUX record view does not expose a full schema catalog.

The live-session tests additionally exercise receive freezing, schema/generation lifetime, whole-frame
failure, exact/one-short frame capacity, creation/data allocation faults, weak controls and concurrent
final release. The MUX tests compare shared and copied client addresses, small quotas, split routes,
unselected records, legacy values and existing callback/failure behavior.

This implementation adds no public RecordFeed, common production admission API,
whole publisher-queue migration, wire format or throughput claim. Existing public callback lifetime,
`LiveEvent` release affinity and queue policies retain their contracts. Complete TICKER/HISTORY
migration and paired end-to-end performance measurements require their own implementation evidence.
