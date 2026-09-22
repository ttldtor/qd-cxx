# No-argument help validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Owner-requested CLI policy: no arguments use the same code path and bytes as standalone `--help`,
with stdout, empty stderr and exit 0 on successful output. Nonempty invalid/incomplete arguments
remain usage exit 2. Help write/flush failure remains I/O exit 3. The current user-facing qdcxx utility
is the QTP inspector; AGENTS.md applies this rule to future qdcxx utilities. Internal operational
scripts/test runners retain their documented invocation contracts.

Author checks executed 2026-09-22 UTC:

- MSVC Release build and all 10 suites.
- WSL GCC Release build and all 10 suites.
- User's `cmake-build-debug` inspector rebuilt with its existing MSVC/Ninja configuration.
- Pinned Uncrustify → clang-format Format and Check.
- Direct no-argument/--help comparison for the user's executable, fixture hashes, bilingual links and diff:
  audit script, log, results/source hashes.

No errors were detected in these checks. CLI tests now require exact no-argument/help equivalence,
no inspection completion row, continued invalid-argument errors, and I/O failure for both help
invocations using a pipe whose reader is closed before process launch. Existing 72 designed CLI
cases and 84 human cases remain covered. Supplied-input output formats, wire behavior, libraries,
Java sources and fixture bytes are unchanged. EN/RU usage and policy are synchronized.

These are author checks, not independent review. Release builds reused configured directories;
only the inspector target was rebuilt in the user's Debug directory. No full Debug/ClangCL/sanitizer
matrix, fresh Java capture or allocation-failure injection was run for this CLI branch change.
Historical evidence remains historical. No commit was created.
