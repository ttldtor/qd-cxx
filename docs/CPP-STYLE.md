# C++ style contract

[Русская версия](CPP-STYLE.ru.md). English is authoritative.

Adopted by the owner on 2026-09-21 for future qd-cxx C++ work. The reference is
[ttldtor/dxfcxx_event_test at d96ef76](https://github.com/ttldtor/dxfcxx_event_test/tree/d96ef76f3c4c6649437ac438bd0fa30f91c12f48),
not the moving `default` branch. [Reference manifest](../tools/format/cpp-style-reference.json)
records the full commit, inspected files, Git blob IDs, SHA-256 and formatter versions.

This contract adopts the owner's naming preference and the reference's observed conventions.
The table distinguishes evidence from qd-cxx rules; it does not assert every reference identifier
is uniform. Formatter files cannot enforce naming, API documentation or design quality.
Google Java Style remains a separate rule for Java.

## Naming

| Category | qd-cxx rule | Reference evidence |
| --- | --- | --- |
| Classes, structures and enum types | `UpperCamelCase` (PascalCase), e.g. `RecordCursor`, `FieldKind` | `TaskPattern`, `EventKind`, `SourceTimeMethodComparison` in [core.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/core.hpp). |
| Type aliases | `UpperCamelCase` | `Clock`, `TimePoint` in [monitoring.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/monitoring.cpp); `Process` in [resources.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/resources.cpp). |
| Methods and free functions | `lowerCamelCase`, e.g. `readCompactInt`, `parseRecord`; use ordinary C++ constructor/operator names | `eventCount`, `marketSymbolCount`, `parseTask`, `isMarkerSymbol` in core.hpp/core.cpp. |
| Parameters and local variables | `lowerCamelCase`; names should describe their role | `sortedValues`, `sourceTimeMs`, `observedAtNs` in core.hpp; `encodedSequence` in [client.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/client.cpp). |
| Public data fields, especially data structures | `lowerCamelCase`, no suffix | `publishPeriod`, `subscribedSymbolCount`, `regionalSourceCount` in core.hpp; `successCriteria` in [runner.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/runner.hpp). |
| Encapsulated private data members | `lowerCamelCase_`, no `m_` prefix | `lastBySeries_`, `lastGlobal_` in core.hpp; `initialCpu_`, `rssMaximum_` in [resources.hpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/include/latency/resources.hpp). Apply the same suffix to protected data members as a qd-cxx consistency rule; no separate protected-member pattern was established. |
| Enumerators | `UPPER_SNAKE_CASE` (the owner's BIG_SNAKE_CASE), scoped through the enum type | `EventKind::QUOTE`, `TRADE_ETH`, `TIME_AND_SALE` in core.hpp; `ClientRole::…` in client.cpp. |
| Named constants | `UPPER_SNAKE_CASE`, without a `k` prefix | `MARKER_SYMBOL_PREFIX` in [core.cpp](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/src/core.cpp); `MONITORING_STAT_PROPERTY`, `CALLBACK_QUIET_PERIOD` in client.cpp. |
| Namespaces | Lowercase; use `lower_snake_case` for multiword components as a qd-cxx rule | Reference namespace `latency` and unnamed implementation namespaces. No multiword namespace naming pattern was established. |
| Source filenames | `lower_snake_case.hpp` / `lower_snake_case.cpp` for new project-owned files | `core.hpp`, `resources.cpp`, `runner_main.cpp`, `graal_delivery_client.cpp`. This does not rename third-party headers. |

Do not uppercase every `const` or `constexpr` variable. Immutable instance state such as
`expectedPerBatch_` still follows member naming; ordinary constant locals such as `largest`,
`interval` and `numericNames` retain lowerCamelCase in the reference. Reserve uppercase for values
used as named constants. Prefer `enum class` and qualify enumerators. Treat abbreviations as words
where practical (`sourceTimeMs`, `initialCpu_`, `rssMaximum_`); preserve external type/API spelling.
Standard-library customization points, required override names, C ABI names and external macros
retain their prescribed names. Do not rename wire identifiers or JSON keys to satisfy C++ naming.


## Numeric format semantics

Numeric values expressing format semantics must have a meaningful name or a local explanation.
Use named constants in `UPPER_SNAKE_CASE` for encoding sizes, bit masks, prefixes, range boundaries
and Unicode values. Equal numbers may have different names when their semantic roles differ.
Keep constants and small helpers in the narrowest useful scope; do not expand the public API or
introduce a generic framework merely to share a literal. Explain nontrivial formulas briefly,
such as compact payload growth, prefix-bit placement and full-width escape sizes.

Obvious counters, indices and ordinary zero/one arithmetic need no named constant. Exact wire bytes
in test examples should remain literal when naming them would obscure comparison with the oracle.
This is an owner-requested qd-cxx rule, not an additional claim about the reference project.
The [codec readability validation](../tests/conformance/encoding/readability/VALIDATION.md) records
its initial application without changing the encoding contracts.

## Layout and complete formatter pipeline

The root [.clang-format](../.clang-format) and [.uncrustify.cfg](../.uncrustify.cfg) are exact copies
of the pinned reference blobs, with copyright/SPDX retained. The copied configuration license is
[BSL-1.0](../tools/format/licenses/dxfcxx_event_test-BSL-1.0.txt); this does not select a license for
qd-cxx itself. [.editorconfig](../.editorconfig) supplies C++ editor defaults separately from Java.

- LLVM-based layout, 4-space block and continuation indentation, no tabs, 120-column target.
- Opening braces stay on the declaration/control-flow line; bodies use explicit braces. Do not
  compress nonempty functions, lambdas, control-flow bodies or loops onto one line.
- Separate definitions and logical groups with blank lines, at most one consecutive empty line.
  No leading/trailing blank lines or blank line at a block's start; terminate files with a newline.
- Separate complete `if`, `for`, `while`, `switch` and `do` blocks from adjacent statements.
  Separate a `return` from preceding statements in the same block; do not invent a blank line
  before a return that is the block's first statement. Uncrustify supplies these statement rules.
- Keep the reference's access-label placement: `IndentAccessModifiers: false` and
  `AccessModifierOffset: 0`. In the inspected classes, `public:` aligns with the member indentation
  (four spaces inside the class); it is not moved flush with the class declaration. There is no
  automatic empty line after the label, and a logical-block gap before it.
- Follow the reference's pointer/reference placement (`const T &value`, `T *value`) and ordinary
  attached `else` layout. Let the pinned formatter resolve line wrapping and include sorting.

The [reference formatting driver](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/tools/format.cmake)
requires **Uncrustify 0.83.0 → clang-format 20.1.8**, in that order. Uncrustify is a supplement;
clang-format makes the final layout pass. Running clang-format alone, reversing the order or
assuming LLVM defaults from a different release is not equivalent.

When an approved task introduces C++ sources, add and validate a repository-owned Format/Check
wrapper using these exact versions. Check must format temporary copies through both tools,
compare bytes and fail on differences without rewriting inputs. Validate idempotence of the
combined pipeline and a deliberate formatting mismatch. Do not silently ignore missing tools or
unknown configuration keys. This records the required future workflow; no C++ formatter was run
or installed in this documentation/configuration change, and no CMake target is added here.

## Source organization and documentation

Use `#pragma once` for project headers. Include the corresponding project header first in its
implementation, then clearly separated dependency/standard-library groups. Headers must include
their own requirements. Keep implementation-only helpers in unnamed namespaces in `.cpp` files;
avoid broad `using namespace` directives in public headers. Use namespace-closing comments.
These rules follow the inspected headers and implementation layout, without adopting the
reference's module structure or public API.

Following the reference's [CONTRIBUTING.md](https://github.com/ttldtor/dxfcxx_event_test/blob/d96ef76f3c4c6649437ac438bd0fa30f91c12f48/CONTRIBUTING.md),
write meaningful Doxygen comments for types, enumerators, public data fields, public methods/free
functions and utility functions. Both `/** … */` and `///` occur in the reference. Explain contracts,
units, lifetime, failure conditions and non-obvious constraints rather than restating an identifier.
Document each parameter with `@param`, each template parameter with `@tparam`, and every non-void
result with `@return`. Describe ownership, lifetime, byte counts and error outcomes where applicable.
The QDCXX-010 owner follow-up makes these tags mandatory, including for utility functions.

### Contract comments versus provenance

Production-code comments explain the current contract, constraints and reasons for decisions, not
the history of the project. Keep upstream versions/commits, task IDs and validation history in
specifications, `upstream.lock` and provenance reports. Prefer a reference to the relevant specification
instead of repeating its pinned upstream version next to every type or constant.

Include a version or historical reference in code only when it explains version-specific behavior,
a compatibility exception or a workaround; state why that reference matters. For example, an enum
comment should explain that it is incomplete and that raw numeric IDs retain representation bits,
without repeating the upstream release merely used to verify those IDs. Required copyright, license
and attribution notices are unaffected. Apply this owner-requested rule to new or touched comments;
it does not authorize bulk cleanup or rewriting historical evidence.

The reference also uses `std::format`, raw strings, standard C++20/23 facilities and doctest.
For qd-cxx, readable standard C++23 remains the existing goal; choose raw strings when clearer and
`std::format` for composed human-readable text when appropriate. This style adoption does **not**
select doctest, a dependency manager, build system, transport or architecture. `[[nodiscard]]`,
`const`, `noexcept` and ownership choices express real contracts and must not be added mechanically
as decoration. Protocol semantics still come from the pinned Java QD evidence.

## Validation and limits

Inspected all four public headers, representative implementation files (core, client, runner,
monitoring and resources), core tests, contributor guidance, both configurations and the formatting
driver at the recorded clean checkout. Configuration/license copies were compared byte-for-byte
with `git show <commit>:<path>`; hashes are in the reference manifest. EN/RU structure and relative
links were checked along with `git diff --check`. The external checkout remains ignored.

This is source/configuration analysis and a future style decision, not an executed C++ formatting,
build, test or compatibility result. No production C++, importer, Java/oracle edit, fixture change
or new build/test dependency is part of this update. Naming enforcement needs review now and a
separately validated linter once C++ tooling is introduced; these two formatters cannot enforce it.

## Project namespace and build prefixes

The owner selects `qdcxx` and the namespace root `org::ttldtor::qdcxx`, reflecting ttldtor.org.
Use subnamespaces such as `org::ttldtor::qdcxx::core`; do not add `qd` aliases. All project CMake
targets, including tests, use `qdcxx_`; the project name is `qdcxx`. Project-owned options, cache
variables and externally visible macros use `ORG_TTLDTOR_QDCXX_`. Standard/dependency identifiers
(`BUILD_TESTING`, `CMAKE_*`, `Python3_*`, `doctest::doctest`) retain their names. Historical evidence,
upstream Java names and fixed output-format identifiers are not renamed by a C++ source migration.
See [migration and current names](NAMING.md). This owner rule supplements the reference style;
it is not a claim about the reference project's namespace.
