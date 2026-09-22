# Exact values for the default Quote profile

[Русская версия](QUOTE-VALUES.ru.md). English is authoritative.

QDCXX-022. Specified after the first production Java capture and before C++ implementation.
The locked oracle is [QuoteOracle](../../tools/upstream/quote-oracle/QuoteOracle.java);
[evidence](../../tests/fixtures/protocol/quote/COVERAGE.md) records the finite observed set.
This extends the explicitly selected local profile to full identities 2, 8, 56 and 120.
It does not reinterpret other identities sharing a serialization primitive.

## UTF_CHAR and TIME_SECONDS

UTF_CHAR (2) reads one Unicode code point, without a length prefix. The scalar reader accepts
one through four bytes, overlong forms and isolated surrogate points. It returns an int32 value
in 0..0x10FFFF, not necessarily one UTF-16 unit. Read the entire indicated tail before checking
continuations; a missing tail consumes the supplied prefix and reports insufficient input.
A bad lead consumes one byte; a bad full tail or point above 0x10FFFF consumes the full sequence.
Error values are zero. Trailing bytes remain untouched. No allocation or continuation state exists.
The canonical writer accepts the same numeric range including surrogates, writes at most four
bytes, and rejects invalid points or insufficient output atomically. Invalid-point validation
precedes output capacity. UTFString framing and its surrogate-pair rules remain separate.

TIME_SECONDS (56) uses the existing compact integer projection: signed low 32 bits of a compact
long are the raw seconds. Multiplication by int64 1000 produces milliseconds exactly, including
INT32_MIN and INT32_MAX. Type 8 remains an ordinary int32, without time interpretation.
Production FileReader stores supplementary UTF_CHAR points as int values; QuoteMapping casts
them to char. Preserve the raw code point and map only its low 16 bits to the exchange code.

## WIDE_DECIMAL

WIDE_DECIMAL (120) is an exact signed compact-long bit pattern. Preserve all 64 bits before
conversion. Let s be its signed high 56 bits and r its unsigned low eight bits. For r != 0,
the mathematical value is s * 10^(128-r). For r == 0, s == 0 is NaN, positive s is positive
infinity and negative s is negative infinity; noncanonical encodings retain their raw bits.
Finite s == 0 maps to positive zero for every rank. There is no negative-zero representation.

The provisional WideDecimal value owns raw bits and exposes kind, significand, exponent, decimal
text and binary64 conversion. Finite conversion rounds the exact decimal value to nearest binary64
with ties to even, using locale-independent standard conversion. The supported 56-bit/rank domain
fits the finite binary64 exponent range. NaN returns a canonical quiet NaN; comparisons classify NaN
instead of assuming NaN equality. Non-NaN results, including zero sign and infinities, are compared
by exact double bits. No epsilon-only oracle comparison. No decimal arithmetic, coercion matrix,
or C++ double-to-wide composition is supplied.

Decimal presentation matches the observed Java WideDecimal text: NaN, Infinity, -Infinity or 0;
otherwise strip trailing significand zeroes, use plain fractional notation when the decimal point
is inside the digits or requires at most six leading zeroes, and plain integer notation for at
most six appended zeroes. Other values use one leading digit, an optional fractional part and E
with an unpadded signed decimal exponent (no plus sign). Temporary/result text is bounded by
32 ASCII bytes. The raw integer is never presented as the price or size itself.

## Tagged storage and defaults

FieldValue owns an exact full SerialFieldType tag and an int64 raw value: type 8/56 sign-extended
int32, type 2 code point, type 120 unchanged bits. Decoder-produced values satisfy those invariants.
No Java VOID tail or physical two-slot layout is introduced into C++ storage.
Fresh missing fields have raw zero with their local tag; this means numeric zero for int/time/char
and NaN for WIDE_DECIMAL. Repeated matching targets overwrite in incoming order. Known fields with
different full type identities are rejected; Java conversions remain excluded. Supported unknown
fields and records are consumed using their exact primitive, with the same field charges and errors.

LocalProfile::COMPACT_INT_V1 remains the default and preserves old acceptance/errors and int-vector
results. Explicit LocalProfile::QUOTE_FIELDS_V1 permits exactly 2/8/56/120. Invalid profile enums fail
validation. A context owns one tagged default shape per local record, bounded by SchemaLimits;
plans retain exact incoming field types and no borrowed schema. Extended results use typedValues
only; legacy results use values only. Both count one logical value per local field, including missing
fields. Existing output-value and schema/construction budgets apply; no unbounded second copy exists.
All existing terminal/prefix/atomic publication and precise position rules remain in force.

## Additive owned-value invariant

[FIELD-TYPES](FIELD-TYPES.md) adds nullable UTF-16 storage to FieldValue. Every scalar value here requires
absent units; mapQuote rejects a present string payload even with an otherwise correct tag/raw value.
QUOTE_FIELDS_V1 acceptance and default output remain unchanged; the new tags require EXTENDED_FIELDS_V1.
