# CFI and additional underlyings

[Русская версия](GLOSSARY-VALUES.ru.md). English is authoritative.

Enable `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`, then link `qdcxx_ipf`. Both headers are
self-contained; Feed, runtime, network and TLS are independent of these value types.
With IPF OFF, these headers and the target are absent from the installed package.

```cpp
#include <qd/glossary/cfi.hpp>
#include <qd/glossary/additional_underlyings.hpp>

int main() {
    using namespace org::ttldtor::qdcxx::glossary;
    constexpr auto code = Cfi::valueOf(std::u16string_view(u"OP"));
    static_assert(code.isOption() && code.getGroup() == 'P');
    const auto underlyings = AdditionalUnderlyings::valueOf(std::u16string_view(u"AAPL 2; USD 10"));
    return code.getCode() == "OPXXXX" && underlyings.getSpc(std::u16string_view(u"AAPL")) == 2 ? 0 : 1;
}
```

`Cfi` accepts zero to six uppercase ASCII letters, pads with X, and treats null/empty
input or packed integer zero as XXXXXX. It rejects malformed packed fields or characters
with `std::invalid_argument`. Unknown standard combinations remain valid values.
`decipher()` returns six small values backed by immutable program-lifetime dictionary
metadata; returned attribute/value names and descriptions remain valid after the Cfi and
result objects are destroyed. `getValues()` copies only the small value list. `describe()`
joins the six value names. Cfi construction and category queries support constant evaluation;
`Cfi::EMPTY` is constant-initialized, including use from another translation unit before main.

`AdditionalUnderlyings` owns exact UTF-16 text and a read-only symbol/SPC map. Text parsing
preserves spelling and order; when symbols repeat, the first occurrence wins. Equality
compares exact text. Map construction sorts by UTF-16 units, rounds the reference shortest digits
from its open binary interval to 15 significant digits
and parses that rendered text, so its map contains the rounded values. Text can admit
infinite SPC through exponent overflow, while map input rejects nonfinite values. Signed zero
is preserved. No conversion depends on the ambient locale. Null text/map means empty;
`empty()` returns a process-lifetime value safely initialized on first use.

Instance `getSpc(symbol)` reads the validated map. Static `getSpc(text, symbol)` deliberately
scans from the right without validating the whole text: duplicate and malformed-text results
can differ from the instance API. Null/empty static symbols return zero. These are upstream
compatibility rules, rather than a new general-purpose number parser.

The default model limits are 1,048,576 UTF-16 text units and 16,384 parsed entries (duplicates
also count). Supply `AdditionalUnderlyingLimits` to either factory to choose other finite
bounds. Limit failures throw `std::length_error`; invalid syntax throws `std::invalid_argument`
with the production message. Allocation errors propagate. Input views are never retained.
Copy assignment stages a complete value before replacement; an allocation failure preserves
the target. Moves leave the source empty and reusable on success; an empty-map sentinel allocation may throw before ownership transfer, leaving the source unchanged. Borrowed text/map references last
until their owner is modified or destroyed. Independent immutable values support concurrent
reads; mutation/destruction must be synchronized with readers.

See the [precise grammar and dictionary contract](../spec/glossary/VALUES.md),
[production Java oracle](../tools/upstream/glossary-oracle/README.md) and
[conformance fixture description](../tests/fixtures/glossary/README.md).
The dictionary's [MPL-2.0 notice](../licenses/cfi/LICENSE.txt) applies to the adapted
private dictionary file; it does not change the license of other project files.
