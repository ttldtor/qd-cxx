# Decorated selectors at the receiving boundary

[Русская версия](DECORATED-SELECTORS.ru.md). English is authoritative.

## Values and identity

[`api::SubscriptionSelector`](../../include/qd/api/subscription_selector.hpp) owns exact UTF-16 event text and inline kind/source/time metadata. It retains no source name, schema, listener or caller view. Construction accepts empty text and arbitrary UTF-16; activation applies the selected receiving domain. Text never implicitly creates a decorator or wildcard.

| Factory | Symmetric identity | Effective demand attribute |
| --- | --- | --- |
| `plain(text)` | PLAIN and exact text | None |
| `indexed(text, source)` | INDEXED, exact text and signed source ID | Source names are ignored and not retained |
| `timeSeries(text, fromTime)` | TIME_SERIES and exact text | Signed Unix milliseconds, including MAX sentinel |
| `all()` | ALL | No event text; explicit permission required |

`operator==` excludes TIME_SERIES `fromTime`; `sameDemand` includes it. Use the latter to decide whether a complete desired snapshot changed. Equal values hash equally. Java-compatible per-kind hashes can collide across unequal kinds, including ALL and plain `"*"`. Native cross-kind equality is symmetric; [ADR-0012](../../docs/decisions/ADR-0012-decorated-subscription-identity.md) records the confirmed pinned Java discrepancy and the explicit correction.

Copy assignment preserves the entire old value on allocation failure: a complete replacement copy precedes noexcept swaps. Self-copy is unchanged. Move operations are noexcept; moved-from and self-moved values stay valid with unspecified text, without a previous-content guarantee. Producer snapshots use copy construction into fresh backing and noexcept move assignment for element replacement, preserving existing atomic publication.
## Typed Feed activation

`Subscription::addSelectors(span)`, `setSelectors(span)` and `removeSelector(value)` copy validated owned values. The original string and CandleSymbol APIs remain plain exact identities. String removal removes only the corresponding PLAIN identity. Source-specific and plain identities can overlap physically; withdrawing one leaves the other's demand active. Batches validate completely before publishing a desired revision, including regional representability, text, count and kind refusals. Producer return and `ACTIVE` are not backend membership acknowledgements.

| Selector | Quote/Profile/Trade/TradeETH/Summary Feed |
| --- | --- |
| PLAIN | Existing exact composite/regional routing, including attributed symbols |
| INDEXED | Same physical market route as PLAIN; production MarketEventDelegateSet ignores source ID for these nonindexed types. Logical kind/source identities remain distinct |
| TIME_SERIES | Atomic `UNSUPPORTED_PROFILE` at `subscription-time-series`; these typed events and endpoint do not implement time-series/HISTORY delivery |
| ALL | Explicit `EndpointOptions::enableWildcard=true` and STREAM only; disabled by default |

Enabling wildcard for TICKER is refused before endpoint publication. It is a controlled-peer capability, never enabled by descriptors, acknowledgements or the spelling `"*"`. PLAIN leading-star text remains an atomic invalid argument. `AB*` and `A?` are ordinary exact text, without glob interpretation. Incoming canonical DATA `*` remains a concrete symbol; All receives it as data.

All selects every admitted selected record binding, including actual regional variants, without requiring catalog expansion. Exact/All overlap delivers each concrete record once through existing captured eligibility. Removing All leaves Exact eligibility; removing a typed source identity leaves other logical identities. Unknown/unselected record kinds, finite incoming limits and callback failure isolation retain their existing contracts. Regional physical base text and returned logical exchange suffix follow the existing invertible mapping; no source ID is an exchange code.

For example, with a main-thread-owned runtime and an explicitly configured composite/regional registry:

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::STREAM;
options.enableWildcard = true;
// Assign options.eventRegistry when actual regional records are required.
auto endpoint = runtime.createEndpoint(options);
auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
const std::array selectors{
    q::api::SubscriptionSelector::indexed(u"AAPL&Q", q::events::IndexedEventSource::DEFAULT),
    q::api::SubscriptionSelector::all()};
subscription->setSelectors(selectors);
```

Include `<array>` and `qd/api/feed.hpp`; retain the runtime on its creator thread through endpoint/callback retirement. Connect explicitly through the ordinary Endpoint API. This fragment selects current Quote routes; it does not add typed Order or Candle support.

## Existing known-binding source/time demand

The independent `qdcxx_subscription_selectors` target is available with Feed and native modules disabled. `indexedSelectorRequest(selector, PreparedOrderSource)` uses an already validated context/source/Index binding and returns a borrowed existing STREAM/TICKER request, no match for a different nonnegative source, or a typed kind/context refusal. PLAIN and negative explicit source IDs select the available prepared source set, following production source delegate dispatch. Negative IDs still have distinct typed identity. This helper neither discovers a complete source catalog nor infers capability from `isPublishable`.

`SubscriptionSelector::qdTime()` is only a value conversion: it maps retained fromTime milliseconds to the production generic QD time-sequence value, preserving `Long.MAX_VALUE`. It does not assert record or TimeSeriesEvent business eligibility and creates no request. A validated raw indexed-time layout alone cannot prove such business capability. Lower-level tests explicitly compose the existing raw SubscriptionRequest and let the existing HISTORY owner validate context/layout; this is raw protocol capability, not typed time-series activation.

The source adapter allocates nothing and retains no text: keep the owning selector alive until SubscriptionState/MuxQueue synchronously copies its returned request. Copied context provenance survives schema destruction. Neither value conversion nor a raw request implements or admits a future typed event family.
## Finite storage, progress and lifecycle

Runtime symbol count and per-symbol units remain finite; defaults and schema16/field128 caps are unchanged. Typed producer APIs inspect at most `maximumSymbolsPerSubscription` arguments, including duplicates. Retained logical identities count independently even when they share physical demand. Source ID, kind and time are inline; no source-name or per-selector framework allocation is added. The producer checks representable four-bank capacity before bootstrap. The four-bank bound covers retained, parsed, candidate and conversion/retirement storage; each bank requests at most N wrappers and N*(U+1) UTF-16 units. This is logical requested backing, excluding implementation capacity, allocator overhead and process RSS.

The new136 `frontendSelectorBytes` metric applies to one in-flight producer mutation attempt, not the whole runtime. Concurrent caller attempts have independent transient costs. Existing aggregate core/MUX/collector/queue and serialized worker limits are unchanged: this is a separate new metric, not a global heap cap or part of the routing work grant. The original frontend also parsed inputs before the port mutex. For detached handles, a previously supported All remains a `CLOSED` rejection after owner retirement, rather than becoming an unsupported profile.
The worker snapshots only bounded logical text plus an explicit Exact/All tag. Regional physical backing, borrowed requests and queue-owned copies have separate existing bounds. Routing byte/work reservations use the actual wrapper sizes and are debited before snapshot allocation/publication. A fresh finite turn funds progress; exhausted shared turns postpone without refund. Exact/one-below routing prepay tests preserve all coordinates on refusal. Source preparation and explicit raw requests use existing independently finite owners.

Existing selected-callback lifetime, listener/capture retirement, cancellation, remove/re-add and fresh reconnect epochs are preserved. Tests distinguish ordinary strict `DRAINED/noGap/noExpiry`, named callback THROW/CLOSE local ABORT with live causal survivor delivery, and explicit reconnect gap. A final native read cancellation after owned cutoff is recorded separately from the healthy pre-cutoff frontier. No lifecycle or transport failure is ignored to make a selector test pass.

## Production reference and verification boundary

The [raw capture](../../tests/fixtures/selectors/oracle.tsv) and [provenance](../../tests/fixtures/selectors/provenance.json) contain actual pinned Java identity/hash, construction, delegate source/regional/wildcard selection and signed time-bound observations. Raw Java asymmetry is preserved rather than rewritten as corrected expectations. Focused C++ cases classify only the documented cross-kind identity difference. The [oracle launcher](../../tools/upstream/selector-oracle/README.md) uses existing production classes, not copied implementation logic.

[Selector values](../../tests/conformance/subscription_selector_tests.cpp), [finite profile](../../tests/conformance/selector_profile_tests.cpp) and [real Feed peer consumer](../../tests/conformance/selector_feed_tests.cpp) cover their respective boundaries. The peer uses an opt-in single exact followup ADD/response for causal overlap mutation, retains strict defaults, and permits exact multiset final REMOVE grouping across asynchronous frames. Native/compiler/package execution evidence is reported separately; source coverage alone is not a passing execution claim. No public service, arbitrary provider, typed Order/Candle or whole-client readiness is claimed.
