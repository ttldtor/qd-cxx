# Option-chain grouping contract

[Русская версия](OPTION-CHAINS.ru.md). English is authoritative.

Reference: pinned `dxfeed-api/src/main/java/com/dxfeed/ipf/option/OptionChainsBuilder.java`,
`OptionChain.java`, `OptionSeries.java` at the revision in [upstream.lock](../../upstream.lock).
[Oracle provenance](../../tests/fixtures/option-chains/provenance.json) records source and
production-class hashes. This specification covers profile-based grouping, without the
Java generic-object setter interface or mutable container views.

Only exact `TYPE=OPTION` profiles are considered. CFI must start with `OC` or `OP`,
expiration day ID must be nonzero, strike must be finite, and product or underlying must
be nonempty. Other profiles are ignored. Missing strike is zero and is valid. Negative
expiration IDs and negative finite strikes are valid. Classification checks no other CFI
characters, field formats or relationships between metadata values.

A profile is added to both nonempty product and underlying destinations. Equal destinations
produce one slot. Chains are keyed by exact UTF-16 symbols. Profiles with equal series,
side and strike replace the previous slot in input order, irrespective of profile symbol;
a repeated symbol at a different strike/series is not deduplicated. This is snapshot
construction, without feed updates or removal semantics.

Series comparison priority is expiration, last-trade day ID, multiplier, SPC, additional
underlyings, MMY, option type, expiration style, settlement style, CFI. Dates are signed32;
doubles use `Double.compare`, making negative zero precede positive zero, infinities ordered
normally and all NaN payloads equivalent after positive infinity. Text uses UTF-16 code-unit
lexical order. Series CFI replaces its second unit with `X`, merging matching call/put series
while preserving all trailing text. No full CFI parser is implied. Strike maps also use
`Double.compare`, keeping -0 and +0 distinct; inadmissible nonfinite strikes never enter them.

Strikes are the ordered call/put union. For nonnegative count n and insertion/exact-match
index i of center under `Double.compare`, selection is `[max(0,i-n/2), min(size,from+n))`.
The range is truncated at the end without shifting it left. A negative count is an error.
The C++ implementation avoids intermediate integer overflow and returns owned vectors.

Each successful inspection charges one profile and `1 + input.retainedUnits()` work units.
An admitted option additionally charges the previous aggregate's retained units, chain count,
series count and slot count as logical staging work. This is a conservative reproducible
admission metric, not measured CPU instructions/time. All arithmetic checks remaining
capacity before addition. A rejected operation consumes no counters.

Retained UTF-16 storage charges each chain symbol, each series' six metadata strings and
each slot's complete profile `retainedUnits()`, even when immutable profile copies are shared.
Counts include duplicated destinations. All displaced slot charges are subtracted before
the complete transaction's positive retained charges are checked, so destination order
cannot reject an otherwise fitting aggregate.

Staging is distinct from logical retained accounting. Let R/R' be old/projected logical
retained units, S/S' the old/projected sums of chain symbol lengths, and I the incoming
profile's retained units. Before metadata extraction, `R + S + I` must fit `stagingUnits`.
Before copying containers, `2*R + R' + S + S' + 3*I` must fit, using checked summands.
The old and projected graphs each own separate map-key and chain-symbol text; the S terms
account for the second symbol copies absent from logical retained accounting. The extra
R allowance bounds old cloned series storage still alive during vector insertion. Three
input-sized temporary allowances bound extracted metadata and series/key insertion
intermediates; the owned profile itself is already charged in R'. The bound counts live
UTF-16 content, not string capacity, allocator overhead or RSS. Container/allocator overhead
is separately bounded by finite chain/series/slot/profile-unit counts. An admission can
therefore hit staging before retained capacity, even when no caller-held snapshot exists.
Caller-held snapshots are independent owners and are outside one builder's resource limits.

Admission prepares copied symbols, affected chains and the immutable profile before swapping
one complete map. Checked limit/source errors and allocation exceptions preserve all original
chains and counters. Copy assignment constructs replacement state before swapping; a successful
move leaves an empty reusable source. No `noexcept` allocation promise is made. There are no
callbacks or internal threads. See [public ownership/error guide](../../docs/OPTION-CHAINS.md).
