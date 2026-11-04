# Serialized event attachments

[Русская версия](ATTACHMENTS.ru.md). English is authoritative.

This is the selected portable contract for Message/Configuration receiving APIs,
under [ADR-0014](../../docs/decisions/ADR-0014-portable-attachment-semantics.md).
The [receiving API](../../docs/MISC-EVENTS.md) implements owning projections and explicit local codec
registration. ADR-0014 records the preceding policy decision; concrete public names are specified by the API.
The current [generic field API](../encoding/FIELD-TYPES.md) preserves the bytes already.

## Representation and ownership

An attachment consists of its exact full QD field identity and an owning optional byte sequence.
For standard Message/Configuration the full identity is 249 `SERIAL_OBJECT`. A null field has no
sequence; engaged-empty has a sequence of length zero. Neither state may be replaced by the other.
Bytes, including Java stream headers, unrecognized class descriptors and malformed serialized
content, are preserved exactly. Outer QD length/framing validation still applies.

Full identities 9, 233 and 249 remain distinct even when their bodies match. A typed standard
mapping requires its exact declared field identity; it cannot accept type 9 merely because both
use bytes. A Java byte[] attachment arrives as a serialized Java array inside 249. A String
attachment similarly arrives as serialized bytes, never as an implicit native String.
No Java object, class loader or JVM is constructed by generic reception or attachment storage.

The received attachment owns storage independently of source buffers, schemas, readers,
callbacks and endpoint lifetime. Copying yields independent value semantics; modifying a caller's
input or a copied value cannot change the retained original. Moves transfer ownership under the
event value's documented move rules. Borrowed byte views, if offered, are valid only while their
owning value remains alive and unchanged. Receiving callbacks may borrow their enclosing batch;
retention beyond the callback requires an owning copy. No background decode is scheduled.

Construction/replacement from caller bytes validates the field kind and configured length bound
before publication and copies or explicitly transfers ownership. Validation/allocation failure
leaves the previous value intact. Replacing payload bytes is explicit; a decoded value never
silently replaces the original serialization. No implicit decoded cache changes value equality.
Attachment equality compares field identity, null engagement and every body byte.

## Explicit codecs and lifetime

The caller selects a codec key, expected native output type and format version through an explicit
local decode request. These are application configuration, never inferred from wire bytes, record
name, Java class name, magic bytes or Configuration version. Nothing is added to the wire.
There is no default fallback codec or automatic retry with another codec.

A registry is instance-owned and produces immutable snapshots. Registration is completed before
using a snapshot; duplicate key/version registration is an error and leaves it unchanged. A decode
pins one snapshot and a strong reference to its selected codec through completion. Destroying or
replacing an application's registry cannot invalidate an admitted decode. Codec callback state is
owned by that registration, rather than borrowed from a caller's stack. A decoder never keeps a
borrowed payload beyond the synchronous call. An owning returned native value must remain usable
after attachment, registry, codec and request-context destruction. Pointer/view results that borrow
the input are outside this owning contract.

Calls on one codec registration are serialized; reentry into that same registration is rejected
before invoking its callback. Different independent registrations may run concurrently. Callbacks
compete through nonblocking admission: contention returns a typed busy failure without waiting,
queuing or retry. This also prevents cross-registration nested decode from creating an SDK lock cycle.
Callbacks are invoked without a registry mutation lock. Registration mutation is unavailable through the
immutable snapshot, so codec execution cannot invalidate the selected registration. No asynchronous
callback, cancellation or background task is implied. A future asynchronous API needs a separate
lifetime and completion contract.

Selection validates registration, output type and format version before invoking code, including
for null input. An unknown registration returns an unknown-codec error with the raw attachment
unchanged. A matching registration and null attachment return a successful absent native value
without invoking the codec. Engaged-empty invokes it with a non-null empty byte span. The callback
must consume the complete body; an unconsumed suffix is malformed input unless the selected format
explicitly includes and consumes that suffix. No successful partial result is published.

## Failure and resource guarantees

Explicit decode returns either an owning native result or a typed failure. Required distinguishable
failure meanings are unknown codec, unsupported format version, output-type mismatch, malformed
body, resource limit, allocation failure, codec exception and forbidden same-registration reentry.
The [API](../../docs/MISC-EVENTS.md) specifies concrete result types and resource units. Error diagnostics are bounded
and do not dump payload content or application exceptions. A caught codec exception is translated
to codec-exception failure; `std::bad_alloc` is classified as allocation failure. Ordinary C++
exceptions do not escape the codec call boundary. Process faults are outside the recoverable
exception guarantee.

All failures preserve the exact original body and publish no decoded value. They do not invalidate
other attachments, close a feed, alter Configuration version state or retry automatically. A caller
can retain, inspect or forward opaque data after failure under the existing outgoing capabilities.

Receiving still applies the existing finite encoded-body, payload-work and retained-body limits,
including zero, before allocation. An explicit decode has separate caller-supplied finite input,
output, scratch, work and diagnostic bounds; registration count and retained registration state
also have finite instance limits. A decode cannot raise the receiving limits. Input over its decode
bound fails before invoking the codec. Null requires no body storage; engaged-empty remains engaged
even at a zero-byte limit. The implementation must document accounting units, reservation timing,
overflow checks and failure atomicity before exposing the API.

Registered native codecs are trusted application code. Enforced output/scratch bounds require the
codec's allocation/work context; arbitrary external allocations or non-returning code cannot be
contained by a synchronous C++ callback. Such behavior violates the codec contract and must not be
advertised as sandboxed execution or a hard wall-clock timeout. No arbitrary Java deserialization
is enabled by this mechanism.

## Configuration and text boundaries

Configuration's signed 32-bit event version is ordinary event metadata. Pinned Java drops lower
versions only for TICKER when that field is enabled; [production collector observations](../../tests/fixtures/configuration-version/README.md)
qualify equal versions, STREAM, batch selection and the fresh default-zero record. Codec format-version selection never reads, advances
or resets this state. Decode success/failure is independent of whether a Configuration event was
admitted. Version-filter implementation is a separate receiving concern described by the receiving API.

TextMessage and TextConfiguration keep a separate nullable String API with exact UTF-16 semantics
and their own field/mapping qualification. They are not convenience decoders for serialized Object
attachments. The [oracle fixtures](../../tests/fixtures/attachments/README.md) witness the standard
Text field identity only; they do not qualify the complete Text event API or version filtering.
