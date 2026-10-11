# QDCXX-007 documentation validation

[Русская версия](QDCXX-007-VALIDATION.ru.md). This English document is authoritative.

Date: 2026-09-21. Scope: documentation and evidence review. QDCXX-007, [requirements](../spec/schema/NORMALIZED-MODEL.md), [proposed ADR](decisions/ADR-0001-normalized-model-boundaries.md).

## Executed checks

- Read architecture/compatibility/current state/open questions, schema inventory/source index, protocol schema exchange/source index and encoding inventory. Read both [schema COVERAGE](../tests/fixtures/schema/COVERAGE.md)/[VALIDATION](../tests/fixtures/schema/VALIDATION.md) and [protocol COVERAGE](../tests/fixtures/protocol/COVERAGE.md)/[VALIDATION](../tests/fixtures/protocol/VALIDATION.md), plus [CAPTURES](../spec/protocol/CAPTURES.md). Reviewed all 17 schema expectations and the protocol examples used by the requirements; preserved exact subcase limits.
- Rechecked local external checkout HEAD with `git rev-parse HEAD`: `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; `git status --short` was empty. Read SchemeRecord.Field.copyFrom/getMainAlias, SerialFieldType.createDefaultIntInstance, DefaultRecord constructor, BinaryRecordDesc's default layout/isEmpty and BinaryQTPComposer.getRecordDesc/describeRecord. Confirmed source basis for copy pointer loss, null construction failure, VOID omission, original-empty distinction, cached-writer behavior and version-dependent wide-decimal fallback. No new execution of these behaviors is claimed.
- Checked relative inline Markdown link targets across repository Markdown files, including newly created files; checked fragments against Markdown headings. Checked adjacent English/Russian result pairs and reciprocal links, and matching evidence-file links within each pair. Reviewed translations for equivalent structure, claims, limits and proposal status.
- Ran `git diff --check`; inspected changed/untracked paths and task/ADR status. Changes are Markdown only, task is done, ADR remains proposed. `git diff --exit-code -- tests tools upstream.lock` confirms no changes to tracked evidence, oracle tooling or lock.

Local link check result: 57 Markdown files, 577 relative link targets, 127 heading fragments, zero problems. Bilingual structural/evidence check result: 10 pairs, zero problems (reciprocal links, heading/table counts and evidence-link sets). These checks do not substitute for semantic translation review.

## Acceptance review

| Criterion | Result |
| --- | --- |
| Compatibility-sensitive requirements cite evidence or carry a proposal label | Requirements use C/S/P/U and exact fixture/source references; no whole-family promotion. |
| Local IDs/sender IDs, wire order/slots, aliases/transmitted names explained | Worked S07/S08/S15, P03/P04/P05 examples; S-derived VOID wire illustration explicitly not a capture. |
| Quirks and unknowns have dispositions | Pinned-profile alias proposal, explicit diagnostic deviation, Java slot projection and bounded first-use requests; remaining SC/QT cases stay open. |
| Transport-independent core and no Java architecture mandate | Three owners/lifetimes separated; physical storage, public API and dependencies unselected. |
| Reviewable without implementation or implied approval | ADR proposed, alternatives/impact/migration and owner choices recorded; scoped backlog has evidence/decision gates. |

## Limits

No Java build, capture regeneration, upstream test, importer, C++ implementation/conformance or new semantic probe was run. Previous independent reproduction and positive/negative verification are QDCXX-005/006 evidence, not checks rerun by QDCXX-007. Documentation link checking is local; remote URL availability was not revalidated. Representation examples are proposals, not JSON goldens. No accepted schema/wire semantics or fixtures changed. Owner review of ADR-0001 remains pending; runtime unknowns are not resolved by this validation.
