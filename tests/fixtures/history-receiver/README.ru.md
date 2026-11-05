# Наблюдения стандартного HISTORY receiver

[English version](README.md).

`oracle.tsv` и семь `wire-step-N.qtp` файлов получены настоящими закреплёнными Java production-классами.
`provenance.json` связывает исходники, JDK, harness/launcher, class inventory и каждый fixture.
Используйте [конечный Capture/Verify](../../../tools/upstream/history-receiver-oracle/README.ru.md).
Первый wire-файл содержит DescribeRecords и первый HISTORY DATA; последующие файлы продолжают
тот же словарь composer. Их читают по номеру в одном session context, с явным opt hs.
Native-схема имеет 10 логических полей; пять физических VOID хвостов 64-bit полей исключены.
Это общий нижний receiver witness, а не публичный типизированный native TimeAndSale mapper.