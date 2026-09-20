# QDCXX-008 contract example coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

These 17 JSON files are designed projections of unchanged QDCXX-005 TSV, not Java-produced JSON, new oracle captures or importer output. There is one example per subcase, without deduplication, so ordered input provenance and failure attempts remain explicit. The [contract](../JSON-CONTRACT.md) defines their meaning.

| Subcase | Example | Existing expectation | Projection |
| --- | --- | --- | --- |
| S01-empty | [JSON](S01-empty.json) | [TSV](../../../tests/fixtures/schema/expected/S01-empty.tsv) | Success: no records; one empty XML input. |
| S01-no-urls | [JSON](S01-no-urls.json) | [TSV](../../../tests/fixtures/schema/expected/S01-no-urls.tsv) | Failure: two no-source-inputs diagnostics, distinct loader attempts. |
| S04-order-disabled | [JSON](S04-order-disabled.json) | [TSV](../../../tests/fixtures/schema/expected/S04-order-disabled.tsv) | Success: Alpha=0, Beta=1; disabled entities retained through source references; declaration order differs from Java category order. |
| S05-both | [JSON](S05-both.json) | [TSV](../../../tests/fixtures/schema/expected/S05-both.tsv) | Success: First/Second heads at int 0/1, Ordinary at 2. |
| S05-first | [JSON](S05-first.json) | [TSV](../../../tests/fixtures/schema/expected/S05-first.tsv) | Success: First at 0, placeholder at 1; Second remains ordinary. |
| S05-second | [JSON](S05-second.json) | [TSV](../../../tests/fixtures/schema/expected/S05-second.tsv) | Success: placeholder at 0, Second at 1; First remains ordinary. |
| S05-disabled-first | [JSON](S05-disabled-first.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-first.tsv) | Success: disabled First omitted; placeholder at 0. |
| S05-disabled-second | [JSON](S05-disabled-second.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-second.tsv) | Success: disabled Second omitted; placeholder at 1. |
| S05-disabled-both | [JSON](S05-disabled-both.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-both.tsv) | Failure: all-index-fields-disabled after successful model recognition. |
| S07-explicit | [JSON](S07-explicit.json) | [TSV](../../../tests/fixtures/schema/expected/S07-explicit.tsv) | Success: Chosen.Name effective; Value property and ChosenName record lookup retained. |
| S07-fallback | [JSON](S07-fallback.json) | [TSV](../../../tests/fixtures/schema/expected/S07-fallback.tsv) | Success: First.Name fallback; Plain uses its semantic name. |
| S08-before | [JSON](S08-before.json) | [TSV](../../../tests/fixtures/schema/expected/S08-before.tsv) | Success: base, copy, overlay; Copy has Original only. |
| S08-after | [JSON](S08-after.json) | [TSV](../../../tests/fixtures/schema/expected/S08-after.tsv) | Success: base, overlay, copy; Copy has Original and Added. |
| S08-main-alias | [JSON](S08-main-alias.json) | [TSV](../../../tests/fixtures/schema/expected/S08-main-alias.tsv) | Success: Base uses Chosen.Name, Copy uses First.Name; explicit-main declaration retained by source/evidence references. |
| S15-slots | [JSON](S15-slots.json) | [TSV](../../../tests/fixtures/schema/expected/S15-slots.tsv) | Success: six logical fields, six int slots and two obj slots; LONG/WIDE_DECIMAL retain owned VOID tails. |
| S15-byte | [JSON](S15-byte.json) | [TSV](../../../tests/fixtures/schema/expected/S15-byte.tsv) | Failure: recognized BYTE=1; approved unsupported-local-type diagnostic at local construction. |
| S15-short | [JSON](S15-short.json) | [TSV](../../../tests/fixtures/schema/expected/S15-short.tsv) | Failure: recognized SHORT=3; approved unsupported-local-type diagnostic at local construction. |

The outcome is 13 successful schemas and four failed cases (five diagnostic entries because no-URLs has two attempts). All examples reference the same original input/evidence files in both language versions. Scoped lookup probes include successful and absent keys; source metadata is retained by reference, not duplicated.

Coverage is only the existing subcases, not whole S families, provider parity, wire conformance or general XML semantics. No protocol/session examples are added: [P05 evidence](../../protocol/CAPTURES.md) constrains separation of requests and selection, not this local schema body. LONG/time-index wire rules remain source-derived and require later probes. See [executed validation and limits](VALIDATION.md).
