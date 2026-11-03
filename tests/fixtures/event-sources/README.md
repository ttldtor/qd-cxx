# Event source observations

[Русская версия](README.ru.md). English is authoritative.

[oracle.tsv](oracle.tsv) contains 311 actual observations of the pinned Java QD
IndexedEventSource and OrderSource classes. [provenance.json](provenance.json)
records revision, harness/launcher/upstream hashes, production-class inventory,
JDK identity and exact fixture SHA-256.
Rows contain operation, input, ID, name UTF-16 hex and signed hashCode; error rows
contain the Java exception class. EQUAL/CROSS/UNEQUAL contain observed booleans.
NULL differs from empty text; four hex digits encode each UTF-16 code unit.

Reflection inventories every built-in field; both valueOf directions are exercised.
Additional cases cover unknown names, ASCII boundaries, embedded NUL,
negative/small/non-alphanumeric IDs, nullable/empty/unpaired UTF-16 base names,
different names for one ID, and equality across source families.
This is a bounded useful corpus, not exhaustive enumeration of all 32-bit IDs.

[Reproduction](../../../tools/upstream/event-source-oracle/README.md).
Capabilities, index packing, record mappings and routing are outside this fixture.
