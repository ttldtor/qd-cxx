# Наблюдения грамматики локального несжатого IPF

[English version](README.md). Английская версия нормативная.

[Provenance](provenance.json) связывает 42 именованных случая с закреплённым QD 3.355, ревизией
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK 8, отформатированными harness/launcher,
инвентаризацией production classes и точными хешами входов/результатов. [Индекс](capture/cases.tsv)
содержит ID, длину входа в bytes и число выданных profile/control events.

`<id>.ipf` — исходный несжатый input. `<id>.tsv` перечисляет целые `PROFILE` с точными UTF-16 hex
парами name/value, упорядоченные `FLUSH`/`COMPLETE` и `EOF` либо первый hex-encoded `ERROR`.
Финальная row `READER`/`READER_ERROR` сохраняет фактические list size/completion публичного Java
reader либо первое исключение. Raw CRLF, нулевые единицы и malformed UTF-8 сохранены без изменения.

Покрытие — фиксированный именованный корпус, а не случайная генерация CSV:

| Случаи | Контракт |
| --- | --- |
| basic, header-only-eof, header-only-newline, profile-eof, zero-fields | Реальные formats/records и EOF последней row |
| blank-lines, empty, empty-format, empty-type | Границы blank/empty type/header |
| quoted, multiline, unicode | Quoted comma/escaped quote/newline и точный supplementary/null text |
| unquoted-quote, quoted-suffix, quoted-space, quote-unfinished | Точные CSV-ошибки и выданный правильный префикс |
| short-row, extra-row, extra-bad-quote, invalid-number-first, invalid-date | Число полей, преобразования и приоритет ошибок |
| header-redefine, duplicate-fields, empty-custom-name, metadata-as-type | Замена формата, пропуск empty duplicates, custom names и приоритет поиска |
| comments-controls, complete-eof, complete-then-error, control-malformed-tail | Порядок controls, marker против EOF и поздние ошибки |
| unknown-type, unknown-after-prefix, forbidden-flush, forbidden-complete, forbidden-header | Отказы undefined/reserved formats |
| bom, utf8-stray, utf8-overlong, utf8-surrogate, utf8-bad-middle, utf8-bad-third, utf8-out-of-range, utf8-truncated | Фактическая группировка замен JDK UTF-8 и сохранённый BOM |

Примеры: [basic input](capture/basic.ipf), [basic outcome](capture/basic.tsv),
[multiline input](capture/multiline.ipf), [исход незакрытой кавычки](capture/quote-unfinished.tsv).
[Oracle launcher](../../../tools/upstream/ipf-reader-oracle/README.ru.md) захватывает/проверяет файлы
с закреплённым default missing-completion warn. Нативные тесты сравнивают каждый byte split и tiny
work turns, затем проверяют бюджеты, логические позиции, локальные binary files, владение throwing
stream и явную REQUIRE policy. C++ prefix results/cursors определяет
[контракт reader](../../../spec/ipf/READER.ru.md); Java buffer prefetch не выдаётся за native byte offset.
