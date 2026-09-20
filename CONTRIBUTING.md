# Contributing

Keep each change tied to one task under `tasks/`. Before editing, read `AGENTS.md`, the assigned task, `docs/CURRENT-STATE.md`, `docs/upstream/OPEN-QUESTIONS.md`, and the documentation or specification areas named by the task.

## Scope and evidence

- Preserve Java QD behavior at the revision recorded in `upstream.lock` unless a reviewed specification and ADR explicitly define a deviation.
- Cite upstream-derived findings using the traceability convention in [`docs/UPSTREAM.md`](docs/UPSTREAM.md).
- Record uncertain behavior in [`docs/upstream/OPEN-QUESTIONS.md`](docs/upstream/OPEN-QUESTIONS.md) instead of guessing.
- Update specifications, documentation, fixtures, and conformance coverage together when protocol or schema behavior changes.
- Keep changes within the assigned task and leave unrelated working-tree changes intact.

## Documentation-only validation

Before declaring a documentation-only task complete:

1. Confirm every added relative Markdown link resolves from its containing file.
2. Confirm every repository path named by the changed documentation exists or is explicitly described as planned.
3. Run `git diff --check` to catch whitespace errors.
4. Review `git status --short` and `git diff` for unintended files, implementation, or dependency choices.
5. Update `docs/CURRENT-STATE.md`, the task status, or the open-question list when the change alters any of them.

Do not commit generated or editor-local files. Fixture and generated-output provenance rules are documented in [`tests/fixtures/README.md`](tests/fixtures/README.md) and [`tools/upstream/README.md`](tools/upstream/README.md).
