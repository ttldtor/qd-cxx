# Pinned primitive contract

[Русская версия](PRIMITIVES.ru.md). English is authoritative.

QDCXX-009 describes Java QD 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Source-derived rules below are supported by bounded [executed vectors](../../tests/fixtures/encoding/COVERAGE.md), not a claim of exhaustive Unicode or C++ conformance. Exact outputs come from Java, never an independent encoder in the harness.

## Exact methods and source boundary

- [BufferedOutput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedOutput.java): `writeCompactInt`, `writeCompactLong`, `writeUTFString` (lines 323, 362, 453).
- [BufferedInput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedInput.java): `readCompactInt`, `readCompactLong`, `readUTFString`, `readUTFBody`, `readUTF2/3/4`, `checkEncapsulatedLength` (484, 509, 610, 635, 922–944, 121).
- [BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java) `describeRecord` writes record/field names with `msg.writeUTFString` (214/217); [BinaryQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java) `parseDescribeRecords` reads them with `msg.readUTFString` (362/369).
- [BinaryRecordDesc](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java) separately uses UTF char, IOUtil char-array strings and UTFString (349/361/367, 541/555–583). Those other representations and PentaCodec are outside this contract. IOUtil equivalents are documented by upstream but not independently executed here. JSON Unicode policy does not define these wire primitives.

## Canonical compact writer

Signed values use prefix length selection and big-endian payload bits, not LEB128 or ZigZag. Source-derived long length table:

| Bytes | Inclusive value range | Prefix | Executed boundary IDs |
| --- | --- | --- | --- |
| 1 | −2^6 … 2^6−1 | 0xxxxxxx | I/L-b6-* |
| 2 | −2^13 … 2^13−1, excluding shorter | 10xxxxxx | I/L-b13-* |
| 3 | −2^20 … 2^20−1, excluding shorter | 110xxxxx | I/L-b20-* |
| 4 | −2^27 … 2^27−1, excluding shorter | 1110xxxx | I/L-b27-* |
| 5 | −2^34 … 2^34−1, excluding shorter | 11110xxx | L-b34-* |
| 6 | −2^41 … 2^41−1, excluding shorter | 111110xx | L-b41-* |
| 7 | −2^48 … 2^48−1, excluding shorter | 1111110x | L-b48-* |
| 8 | −2^55 … 2^55−1, excluding shorter | fe + 7 payload bytes | L-b55-* |
| 9 | remaining signed 64-bit values | ff + 8 payload bytes | L-min/max, L-b55-* |

For lengths 1–7 the remaining prefix bits belong to the signed payload; length 8 has 56 signed payload bits. Int writer uses lengths 1–5: the fifth-byte form has f0 for nonnegative and f7 for negative values followed by the 32 raw bits. Long writer delegates int-range values to int writer. `I/L-zero`, `minus1`, `plus1`, `minus2`, `min`, `max` cover sign and extrema. Every boundary `b` has `neg/pos-d-1/d0/d1`, meaning −2^b or +2^b plus −1/0/+1. This includes both sides of every length transition; no out-of-domain int is passed to its writer.

## Additional compact reader forms

[integers.tsv](../../tests/fixtures/encoding/expected/integers.tsv) retains writer bytes and separate reader observations. `X0–X7/as-int` demonstrate complete consumption with low-32-bit signed results, including values outside int range; no overflow rejection occurs in this measured set. `XI*/as-long` checks four int-written inputs with long reader.

[forms.tsv](../../tests/fixtures/encoding/expected/forms.tsv) N0–N15 independently supply zero and −1 in each length 2–9. Both readers accept them. N16/N17 use f1/f6 with zero low bits: long returns 4294967296/−8589934592; int returns 0. Reader acceptance does not make these canonical writer representations of zero. The full bit grammar follows source; probes do not enumerate all bit patterns.

## UTFString writer and reader

The writer prefixes UTF byte length using compact long; −1 means null (`7f`), zero means empty (`00`). ASCII/NUL use one byte, BMP values use one to three, valid surrogate pairs use a four-byte code point, isolated surrogates use three-byte CESU-8 forms. [strings.tsv](../../tests/fixtures/encoding/expected/strings.tsv) preserves raw UTF-16 code units: NUL in A/NUL/B is `03410042`, U+1F600 is `04f09f9880`, lone d800 is `03eda080`, lone dc00 is `03edb080`. Reversed surrogates remain two isolated units. Length counts bytes, not UTF-16 units; len63/len64 exercise the compact-length boundary.

Additional accepted reader forms: F-overlong2/3/4 each decode NUL; F-cesu-pair decodes d83dde00 from six CESU bytes; F-surrogate4 accepts d800 encoded in four bytes. F-overlong-null/empty/length accept overlong compact lengths. No normalization or replacement character is inserted. This differs from strict UTF-8 validators and from Java DataInput modified UTF; do not import either policy.

## Measured failures and consumed position

Each direct read starts at zero on a fresh ByteArrayInput; positions are observed with `totalPosition()`, with no mark/reset. Every strict prefix of the canonical integer/string cases is captured. In those canonical-prefix observations EOF consumes the entire supplied prefix, including partial payloads. This does not promise rollback or characterize all BufferedInput subclasses or outer QTP buffering.

Malformed forms are independent hand-authored inputs, not writer outputs. Invalid continuation bytes, continuation-as-lead, f8/ff leads and >U+10FFFF yield `java.io.UTFDataFormatException` at the positions in forms.tsv. `01c280` consumes 3 bytes before rejecting an overrun of its declared one-byte body; `01c2` yields EOF at 2. `0241` yields EOF at 2. `014142` succeeds at 2, leaving one byte. Prefixes of malformed forms may themselves succeed or fail differently; captures retain actual outcomes rather than labeling every prefix EOF.

The source length check allows −1 … 8589934588 (Integer.MAX_VALUE × 4). F-negative-length rejects −2 at position 1; F-limit-plus1 probes 8589934589 (one above the source maximum) at position 5; F-longmax-length rejects Long.MAX_VALUE at 9, all with IOException before allocation. Accepted huge lengths and exact allocation limits are deferred; no huge allocation was attempted. Bounds are operational safeguards, not proposed wire limits.

## Scope

See [validation](../../tests/fixtures/encoding/VALIDATION.md) for independent clean builds, exact comparisons and mutation detection. No production C++, importer, transport, universal fuzzing, PentaCodec, other UTF APIs, full parser error policy or upstream test-suite result is supplied. Remaining extreme-length/resource and alternate-buffer behavior requires focused follow-up. The [QTP parser buffering contract](../protocol/README.md) remains separate.
