# Developer documentation {#developer-documentation}

[Русская версия](developer.ru.md) · [Documentation home](index.md)

Use this section when building optional components, changing the library or checking compatibility.
Application setup and usage start in [Getting started](getting-started.md) and [Practical guides](guides.md).

## Build and tooling

- [Detailed build options and component reference](CPP-BUILD.md)
- [Contribution requirements](../CONTRIBUTING.md), [C++ conventions](CPP-STYLE.md) and [naming](NAMING.md)
- [Developer-tool catalog](../tools/dev/README.md): UTC logging, formatting and offline compatibility tools
- [Build this documentation](../tools/docs/README.md)
- [Upstream reference](UPSTREAM.md) and [compatibility policy](COMPATIBILITY.md)

## Verification results and limits

Results describe exact inputs and toolchain profiles. Source analysis, native execution and independent
reruns are different evidence; none is a general security certification or an RSS/hard-time guarantee.

- [TLS runtime/resource qualification](../tests/conformance/transport/TLS-VALIDATION.md)
- [TCP runtime qualification](../tests/conformance/transport/TCP-VALIDATION.md)
- [Recovery-owned HISTORY range observation](../tests/conformance/transport/HISTORY-STATUS-VALIDATION.md)
- [Tape reader and supported inputs](../spec/protocol/TAPE-READER.md)
- [Tape writer validation](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.md)
- [Schema export validation](../tests/conformance/schema-export/VALIDATION.md)
- [Subscription compatibility](../tests/conformance/subscriptions/VALIDATION.md)
- [Composed receiving profiles and CLI qualification](../tests/conformance/subscriptions/DELIVERY-VALIDATION.md)
- [Subscription receiver and installed console example](../tests/conformance/subscriptions/RECEIVER-VALIDATION.md)
- [Public feed API, installed package and live Quote](../tests/conformance/subscriptions/FEED-VALIDATION.md)
- [Knowledge base and decision trace](knowledge/README.md)

[Current capabilities](CURRENT-STATE.md) distinguish delivered components from the
[remaining product direction](NEXT-STEPS.md). Conformance captures and technical specifications remain
part of the source distribution; the navigation above does not turn proposals into supported behavior.
