# Configured event captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The [135 expected artifacts](expected/) were copied byte-for-byte from clean capture A and
reproduced by independent clean capture B. Each run rebuilt the unchanged pinned upstream reactor
and passed unchanged baseline tape Verify. Each of 21 scenarios had a fresh JVM. Original files,
UTF-16 units, timestamps and output bytes were not normalized. upstream.lock remains unchanged.

- [Capture A](evidence/a/event-metadata.json), [baseline build/provider/classpath provenance](evidence/a/metadata.json).
- [Capture B](evidence/b/event-metadata.json), [baseline provenance](evidence/b/metadata.json).
- [Cross-capture comparison](evidence/compare.txt).
- [14 rejected mutations](evidence/negative-results.json), [unchanged baseline](evidence/negative-baseline.txt).
- [Full negative Verify](evidence/negative/event-metadata.json), [expected rejection](evidence/negative/event-verify.stderr).
- [Full positive Verify](evidence/verify/event-metadata.json), [PASS](evidence/verify/event-result.txt).
- [Post-negative unchanged expectations](evidence/after-negative-unchanged.txt).

Adjacent stdout/stderr, build logs, environment inventory and metadata retain actual UTC timestamps,
paths, commands, source/tool/input hashes, classpath content/dependencies and provider identities.
The 76 original FileWriter files and 38 separately constructed wire-omission files are retained
with all 21 observation TSVs. Direct mapping probes are explicitly identified by phase.
The package-private flag/sequence observations call unchanged upstream methods; the harness does
not reimplement delegates. Missing-required rejection is the harness reporting absence of a
compatible production delegate, not an assertion that DefaultRecord itself necessarily throws.

The initial development run found that the synthetic XML type named string resolves to full137;
its source is preserved as unsupported-extension.xml and the actual rejected-profile observation
is xml-unsupported. A separately named compatible extension.xml uses utf_char_array full10.
The unchanged dxlink/default witnesses were never edited to fit C++ restrictions.
Early development runs are not clean reproducibility evidence. New oracle source remains separate
from historical harnesses, fixtures and lock. No private tape was accessed.
