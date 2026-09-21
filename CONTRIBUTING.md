# Contributing

Keep each change tied to one task under `tasks/`. Before editing, read `AGENTS.md`, the assigned task, `docs/CURRENT-STATE.md`, `docs/upstream/OPEN-QUESTIONS.md`, and the documentation or specification areas named by the task.

## Scope and evidence

- Preserve Java QD behavior at the revision recorded in `upstream.lock` unless a reviewed specification and ADR explicitly define a deviation.
- Cite upstream-derived findings using the traceability convention in [`docs/UPSTREAM.md`](docs/UPSTREAM.md).
- Record uncertain behavior in [`docs/upstream/OPEN-QUESTIONS.md`](docs/upstream/OPEN-QUESTIONS.md) instead of guessing.
- Update specifications, documentation, fixtures, and conformance coverage together when protocol or schema behavior changes.
- Keep changes within the assigned task and leave unrelated working-tree changes intact.

## Java formatting

Repository-owned Java uses [Google Java Style](https://google.github.io/styleguide/javaguide.html).
Run `pwsh -File tools/format/Format-Java.ps1 -Mode Format -JavaHome <JDK-21-or-newer>` after edits,
then `-Mode Check` before review. The formatter version and artifact SHA-256 are pinned;
the formatter JDK does not replace the Java QD oracle's JDK 8.
See [commands, scope and provenance](tools/format/README.md) ([Russian](tools/format/README.ru.md)).

## C++ style and checks

Use the owner's reference-derived [C++ style contract](docs/CPP-STYLE.md)
([Russian](docs/CPP-STYLE.ru.md)). It covers naming, documentation and source organization as well
as the root `.clang-format` and `.uncrustify.cfg`. The complete formatting pipeline is
Uncrustify 0.83.0 followed by clang-format 20.1.8. This is separate from Google Java Style.
Use the [Format/Check workflow](tools/format/CPP.md) ([Russian](tools/format/CPP.ru.md)) and
[build/conformance commands](docs/CPP-BUILD.md) ([Russian](docs/CPP-BUILD.ru.md)).
QDCXX-010/011 implement bounded compact int/long and UTFString with minimal CMake/CTest; additional components need approval.

## Documentation-only validation

Before declaring a documentation-only task complete:

1. Confirm every added relative Markdown link resolves from its containing file.
2. Confirm every repository path named by the changed documentation exists or is explicitly described as planned.
3. Run `git diff --check` to catch whitespace errors.
4. Review `git status --short` and `git diff` for unintended files, implementation, or dependency choices.
5. Update `docs/CURRENT-STATE.md`, the task status, or the open-question list when the change alters any of them.

Do not commit generated or editor-local files. Fixture and generated-output provenance rules are documented in [`tests/fixtures/README.md`](tests/fixtures/README.md) and [`tools/upstream/README.md`](tools/upstream/README.md).
