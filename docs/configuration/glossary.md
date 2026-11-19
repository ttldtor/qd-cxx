# Glossary values

[Русский](glossary.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [AdditionalUnderlyingLimits](glossary.md#cfg-0341-owner) — `include/qd/glossary/additional_underlyings.hpp`
- [PriceIncrementLimits](glossary.md#cfg-0343-owner) — `include/qd/glossary/price_increments.hpp`

## AdditionalUnderlyingLimits {#cfg-0341-owner}

[include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp)

### `AdditionalUnderlyingLimits::textUnits` — CFG-0341 {#cfg-0341}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: canonical text UTF-16 units
- Accepted values: Actual length&lt;=min(textUnits,INT32_MAX). Null/empty input returns empty before checkText; entry count and canonical output are checked separately.
- Read timing: valueOf reads limits on each parse/canonical construction call; the completed value does not reread them.
- Mutability and lifetime: The parameters bound one valueOf call. The immutable result contains the canonical value, with no retained budget; the next call may choose different limits.
- Failure: std::length_error("additional underlying text limit exceeded") for excess; size overflow has a separate length_error; invalid grammar and bad_alloc do not become successful values.
- Consumer sources: [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp), [src/glossary/additional_underlyings.cpp](../../src/glossary/additional_underlyings.cpp), [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp).

### `AdditionalUnderlyingLimits::entries` — CFG-0342 {#cfg-0342}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: additional-underlying input entries
- Accepted values: Each input list entry consumes entries before parsing/deduplication; map construction checks map.size(). Zero permits an empty value; nonempty input rejects.
- Read timing: valueOf reads limits on each parse/canonical construction call; the completed value does not reread them.
- Mutability and lifetime: The parameters bound one valueOf call. The immutable result contains the canonical value, with no retained budget; the next call may choose different limits.
- Failure: std::length_error("additional underlying entry limit exceeded") for excess; size overflow has a separate length_error; invalid grammar and bad_alloc do not become successful values.
- Consumer sources: [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp), [src/glossary/additional_underlyings.cpp](../../src/glossary/additional_underlyings.cpp), [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp).

## PriceIncrementLimits {#cfg-0343-owner}

[include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp)

### `PriceIncrementLimits::textUnits` — CFG-0343 {#cfg-0343}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: price-increment text UTF-16 units
- Accepted values: Input and canonical output length&lt;=min(textUnits,INT32_MAX). Null/empty input returns empty before checking; special empty representations follow separate grammar.
- Read timing: valueOf reads limits on each parse/canonical construction call; the completed value does not reread them.
- Mutability and lifetime: The parameters bound one valueOf call. The immutable result contains the canonical value, with no retained budget; the next call may choose different limits.
- Failure: std::length_error("price increments text limit exceeded") for excess; size overflow has a separate length_error; invalid grammar and bad_alloc do not become successful values.
- Consumer sources: [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp), [src/glossary/price_increments.cpp](../../src/glossary/price_increments.cpp), [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp).

### `PriceIncrementLimits::ranges` — CFG-0344 {#cfg-0344}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: price-increment ranges
- Accepted values: Range count&lt;=ranges; text count=semicolon count+1 after empty-representation handling. Also count&lt;=(SIZE_MAX-1)/2 for paired storage. Zero permits an empty value only.
- Read timing: valueOf reads limits on each parse/canonical construction call; the completed value does not reread them.
- Mutability and lifetime: The parameters bound one valueOf call. The immutable result contains the canonical value, with no retained budget; the next call may choose different limits.
- Failure: std::length_error("price increments range limit exceeded") for excess; size overflow has a separate length_error; invalid grammar and bad_alloc do not become successful values.
- Consumer sources: [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp), [src/glossary/price_increments.cpp](../../src/glossary/price_increments.cpp), [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp).
