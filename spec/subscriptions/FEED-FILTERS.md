# Feed filter expressions and classification

[Русская версия](FEED-FILTERS.ru.md). English is authoritative.

A feed filter is an immutable query classifier. It does not install a subscription filter,
change desired demand, start a source or perform network progress. The builder owns a copy
of ordered category names and expressions and retains an immutable `EventRegistry`. The
first category matching through any selected delegate wins, regardless of delegate order.
Both acceptance and classification use the same selected coordinate set.

## Existing coordinate capabilities

The native registry currently supports Quote, Profile, Trade, TradeETH, Summary, TextMessage,
TextConfiguration, Message and Configuration. A query uses actual configured record names and physical symbols; it never infers an event type
from a record spelling. Regional selection uses an exact available route, otherwise the
composite fallback. A selected regional route removes the symbol's exchange suffix while
preserving attributes. The current native Profile receiving adapter uses text coordinates
without regional conversion. This is the existing native registry capability; arbitrary
Java provider/delegate plugins are not asserted.

Subscription queries inspect time-series selectors before indexed selectors. The current
projections have no time-series delegate. An explicit indexed source does not
invent source capabilities for market records. A plain string starting with `*` has no
subscription delegates; explicit ALL remains a distinct typed query. Event queries use
publisher coordinates and do not apply that subscription reserved-prefix rule.

TextMessage, TextConfiguration, Message and Configuration use generic delegate coordinates: the complete symbol,
including `&Q`, is preserved even when a caller record name looks regional. In an indexed
query, source0 and negative IDs select the available delegates; a positive ID gives no match.
This differs from MarketEventDelegateSet and does not change the existing market rules.
Null/empty/body payload, version and time metadata do not participate in classification.
Message and Configuration retain opaque serialized attachment bytes; classification does not decode them or invoke a codec.
The owning C++ symbol is a `std::u16string`: empty corresponds to an explicit empty Java
symbol, while a Java null symbol/model is not representable by this value API. Raw production
Java throws NullPointerException for null, including an event without an assigned symbol;
null is not converted to empty text or no-match.

## Expression grammar

Precedence, from strongest to weakest, is unary `!`, intersection `&`, union `,`.
Parentheses group expressions. The delimiter-aware keywords `not`, `and`, `or`, `record`
and `any` expand to `!`, `&`, `,`, `:` and `*`. Square brackets shield composite operators;
curly braces outside brackets also shield them. A backslash shields the next code unit.
`\Q...\E` shields a block at composite-tokenization time; the resulting token still passes
through the selected leaf parser. Leading ASCII spaces are skipped. Remaining spaces
are significant except around recognized keywords, matching the production parser.

A leading `:` selects a record-name pattern. Other unknown tokens use QD glob syntax,
not regular expressions. A lowercase initial letter is reserved unless quoted, for
example `[a]bc`. A glob has at most two nonadjacent stars, each matching zero or more
UTF-16 code units. `**` and a third star are invalid. Character classes, `^` immediately
after `[`, inclusive ranges, backslash escapes and block quotes follow QD bit-class
semantics. A descending range adds no range members; an already added left endpoint
remains present. Empty classes are accepted syntax. Unclosed classes/ranges, dangling
escapes and unclosed block quotes are errors. Reserved unescaped operators are errors
outside classes. Pattern source units must be ASCII32..126. Input units above126 share
bucket127; supplementary characters occupy two UTF-16 positions. NUL never matches an
inverted class. `?` is reserved, not a one-character wildcard.

The admitted standard factory tokens are `feed`, `compfeed`, `regfeed`, `chartdata`,
`convrates`, `nwc`, `hashMofN`, `range-A-B-`, and the symbol families `bs`, `fut`, `opt`,
`spread`, `cs`, `csopt`, `bsopt`, `futopt`, `ind`, `prod`, `fx`. Each family has a
`symbol` suffix form that omits its implicit `feed` record constraint. Hash indices
are nonnegative Java-int decimal values; N is a power of two of at least2 and M<N.
The hash is the Java UTF-16 string hash multiplied modulo2^32 by0xB46394CD, selecting
the high log2(N) bits. Range boundaries contain at most8 ASCII letters/digits and
are ordered lower-inclusive/upper-exclusive. Empty bounds are open. Range comparison
skips an optional numeric spread multiplier and then a non-dictionary prefix.

## Wildcard query projection

ALL follows QD wildcard semantics. Record criteria remain exact; ordinary symbols
and data use the resulting exact predicate. Static patterns without stars or a
non-ASCII bucket are classified as symbol sets up to the pinned10000 cardinality
cutoff, as in Java. This compatibility cutoff is separate from the finite
`finiteSymbols` and `normalizationWork` grants.

When an AND has such a finite seed, other static symbol-only criteria are tested
against exact candidates. An empty intersection becomes NOTHING and rejects ALL:
`A&B`, `A&!A`, `(A,B)&!A&!B` are false. Record/mixed and dynamic criteria are not
replaced with deductions about symbols. Cofinite NOT is tested against a finite
seed without enumerating its unbounded complement. Symbol-set membership of `*`
is distinct from accepting the QD wildcard cipher: `[*]&A` is false, whereas
`[*]&![*]` accepts ALL.

Patterns above the cutoff and patterns with stars are not enumerated. Therefore
`A*&!A*` accepts ALL, matching actual Java behavior; likewise a14641-combination
pattern intersected with its negation retains glob projection. A smaller enumeration
or work grant produces a typed RESOURCE_LIMIT/WORK_LIMIT before publication, rather
than an approximate answer. An unverified Java TODO is not classified as a defect.
A textual `*` subscription is rejected before projection; textual `*` data follows
the publisher path and encodes to the QD wildcard cipher.

## Resource boundary

Construction and queries have explicit finite category, source-unit, node, tree-depth,
pattern-position, coordinate, query-symbol and conservative retained-storage bounds.
A refusal publishes no filter. Builder rejection preserves previously admitted categories.
A filter retains its own strings and registry after the builder and caller inputs die.
Category views require an owning filter copy to remain alive. Allocation failures are
reported through detached public errors. No diagnostic contains source credentials.

IPF filters require an explicit owning resource context. Parsing a classifier cannot
implicitly bootstrap native networking. The immutable snapshot, tracker and optional
source service are separate ownership responsibilities. Initial or replacement snapshots
are published only after the complete source operation succeeds under its explicit
completion policy; a semantic COMPLETE marker does not bypass malformed trailing input.
A missing marker may be accepted under the default warning policy. Existing snapshots
remain unchanged after an update or source failure. A source service owns finite attempts,
clock/schedule configuration and shutdown; applications do not drive a polling loop.

## Compatibility evidence

The locked primary classes are `DXFeedFilter`, `DXFeedFilterImpl`, `DXFeedFilterTrackerImpl`,
`CompositeFilters`, `PatternFilter`, `FilterFactoryImpl`, `HashFilter`, `HashStriper`,
`RangeFilter`, `RangeUtil`, `IPFSymbolFilter` and `IPFUpdater` at the revision in
[`upstream.lock`](../../upstream.lock). The
[production oracle](../../tools/upstream/feed-filter-oracle/README.md) retains raw
[classifier observations](../../tests/fixtures/feed-filter/oracle.tsv) and
[IPF observations](../../tests/fixtures/feed-filter/ipf-oracle.tsv) with source and class
inventory hashes. Source inspection, actual Java execution and native regression results
are distinct evidence.


IPF content identity includes the exact symbol set and the global exact TRADING_HOURS
set. Order, duplicates and association with individual profiles do not affect identity;
a change only to the hours set creates a new generation and notifies the tracker.


Primary sources at the locked revision: [DXFeedFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedFilter.java), [DXFeedFilterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedFilterImpl.java), [CompositeFilters](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/CompositeFilters.java), [PatternFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PatternFilter.java), [standard factory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/FilterFactoryImpl.java), [IPFSymbolFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-ipf-filter/src/main/java/com/dxfeed/ipf/filter/IPFSymbolFilter.java), [tracker](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedFilterTrackerImpl.java).
