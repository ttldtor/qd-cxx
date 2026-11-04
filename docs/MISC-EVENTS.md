# Message and Configuration

[Русская версия](MISC-EVENTS.ru.md). English is authoritative.

`qd/events/message.hpp` and `qd/events/configuration.hpp` expose owning event values through
`qdcxx_misc_events`. Their symbol, FIELD event time and serialized attachment survive input-buffer,
mapper and endpoint destruction. Configuration also exposes its signed 32-bit event version.
`getAttachment()` returns an immutable `SerializedAttachment`; copying an event copies its bytes.
Keep an owning event copy to retain a callback value after the enclosing borrowed batch returns.

`PreparedMessage::prepare` and `PreparedConfiguration::prepare` bind an explicit record position in a
complete local schema. They accept caller aliases and validate every field, including unselected
extras. Standard payload fields are `Message` and `Configuration`, respectively, with exact full
identity249 (`SERIAL_OBJECT`). Type9 and233 cannot substitute for249. `defaultMessageSchema` and
`defaultConfigurationSchema` create independent standard schemas. `map` and `mapView` require explicit
`AttachmentEventLimits`: UTF-16 symbol units, payload body bytes and combined copied bytes, all finite
and including zero. The combined charge uses two bytes per symbol unit plus body bytes. Mapping
validates before publishing a complete owning result; errors return no partial event.

The public Feed accepts `Subscription<Message>` on STREAM and `Subscription<Configuration>` on
STREAM or TICKER, including `RegisteredEvent` and their explicit `EventSelection` bits. Register
each caller record with the corresponding `EventType` and exact schema context. Native event type
numbers7 and8 identify local dispatch types; they are unrelated to wire record IDs. Ordinary listener,
close, capture-lifetime and callback exception rules remain those of the [Feed API](FEED-API.md).

## Serialized bytes and explicit native decoding

`qd/events/serialized_attachment.hpp` is available through `qdcxx_attachments`. A null attachment has
an absent optional body. An engaged zero-length body remains distinct. `wireKind()` preserves the full
field identity and `bytes()` borrows exact immutable bytes while the owner remains alive and unchanged.
Creation checks identity and the caller's body-byte bound before allocation. Copy assignment prepares
the complete replacement before changing the destination; allocation failure leaves it unchanged.

`AttachmentCodecRegistry::create` takes explicit `AttachmentCodecRegistration` values and finite
`AttachmentCodecRegistryLimits`. It copies keys and callbacks into an immutable shared snapshot.
Choose an application-agreed key, format version, exact field identity and `typeid(T)` output type.
No production codecs are registered automatically. In particular, Java serialization bytes are never
presented as decoded Java objects. A Java byte-array attachment contains a serialized Java array.

`registry.decode<T>(attachment, key, version, limits)` returns `expected<optional<T>, AttachmentError>`.
The requested type must be a copyable owning value. Raw pointers, references, standard string views,
spans and reference wrappers are rejected at compilation; application-defined types must independently
own their dynamic data. Selection validates key, version, output type and field identity even for null.
Validated null returns an absent native value without invoking the callback. Engaged-empty invokes it
with a non-null span of length zero. Success requires the exact output type and complete body consumption.
Unknown codec, version/type mismatch, malformed body, resource refusal and callback failures preserve
the original attachment. Ordinary exceptions become `CODEC_EXCEPTION`; `bad_alloc` becomes
`ALLOCATION_FAILURE`. Diagnostics are enum values and retain no exception text or payload dump.

Decode is synchronous. A registry snapshot pins callback captures through the call. The caller must
use owned captures and independently owning results. Copies share each registration's invocation gate.
Another active invocation returns `CODEC_BUSY` immediately; same-thread recursive use of that registration
returns `REENTRY`. No wait, queue, retry or background callback is introduced. Different registrations
can run concurrently, including nested decode; contention still fails immediately, preventing an SDK
lock cycle between nested callbacks. User callbacks themselves remain trusted native application code.

`AttachmentDecodeLimits` bounds input bytes before invocation. The callback must charge output bytes,
scratch bytes and documented work units through `AttachmentDecodeContext` before allocating or working.
Charges are cumulative and are never refunded; any refused charge prevents a successful result even if
the callback ignores its return value. Output charges include the logical native value, dynamic owned
data and terminators; scratch charges include temporary logical data. Fixed enum diagnostics use zero
diagnostic bytes. Registry storage accounting checks actual SDK allocation requests for wrappers,
shared state, entry storage and keys, plus declared `retainedStateBytes`, before allocation; allocator
bookkeeping and caller-owned inputs are excluded. Declare all dynamic callback/capture state conservatively.
The context cannot enforce arbitrary external allocations, process RSS or termination of user code.

## Configuration version admission

The prepared mapper's optional `versionField()` selects a physical integer field through the existing
mapping rules. Wide integer fields use their first signed32 slot. Absence disables filtering. The
immutable EventRegistry retains this selection from its single preparation step. Only explicitly
registered Configuration routes install the policy in TICKER; a generic record with the same name
retains its existing behavior. The common collector binding can also support independently registered
versioned event families. STREAM preserves all admitted records in source order.

TICKER compares incoming version against the latest staged value for that symbol, including earlier
candidates in the same atomic batch. Lower versions do not replace the cache or reach replay/delivery.
Equal versions remain eligible for ordinary data-change comparison. Signed extrema have no wraparound
ordering. A fresh cache compares against Java's default-zero record: a first negative version produces
an observable event with version0, null attachment and eventTime0. The subscription symbol remains the
requested symbol. A later nonnegative update follows ordinary comparison. An absent Version field
disables this rule. Codec format versions and decode outcomes never affect event admission.

The collector distinguishes projected fields from provenance. `TickerValue::sourceRecord` exposes the
selected normalized zero/null view when required; `rawSourceRecord` retains the original source record.
MUX keeps original raw source metadata and carries `versionDefault` separately so typed projection uses
eventTime0. Raw tracing therefore remains attributable without presenting original metadata as the
default event's public values.

The [production Java witness](../tests/fixtures/configuration-version/README.md) covers default-zero,
signed extrema, equal/lower versions, intra-batch updates, absent mapping and wide integer
mapping. Native `misc_feed_tests.cpp` separately covers late-subscription cache replay. The [attachment
fixtures](../tests/fixtures/attachments/README.md) supply actual standard-record
serialization through delegates, composer/parser and tape paths. See [attachment semantics](../spec/events/ATTACHMENTS.md)
and [ADR-0014](decisions/ADR-0014-portable-attachment-semantics.md) for the portable policy.
