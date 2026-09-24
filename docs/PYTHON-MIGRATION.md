# Removing Python from current workflows

[Русская версия](PYTHON-MIGRATION.ru.md). English is authoritative.

The QDCXX-020 source inventory below is preserved as a historical snapshot. QDCXX-027 implements
its first two migration steps; the other steps remain required follow-up work. Historical scripts,
captures and recorded hashes remain historical evidence.

## Inventory and dependency edges

### Current assignment: QDCXX-027

QDCXX-027, High, implements steps 1–2 from actual starting HEAD
`dcccd0242252cadb674f4970cc723fbf113bed53`. Normal configure/build/CTest now uses CMake and
test-only C++ for both mandatory CLI suites and their mutations. Active UTC logging is
`tools/Run-Utc.ps1`; existing Windows formatter acquisition is `tools/format/Setup-Cpp.ps1`.
Both require PowerShell 7.4 or newer. See the [named coverage map](../tests/conformance/python-migration/COVERAGE.md)
and [author verification](../tests/conformance/python-migration/VALIDATION.md) for parity, process-image
Python denial, compiler/option matrices and limitations. Independent review remains pending.

Manual primitive/protocol negatives, schema JSON validation, evidence audits, generators and tracked-bytecode
cleanup remain follow-up work (steps 3–4). They are not retired or optional merely because normal CTest is migrated.
Old comparison scripts and tracked bytecode stay unchanged in QDCXX-027; disable bytecode writing during parity runs.
The original QDCXX-020 inventory below is retained; the QDCXX-027 plan refreshes the active migration scope.

The following table describes the original QDCXX-020 state, not the current entry points above.
Paths are repository-relative. Enumeration used `rg --files tools tests` filtered by `.py`,
then CMake/PowerShell/document references and script imports/process calls. This distinguishes
mandatory current execution from optional/manual historical reproduction; merely existing in Git
does not mean a script runs in CTest. None of these scripts is a production C++ dependency.

| Files / current entry | Role and replacement destination |
| --- | --- |
| `CMakeLists.txt` → `tests/conformance/check_offline_inspection.py`, `check_human_inspection.py` | Mandatory only when BUILD_TESTING and ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT are both ON. Python3 Interpreter is REQUIRED; human imports run/validate_diagnostic from offline. Replace process supervision with small PowerShell/CMake launchers and strict byte/manifest/diagnostic assertions with test-only C++/doctest. Do not lose shared validation. |
| `tools/run_utc.py`; commands in `docs/CPP-BUILD.md` | Active documented logging, including WSL. Child TZ=UTC, timestamped UTF-8 merged stdout/stderr, launch failures and child exit propagation. Replace with small PowerShell process launcher using UTC timestamps and child-only TZ; validate on Windows and Linux/WSL. Existing launcher decodes UTF-8 with replacement and strips CR/LF line ends: it is not a raw-byte capture tool. |
| `tools/format/setup_cpp.py`; `tools/format/CPP.md` | Active formatter acquisition: locked archives, SHA-256, ZIP containment and expected executable checks. Replace with PowerShell using unchanged cpp-tools.lock.json; keep Uncrustify 0.83.0 then clang-format 20.1.8. Format-Cpp.ps1 itself is already PowerShell. |
| `tests/conformance/check_compact_negative.py`, `check_utf_string_negative.py`, `check_frame_negative.py`, `check_description_negative.py`, `check_binding_negative.py`, `check_symbol_negative.py`, `check_stream_data_negative.py` | Manual negative fixture/projection drivers invoke built C++ tests against temporary mutations; some include formatting/provenance checks. Move mutations/assertions to test-only C++/doctest and process/format orchestration to PowerShell/CMake. Preserve every mutation label and expected rejection stage, not just case counts. |
| `tests/conformance/check_offline_negative.py` | Manual mutated manifest/expectation checks invoke the offline checker with the current Python interpreter. Migrate with both CLI checkers; temporary fixtures, rejection and unchanged baseline remain mandatory. |
| `tests/conformance/check_json_contract.py` | Manual JSON examples/schema-evidence validator: 17 examples and 13 negative checks in source, including duplicate JSON keys, bool-vs-int and slot ownership. Move to test-only Java validation/oracle tooling; JSON parsing dependency/API needs explicit approval, not an implicit new production importer. Do not use a parser that silently discards duplicate keys. |
| `tests/conformance/check_encoding_evidence.py`, `check_symbol_evidence.py` | Manual retained-evidence hash/provenance audits; symbol also checks preservation/links. Replace with read-only PowerShell hash/metadata/link checks, preserving strict missing/extra/error detection. |
| `tests/conformance/protocol/audit.py`, `protocol/descriptions/audit.py`, `protocol/binding/audit.py`, `protocol/stream-data/audit.py`, `protocol/offline-inspection/audit.py`, `protocol/human-inspection/audit.py`; `tests/fixtures/protocol/p02/audit.py` | Task-era validation/provenance/link/log assertions, some tied to specific .work layouts and historical source hashes. Extract still-needed checks into current PowerShell validation; archive task-specific assumptions, do not reinterpret old hashes as current. |
| `tests/conformance/naming/audit.py`, `executable-naming/audit.py`, `cli-help/audit.py` | Migration-era audits, including process help and protected-file checks. Move enduring no-args/help/invalid-argument and naming assertions into current CTest/PowerShell checks; retain historical logs/metadata separately. |
| `tests/conformance/protocol/human-inspection/design_fixtures.py`, `prepare_evidence.py`, `format_negative.py` | Manual designed-fixture generation, temporary byte reproduction/build metadata and formatter rejection. prepare_evidence invokes design_fixtures with sys.executable and writes reports: do not run it to refresh historical evidence. Replace generator with a test-only C++ fixture producer, metadata/formatter orchestration with PowerShell; prove identical designed bytes in a fresh directory. These are designed expectations, not Java oracle captures. |
| `tests/conformance/probe_utf_reuse.py` | Historical UTF library-reuse experiment: generates C++/CMake in ignored work storage from pinned inputs. Not current CTest; keep archived reproducibility unless reused, then migrate bounded generator to C++/CMake. It must not become a current prerequisite. |

The scoped Java oracle `Invoke-Oracle.ps1` workflows and tape `Audit-Evidence.ps1` already provide
non-Python paths. This does not prove all older audits are replaced or that their negative checks
are equivalent. No workflow was retired by this task. Current library-only tests and
BUILD_TESTING=OFF have no CMake Python requirement; documented logging/acquisition can still need it.

## Ordered migration acceptance

1. Replace UTC logging and formatter acquisition first so later migration can be validated without
   adding Python dependencies. Test launch failure, nonzero child exit, spaces/Unicode in argv,
   mixed streams, output/log failure, UTC isolation, missing executable, bad archive hash and path
   traversal. Do not change host timezone or normalize old logs.
2. Replace offline/human process checks together with their imports and offline negative driver;
   integrate CTest, then remove find_package(Python3) only after equivalence. Assert exact stdout,
   stderr, exit codes, 30-second process bounds, help/no-args, invalid args, missing input/output
   failures, TSV/human consistency, UTF-16 escaping and output budgets. Keep injected IO doctests.
3. Migrate each primitive/protocol negative family and common evidence checks. Run old and new
   validators on identical temporary mutations, require matching rejection and baseline success,
   compare fixture hashes before/after, retain separate new UTC validation in EN/RU.
4. Move JSON validation and designed-fixture reproduction, including duplicate-key/unknown-row
   rejection and byte-for-byte fresh regeneration. Archive only experimental or completed-task
   entry points; port any check still needed by current acceptance before removing active references.
5. Verify a fresh Windows and Linux/WSL environment with Python unavailable: production tool ON/OFF,
   BUILD_TESTING ON/OFF, CTest, formatter acquisition/check, relevant Java Verify, negative suites and
   evidence audits. Search active entry points/docs for Python references and distinguish archive
   links. Missing runtime must not silently skip tests. Report only profiles actually run.

The final migration acceptance is **no Python requirement in current workflows**, not deletion of
every `.py` or historical mention. Preserve fixtures/captures/logs, lock files, hashes and timestamps;
new reports reference old evidence without editing it. Do not convert an archived author report
into an independently rerun check. These are later bounded tasks, not implementation authorization.
