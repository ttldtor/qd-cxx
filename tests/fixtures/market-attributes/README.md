# Market-attribute oracle coverage

[Русская версия](README.ru.md). English is authoritative.

`oracle.tsv` contains 276,958 calls to the actual pinned
`MarketEventSymbols` production class. [provenance.json](provenance.json) records
the revision/source hashes, Java tools, harness/launcher hashes, fixture hash,
unchanged production-class inventory hash and UTC/Honolulu comparison.
[Reproduction](../../../tools/upstream/market-attribute-oracle/README.md) retains
private logs and verifies the fixture separately from the regional/base corpus.

Each row has five tab-separated columns: operation (`has`, `get`, `remove`,
`change`), nullable symbol, nullable key, nullable value, result. Strings use
four lowercase hexadecimal digits per UTF-16 unit; `NULL` denotes null and an
empty column denotes empty. `has` results are booleans. Other results are encoded
nullable strings or `THROW:<Java simple exception class name>`.

Cases include null/empty keys and values, exchange preservation, duplicate and
unsorted keys, delimiters inside arguments, NUL and lone surrogates, unknown text,
missing/nested braces, and all 780 blocks of length 1–4 over `a=,{}`. Additional
calls exercise every UTF-16 unit as an inserted key and a queried key.
The corpus observes 9,672 `NullPointerException` results and no other runtime
exception; it does not establish behavior for every possible string or allocation
failure. C++ tests compare every row, retain regional/base regression coverage,
and check independent result ownership and null-key error precedence.
Candle, sources, spreads, regex expansion and subscription routing are outside scope.
