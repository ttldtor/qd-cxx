# Option chains from instrument profiles

[Русская версия](OPTION-CHAINS.ru.md). English is authoritative.

Enable `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON` and link the installed `qdcxx_ipf` target.
The self-contained public header is `qd/ipf/option_chains.hpp`; no feed, runtime, TLS,
network or private headers are required. With IPF disabled, the target and its headers
are not installed. Existing `qdcxx_feed` has no dependency on this optional component.

`OptionChainsBuilder::build(profiles)` groups profiles in input order. `addProfile`
returns `true` for admission/replacement, `false` for an ignored profile, or
`OptionChainError`. See the [exact grouping specification](../spec/ipf/OPTION-CHAINS.md).

```cpp
#include <qd/ipf/option_chains.hpp>
#include <array>

int main() {
    using namespace org::ttldtor::qdcxx::ipf;
    InstrumentProfile call;
    if (!call.setField(u"TYPE", u"OPTION") ||
        !call.setField(u"SYMBOL", u"ABC-C10") ||
        !call.setField(u"UNDERLYING", u"ABC") ||
        !call.setField(u"CFI", u"OC") ||
        !call.setField(u"EXPIRATION", u"2027-01-15") ||
        !call.setNumericField(u"STRIKE", 10)) {
        return 1;
    }
    const std::array profiles{call};
    auto builder = OptionChainsBuilder::build(profiles);
    if (!builder) {
        return 1;
    }
    auto snapshot = builder->getChains();
    const auto &series = snapshot.at(u"ABC")->getSeries().front();
    return series.getCalls().at(10)->getField(u"SYMBOL") == u"ABC-C10" ? 0 : 1;
}
```

The copied map owns immutable chains. Chains own series and strike maps; slots share
immutable **copies** of admitted profiles. Input mutation/destruction and later builder
changes cannot change a retained snapshot. This intentionally differs from Java's mutable
shared instrument references. `getChains()` returns a borrowed map: copy it before changing
or destroying its builder. References through a retained chain remain valid while that
chain is owned. Copied builders evolve independently; moving a builder leaves the source
empty and reusable with its original limits.

Metadata and strike maps preserve Java ordering. The outer map uses deterministic UTF-16
symbol order, since Java's `HashMap` promises no order. `getStrikes()` and
`getNStrikesAround(count, center)` return independent vectors. Selection keeps Java's
insertion-point and tail-truncation behavior, supports nonfinite centers, and reports
`INVALID_COUNT` for negative counts.

`OptionChainLimits` is sampled once. Defaults: 100,000 inspected profiles, 4,096 chains,
16,384 series, 200,000 call/put slots, 16,777,216 retained UTF-16 units, a 50,331,648-unit
conservative staging charge, and 1,073,741,824 cumulative logical work units. Ordinary use
needs no per-call budget plumbing. Supply other finite limits at construction or `build`.

A failed admission leaves all chains and counters unchanged, including when allocation
throws. Allocation failures propagate as C++ exceptions. Limit errors return
`LIMIT_EXCEEDED`; source-profile numeric/date access restrictions return `PROFILE_ACCESS`.
Ignored inputs consume input-count and inspection-work limits. Profile-specific limits
still apply to copies. Mutable builder access requires external synchronization;
published immutable snapshots support concurrent readers.

Full CFI validation, additional-underlying parsing and price increments are separate APIs.
This component only inspects the `OC`/`OP` prefix and preserves the remaining series text.
The [production Java oracle](../tools/upstream/option-chains-oracle/README.md) and
[fixtures](../tests/fixtures/option-chains/README.md) record this compatibility boundary.
