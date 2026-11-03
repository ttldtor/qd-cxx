# Indexed and order event source identities

[Русская версия](EVENT-SOURCES.ru.md). English is authoritative.

The independent C++23 target qdcxx_event_sources exports
[IndexedEventSource](../../include/qd/events/indexed_event_source.hpp) and
[OrderSource](../../include/qd/events/order_source.hpp) in
org::ttldtor::qdcxx::events. qdcxx_feed links it publicly. The pure target needs
no SDK, network, ambient configuration or startup.

The reference is [upstream.lock](../../upstream.lock), revision
0ddc0a09779fb61d74cc0b37c6449ff47dcb715f,
dxfeed-api/src/main/java/com/dxfeed/event/IndexedEventSource.java and
dxfeed-api/src/main/java/com/dxfeed/event/market/OrderSource.java.
Production observations take precedence over the nonnegative-ID base-class Javadoc.

## Identity and resolution

IndexedEventSource(id, name) accepts every signed 32-bit ID and an owned
optional UTF-16 string, including empty strings, NUL and unpaired surrogates.
nullopt represents Java null. No Unicode normalization occurs.

Equality ignores names and source families, using only id().
hashCode() returns the exact signed ID. std::hash converts it to size_t;
the result is a C++ container hash, without a signed Java representation promise.

OrderSource::valueOf(optional UTF-16 view) first resolves exact case-sensitive
built-in names. Otherwise it accepts one to four ASCII letters/digits (0-9A-Za-z),
packing successive units into high-to-low bytes. valueOf(int32_t) first resolves
built-in IDs; otherwise it removes leading zero bytes and requires every remaining
byte to be ASCII alphanumeric. Embedded zero bytes, negative IDs and unknown small
special IDs are invalid. ID zero resolves DEFAULT.

| ID | Built-in name |
| --- | --- |
| 0 | DEFAULT |
| 1, 2 | COMPOSITE_BID, COMPOSITE_ASK |
| 3, 4 | REGIONAL_BID, REGIONAL_ASK |
| 5, 6 | AGGREGATE_BID, AGGREGATE_ASK |
| 7, 8, 9 | COMPOSITE, REGIONAL, AGGREGATE |

All other built-ins use their encoded names, including NTV. All pinned built-ins
have constants. Java lowercase names use C++ uppercase constants with _LOWER
(for example Java ntv → NTV_LOWER); name() and valueOf retain exact Java spelling.
See the [captured inventory](../../tests/fixtures/event-sources/oracle.tsv).

## Ownership and failures

Both types are owned values with independent copies and no interned object-address
guarantee. OrderSource uses composition; indexedSource() borrows its base identity.
Names are borrowed views. Views/references expire on owner destruction, assignment
or moving from the owner. Inputs to valueOf are borrowed only during the call.
Read-only access to unchanged values requires no shared cache.

Malformed order names/IDs throw std::invalid_argument. Java null's
NullPointerException explicitly maps to std::invalid_argument too.
Allocating operations propagate ordinary std::bad_alloc/std::length_error without
exposing partial returned values. Error text is not a compatibility contract.
Base construction does not reject nullable names or signed IDs.

Source identity is distinct from a regional exchange code and physical route origin.
Capabilities, Order record mapping/index packing, source subscriptions and new wire
events are outside this API.

## Conformance

The [oracle tool](../../tools/upstream/event-source-oracle/README.md) invokes actual
pinned production classes with provenance and class inventories. Its 311 observations
cover all built-ins and round-trips, invalid names/IDs, nullable/empty/malformed UTF-16
base names, duplicate IDs with different names, and cross-family equality/hash.
The focused C++ test consumes these observations directly.
[Usage](../../docs/EVENT-SOURCES.md) covers the independent installed API.
