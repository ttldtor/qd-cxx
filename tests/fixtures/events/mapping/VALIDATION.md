# Configured event oracle validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Starting HEAD: `e5a3764992ba3a90a07d3d092a4383e37a901de2`. Executed 2026-09-23 UTC with locked
QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 and Maven 3.9.16.
Formatting used OpenJDK 23.0.2 and pinned google-java-format 1.36.1. These are recorded checks;
independent review remains pending. No errors were detected in the successful checks below.

| Executed check | Result |
| --- | --- |
| Java Format then Check | PASS, 17 discovered Java sources; unchanged historical Java files |
| Clean capture A `.work/event-mapping-final-a` | PASS, clean upstream build, old tape Verify, 21 fresh-JVM cases, 135 artifacts |
| Clean capture B `.work/event-mapping-final-b` | PASS, same procedure; 135 artifacts byte-identical to A |
| `.work/event-mapping-negatives` | All 14 mutations rejected; baseline passes and expectation hashes unchanged |
| Full negative Verify against separate profile-flags expected copy | Expected exit1; changed event flags contradict actual delegate/FileReader event values |
| Full baseline Verify | PASS against unchanged repository expectations; all 135 artifacts agree |
| Documentation links/whitespace and owned-file inventory | Checked separately from raw evidence; no historical evidence normalized |

[Capture links](CAPTURES.md) retain the actual logs and metadata. [Reproduction commands](../../../../tools/upstream/event-mapping-oracle/README.md)
use JDK 8 for the oracle and a separate JDK 21+ for formatting. Each scenario has heap 256 MiB,
a 30-second process limit and a 64-record callback limit. No expected file is rewritten by Verify.
All retained source hashes describe their actual run; later tool/report additions do not rewrite
old metadata to pretend that they existed in earlier captures.

During development, the empty-wire probe initially used a symbol requiring text with a null text
argument and was corrected before clean captures. The synthetic XML string137 case was retained
as a separate unsupported witness. A later log-retention rerun overlapped a clean rebuild against
the same upstream classes and failed with ClassNotFoundException; that run is excluded from
acceptance. Sequential invocations are required and final retained Verify runs complete sequentially.
These orchestration failures are distinct from the intentional semantic negative Verify.

The optional/default/representation, properties/XML, string/flags/time and rejection conclusions
are limited to [coverage](COVERAGE.md). This report makes no claim about C++ compiler/sanitizer/CLI
results, arbitrary providers, additional types, indexed schemas or complete private-tape support.
The durable configuration timing and local-absence versus wire-omission distinctions are recorded
in the bilingual contracts.
