# Market-symbol oracle coverage

[Русская версия](README.ru.md). English is authoritative.

[oracle.tsv](oracle.tsv) contains 66,378 observations of the actual pinned
`MarketEventSymbols` production class. [provenance.json](provenance.json) records
the upstream revision/source hashes, Java tools, harness/launcher hashes, fixture
hash, unchanged production-class inventory hash and timezone comparison.
[Reproduction](../../../tools/upstream/market-symbol-oracle/README.md) creates
private logs and independently checks the fixture.

Each row has four tab-separated columns: operation, nullable input, nullable
argument, result. Strings use four lowercase hexadecimal digits per UTF-16 unit;
`NULL` denotes null and an empty column denotes empty. `has` results are booleans;
all other results are nullable encoded strings. `code` encodes a single unit.
`unit` rows exercise all 65,536 possible exchange code units after `A&`.

The named cases cover composite/regional distinctions, `&Q`, supported/default
lists, empty base, missing/extra ampersands, unsupported ASCII/non-ASCII codes,
NUL, lone surrogates and surrogate pairs, missing/empty/nested/repeated attribute
braces, arbitrary block contents, duplicate/unsorted attributes and null replacement
bases. C++ compares all observations, checks all code units and independent result
ownership. Attribute normalization, regex expansion, Candle, sources and physical
regional subscriptions are outside this fixture's claim.
