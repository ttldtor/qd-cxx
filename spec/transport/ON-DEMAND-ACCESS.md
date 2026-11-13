# OnDemand replay capability and access

[Русская версия](ON-DEMAND-ACCESS.ru.md)

In the Java QD revision recorded in `upstream.lock`, `OnDemandService`
loads its provider from the optional `dxfeed-ondemand` module. A `FEED`
or `ON_DEMAND_FEED` endpoint needs an `ondemand:` connector for replay.
The Java implementation checks the presence of the connector in
`isReplaySupported()`. That check alone does not establish service
reachability or permission to receive a requested dataset.

The replay connector uses a separate service protocol. The ordinary QTP
demo feed, the historical tick extraction API and local file playback
provide different access paths. A service acceptance test needs an
authorized replay endpoint, replay entitlement and an event type,
symbol and historical interval covered by that entitlement.

The current C++ Feed rejects Java's parenthesized `(ondemand:<address>)`
form synchronously with `UNSUPPORTED_PROFILE` before attempting a connection. Local
playback is available through [the tape API](../protocol/FEED-TAPE-SOURCE.md).
No live OnDemand replay result has been verified.

Upstream source anchors: `OnDemandService.getInstance`,
`OnDemandServiceImpl.isReplaySupported`, `OnDemandConnectorFactory` and
`MarketDataReplay` in the pinned Java QD revision.
