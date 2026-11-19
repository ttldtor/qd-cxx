# Encoding

[Русский](encoding.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [CharArrayLimits](encoding.md#cfg-0304-owner) — `include/qd/encoding/char_array.hpp`
- [SymbolLimits](encoding.md#cfg-0306-owner) — `include/qd/encoding/symbol_reader.hpp`
- [UtfStringLimits](encoding.md#cfg-0309-owner) — `include/qd/encoding/utf_string.hpp`

## CharArrayLimits {#cfg-0304-owner}

[include/qd/encoding/char_array.hpp](../../include/qd/encoding/char_array.hpp)

### `CharArrayLimits::encodedBytes` — CFG-0304 {#cfg-0304}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete CharArray including its prefix
- Accepted values: 0 cannot hold even a null/empty prefix. The reader bounds available input by this number; the writer includes prefix and each UTFChar.
- Read timing: readCharArray receives limits by value per call and applies them in this order: bounded prefix → count/null domain → original MESSAGE boundary → codeUnits → character reads. writeCharArray performs its own complete preflight before writing.
- Mutability and lifetime: A new call may receive new limits; an already returned value does not change.
- Failure: Reading first bounds prefix bytes by bounded=min(input.size(),encodedBytes). Without a complete prefix, an artificial cutoff yields RESOURCE_LIMIT and natural exhaustion yields INSUFFICIENT_INPUT. After a complete prefix, lengths outside -1..INT32_MAX yield INVALID_LENGTH and -1 completes as null. For nonnegative count under MESSAGE, count&gt;input.size()-consumed first yields INVALID_LENGTH against the original physical boundary; only afterward does count&gt;codeUnits yield RESOURCE_LIMIT. PRIMITIVE skips the enclosing-message size check. In the UTFChar loop, an artificially clipped end yields RESOURCE_LIMIT and naturally short input yields INSUFFICIENT_INPUT. The writer separately returns INVALID_LENGTH for count&gt;INT32_MAX, RESOURCE_LIMIT for codeUnits/encodedBytes and INSUFFICIENT_OUTPUT for its output buffer.
- Consumer sources: [include/qd/encoding/char_array.hpp](../../include/qd/encoding/char_array.hpp), [src/encoding/char_array.cpp](../../src/encoding/char_array.cpp).

### `CharArrayLimits::codeUnits` — CFG-0305 {#cfg-0305}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: CharArray UTF-16 code units
- Accepted values: 0 admits null and empty values when prefix bytes fit. Unit count ≤ codeUnits; grammar separately bounds count by INT32_MAX.
- Read timing: readCharArray receives limits by value per call and applies them in this order: bounded prefix → count/null domain → original MESSAGE boundary → codeUnits → character reads. writeCharArray performs its own complete preflight before writing.
- Mutability and lifetime: A new call may receive new limits; an already returned value does not change.
- Failure: Reading first bounds prefix bytes by bounded=min(input.size(),encodedBytes). Without a complete prefix, an artificial cutoff yields RESOURCE_LIMIT and natural exhaustion yields INSUFFICIENT_INPUT. After a complete prefix, lengths outside -1..INT32_MAX yield INVALID_LENGTH and -1 completes as null. For nonnegative count under MESSAGE, count&gt;input.size()-consumed first yields INVALID_LENGTH against the original physical boundary; only afterward does count&gt;codeUnits yield RESOURCE_LIMIT. PRIMITIVE skips the enclosing-message size check. In the UTFChar loop, an artificially clipped end yields RESOURCE_LIMIT and naturally short input yields INSUFFICIENT_INPUT. The writer separately returns INVALID_LENGTH for count&gt;INT32_MAX, RESOURCE_LIMIT for codeUnits/encodedBytes and INSUFFICIENT_OUTPUT for its output buffer.
- Consumer sources: [include/qd/encoding/char_array.hpp](../../include/qd/encoding/char_array.hpp), [src/encoding/char_array.cpp](../../src/encoding/char_array.cpp).

## SymbolLimits {#cfg-0306-owner}

[include/qd/encoding/symbol_reader.hpp](../../include/qd/encoding/symbol_reader.hpp)

### `SymbolLimits::inputBytes` — CFG-0306 {#cfg-0306}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of supplied input and UTF symbol body
- Accepted values: input.size() ≤ inputBytes; in UTF_BYTES the body length is also ≤ inputBytes. 0 cannot read a nonempty symbol encoding.
- Read timing: Limits are copied by SymbolReader/PmrSymbolReader construction without a local numeric validator and enforced by read/readBounded.
- Mutability and lifetime: Fixed within the instance; readBounded can tighten codeUnits for one call. beginMessage resets state without changing limits.
- Failure: Exceeding byte or unit limits returns SymbolStatus::RESOURCE_LIMIT and makes the message FAILED; subsequent reads require beginMessage. No partial current value is published.
- Consumer sources: [include/qd/encoding/symbol_reader.hpp](../../include/qd/encoding/symbol_reader.hpp), [src/encoding/symbol_reader.cpp](../../src/encoding/symbol_reader.cpp).

### `SymbolLimits::codeUnits` — CFG-0307 {#cfg-0307}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 result-symbol code units
- Accepted values: The effective bound is min(codeUnits,stateUnits), and readBounded can further reduce codeUnits. 0 leaves no unit budget; settings are not required to be equal.
- Read timing: Limits are copied by SymbolReader/PmrSymbolReader construction without a local numeric validator and enforced by read/readBounded.
- Mutability and lifetime: Fixed within the instance; readBounded can tighten codeUnits for one call. beginMessage resets state without changing limits.
- Failure: Exceeding byte or unit limits returns SymbolStatus::RESOURCE_LIMIT and makes the message FAILED; subsequent reads require beginMessage. No partial current value is published.
- Consumer sources: [include/qd/encoding/symbol_reader.hpp](../../include/qd/encoding/symbol_reader.hpp), [src/encoding/symbol_reader.cpp](../../src/encoding/symbol_reader.cpp).

### `SymbolLimits::stateUnits` — CFG-0308 {#cfg-0308}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units of retained previous symbol
- Accepted values: Bounds the result through min(codeUnits,stateUnits); 0 has no special sentinel meaning. The previous symbol is retained only after complete successful reading.
- Read timing: Limits are copied by SymbolReader/PmrSymbolReader construction without a local numeric validator and enforced by read/readBounded.
- Mutability and lifetime: Fixed within the instance; readBounded can tighten codeUnits for one call. beginMessage resets state without changing limits.
- Failure: Exceeding byte or unit limits returns SymbolStatus::RESOURCE_LIMIT and makes the message FAILED; subsequent reads require beginMessage. No partial current value is published.
- Consumer sources: [include/qd/encoding/symbol_reader.hpp](../../include/qd/encoding/symbol_reader.hpp), [src/encoding/symbol_reader.cpp](../../src/encoding/symbol_reader.cpp).

## UtfStringLimits {#cfg-0309-owner}

[include/qd/encoding/utf_string.hpp](../../include/qd/encoding/utf_string.hpp)

### `UtfStringLimits::bodyBytes` — CFG-0309 {#cfg-0309}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTFString body bytes excluding the prefix
- Accepted values: 0 admits null and an empty body. Body size ≤ bodyBytes; grammar separately allows at most 8589934588 bytes and null length -1.
- Read timing: Read on every read/write call; limits are passed by value. There is no separate configuration factory.
- Mutability and lifetime: A new call may receive new limits; an already returned value does not change.
- Failure: Exceeding bodyBytes is Status::RESOURCE_LIMIT; violating the grammar bound is INVALID_LENGTH.
- Consumer sources: [include/qd/encoding/utf_string.hpp](../../include/qd/encoding/utf_string.hpp), [src/encoding/utf_string.cpp](../../src/encoding/utf_string.cpp).

### `UtfStringLimits::codeUnits` — CFG-0310 {#cfg-0310}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTFString UTF-16 code units
- Accepted values: 0 admits null and an empty string. A character consumes one or two units; an unpaired surrogate keeps its own unit according to the grammar.
- Read timing: Read on every read/write call; limits are passed by value. There is no separate configuration factory.
- Mutability and lifetime: A new call may receive new limits; an already returned value does not change.
- Failure: Exceeding codeUnits is Status::RESOURCE_LIMIT; invalid UTF and insufficient output have separate statuses.
- Consumer sources: [include/qd/encoding/utf_string.hpp](../../include/qd/encoding/utf_string.hpp), [src/encoding/utf_string.cpp](../../src/encoding/utf_string.cpp).
