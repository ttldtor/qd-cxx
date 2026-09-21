# P08 Java evidence validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 UTC before C++ implementation: two independent clean captures and negative/positive
Verify. [Capture A metadata](captures/capture-a/metadata.json), [capture B metadata](captures/capture-b/metadata.json),
[negative result](captures/mismatch/result.txt), [baseline result](captures/baseline/result.txt),
[proof](captures/proof.json), [capture log](captures/description-capture-a.log),
[sequential verification log](captures/description-evidence.log).

Both normalized descriptions.tsv outputs equal the separately promoted [expectation](expected/descriptions.tsv).
The negative run used only a temporary copy with raw descriptor type 8 changed to 9; it failed exactly
`VERIFY FAILED: descriptions.tsv`. Temporary and baseline hashes stayed unchanged. Each run rebuilt
locked upstream cleanly with JDK 8/Maven, no upstream tests, patches or replacement binary. Normalization
is CRLF to LF only. Shared-checkout builds were sequential. Exact commands, sources, input, providers,
class outputs and dependency hashes are retained in metadata; raw stdout/stderr and build logs are adjacent.

The standalone driver/launcher were added separately, leaving old hashed Java tools untouched.
Historical hashes cover all prior tracked fixtures and upstream.lock;
final audit checks them and current probe provenance.
The [harness documentation](../../../../tools/upstream/description-oracle/README.md) identifies diagnostic
versus actual-parser hooks and profile limits. Source-derived rules, instrumented raw projection and
actual Java installation/corruption observations are not interchangeable.

Pinned Java Format/Check passed. Later C++ [validation](../../../conformance/protocol/descriptions/VALIDATION.md)
is separate. No full upstream suite, local factory rerun, huge resource test, event or transport claim.

The checked-in Test-Verify.ps1 was also executed after documentation/tool packaging; its separate
[final self-test proof](captures/final-verify/proof.json) and [UTC log](captures/final-verify/run.log)
retain fresh negative/positive builds with complete current tool-directory metadata.

Git attributes preserve the exact captured probe-script/document/input bytes and raw artifacts;
Java source is formatter-canonical LF. No stored source hash was rewritten to accommodate line endings.
