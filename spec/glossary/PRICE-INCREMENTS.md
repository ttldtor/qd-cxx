# Price increment contract

[Русская версия](PRICE-INCREMENTS.ru.md). English is authoritative.

Compatibility authority is `com.dxfeed.glossary.PriceIncrements` and its `AdditionalUnderlyings` and `MathUtil` callees at `upstream.lock`. See the [user guide](../../docs/PRICE-INCREMENTS.md) for the public C++ API and limits. The translated numerical implementation retains its MPL2 file license; the repository-wide license is unchanged.

## Value and admission

A rule owns exact UTF-16 text, alternating binary64 increments/upper limits and a derived precision table. Parsed tables are immutable and eagerly complete before publication. Copy shares immutable state; move leaves empty state. No cache or lazy mutation is needed. Array getters return independent copies. Text identity/hash are independent of numeric equality.

Input/output text is bounded by `PriceIncrementLimits::textUnits` and Java signed32 string length; range count is bounded by `ranges` before table allocation. Empty/null input is admitted with zero bounds. Nonempty zero spellings are scanned only after the text bound is checked; the empty rule does not retain an allocation. Failure cannot replace an existing value, and `bad_alloc` is never converted into invalid syntax.

Text grammar and legacy decimal parsing follow the actual production source. Multiple ranges require a finite increment and limit at least `1e-18`; later limits must exceed their predecessor plus `2e-18`. The last range has no limit. A single text increment accepts nonnegative finite values smaller than the multiple-range floor. Array formatting imposes the floor on every nonempty entry before formatting; single-double construction does not. Numeric constructors format to fifteen significant digits, then parse again and validate the rendered range ordering. Unknown text is not silently mapped to empty.

## Observable calculations

For range selection, reflect a negative price and negate direction with signed32 modulo arithmetic. NaN selects the first range. Scan limits until `price <= limit + min(adjacent increments) * 1e-6`. Below the corresponding negative tolerance, select the lower increment. Within tolerance, negative direction selects lower, positive selects upper, zero selects their minimum. Precision breakpoints are cleaned to fifteen significant digits; each range precision is computed by multiplying its increment by ten until it is within relative `1e-6` of an integer, up to eighteen places.

For rounding, divide by the selected increment. Ceiling/floor adjust the quotient by minus/plus `1e-6`; half-up/down adjust by `0.5 + 1e-6`, reflecting tie direction around zero. Half-even floors the quotient and increments when its remainder is at least `0.5 + 1e-6`, or exceeds `0.5 - 1e-6` and the floor is odd. Multiply by the increment and apply numerical precision15 HALF_UP cleanup. `UNNECESSARY` compares that result with the input and refuses a changed price. NaN and zero-increment paths preserve input before policy evaluation.

Numerical cleanup preserves nonfinite values and signed zero. It uses the production integer mantissa for integral values with leading decimal exponent zero through sixteen. Other finite values use the shortest-decimal policy. Exact decimal-half ties follow `BigDecimal.valueOf`'s Java8 `Double.toString` representation, including its integral fast path below `2^63`; they do not use the additional-underlying text formatter's half-up policy. Boundary tests must exercise these paths separately.

Advancement validates nonzero direction and nonnegative finite step before handling NaN. Select `Math.max(increment, step)`, giving positive zero precedence, add/subtract according to direction, then half-even round. Binary infinities and multiplication overflow retain observed Java behavior. The reference's precise decimal domain is at most fourteen significant digits and fourteen digits after the decimal point.

## Durable verification

`tests/fixtures/price_increments/oracle.tsv` records actual production Java8 outcomes for text/scalar/array factories, complete public value projections, breakpoint neighborhoods, reflected directions including signed32 extrema, all eight rounding modes, steps, NaN/infinities/signed zero, seeded random values and numerical half-tie neighborhoods. Expectations are captured, never hand-repaired. `provenance.json` binds the launcher, harness, production sources/classes and fixture. Locale/timezone captures must agree.

Native conformance compares exact binary64 bits with NaN canonicalization only where Java's observation does so. Additional tests preserve NaN payloads, text ownership, copy/move state, atomic failure behavior, finite limits and startup access. Windows/Linux installed and moved consumers compile self-contained public headers and the guide example against `qdcxx_ipf`. The component OFF graph excludes the price sources, headers and per-file license installation. No live service is needed for these value contracts.
