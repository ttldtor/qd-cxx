# Default Quote Java validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Checks executed on 2026-09-22 UTC. These are implementation verification results; independent
review remains pending. Starting repository HEAD was `fa5a0b967e9c021e656846cdd3d6faee5faa4776`.
The first runtime observation preceded the new contracts and C++ implementation.

- Two clean upstream reactor captures A/B completed, with the unchanged full default scheme.
  [All 86 artifacts matched](evidence/compare.txt), without normalization; the original FileWriter
  output is 326 bytes and contains five standard Quote records. Actual raw fields and mapped values
  are retained separately from input events. [Capture provenance](CAPTURES.md).
- Nine negative mutations were rejected: full type, raw value, field order, record identity,
  row arity, missing completion, extra file, mapped decimal bits and binary byte.
  The unchanged typed baseline then passed. [Actual results/logs](evidence/negatives/).
- A full clean `Invoke-Oracle.ps1 -Mode Verify` against the changed-value expectation failed.
  Its old tape baseline passed; its Quote comparison rejected `default-quote.tsv`.
  [Negative metadata](evidence/negative/quote-metadata.json) and
  [fresh retained comparison rejection](evidence/negative-rejection.txt).
- A subsequent full clean Verify against the original expected directory passed:
  [result](evidence/verify/quote-result.txt), [typed comparison](evidence/verify/quote-verify.stdout),
  [metadata](evidence/verify/quote-metadata.json). The final comparison also requires exact equality
  of every raw and mapped row between `put.tsv` and production FileReader output.
- Every one of the four clean invocations passed the existing QDCXX-019 tape baseline Verify,
  including unchanged default/constrained/text/truncated artifacts and the old QTP inspector's
  precise rejection output. Existing expectations and upstream.lock were not rewritten.
- Pinned google-java-format 1.36.1 wrapper Format then Check passed for 13 Java files using OpenJDK
  23.0.2. Only the new QuoteOracle Java source was changed. A final
  [wrapper Check](evidence/java-format-check.txt) passed; actual final Java/PowerShell tool hashes
  matched the final Verify metadata.

Commands are in the [oracle README](../../../../tools/upstream/quote-oracle/README.md). Runtime uses
Zulu JDK 8 and Maven 3.9.16 with locked clean QD 3.355. Each clean invocation calls upstream
`clean compile` with build cache reuse disabled and tests skipped. These observations do not claim
to run the upstream Java test suite. Metadata retains the actual command arrays, versions, classpath,
providers, dependencies and hashes. Quote runs finished normally inside the 30-second process bound;
no timeout was interpreted as successful EOF.

No errors were detected in these checks. Coverage is [finite and explicit](COVERAGE.md): ordinary
and lossy real Quote inputs, raw decimal/character/time bounds and focused production parser probes.
Malformed UTF_CHAR consumed positions are primitive observations. Diagnostic constructed tapes
do not independently establish the full production event-to-file path; `default-quote.qtp` does.
No RAW_DATA, MESSAGE timing, general tape, compression, regional mapping, transport, schema importer
or private-file acceptance was added. No new Python code was created.

The C++ implementation and its separate compiler/sanitizer matrix are outside this Java validation
report. Final task-level handoff must keep these original observations distinct from independently
rerun review checks.
