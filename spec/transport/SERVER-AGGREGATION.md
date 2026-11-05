# Requested server aggregation

[Русская версия](SERVER-AGGREGATION.ru.md) · [Feed API](../../docs/FEED-API.md)

The endpoint exposes three separate observations: current local optional request, a local
configuration revision, and the current source's optional peer-reported min/max interval.
An absent request asks for the server default; zero requests no aggregation. The public setter
accepts integral milliseconds in 0..86400000 inclusive. This is the production public
QDEndpoint/ChannelShaper validation bound, verified through DXEndpoint; it is not inferred from a
server scheduler clamp. Invalid values reject atomically before revision or native output changes.

EndpointOptions.requestedAggregationPeriod is the typed initial value. The configuration key
dxfeed.requestedAggregationPeriod accepts the existing QD TimePeriod grammar, bounded to 24 hours.
Construction's connector alias requestedAggregationPeriod is ASCII case insensitive, like the
production connector property. An empty inline alias resets to absent. An empty canonical endpoint
property is invalid, matching TimePeriod.valueOf. Property precedence remains typed fallback,
explicit defaults, explicit overrides, then ordered inline connector properties. Construction
never activates connectivity.

EndpointConfiguration remains immutable. Endpoint::getEffectiveConfiguration copies that original
snapshot and, after an explicit setter change, replaces only the requested-aggregation row with
current local intent, RUNTIME_UPDATE origin and requestedAggregationRevision. Every other property
retains its construction value and timing. Peer min/max never enters a local property row.
Equal setter values do not increment the revision; revision exhaustion rejects without mutation.

## Existing wire and timing

The current receiving contract's RECEIVE DATA message carries requestedAggregationPeriod:
canonical TimePeriod text, or an explicitly empty string for reset. Initial/fresh-attempt descriptors
include the current value; online changes produce a property-only DESCRIBE_PROTOCOL delta.
TICKER_DATA=10 and STREAM_DATA=15 use the same property. The shared recovery helper also handles
an already-configured HISTORY_DATA=20 route without introducing a new high-level delivery mode.
No message ID, acknowledgement or protocol version is added.

The setter commits owned local intent under the endpoint mutex and wakes the existing runtime.
It neither starts a worker nor reconnects. Pending changes coalesce into one latest fixed-size
value and share the existing single bounded control-output slot; no request queue is added.
A previously admitted frame remains owned until its ordinary completion. Updates in flight may
therefore produce intermediate requests. A fresh attempt sends the latest local intent.
Before connect, after disconnect and after close, the value remains inspectable and mutable
without native activity. This matches the production setter's local-intent behavior after close.

Peer SEND DATA aggregationPeriodInfo is independent. Production Java sends compact JSON seconds
with min/max, may clamp a request to its channel/connector range, and can report an older value
before the next response. There is no distinct refusal ACK or correlation ID. The native API
must never manufacture an effective result from the request. Missing legacy information,
malformed/UNKNOWN information, offline/fenced/closed state and a fresh source without a report
return std::nullopt. A known 0..0 interval remains distinct from UNKNOWN. An omitted delta
property retains the accumulated old report, as in the production descriptor merge. A nullable
local property inherits the top-level value; an explicit empty local value overrides it.
Known reports from configured matching families combine minimum=min(minima), maximum=max(maxima).
Peer ranges are nonnegative signed-64 millisecond values and are not restricted by the request cap.

The source report is fenced by connection-control revision immediately on disconnect/reconnect,
and by actual recovery source replacement. A late old-source publication cannot pass that fence.
Native access stays on the runtime owner thread; public reads copy values under the endpoint mutex.
No application callback runs under parser, endpoint or native container locks.

## Bounded parser and failure behavior

The parser recognizes the pinned TimePeriodInfo source pattern, not arbitrary JSON. It requires
the exact compact min-then-max prefix and Java numeric syntax, rounds seconds using Math.round
semantics and saturates to signed-64 bounds. Negative/inverted rounded ranges are UNKNOWN.
The pinned extension alternatives include quoted strings, flat objects/arrays and a bare scalar
alternative; the latter deliberately accepts some nested-looking text. Raw Java outcomes remain
unchanged. The 68-row oracle includes these non-JSON-library edge cases.

One value is limited to 65536 UTF-16 units, within the admitted live property envelope. All examined
text, scan visits and scratch initialization consume caller work. Fixed stack scratch is bounded
to roughly 128 KiB for number/tail recognition; it retains no peer text and allocates no heap.
No recursion or JSON dependency is introduced. Out-of-profile values yield UNKNOWN. Short turn
work retains the owned protocol event and retries before publishing/releasing it; a partial
multi-family range is never published. Existing finite event/resident/output ownership limits
continue to apply. No external worker, network service or SDK profile is added.

## Primary evidence

[DXEndpoint API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java),
[DistributorAdapter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/DistributorAdapter.java),
[AgentAdapter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AgentAdapter.java)
and [TimePeriodInfo](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimePeriodInfo.java)
are bound to the [production oracle](../../tools/upstream/server-aggregation-oracle/README.md).
The raw fixtures include binary full/delta/reset, clamp, legacy/invalid, inheritance, STREAM and
a real production SocketConnector reconnect. Native tests consume these exact wire bytes and
compare emitted request deltas, rather than accepting a request echo as negotiation.

## Intentional numeric compatibility correction

For min and max both equal to 10000000000000000000e2147483647 seconds, the pinned Java
FastDoubleUtil parser overflows its internal signed-int exponent subtraction and reports a known
0..0 interval. The appended raw oracle row preserves that actual outcome. Native deliberately
uses overflow-safe exponent handling and reports INT64_MAX..INT64_MAX milliseconds, the saturated
positive range. It does not reproduce the Java arithmetic defect. Ordinary numeric observations
remain separately compared; complete numeric equivalence is not claimed for this overflow case.
The baseline 1e1000 remains saturated in both implementations.
