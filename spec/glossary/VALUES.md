# Glossary value contract

[Русская версия](VALUES.ru.md). English is authoritative.

The reference is [CFI.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/glossary/CFI.java)
and [AdditionalUnderlyings.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/glossary/AdditionalUnderlyings.java),
module `dxfeed-api`, at the existing [upstream lock](../../upstream.lock).
Map formatting delegates to `dxlib` MathUtil/FastDoubleUtil precision 15. These are pure
values in optional `qdcxx_ipf`; no Feed reverse dependency or implicit profile-field validation is added.

## CFI

Input consists of at most six A–Z UTF-16 units. Short codes are padded with X; null/empty
and integer zero produce XXXXXX. Packed codes use six five-bit fields with A=1 through Z=26,
first letter most significant; upper two bits must be zero. Invalid lengths, fields or characters
throw `std::invalid_argument` with `code is too long`, `code is invalid` or
`code contains invalid character`, following the reference validation order.

Category/group are the first two letters. E/D/R/O/F/M predicates inspect only the category.
A syntactically valid code need not be a recognized ISO combination. All active reference
category/group mappings and attribute/value order, names and descriptions are retained,
including structured/referential categories and their draft-standard notes. The historical
commented EU mapping is excluded; its active sixth attribute is Strategy. Missing group or
attribute mapping uses Undefined. A recognized attribute's missing X value becomes Unknown;
other missing letters become Unrecognized, retaining the original letter and owning attribute.
The Undefined attribute explicitly contains X=Undefined. Description joins six names with `; `.
Dictionary descriptors contain only constant-initialized pointers/views to static literals.
`Cfi::EMPTY` is constant-initialized; no lazy cache or dynamic dictionary construction is needed.

## Additional underlying text

The documented grammar is `TEXT := "" | AU ("; " AU)*`, `AU := SYMBOL " " SPC`.
Compatibility follows the production implementation even where it is more permissive:
a leading semicolon before the first entry is admitted, symbols may contain tabs, NUL and
unpaired UTF-16 units, duplicates and unsorted text are admitted. A symbol may not contain
ASCII space or semicolon. Parsing scans backward, preserving exact original text and assigning
each symbol's SPC so the first textual occurrence wins. Separator errors throw
`inappropriate use of separators`; invalid SPC throws `SPC is missing or infinite`.
Null and empty text produce empty text/map. The C++ map uses deterministic UTF-16-unit
ordering; Java HashMap iteration order is not a contract.

SPC parsing is a separate legacy decimal grammar: optional sign, digits with at most one dot,
at least one digit, optionally E/e and an exponent. The exponent requires one following unit,
but a sign alone is accepted as zero; exponent arithmetic and decimal-divider subtraction wrap
signed32 exactly. Exponent sentinel 2147483647 is invalid. Mantissa accumulation uses binary64
multiply-by-ten then addition. Scaling uses the correctly rounded binary64 powers 10^0…10^308.
Adjusted powers beyond ±308 produce signed zero or infinity before scaling; within the range,
scaling multiplies/divides. No whitespace, NaN/Infinity spelling, hex syntax or type suffix is
accepted. Numeric overflow infinity remains valid text; NaN is rejected. Negative zero survives.

Instance SPC lookup reads this map (absent symbol gives +0). Static lookup skips whole-text
validation and scans matching symbols backward until a non-NaN SPC is found; duplicate lookup
therefore prefers the last valid textual match. Null/empty text or symbol returns +0.

Map construction sorts symbols, rejects empty keys (`symbol is empty`), space/semicolon keys
(`symbol contains prohibited character`) and nonfinite numbers (`SPC is missing or infinite`).
C++ map keys/values cannot represent Java null keys/boxed null SPC. Null map itself means empty.
Formatting preserves signed zero. It rounds the reference shortest decimal digits to 15
significant digits with half-up magnitude rounding, then removes trailing zeros. The shortest
conversion excludes binary rounding interval endpoints; an included host midpoint can create
a false decimal tie. This is distinct from directly rounding the exact binary value to 15 digits.
Fixed notation applies to original magnitudes in [1e-9,1e10), scientific notation otherwise
(uppercase E, no plus sign).
Smallest-subnormal observations
retain the production extra digit, including 4.9E-324. Rendered text is parsed again; SPC may
change due to rounding or the legacy exponent window. Formatting does not change IPF field or
Candle-period policies. Shared decimal arithmetic is reused only for the subnormal digit case.

## Ownership, failure and bounds

Factories borrow inputs only for the call and return owning values. Borrowed map/text access
lasts until owner mutation/destruction. Metadata views are exclusively program-lifetime dictionary
data. Returned containers are independent. Copy assignment stages the replacement; allocation
failure leaves the target unchanged. Moves leave AU empty and reusable on success; empty-map sentinel allocation may throw before transfer and preserve the source. Allocation failures
propagate, and failed factories publish no partial value. Distinct immutable values may be read
concurrently; mutation/destruction of one owner requires synchronization.

`AdditionalUnderlyingLimits` defaults to 1,048,576 UTF-16 units and 16,384 entries; text parsing
counts duplicates against the entry limit. Input/output length is additionally restricted to
signed32 Java indexing. Exceeding a limit throws `std::length_error`; bounds describe logical
model size, not exact RSS or an external stream budget. Static lookup performs no allocation
and has no retained model or hidden progress budget. Ordinary examples use the defaults.

The [guide](../../docs/GLOSSARY-VALUES.md) describes the public API. The
[oracle](../../tools/upstream/glossary-oracle/README.md) captures observable production behavior,
with [fixture provenance](../../tests/fixtures/glossary/provenance.json). The private dictionary
retains its upstream MPL-2.0 notice; the [license](../../licenses/cfi/LICENSE.txt) is installed
with IPF. Other source licenses remain unchanged.
