# IPF value oracle

[Русская версия](README.ru.md). English is authoritative.

`value-oracle.tsv` contains 22,738 actual observations from pinned Java QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, using JDK 8. [Provenance](provenance.json)
binds the fixture to upstream sources, production-class inventory and formatted harness/launcher.
The fixture retains the captured Windows CRLF bytes; UTF-16 strings are hexadecimal code units.

Each row is `operation<TAB>nameHex<TAB>input<TAB>result`. `text`, `number` and `date` start from a
fresh profile, set textual data and observe normalized text, binary64 bits or day ID. `format`
sets binary64 bits and observes text; `date-format` sets a signed day ID and observes text.
Input is UTF-16 hex except for numeric setter bits/day IDs. Results are UTF-16 hex, numeric bits,
signed day IDs or `ERROR` for Java IllegalArgumentException. Empty final columns are meaningful.

Coverage includes every standard field/default, custom and textual-category behavior, numeric
syntax/overflow/underflow, integer saturation, signed zero, nonfinite values, precision thresholds,
16,384 deterministic random binary64 values, every binary exponent, decimal powers and adjacent
values, subnormals, historical/lenient/invalid dates, Unicode 6.2 digits and overflowing date parts.
UTC and Pacific/Honolulu runs produce identical bytes. Native conformance adds exact UTF-16 custom
ownership, null/empty normalization, copy/move/edit isolation, symbol identity, equality/presentation
and finite per-instance limit failures. Parser/writer/network behavior is not covered.

The [reproducible oracle](../../../tools/upstream/ipf-value-oracle/README.md) uses existing pinned
classes without downloading dependencies or rebuilding the upstream SDK. Reference runtime/tool
hashes are provenance, not public binary prerequisites for ordinary C++ consumers.
