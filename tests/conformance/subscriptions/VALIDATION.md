# Subscription oracle validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed on 2026-10-31 with Windows NT 10.0.26340, PowerShell 7.6.9, CMake 4.4.2,
Zulu OpenJDK 1.8.0_422 and the unchanged locked source/classes described by the
[capture profile](../../fixtures/subscriptions/CAPTURES.md). Existing prepared provider bytes
were reused and verified against their trusted receipt (1317 files, 14 module roots);
this is not a fresh upstream build. Actual runtime version advertisement remains QDS-UNKNOWN.

Two fresh final-source captures compared byte-identical across all 108 public artifacts,
392286 bytes. The manifest covers every other artifact. Component counts:

| Component | Cases | Observation rows |
| --- | --- | --- |
| Wire | 22 | 2169 |
| Collectors | 21 | 2507 |
| Session | 32 | 899 |
| Total | 75 | 5575 |

JDK 8 compiled all three helpers and the help-failure probe. Production assertions completed
for six add/remove types, requested layouts, collector cases, protocol deltas, all heartbeat
masks, capability readiness, client login and server authentication. The two diagnostic
malformed frames retain their narrower evidence classification.

No-argument/standalone help produced identical stdout and empty stderr without captures.
CMake uses standalone -DHELP=ON; Java uses --help. Invalid Java arguments returned 3.
Existing-output rejection preserved each artifact tree's hashes. Injected help-output failure
returned Wire 2, Collector 1 and Session 1; the collector/session checks were observed failing
before their error-handling correction, then passed on final source.

The comparator regression suite exercises typed TSV, duplicate keys, case coverage, provenance,
manifest, exact changed/missing/extra files, binary mutation and resource limits. Host denial of
creating a private junction is explicitly recorded as a reparse-test skip, not a successful
runtime rejection test. Reparse rejection is implemented before traversal.
The suite's final receipt accompanies integration evidence; no cross-platform result is inferred.

The pinned google-java-format 1.36.1 wrapper ran Format then Check successfully over all 40
repository Java sources using a separate JDK 25. Ordinary C++ source, public headers and root
build configuration were unchanged. Native CTest, sanitizers and Doxygen were not rerun:
these checks compile/run Java and PowerShell tooling; Doxygen's configured inputs are headers.
No Windows/Linux native subscription parity, Linux Java execution, sockets, TLS, DNS, external
endpoint, real credentials or full business-event model qualification is claimed.

Reproduction commands, required inputs, finite limits, error exits and side effects are in the
[tool instructions](../../../tools/upstream/subscription-oracle/README.md). The committed indexed
fixtures permit exact offline comparison; source/provider/protocol profile differences must
be reported rather than normalized away.
