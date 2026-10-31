# QD and qd-cxx knowledge base

[Русская версия](README.ru.md). English is authoritative.

This index helps library contributors and application authors find the requirement, evidence and
decision behind a behavior. It separates Java QD compatibility from native design choices and open
questions. Existing specifications remain authoritative; this index introduces no API or wire format.

## Reading paths

- For a required capability or an unresolved design question, start with the
  [requirements ledger](REQUIREMENTS.md).
- To follow a claim through source, observable behavior and checks, use the
  [decision trace](DECISION-TRACE.md).
- To use an implemented component, follow its contract from the source index below and check the
  corresponding validation profile before relying on it.

## Evidence baseline

The native reference for this edition is commit `7580559f97419f6c43dd520ec4a622a5d5969ea2`.
Java QD is pinned to version 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, by
[upstream.lock](../../upstream.lock). The published [subscription capture profile](../../tests/fixtures/subscriptions/CAPTURES.md)
and its manifest identify the Java observations being used. Source inspection, production observations
and native qualification are different evidence levels; each claim retains its own profile.

STREAM and TICKER have bounded native collector contracts. Native HISTORY delivery, shared upstream
multiplexing and complete end-to-end subscription behavior remain pending in this baseline; generic
decoding or a Java observation does not establish those capabilities.
[Current capabilities](../CURRENT-STATE.md) records the implemented boundaries.

## Source index

| Reader question | Primary repository references |
| --- | --- |
| Which Java revision and modules define the comparison? | [Upstream baseline](../UPSTREAM.md), [module inventory](../upstream/MODULE-INVENTORY.md). The inventory is navigation, not a semantic specification. |
| Which boundaries are implemented or selected? | [Architecture](../ARCHITECTURE.md), [current capabilities](../CURRENT-STATE.md), [ownership decision](../decisions/ADR-0005-live-subscriptions.md), [runtime decision](../decisions/ADR-0006-live-runtime.md). ADR status and implementation status are separate. |
| Where does transport end and protocol begin? | [Pure live session](../../spec/subscriptions/LIVE-SESSION.md), [optional TCP runtime](../../spec/transport/TCP-RUNTIME.md). |
| What is a subscription identity? | [Desired state](../../spec/subscriptions/STATE.md), [subscription contracts](../../spec/subscriptions/CONTRACTS.md), [schema decision](../decisions/ADR-0003-configurable-schemas-and-event-mappings.md). |
| How are records delivered? | [STREAM](../../spec/subscriptions/STREAM-COLLECTOR.md), [TICKER](../../spec/subscriptions/TICKER-COLLECTOR.md), [Java capture profile](../../tests/fixtures/subscriptions/CAPTURES.md). |
| Which observations support delivery claims? | [STREAM validation](../../tests/conformance/subscriptions/STREAM-VALIDATION.md), [TICKER validation](../../tests/conformance/subscriptions/TICKER-VALIDATION.md). Each states its actual profiles and exclusions. |
| How do configuration and schemas acquire meaning? | [Configuration requirements](../CONFIGURATION.md), [schema decision](../decisions/ADR-0003-configurable-schemas-and-event-mappings.md). |
| What does upstream provide beyond a socket client? | [qds-tools inventory](../upstream/QDS-TOOLS.md), including connectors, file/tape, Feed and Multiplexor. Source-derived tool behavior is not native tool parity. |

## Scope of route and channel questions

The confirmed goal is to document QD and qd-cxx requirements, evidence and open design questions.
Multiple channels and event origin across intermediaries are discussion topics, not approved new
capabilities or route guarantees. Logical channel identity, its relationship to connections, and
route metadata are unresolved. The ledger
distinguishes event data flow from subscription-demand flow and local ingress knowledge from a
complete producer-to-consumer chain. No route field, source-ID packing, topology or gateway/MUX
deployment is selected here. Concrete infrastructure is separate work.
