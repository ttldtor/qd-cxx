# Relevant Java QD modules

This is a shallow navigation inventory for Java QD 3.355 at commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. It identifies likely starting points for later research; it does not define effective schema rules, encoding semantics, QTP framing, or C++ component boundaries.

The upstream [root POM](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/pom.xml) is the source for the top-level module names and their packaging groups.

## Primary modules

| Module | Relevant surface | Initial inspection points |
| --- | --- | --- |
| `qd-core` | Core `DataScheme`, record and field abstractions; field and symbol codecs; record buffers; binary QTP parsers, composers, descriptors, options, and message types. | [`com.devexperts.qd`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd), [`com.devexperts.qd.kit`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit), [`com.devexperts.qd.ng`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng), and [`com.devexperts.qd.qtp`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp). |
| `dxlib` | Shared byte, buffered-stream, compact-serialization, chunk, and compression primitives used beneath QD encodings. | [`com.devexperts.io`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io). |
| `dxfeed-scheme` | XML scheme resources and XSDs plus the model, import processing, XML reader/writer, loading options, and `DataSchemeFactory` provider. | [`src/main/resources`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources) and [`com.dxfeed.scheme`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme). |
| `dxfeed-impl` | Programmatic scheme construction and record-to-event mappings used alongside the XML scheme layer. | [`com.dxfeed.api.impl`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl), event mapping packages under [`com.dxfeed.event`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event), and [`META-INF/services`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/resources/META-INF/services). |
| `qds-file` | Text and blob QTP parsers/composers and file/tape readers, writers, and format selection. | [`com.devexperts.qd.qtp.text`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/text), [`blob`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/blob), and [`file`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file). |

## Adjacent modules and boundaries

| Module | Why it may matter | Boundary for later research |
| --- | --- | --- |
| `qds` | The main aggregate deliverable; its [POM](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds/pom.xml) shades `qd-core`, `proto`, `dxfeed-scheme`, `dxfeed-impl`, and other modules. | Use it to understand shipped composition, not as the source of protocol semantics; the module has no `src/main` tree at this revision. |
| `proto` | Generic connector and codec infrastructure used below QTP connections. | Inspect [`com.devexperts.connector`](https://github.com/devexperts/QD/tree/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/proto/src/main/java/com/devexperts/connector) only when locating the transport/protocol boundary. Do not treat transport lifecycle as `qd-core` schema or encoding behavior. |
| `qd-nio` | NIO connector implementation and QTP connector provider. | It is transport-specific context, not the starting point for wire encoding or schema contracts. |
| `dxfeed-api` | Event types and mapping annotations consumed by implementation layers. | Consult it when a record/event mapping needs context; it does not replace `DataScheme` or the scheme loader as the compatibility surface. |

Tests under each primary module are candidate evidence for later tasks, but no test has been promoted to a conformance fixture by this inventory.
