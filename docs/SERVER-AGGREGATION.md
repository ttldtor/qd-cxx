# Server aggregation

[Русская версия](SERVER-AGGREGATION.ru.md) · [Feed API](FEED-API.md)

Use EndpointOptions.requestedAggregationPeriod for initial policy, or the explicit
Endpoint::setRequestedAggregationPeriod setter for a current endpoint. Values are optional integral
milliseconds from zero to 24 hours. Zero requests no server aggregation; std::nullopt resets to the
server default. The immutable configuration key is dxfeed.requestedAggregationPeriod with QD
TimePeriod text; a construction address can use the connector alias requestedAggregationPeriod.

~~~cpp
#include <qd/api/endpoint_configuration.hpp>
using namespace org::ttldtor::qdcxx;
using namespace std::chrono_literals;

api::ApiRuntime runtime;
api::EndpointOptions options;
options.requestedAggregationPeriod = 5s;
auto endpoint = runtime.createEndpoint(options);
endpoint->connect("localhost:7300");

// A later application action changes only server-aggregation intent.
endpoint->setRequestedAggregationPeriod(250ms);
const auto requested = endpoint->getRequestedAggregationPeriod();
const auto reported = endpoint->getAggregationPeriodInfo();
// reported may be absent or describe the previous request. It is never a local echo.

endpoint->setRequestedAggregationPeriod(std::nullopt);
static_cast<void>(endpoint->closeAndAwaitTermination());
runtime.closeAndAwaitTermination();
~~~

Ordinary use needs no polling or custom worker. A setter commits local intent and the existing SDK
worker sends the latest pending value. It does not activate an offline endpoint, reconnect it or wait
for a server response. Repeated updates can coalesce. Before connect, after disconnect and after close,
local intent remains inspectable and mutable without network work.

getAggregationPeriodInfo returns an owning optional AggregationPeriodInfo with minimum/maximum
milliseconds from the current peer. The server may clamp a request; reports can lag requests.
Legacy peers and malformed/unknown reports yield no value. Offline, fenced, closed and fresh
connections without information also yield no value. A known zero interval is different from absent.
There is no acknowledgement or request/report correlation. Never treat requested as effective.

EndpointConfiguration stays immutable. getEffectiveConfiguration returns a new owning local snapshot:
after an explicit setter change only the aggregation row has current intent, RUNTIME_UPDATE origin
and a new requestedAggregationRevision. Other properties and their timing remain unchanged.
Peer reports never enter that local snapshot. Invalid values reject atomically.

The [wire contract](../spec/transport/SERVER-AGGREGATION.md) specifies descriptor deltas, reset,
reconnect, bounded parsing and failure behavior. The [production oracle](../tools/upstream/server-aggregation-oracle/README.md)
contains pinned Java outcomes and actual full/delta wire bytes. This feature uses the existing plain
Feed receiving path and adds no TLS, authentication or server capability.

## Intentional numeric compatibility correction

For min and max both equal to 10000000000000000000e2147483647 seconds, the pinned Java
FastDoubleUtil parser overflows its internal signed-int exponent subtraction and reports a known
0..0 interval. The appended raw oracle row preserves that actual outcome. Native deliberately
uses overflow-safe exponent handling and reports INT64_MAX..INT64_MAX milliseconds, the saturated
positive range. It does not reproduce the Java arithmetic defect. Ordinary numeric observations
remain separately compared; complete numeric equivalence is not claimed for this overflow case.
The baseline 1e1000 remains saturated in both implementations.
