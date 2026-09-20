# Executed primitive coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

QD 3.355, locked commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; QDCXX-009. [Driver](../../../tools/upstream/encoding-oracle/EncodingOracle.java), [family manifest](cases.json), [contract/source mapping](../../../spec/encoding/PRIMITIVES.md). Inputs are independently authored; expected bytes and results are Java captures. Fresh ByteArrayInput per read; direct BufferedInput/Output methods only.

| File | Writer rows | Reader rows | Strict-prefix rows | Error rows |
| --- | --- | --- | --- | --- |
| [integers.tsv](expected/integers.tsv) | 96 | 504 | 396 | 396 |
| [strings.tsv](expected/strings.tsv) | 13 | 200 | 187 | 187 |
| [forms.tsv](expected/forms.tsv) | 0 | 333 | 275 | 287 |

Totals: 109 writer observations, 1037 reader observations, 858 strict-prefix reads. Prefix rows are included in reader counts. All 583 canonical-prefix reads fail with EOF at the supplied length; malformed-form prefixes retain their actual mixed outcomes.

## Integer IDs and transitions

`I` means int, `L` long. For each b in I={6,13,20,27}, L={6,13,20,27,34,41,48,55}, all six IDs `{I|L}-b{b}-{neg|pos}-d{-1|0|1}` are executed. Values are sign × 2^b + delta. The [contract table](../../../spec/encoding/PRIMITIVES.md#canonical-compact-writer) maps those boundaries to source branches. `{I|L}-{zero,minus1,plus1,minus2,min,max}` cover zero/sign and extrema. Every such vector has matching writer/reader and every strict prefix `/prefix-N`.

`X0–X7` are long-written: −2147483648, 2147483647, −2147483649, 2147483648, 4294967296, −4294967297, Long.MIN_VALUE, Long.MAX_VALUE. Each has a matching long reader plus `/as-int` and all its strict prefixes. `XI-2147483648`, `XI-1`, `XI0`, `XI2147483647` have int writer/reader and `/as-long`.

N0/N1, N2/N3, N4/N5, N6/N7, N8/N9, N10/N11, N12/N13, N14/N15 independently supply overlong 0/−1 with lengths 2,3,4,5,6,7,8,9. N16/N17 supply f100000000/f600000000. Every N ID is read by both widths with every strict prefix. Complete bounded compact inputs are accepted; malformed compact coverage is truncation, not an invented reserved-prefix rejection rule.

## String IDs

Writer/reader plus all strict prefixes: `S-null`, `S-empty`, `S-ascii`, `S-nul`, `S-bmp`, `S-supplementary`, `S-high`, `S-low`, `S-reversed`, `S-high-ascii`, `S-utf-boundaries`, `S-len63`, `S-len64`. The boundary string contains 007f,0080,07ff,0800,ffff,dbff,dfff code units; it covers the maximum scalar pair too. Values remain raw UTF-16 hex, including isolated surrogates.

Independent reader-only forms, each with every strict prefix:

- Accepted extra forms: `F-overlong2`, `F-overlong3`, `F-overlong4`, `F-cesu-pair`, `F-surrogate4`, `F-overlong-null`, `F-overlong-empty`, `F-overlong-length`.
- Malformed UTF: `F-bad-cont2`, `F-bad-cont3`, `F-bad-cont4`, `F-continuation`, `F-lead-f8`, `F-lead-ff`, `F-too-large`.
- Declared/body length and remaining input: `F-short-declared`, `F-short-declared-eof`, `F-long-declared`, `F-trailing`.
- Safe preallocation rejection: `F-negative-length` (−2), `F-limit-plus1` (8589934589), `F-longmax-length` (9223372036854775807).

## Limits

No accepted large length is allocated. Maximum canonical string body is 64 bytes; input cap is 1024 bytes, JVM heap 64 MiB and oracle timeout 30 seconds. Extreme accepted lengths, allocation/OOM behavior, other buffer subclasses, IOUtil/DataInput equivalence, other UTF APIs, PentaCodec, properties maps, full QTP parser behavior and C++ conformance remain unexecuted. No universal Unicode/fuzzing claim or new wire size limit. [Validation](VALIDATION.md) records reproduction and unchanged expectations.
