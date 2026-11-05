# Standard HISTORY receiver observations

`oracle.tsv` and the seven `wire-step-N.qtp` files are actual pinned Java production outputs.
`provenance.json` binds sources, JDK, harness/launcher, class inventory and each fixture.
Use the [bounded capture/verify tool](../../../tools/upstream/history-receiver-oracle/README.md).
The wire stream begins with DescribeRecords and the first HISTORY DATA; later files continue the
same composer dictionary. It must be consumed in numbered order with the same session context.
The native schema has 10 logical fields; five physical VOID tails of 64-bit fields are omitted.
This is a common lower receiver witness, not a public native typed TimeAndSale mapper.
