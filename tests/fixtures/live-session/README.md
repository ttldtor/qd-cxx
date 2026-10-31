# Native session control fixtures

[Русская версия](README.ru.md). English is authoritative.

These synthetic fixtures compare native protocol/heartbeat output with actual
callbacks from the unchanged locked Java QD production parser. They exercise
descriptor deltas, nullable/inherited properties and heartbeat field presence.
They are not a network recording or a provider catalog.

The `expected` directory contains four exact artifacts:

- `native-controls.qtp`: complete native-generated control envelopes.
- `actual.tsv`: canonical production callback observations compared with the
  independent literal ledger emitted by the native test.
- `provenance.tsv`: locked source/runtime identity, provider receipt and native,
  tool and source hashes.
- `manifest.tsv`: sizes and SHA-256 values of the other artifacts.

The binary contains a deliberately public synthetic Basic token. The observer
admits only that fixed test value and redacts nonempty authorization in its TSV.
Never substitute real credentials or production captures into these fixtures.

Use the [verification workflow](../../../tools/upstream/live-session-oracle/README.md)
with explicit prepared JDK 8 providers and freshly built native test outputs.
Capture compares production observations with native expectations; Verify also
compares all four artifacts byte for byte with this directory. Hashes of current
source files do not alone prove which source built a producer: retain the
corresponding build commands and results when reproducing the comparison.

See the [validation boundary](../../conformance/subscriptions/LIVE-VALIDATION.md).
Transport, collectors, application callbacks and reconnect behavior are outside
this corpus.
