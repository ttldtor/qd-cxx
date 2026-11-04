# Instance-local endpoint configuration

[Русская версия](ENDPOINT-CONFIGURATION.ru.md). English is authoritative.

The public contract is declared in qd/api/endpoint_configuration.hpp and explained in the
[application guide](../../docs/ENDPOINT-CONFIGURATION.md). This contract extends the existing
[Feed lifecycle](../../docs/FEED-API.md), preserving first-address identity, offline desired
subscriptions, finite recovery and joined native/callback retirement.

## Resolve, admit, activate

1. Resolve explicit layers into an immutable data-only configuration. Bound all borrowed entries
   before retaining input. Unknown keys and unsupported connectors reject. Validate the selected
   route through the same pure host grammar used by client endpoint validation.
2. CreateEndpoint retains the configuration, validates/reserves ordinary endpoint resources and
   publishes only a complete inactive endpoint. A preparation failure publishes no partial slot.
3. Explicit connect validates/copies its plain route before the existing runtime/endpoint admission
   locks. Revision and state-notification admission precede worker creation and commit.
4. Read policy from the immutable retained configuration. Read accepted address, intent and revision
   together under the endpoint mutex. Copy failure leaves lifecycle/admission unchanged.
5. Disconnect/close preserve immutable policy and accepted-address history. Terminal fences remove
   activation intent from the observation. Keeping snapshots/facades retains no native runtime.

The resolver performs no I/O or callbacks. It adds no worker, dispatch queue or callback ownership.
No application capture is constructed or destroyed by configuration inspection. Existing data/state/
subscription observer fences and closeAndAwaitTermination rules remain unchanged.

## Compatibility and finite ownership

Property layer order, grammar, ranges, selected-value validation, finite counts/bytes and errors
are normative in the application guide. Native name is metadata; it does not reconfigure SDK
threads. Requested retry delay is retained even when one-attempt effective delay is zero.

The [pinned production oracle](../../tools/upstream/endpoint-configuration-oracle/README.md)
preserves actual Java observations. Supported flat reconnectDelay uses verified milliseconds.
Native construction-time resolution, explicit activation, immutable snapshots, strict booleans,
unknown-key rejection and narrower route syntax are explicit deviations. No ambient settings
precedence, Java connector extension or secure credential capability is implied.

Rejected credential/userinfo values enter no retained effective storage or formatted diagnostics.
This is an exclusion/redaction boundary, not a secure secret store. TLS/authentication and the
broader connector catalogue retain their separate capability requirements.