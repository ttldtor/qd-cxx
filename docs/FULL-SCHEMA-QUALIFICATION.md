# Full-schema distribution qualification

[Русская версия](FULL-SCHEMA-QUALIFICATION.ru.md).

The full binary catalog distribution supports the default and dxlink QD schema profiles. A binary-v2 bundle pairs the logical record declarations with the exact outgoing property names. Applications can load the pair with `qdcxx_schema_json`, construct a publisher for the validated catalog, and link the installed `qdcxx::feed` target. Binary-v1 loader compatibility is covered by the schema regression suite.

## Test matrix

The product sources match revision `dee3a879128ad50882faa5fd97b55ff8c84c1800`. The qualification uses Windows MSVC 19.42.34444 Release and Linux GCC 13.3.0 Debug with ASan/UBSan. Each platform installs the SDK into a fresh directory, builds an independent consumer and the shipped distribution example against that SDK, and starts separate distribution processes over TCP.

| Scenario | QD default | QD dxlink |
| --- | --- | --- |
| Installed SDK consumer | 331 records, 3,186 fields | 297 records, 2,750 fields |
| Java QD 3.355 field-level comparison in both directions | All 331 records | All 297 records |
| TICKER over TCP | 331/331 | 297/297 |
| STREAM over TCP | 331/331 | 297/297 |
| HISTORY over TCP, zero raw bound and removal index | 160/160 indexed records | 126/126 indexed records |

The installed NetTest command passed its English and Russian help and finite startup checks. Each build passed the nine selected regressions covering the publisher, Feed filters, MUX queue, HISTORY collector, subscription writer, schema loader, NetTest, and distribution help.

The Java comparison covers four value scenarios with boundary scalars, null and empty objects, ordered descriptions, separately assigned sender IDs, and full signed deletion keys. The TCP catalog cases check upstream ADD and REMOVE and downstream record data, using one record occurrence per DATA frame. A separate two-client TCP regression covers HISTORY snapshot beginning and end and signed removal on a subset.

## Using the result

The full relay allows up to 32 mapped occurrences in one incoming DATA frame. Its source negotiates TICKER, STREAM, HISTORY and `hs`. The dxlink profile selects records and fields for QTP. A dxLink WebSocket connection requires a separate adapter.

[Distribution example](../examples/feed_distribution/README.md) · [Publisher contract](../spec/subscriptions/PUBLISHER.md) · [Schema export guide](../tools/schema-export/README.md) · [Record workload results](RECORD-WORKLOAD.md)
