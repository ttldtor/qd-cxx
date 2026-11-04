# ADR-0014 — Portable attachment semantics

[Русская версия](ADR-0014-portable-attachment-semantics.ru.md). English is authoritative.

Status: portable policy selected; independent contract review and typed event implementation are separate gates.

## Context

Pinned QD exposes `Object` attachments on Message and Configuration. Their setters use
`Marshalled.forObject`; getters can lazily deserialize with application classes and a class loader.
The standard record's attachment is full field identity 249 `SERIAL_OBJECT`. A Java `byte[]`
attachment is itself serialized, including its array descriptor and length. It is different from
the body of a full identity 9 `BYTE_ARRAY` field. TextMessage/TextConfiguration use a separate
String API and their standard Text field is full identity 10.

The native generic reader already owns exact nullable field bodies for 9/233/249, without Java
object execution. Calling those bodies decoded Java objects would promise behavior it cannot
provide. [Production witnesses](../../tests/fixtures/attachments/README.md) bind the distinction
to actual standard records, delegates, composer/parser callbacks and FileWriter output.

## Decision

Adopt portable C++ with exact owning nullable serialized attachment bytes and explicitly selected
registered native codecs. No JVM is required. Receiving, retaining, copying, comparing or forwarding
an attachment never implicitly decodes it. Raw bytes are never represented as a decoded Java Object.

The [attachment reference](../../spec/events/ATTACHMENTS.md) defines ownership, codec selection,
errors, resource limits, callback lifetime and version separation. It constrains the subsequent
typed implementation; it does not introduce public type names or a production codec registry.
The existing generic reader remains usable under its existing field/profile contracts.

No codec identifier, header, envelope or new payload format is inserted into QD wire data.
Applications agree on a payload representation and choose its codec through explicit local
configuration. Codec format versions remain separate from Configuration's event version.
Text variants retain their independent nullable String contract.

## Alternatives

- A JVM bridge could supply class loading and arbitrary Java serialization compatibility. It
  requires a separate runtime, deployment, resource and security contract and was not selected.
- Native decoding of a finite list of Java classes can be an explicit codec. It cannot promise
  arbitrary `Object` compatibility or reinterpret unknown payloads as known values.
- Automatic format sniffing would make decoding implicit and ambiguous; it is excluded.

## Consequences and compatibility

An application that requires Java object semantics must supply a compatible explicit codec or use
a separately designed bridge. Unknown classes, malformed Java serialization and even engaged-empty
bodies can remain valid opaque received data. A later explicit decode may fail without losing that
data. Null is different from engaged-empty and a serialized empty Java array.

Configuration version filtering remains the responsibility of the receiving implementation and
its selected contract. Payload decoding cannot accept, reject or advance event-version state.
No wire grammar, schema identity, existing limits or outgoing-field support changes here.

## Primary references

- [Message Object access](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/Message.java#L114),
  [Configuration and version semantics](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/Configuration.java#L26).
- [Marshalled lazy representation](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/Marshalled.java#L21),
  [standard misc fields and delegates](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/misc/MiscFactoryImpl.java#L38).
- [Native exact field contract](../../spec/encoding/FIELD-TYPES.md#byte-utfstring-and-opaque-object-values).
