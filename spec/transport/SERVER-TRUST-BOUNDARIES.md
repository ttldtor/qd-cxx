# Server trust and resource boundaries

[Русская версия](SERVER-TRUST-BOUNDARIES.ru.md). English is authoritative.

This page distinguishes the current plaintext publisher from the accepted future
architecture. It does not add an API or change the current implementation.

## Current listener and publisher

[`TcpListenerOptions`](../../include/qd/transport/tcp_listener.hpp) defaults to the
numeric IPv4 loopback address `127.0.0.1`, port `0` (an ephemeral port), backlog
`16` and `maxConnections` `16`. It accepts one numeric IPv4 or IPv6 bind address;
this is a local bind choice, not a remote identity or access policy. IPv6 listeners
set `IPV6_V6ONLY`, so an IPv6 bind does not accept IPv4 peers. A non-loopback
or wildcard bind can expose the listener to other hosts subject to the operating
system and network configuration. Choose the address and network controls for the
intended trust boundary; a loopback default does not guarantee isolation from other
local processes.

`maxConnections` reserves accepted-socket slots, including sockets awaiting a retry
of failed retirement. The standalone listener accepts values from 1 through 4096.
At capacity, `accept()` returns `RESOURCE_LIMIT` without consuming the native
backlog. `backlog` is a separate positive request passed to the operating system's
`listen`; its effective queue behavior is platform controlled, and it does not
increase the accepted-slot limit. See the [listener implementation](../../src/transport/tcp_listener.cpp).

[`PublisherOptions`](../../include/qd/api/publisher.hpp) overrides the listener
slot default to `4`; `PublisherEndpoint::create` accepts 1 through 8 and rejects
other values with `INVALID_ARGUMENT` at `publisher-limits`. The listener backlog
remains independently configurable. The [publisher contract](../subscriptions/PUBLISHER.md)
describes its per-client demand, publication and failure limits. These bounds are
implementation-profile limits, not universal QTP limits.

The current publisher uses plain TCP and QTP. Its listener has no server-side TLS
or client-authentication configuration. Numeric peer-address demand policies select
subscription filters; they are not proof of client identity or authentication. Do not
rely on them as an authentication boundary. The separate [Feed TLS profile](../../docs/FEED-API.md)
verifies the server from the client side; enabling it does not provide TLS or
client authentication for this publisher.

## Accepted future contract

The accepted architecture assigns resource and security policy to the consuming
component. Client capacity, operating-system listen backlog, authentication,
external exposure and local bind-address scope are distinct decisions. A future
server must state each supported choice and its effective scope; selecting a bind
address or a subscription policy must not be represented as authenticating a peer.
Server TLS support and its credentials, trust and verification behavior require their
own explicit contract and qualification, independent of client TLS. No server TLS
configuration or additional numeric limit is specified by this page.

For supported configuration, document type, units where applicable, owner, default,
validation and whether a change applies immediately, on reconnect or on restart.
Requested and effective values must be distinguishable; unsupported settings must
not appear effective, and secrets must be redacted. Validate structural configuration
before resource activation. Existing profile limits and error behavior remain until
an independently implemented and verified replacement changes them. Publication and
subscription policies retain their own resource and ownership contracts.

Verification of a future implementation must separately cover capacity exhaustion,
backlog behavior on the qualified platform, bind scope and actual reachability,
authentication success and failure, and server TLS when supported. A local bind,
a client TLS test, or one shared mechanism alone does not prove the other cases.
