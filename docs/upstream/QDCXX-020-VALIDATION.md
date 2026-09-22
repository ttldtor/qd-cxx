# QDCXX-020 validation and review handoff

[Русская версия](QDCXX-020-VALIDATION.ru.md). English is authoritative.

Date: 2026-09-22 UTC. Research/documentation only; no production code, dependencies, new Python,
fixture/lock changes or commit. No C++ matrix is required for this document-only task.

## Outputs and recommendation

- [QDS-TOOLS](QDS-TOOLS.md): 20 registered commands, case-insensitive spellings, dispatch/help,
  packaging, delegated IPF and separate MICBuilder, priorities and compatibility proposals.
- [REAL-EVENT-BASELINE](../REAL-EVENT-BASELINE.md): unmodified default composite Quote, eight fields /
  twelve Java int slots, types 56/2/120; source-derived production-file test proposal, not a capture.
- [PYTHON-MIGRATION](../PYTHON-MIGRATION.md): mandatory CLI tests versus manual/historical workflows,
  destinations, negative equivalence and archive policy.
- [NEXT-STEPS](../NEXT-STEPS.md): one unassigned next implementation, bounded uncompressed binary
  tape reader with optional qdcxx-tape-inspect, then the real Quote milestone and operational tools.

Owner decisions are concrete proposals, not already accepted: tape descriptor/additive binding/EOF
ADR, new executable/output contract, per-utility CLI/configuration compatibility; later Quote
adapter/API and transport/subscription choices. No task ID beyond QDCXX-020 was assigned.

## Author: executed read-only investigation

Checkout: `C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd`. These commands were executed using
PowerShell; Git trust was scoped to this checkout for each process, not saved in user settings:

```powershell
$u = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
git -c safe.directory=$u -C $u rev-parse HEAD
git -c safe.directory=$u -C $u status --short
git -c safe.directory=$u -C $u remote get-url origin
git -c safe.directory=$u -C $u describe --tags --exact-match
git -c safe.directory=$u -C $u ls-files '*META-INF/services/*AbstractTool*' '*qdshelp.txt' '*StartupService*' '*qds*.sh' '*qds*.bat'
rg -n '<version>3.355</version>' "$u/pom.xml"
Test-Path "$u/qd-tools/target/classes/com/devexperts/qd/tools/Tools.class"
rg --files tools tests
rg -n 'Python|\.py' CMakeLists.txt tools tests --glob '*.cmake' --glob '*.ps1' --glob 'CMakeLists.txt'
git status --short
git diff --numstat
git ls-files --others --exclude-standard
```

Results: HEAD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, origin
`https://github.com/devexperts/QD.git`, exact tag `qd-qds-3.355`, POM 3.355, no tracked/untracked
upstream changes reported. Tools.class is absent. Source searches/reads covered every registry
entry, Tools/AbstractTool/Options/Help, help resources, packaging POM, adjacent dxfeed-tools entries,
Quote factory/properties/builder/mapping/delegates/API/serial types and Python workflow entries.
The source-linked result documents identify the files/methods used; they are not runtime logs.

Some exploratory searches initially failed because PowerShell does not expand brace lists or
literal wildcard paths as a Unix shell does, or because a guessed factory filename was absent.
They were replaced with explicit paths, `rg -g` and the actual MarketFactoryImpl. Those failures
are not successful checks and yielded no evidence. They changed no files.

## Help and runtime limitations

No Java dispatcher or per-command help was executed/captured: complete tools runtime unavailable;
help can instantiate providers and normal invocation calls Services.startup. Source articles,
annotations, options, fallback and startup path were inspected instead. No external feeds,
listeners, publication, rotation/deletion or user-setting changes were performed. No new Java
event/tape capture, clean upstream build, Python script or C++ tests were run in QDCXX-020.
Earlier QDCXX-019 runtime results are cited as historical evidence, not independently rerun here.

## Author: final document checks

Final scope is the actual changed/new Markdown set, including untracked deliverables. Checks:
local relative file links and reciprocal EN/RU links; pinned source paths against the verified
checkout; 20-provider table reconciliation; diff whitespace for tracked and new files separately;
protected source/fixture/lock paths and no new Python. EN/RU claims were read side by side,
preserving source-only versus measured/proposed distinctions. Raw historical evidence was not
normalized or rewritten.

Final author run at `2026-09-22T14:21:42Z`: 18 Markdown files (10 tracked modifications / 8 new),
326 relative links, 88 pinned links / 43 unique source paths, reciprocal links in all 18 files,
20 providers present in each language. No errors detected in these checks. Protected diff empty,
new Python count zero, upstream status clean. Inline PowerShell enumerated `git diff --name-only`
plus `git ls-files --others --exclude-standard`, resolved Markdown targets relative to each file
and pinned paths against the checkout, and matched every registry simple name in both tables.
`git diff --check` and `git diff --no-index --check -- .work/qdcxx-020-empty.txt <new.md>` produced
no diagnostics. The latter uses an empty local file: exit 1 alone denotes differences, not a
whitespace finding. Initial checks found changed task-status trailing spaces and extra EOF blank
lines in the new inventory; these were fixed and checks rerun. Historical raw evidence is unchanged.

## Independent review

The separate `independent_review` agent performed read-only source review, not author validation.
Its first pass independently checked pinned SHA, all 20 registrations, help/dispatch/packaging,
MICBuilder, Quote defaults/types/slots/delegates and Python CMake/logging/acquisition paths.
It checked 80 pinned links / 39 unique upstream paths in the first two document pairs; none missing.
It read EN/RU QDS-TOOLS, REAL-EVENT-BASELINE and PYTHON-MIGRATION without finding material translation
differences. No Java/Python/C++/network execution or edits were performed by that reviewer.

First-pass findings: missing Compare --collector/-c (P2), TDP existing-profile → file+"~" and
swallowed IOException path (P2), unspecified Invoke default void return type (P3). Author fixed
all three in EN/RU and also corrected NetTest's --rate-limit spelling from direct source inspection.
The second independent pass reviewed final roadmap/status/validation and corrections, repeated
18-document/326-relative-link, tracked/new whitespace and protected-file/no-new-Python checks,
and found no further issues. It accepted tape-first as a bounded enabling proposal, distinct from
the subsequent Quote milestone and subject to owner assignment.

An author follow-up corrected the distinction between dotted local schema names and outgoing
property names and documented direct-file wide-decimal support independently of EMPTY_SET.
The reviewer independently verified AbstractDataField/DefaultRecord/QuoteMapping/BinaryRecordDesc,
FileFormat/BinaryQTPComposer, EN/RU and all 88 pinned links / 43 unique source paths: no missing
paths or further findings. Review is complete; the three initial findings are closed.
Final status/result bookkeeping was performed by the author after that review. No errors were
detected in the stated checks; this is not a general semantic/runtime compatibility guarantee.
