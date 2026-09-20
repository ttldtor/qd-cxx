# Conformance tests

Conformance tests will verify C++ behavior against specifications and reviewed fixtures derived from the locked Java QD oracle.

Each test should identify the specification rule it exercises and the fixture provenance. Keep protocol tests independent of real sockets where possible; transport integration tests belong at a separate boundary once transport exists.

No test framework has been selected yet.

QDCXX-004 supplies a [protocol capture plan](../fixtures/protocol/README.md) covering incremental framing, directional schema mappings, requested-field adaptation, negotiation and reset/error boundaries. QDCXX-006 now supplies [nine executed Java cases](../fixtures/protocol/COVERAGE.md) with exact captures and separate ordered-record semantic expectations. Unlisted families remain proposals; no C++ tests or framework have been introduced. Compare stream/record semantics separately from parser callback batching and transport close policy.
