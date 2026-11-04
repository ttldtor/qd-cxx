# Classifying feed symbols and events

[Русская версия](FEED-FILTERS.ru.md). English is authoritative.

`FeedFilter` answers whether an existing event type/symbol or an owning event matches an
ordered category. It does not change any subscription. Categories use QD expressions;
the earliest category matching through any selected record wins. A filter owns an immutable
configuration and remains usable after its builder and input strings are destroyed.

For an existing endpoint, prepare ordered categories and ask that endpoint to construct
the classifier using its own immutable registry and delivery contract:

```cpp
#include <qd/api/feed_filter.hpp>
#include <array>

const std::array categories{
    org::ttldtor::qdcxx::api::FeedFilterCategory{u"equities", u"bs"},
    org::ttldtor::qdcxx::api::FeedFilterCategory{u"other", u"*"}};
auto filter = endpoint->createFilter(categories);
const auto category = filter.getCategory(
    org::ttldtor::qdcxx::api::EventType::QUOTE,
    org::ttldtor::qdcxx::api::SubscriptionSelector::plain(u"AAPL"));
```

`category` is an optional UTF-16 view;
retain an owning filter copy while using it. `accept` returns a boolean through the same
subscription-coordinate path. `acceptEvent` and `getEventCategory` instead select publisher
coordinates from a `RegisteredEvent`. A plain string beginning with `*` has no subscription
delegates; explicit `SubscriptionSelector::all()` is a separate wildcard query. The
current registered types are Quote, Profile, Trade, TradeETH, Summary, TextMessage and
TextConfiguration. They have no time-series delegate. An indexed wrapper
does not create a new source/event capability.

Advanced offline callers can construct `FeedFilter::Builder(registry, contract, limits)`
and add categories with `withCategory(name, expression)`. The registry owns complete
validated record declarations, so custom record names and regional routes are used as
configured. There is no ambient process registry or filter setter on a subscription.

## Expressions

Use `!`, `&`, `,` and parentheses for NOT, AND, OR and grouping. A leading `:` matches
record names. Unknown tokens use QD glob syntax, including classes and at most two
nonadjacent `*` wildcards. They are not regular expressions: `?`, `**`, a third `*` and
an unquoted lowercase initial letter are invalid. For example, `:Quote*&[A-Z]*` combines
a record criterion and a symbol criterion; `[a]bc` quotes a lowercase initial letter.

Standard tokens include `feed`, `compfeed`, `regfeed`, `chartdata`, `convrates`, `nwc`,
`hashMofN`, `range-A-B-` and the `bs`, `fut`, `opt`, `spread`, `cs`, `csopt`, `bsopt`,
`futopt`, `ind`, `prod`, `fx` families. A family with the `symbol` suffix omits its implicit
`feed` record restriction. See the [complete expression contract](../spec/subscriptions/FEED-FILTERS.md)
for character classes, UTF-16, reserved characters, range/hash rules and QD ALL
projection. Acceptance of a wildcard query is not acknowledgement of any displayed or
received event.

## IPF resources and updates

An IPF expression needs an explicit resource context. For example:

```text
ipf[file:/data/instruments.ipf,update=1m]
```

Set up one `FeedFilterIpfRuntime` for the application/endpoint lifetime with its
existing pre-thread `RuntimeBootstrap`, complete transfer configuration and finite limits.
Pass `sources->context()` to `endpoint->createFilter(categories, context)` or to a builder's
`withResourceContext`. The builder resolves missing IPF resources under the configured
initial deadline. The session owns one worker and common shutdown; application code does
not start, await or poll each expression.

`update`, `schedule`, `user` and `password` retain their QD textual meaning. The source
address supports QD escaping. An absent or zero update period creates a static resource.
A configured schedule needs an explicit clock and schedule-query capability. An empty
schedule uses the loaded profiles' trading-hours descriptions; a nonempty schedule uses
that description. Missing optional source or schedule capability produces a detached
error instead of silently ignoring the setting. Credentials never appear in diagnostics.

Each successful full source read publishes an immutable replacement. FLUSH and COMPLETE
do not publish partial filters, and a malformed tail after COMPLETE fails. Under the
existing default warning policy, clean EOF without COMPLETE can succeed.
Equal symbol sets and global TRADING_HOURS sets preserve the generation; order,
duplicates and symbol-to-hours associations do not affect identity.
A failed update keeps the previous successful snapshot.

`filter.getTracker()` returns a stable handle. Add a listener to activate ongoing tracking,
then obtain updates with `tracker.getCurrentFilter()`. First listener admission catches up
without an initial callback. Without listeners the current filter may be stale. A removed
listener can finish one already selected call. Listener exceptions are contained and
counted by `callbackFailures()`. Remove listener tokens when no longer needed; retained
callback captures and old generations consume their explicit lifetime allowances.

A retained filter stays unchanged after later updates. Queries on old filters start no I/O.
The owning session is closed/joined once on its creating thread; request-only source
handles and immutable filter snapshots can survive as detached values. Creator owners
must not be destroyed or joined from their worker callbacks. A callback can remove its
tracker listener, request source close and release ordinary filter/tracker handles.

## Failures and packaging

Construction rejects empty/duplicate category names, empty expressions and invalid grammar
before returning a filter. Explicit category, text, node/depth, symbol, coordinate, retained
generation, listener, source, work and lifetime limits have no unlimited zero sentinel.
Old snapshots stay valid when a replacement is refused. Public `Error` diagnostics contain
static stages and numeric categories, with no peer text or credentials.

Link the installed `qdcxx_feed` target. Static classification remains available when the
optional IPF transfer backend is disabled; attempting to create the source runtime then
reports the missing capability. Source-backed applications select the existing IPF transfer
backend and its prerequisites. See [Feed API](FEED-API.md) for endpoint/runtime ownership
and [the native filter contract](../spec/subscriptions/FEED-FILTERS.md) for compatibility
boundaries and production-oracle evidence.
