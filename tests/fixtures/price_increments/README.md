# Price-increment observations

[Русская версия](README.ru.md). English is authoritative.

`oracle.tsv` contains 42,594 actual production Java8 observations captured by the [oracle tool](../../../tools/upstream/price-increments-oracle/README.md). `provenance.json` records the pinned revision, harness/launcher, production source/class inventory and raw fixture hashes. Raw tabs and bytes are significant; do not normalize or edit expectations.

Strings use hexadecimal UTF-16 units (`-` means empty; `~` means null). Numbers use hexadecimal Java `Double.doubleToLongBits`; NaNs are canonicalized in this observation format. Error categories `INVALID` and `ARITHMETIC` identify production exception classes; array-containing message text is not an API requirement.

| Row | Cells after row kind |
| --- | --- |
| P | Input text; exact output text, signed32 hash, comma-separated increments/limits, first precision joined with `|`, or error category |
| S | Input number bits; the same complete value projection or error |
| A | Comma-separated input number bits (`-` empty, `~` null); projection or error |
| Q | Input rule text; price bits; direction; step bits; selected increment bits; precision; direction-rounded price bits; comma-separated outcomes for UP/DOWN/CEILING/FLOOR/HALF_UP/HALF_DOWN/HALF_EVEN/UNNECESSARY; advanced price outcome |
| D | Price bits; single increment bits; half-even rounded price bits |

D rows include seeded random binary64 inputs and both signs around decimal-half boundaries. They distinguish numerical cleanup's integer mantissa and Java8 `BigDecimal.valueOf` fallback from additional-underlying text formatting. Other rows preserve malformed separators, legacy exponent wrapping, single/multiple range admission, boundary neighborhoods, signed zero, infinities, NaN, signed32 direction extrema and invalid/positive steps. The fixture is below the raw asset size limit and is retained in full.
