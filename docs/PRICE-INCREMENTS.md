# Instrument price increments

[Русская версия](PRICE-INCREMENTS.ru.md). English is authoritative.

Use `glossary::PriceIncrements` from `qdcxx_ipf` to interpret an instrument's `PRICE_INCREMENTS` field and to select, round or advance prices. The rule owns its text and immutable range tables. Copies share immutable ownership; a moved-from value is empty and reusable. Const queries may run concurrently; assignment and destruction require caller synchronization.

```cpp
#include <qd/glossary/price_increments.hpp>
#include <qd/ipf/instrument_profile.hpp>

namespace q = org::ttldtor::qdcxx;

int main() {
    q::ipf::InstrumentProfile instrument;
    if (!instrument.setPriceIncrements(u"0.01 1; 0.05")) {
        return 1;
    }
    auto rules = q::glossary::PriceIncrements::valueOf(instrument.getPriceIncrements());
    auto tick = rules.getPriceIncrement(1.0, +1);
    auto price = rules.roundPrice(1.023, q::glossary::PriceRoundingMode::HALF_EVEN);
    auto next = rules.incrementPrice(price, +1);
    return tick == 0.05 && price == 1.0 && next == 1.05 ? 0 : 1;
}
```

The example uses the IPF field's exact text; the profile's lifetime is needed only during `valueOf`. Supply the rule explicitly to order validation or price entry. Receiving a profile does not change application price rules automatically.

## Construction and identity

Text has the form `increment upperLimit; increment upperLimit; increment`, with exactly one ASCII space at each shown separator. Ranges cover the nonnegative axis; negative prices use reflected ranges. Increments and limits in multiple ranges must be finite and at least `1e-18`. Limits must ascend by more than `2e-18`. A sole increment parsed from text may be smaller, including underflowed zero.

Null, empty and signed plain-decimal zero spellings such as `-0.0` produce the empty rule. Its text is empty, increments are `{0}`, precision is zero, and rounding preserves the input. Other accepted spellings retain exact text: `-0e0` retains negative zero and compares differently from the empty rule. Numeric equality does not define text identity. `hashCode` uses Java's signed32 UTF-16 text hash.

`valueOf(double)` rejects negative and nonfinite input; either signed zero is empty. `valueOf(span<const double>)` accepts an empty array or alternating increments/limits of odd length. Its single zero entry is empty; other entries must be finite and at least `1e-18`. Numeric construction renders with the reference 15-significant-digit policy and reparses; supplied binary values can change. Text construction preserves the production legacy decimal grammar, including its exponent arithmetic and signed zero behavior; it does not accept Java hexadecimal literals.

`PriceIncrementLimits` defaults to 1,048,576 UTF-16 code units and 16,384 ranges. Limits are checked before retaining input or allocating range tables. Zero ranges admit only the empty rule. Exceeding a bound raises `std::length_error`; grammar and value errors raise `std::invalid_argument`. Allocation failures propagate without publishing a partial value. Array-validation messages need not reproduce Java's embedded array rendering.

## Selection, precision and rounding

`getPriceIncrement(price, direction)` uses a tolerance of `min(adjacent increments) * 1e-6` at a breakpoint. Positive direction selects the upper increment, negative selects the lower, zero selects their minimum. Negative prices reverse the direction with Java signed32 wrapping; NaN selects the first increment. `getPricePrecision(price)` chooses the lower range at its tolerated boundary and returns zero through eighteen decimal places.

`roundPrice(price, direction)` floors, ceilings or half-even rounds for negative, positive or zero direction. `PriceRoundingMode` also supports away/toward zero, half-up, half-down and `UNNECESSARY`. The last policy throws `std::domain_error` when rounding would change the price. NaN is returned unchanged, including its payload, and zero increment preserves the input before policy validation. Unknown enum values otherwise raise `std::invalid_argument`.

`incrementPrice(price, direction, step)` requires nonzero direction and a nonnegative finite step, even for a downward move. It advances by the larger of the applicable increment and step, then half-even rounds. Signed zero follows Java's `Math.max` precedence.

Decimal cleanup follows the numerical precision15 HALF_UP policy, including its Java8 decimal tie fallback; this differs from formatting an additional-underlying quantity. The reference documents precise decimal results for at most fourteen significant digits and fourteen fractional places. Binary64 infinities and larger/smaller magnitudes retain the observed reference behavior; this is not arbitrary-precision decimal arithmetic.

See the [price-increment contract](../spec/glossary/PRICE-INCREMENTS.md) and the API declaration in `qd/glossary/price_increments.hpp`.
