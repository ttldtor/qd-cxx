# Наблюдения локального IPF writer

[English version](README.md). Английская версия является основной.

[Provenance](provenance.json) связывает32 named cases с pinned QD3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK8, canonical harness/launcher sources,
production-class inventory и точными hashes98 raw assets. Случайная матрица не генерируется.

[Case index](capture/cases.tsv) содержит ID, выбранный native status, число inputs и длину Java
bytes. Каждый .input.tsv перечисляет canonical PROFILE UTF-16 hex name=value pairs; .ipf содержит
actual Java writer bytes; .read.tsv — actual Java reader canonical profiles/EOF completion либо
hex-encoded error. Raw CRLF, встроенные нули и UTF-16 surrogate observations сохраняются.

| Named cases | Наблюдаемая граница |
| --- | --- |
| empty, basic, empty-type, empty-symbol | Complete marker, empty/default/type/symbol records |
| all-text-fields, typed, nonfinite, zero-defaults, typed-boundaries | Все standard categories, даты, пределы, canonical defaults |
| custom-order, custom-case-empty-name, custom-quotes, union-fields | Sorted exact case-sensitive custom names, включая empty, union formats |
| escaping, unicode, embedded-nul, bom-value, paired-boundaries, noncharacters | CSV/CR/LF/quotes, точные BMP/supplementary/null text |
| types-order, duplicate-symbols, hash-type, quoted-type | Header lexical order и source profile order, обычные '#' и quoted types |
| removed, removed-empty-extra, removed-standard, removed-custom | Обычные tombstones работают; Java отбрасывает только nonempty extra data |
| reserved-flush, reserved-complete, reserved-header | Java выдаёт formats, которые отвергает его reader; native INVALID_TYPE |
| unpaired-high, unpaired-low-name | Java заменяет непарный surrogate на '?'; native UNREPRESENTABLE_UTF16 |

[Failure observations](capture/failures.tsv) проверяют actual Java output failures в body,
marker и final flush. Ошибка final flush может оставить полный completion marker снаружи.
Native stream tests проверяют обе exception masks, partial side effects, нижнюю границу
successful-write counts и complete=false; external rollback/atomicity не обещаны.

Native tests сравнивают точные bytes для25 accepted cases и round-trip canonical text через
native reader. Затем [launcher](../../../tools/upstream/ipf-writer-oracle/README.ru.md) может
прочитать actual emitted native files production Java reader. Явные различия определяет
[парный writer contract](../../../spec/ipf/WRITER.ru.md); это не скрытые skipped tests.

