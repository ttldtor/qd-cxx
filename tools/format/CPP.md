# C++ formatting

[Русская версия](CPP.ru.md). English is authoritative.

The [style contract](../../docs/CPP-STYLE.md) applies to the QDCXX-010 C++ component.
[Format-Cpp.ps1](Format-Cpp.ps1) runs **Uncrustify 0.83.0 → clang-format 20.1.8** using the unchanged
root configurations. PowerShell 7.4+ and Git are required; acquisition does not require Python or pip.

```powershell
pwsh -NoProfile -File tools/format/Setup-Cpp.ps1
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
```

[Setup-Cpp.ps1](Setup-Cpp.ps1) downloads the exact Windows x64 archives in
[cpp-tools.lock.json](cpp-tools.lock.json), verifies SHA-256 before extraction and stores everything
under ignored `.work/`. The Uncrustify archive/hash comes from its official GitHub 0.83.0 release;
the clang-format wheel/hash comes from PyPI's clang-format 20.1.8 release metadata (Python wheel
packaging of the formatter, not a runtime dependency). Cached archives are also verified.
No global installation occurs. Re-run setup to restore executables from verified archives.

Setup uses only the repository lock and `tools/format/.work/`; there is no alternate-lock option.
Downloads go to temporary files and are verified before entering the archive cache. Every archive,
including a warm-cache hit, is hash-checked before extraction into a fresh staging directory.
All ZIP names and attributes are checked before extraction: absolute paths, parent traversal with
either separator, alternate data streams, Windows device/ambiguous names, duplicate paths and
link/special/reparse entries are rejected. Existing filesystem reparse points are rejected as well.
Only an installation containing its expected executable is published; extraction failure preserves
the previous installation. Temporary staging/download files are cleaned after failures.

The standalone [setup checks](../../tests/conformance/python-migration/Test-SetupCpp.ps1) exercise
offline hostile archives through a private helper, real cold/warm Windows acquisition, and the existing
wrapper's version rejection, read-only Check, Format and idempotence. They require network access for
the cold-cache run and retain task-local files under `.work/python-migration-u3/`.
This acquisition path is Windows x64 only. Linux/WSL contributors can use validated Windows-host
formatting or provide the exact pinned native formatter versions to the wrapper; these Windows archives
do not supply Linux-native tools. [setup_cpp.py](setup_cpp.py) remains unchanged for historical
reproduction and is no longer the active setup command.

The wrapper validates both executable versions and fails on missing tools/configurations,
Uncrustify diagnostics or clang-format warnings/errors. Optional `-Uncrustify` and `-ClangFormat`
paths must still have the exact versions. Acquisition never silently substitutes versions.
`Check` runs both tools on temporary copies, compares bytes and does not rewrite originals.
`Format` copies the pipeline result back only when changed. Temporary copies are removed afterward.

Default discovery uses Git's tracked/nonignored new files under `include/` (`*.hpp`), `src/`
(`*.cpp`) and `tests/conformance/` (`*.cpp`). Fixtures, upstream and generated directories are
excluded. `-Files` can select files in these roots or `.work/` for deliberate negative checks;
it rejects other paths/extensions. Add new source extensions explicitly when a task needs them.
Naming, braces, header self-containment, namespace comments and meaningful Doxygen were separately
reviewed; these formatters do not enforce all those rules. The combined pipeline's idempotence and
read-only mismatch detection are recorded in [validation](../../tests/conformance/encoding/VALIDATION.md).
Java formatting and oracle provenance rules in [README](README.md) remain separate and unchanged.

QDCXX-017 extends discovery and explicit-file validation to `tools/qtp-inspect/*.cpp` and
`*.hpp`. This keeps the optional tool under the same pinned two-stage pipeline; no upstream,
fixture or generated source enters formatting scope.

QDCXX-021 additionally includes tools/tape-inspect/*.cpp and *.hpp in the same pinned Format/Check
scope. Existing inspector methods remain unchanged; tape presentation reuses bounded row helpers.
