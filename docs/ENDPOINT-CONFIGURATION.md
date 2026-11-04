# Endpoint addresses and effective configuration

[Русская версия](ENDPOINT-CONFIGURATION.ru.md) · [Feed API](FEED-API.md) · [Configuration architecture](CONFIGURATION.md)

Include qd/api/endpoint_configuration.hpp and link qdcxx_feed. This profile configures
the existing plain binary-QTP receiving endpoint. Resolution copies explicit caller input,
validates it and performs no DNS, file lookup, SDK activation or application callback.

## Configure and connect

The fragment assumes an api::ApiRuntime named runtime, owned on main.
Register subscriptions and callbacks through the [Feed API](FEED-API.md), then connect explicitly.
The library owns progress; an application does not poll configuration.

~~~cpp
#include "qd/api/endpoint_configuration.hpp"
#include <array>

namespace api = org::ttldtor::qdcxx::api;

const std::array defaults{
    api::EndpointProperty{"qdcxx.maximumAttempts", "3"},
    api::EndpointProperty{"qdcxx.connectTimeoutMillis", "5000"}};
const std::array properties{
    api::EndpointProperty{"dxfeed.address", "localhost:7300[reconnectDelay=250]"},
    api::EndpointProperty{"qdcxx.name", "quotes"}};
auto configuration = api::EndpointConfiguration::create({}, defaults, properties);
auto endpoint = runtime.createEndpoint(configuration);
auto feed = endpoint->getFeed();
// Install the application's subscriptions and callbacks here.
endpoint->connect();
auto observation = endpoint->getEffectiveConfiguration();
// During application shutdown, on the runtime creator thread:
auto terminal = endpoint->closeAndAwaitTermination();
~~~

Construction never connects. Endpoint::connect() uses the configured route and rejects if
none exists. The existing connect(address) overload accepts a plain route and may choose a
different first route. Once admitted, that exact spelling is fixed; disconnect/reconnect retain
it. Inline properties passed to this overload reject rather than changing immutable policy.
Ordinary lifecycle, callback retirement and creator-thread await rules remain in force.

## Sources and read timing

Precedence is typed EndpointOptions fallback, explicit defaults, explicit properties,
then supported address properties. Last duplicate wins within each layer. Explicit false and
empty native names remain explicit values. No layer reads system properties, environment,
configuration files or process globals. Only the selected address is parsed; every supplied
entry, including shadowed entries, consumes the bounded input envelope and its key must be
supported. Selected values are fully validated.

EndpointProperty views are borrowed only during create(). The result owns all retained
data and the immutable optional event registry. Changing or destroying caller input after success
does not affect it. Distinct retaining handles support concurrent reads/copies; replacement or
destruction of the same handle requires caller synchronization. A moved-from handle may only
be destroyed or reassigned.

## Supported policy

Keys below are case sensitive. The inline connector alias reconnectDelay is ASCII case
insensitive and means milliseconds, as in the pinned Java consumer.

| Key | Default | Accepted value |
| --- | --- | --- |
| dxfeed.address | absent | Validated plain route, optionally followed by flat property blocks at construction |
| qdcxx.name | empty | Printable ASCII local metadata label, at most 128 bytes; does not rename SDK threads |
| qdcxx.deliveryContract | ticker | ticker or stream |
| dxfeed.wildcard.enable | false | ASCII case insensitive true/false; true requires STREAM |
| qdcxx.maximumAttempts | 3 | Complete unsigned decimal, 1 through 4294967295 |
| qdcxx.connectTimeoutMillis | 5000 | Complete unsigned decimal, 1 through 2147483647 milliseconds |
| qdcxx.dnsTimeoutMillis | 5000 | Same millisecond range |
| qdcxx.readTimeoutMillis | 30000 | Same millisecond range |
| qdcxx.writeTimeoutMillis | 5000 | Same millisecond range |
| qdcxx.pollIntervalMillis | 1 | Same millisecond range; internal worker cadence |
| qdcxx.retryDelayMillis | 250 | Same range with zero allowed only for one attempt |
| qdcxx.drainTimeoutMillis | 1000 | Same positive millisecond range |

Typed fallback fields retain their API defaults, including custom registry identity.
With one attempt the consumer receives zero retry delay, while the observation preserves the
selected requested delay separately. Numeric signs, partial conversions and surrounding spaces
in endpoint property values reject. Inline cells trim ASCII spaces around keys and values.
Malformed boolean text rejects.

Ordinary host names use bounded ASCII/A-label DNS labels; Unicode names require explicit
caller-side IDNA conversion. Numeric IPv4 uses four decimal octets without leading zeros.
IPv6 uses brackets around the host. Ports are complete decimal 1 through 65535. The existing
high-level ceiling is 253 host bytes and 261 route bytes; DNS labels are at most 63 bytes.

Construction allows trailing square-bracket blocks, for example:

~~~text
localhost:7300[reconnectDelay=250,qdcxx.name=quotes][qdcxx.maximumAttempts=3]
~~~

Blocks retain original order. Native name/timeouts/attempts and the verified reconnectDelay
alias are supported inside them. Address, delivery-contract and wildcard keys in these blocks
reject. Nested blocks, parentheses, escaping, URI/codec prefixes, host lists, proxy/listen/file
connectors and credentials reject. This profile adds no TLS or negotiated authentication.

## Inspection, errors and limits

snapshot() returns eleven owning canonical policy rows and the optional stripped configured
address. getEffectiveConfiguration() additionally copies the first accepted route spelling,
current admitted connect intent and control revision under the endpoint mutex. Local policy,
metadata and accepted requests do not establish DNS resolution, peer delivery or negotiated
server values. The snapshot remains readable after endpoint/runtime retirement.

EndpointConfigurationLimits bounds combined entries (64), key bytes (96), value/address
bytes (4096) and scanned key/value bytes (16384). Inline entries consume this same envelope
in addition to their containing address value. Output has eleven policy rows and at most two
route strings. These are logical input/output limits, not an exact allocator/RSS cap. Ordinary
runtime, registry, network and retained-copy limits continue independently.

Unsupported keys/forms throw UNSUPPORTED_PROFILE; malformed supported values throw
INVALID_ARGUMENT; input/storage capacities throw RESOURCE_LIMIT or ALLOCATION_FAILURE.
Errors use static labels and never format caller keys, addresses or values. Credential keys
and userinfo routes are rejected and never retained in effective snapshots. Pass credentials
only through a separately supported secure handoff when available; putting them in local labels
or ordinary host strings is outside this profile. Caller input has no secure-erasure guarantee.
Validation failure publishes no endpoint slot or connection intent and starts no worker.

## Java baseline and capability boundary

The [pinned DXEndpoint API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L688-L868)
and [44 production observations](../tests/fixtures/endpoint-configuration/oracle.tsv)
are documented by the [oracle runner](../tools/upstream/endpoint-configuration-oracle/README.md).
Raw outcomes remain Java outcomes: its socket parser permits zero/negative ports and multi-host
lists; its connector supports parentheses and additional properties. Native validation rejects
those unavailable forms before activation.

Java FEED construction auto-connects its selected address; STREAM_FEED construction does not.
Java inline connector properties apply at connector creation. The native profile resolves its
supported inline subset during immutable construction and requires explicit activation.
Java builder mutation is visible through the existing endpoint implementation's property map;
native retained snapshots remain immutable. Java ignores unknown builder keys and treats
malformed boolean text as false at its boolean consumer; native input rejects both.
Java file defaults precede system defaults, with explicit builder values winning; native layers
are supplied explicitly and perform no ambient discovery. These are explicit compatibility
boundaries, not assertions that the Java behavior is erroneous.

The broader configuration catalogue, TLS/authentication handoff, failover/multiple hosts,
proxy/listen/file connectors and server aggregation remain separate capabilities. This profile
does not claim the complete receiving-client milestone.