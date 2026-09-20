# Проверка документации QDCXX-007

[English version](QDCXX-007-VALIDATION.md). Английский документ является основным.

Дата: 2026-09-21. Область: документация и review доказательств, reasoning High. Задача, [требования](../spec/schema/NORMALIZED-MODEL.ru.md), [proposed ADR](decisions/ADR-0001-normalized-model-boundaries.ru.md).

## Выполненные проверки

- Прочитаны AGENTS.md, задача/зависимости, architecture/compatibility/current state/open questions, schema inventory/source index, protocol schema exchange/source index и encoding inventory. Прочитаны [schema COVERAGE](../tests/fixtures/schema/COVERAGE.md)/[VALIDATION](../tests/fixtures/schema/VALIDATION.md) и [protocol COVERAGE](../tests/fixtures/protocol/COVERAGE.md)/[VALIDATION](../tests/fixtures/protocol/VALIDATION.md), а также [CAPTURES](../spec/protocol/CAPTURES.md). Проверены все 17 schema expectations и protocol examples, использованные в требованиях; сохранены точные границы подслучаев.
- Повторно проверен HEAD локального внешнего checkout через `git rev-parse HEAD`: `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; `git status --short` пуст. Прочитаны SchemeRecord.Field.copyFrom/getMainAlias, SerialFieldType.createDefaultIntInstance, DefaultRecord constructor, default layout/isEmpty в BinaryRecordDesc и BinaryQTPComposer.getRecordDesc/describeRecord. Подтверждена source basis для copy pointer loss, null construction failure, VOID omission, original-empty distinction, cached-writer behavior и version-dependent wide-decimal fallback. Новое исполнение этого поведения не заявляется.
- Проверены relative inline Markdown link targets во всех Markdown files репозитория, включая новые; fragments проверены относительно Markdown headings. Проверены соседние English/Russian result pairs и reciprocal links, а также совпадение ссылок на evidence files в каждой паре. Переводы проверены на эквивалентность структуры, утверждений, ограничений и статуса предложений.
- Выполнен `git diff --check`; проверены changed/untracked paths и task/ADR status. Изменения только в Markdown, задача done, ADR остаётся proposed. `git diff --exit-code -- tests tools upstream.lock` подтверждает отсутствие изменений tracked evidence, oracle tooling или lock.

Результат локальной проверки ссылок: 57 Markdown files, 577 relative link targets, 127 heading fragments, ноль проблем. Результат структурной/evidence проверки переводов: 10 пар, ноль проблем (reciprocal links, число headings/table rows и множества evidence links). Эти проверки не заменяют смысловую проверку перевода.

## Проверка критериев приёмки

| Критерий | Результат |
| --- | --- |
| Compatibility-sensitive requirements ссылаются на доказательства или помечены как proposal | Требования используют C/S/P/U и точные fixture/source references; нет повышения статуса целых семейств. |
| Объяснены local IDs/sender IDs, wire order/slots, aliases/transmitted names | Разобраны S07/S08/S15, P03/P04/P05; S-derived VOID wire illustration явно не является capture. |
| Quirks и unknowns имеют dispositions | Pinned-profile alias proposal, explicit diagnostic deviation, Java slot projection и ограниченные first-use requests; остальные SC/QT cases открыты. |
| Core независим от transport, нет требования Java architecture | Разделены три owners/lifetimes; physical storage, public API и dependencies не выбраны. |
| Возможен review без implementation или подразумеваемого утверждения | ADR proposed, записаны alternatives/impact/migration и решения владельца; ограниченный backlog имеет evidence/decision gates. |

## Ограничения

Не запускались Java build, capture regeneration, upstream test, importer, C++ implementation/conformance или новые semantic probes. Предыдущие independent reproduction и positive/negative verification — доказательства QDCXX-005/006, а не проверки, повторённые QDCXX-007. Documentation link checking локальный; доступность remote URLs повторно не проверялась. Representation examples — предложения, не JSON goldens. Принятые schema/wire semantics и fixtures не менялись. Review ADR-0001 владельцем остаётся pending; эта validation не разрешает runtime unknowns.
