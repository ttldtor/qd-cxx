# Time-series identity and logical range model

[Русская версия](TIME-SERIES-IDENTITY.ru.md). English is authoritative.

The public [`qd/api/time_series_subscription.hpp`](../../include/qd/api/time_series_subscription.hpp)
provides owning decorated values, explicit business capability declarations and a bounded mutable
`TimeSeriesSubscriptionModel`. It is part of `qdcxx_subscription_selectors`, available without
Feed, transport or a runtime bootstrap. It does not implement a HISTORY receiver, event mapping,
wire request, range completeness, event flags, listener dispatch or subscription close lifecycle.

## Pinned Java facts

The [actual production corpus](../../tests/fixtures/time-series/oracle.tsv) and
[provenance](../../tests/fixtures/time-series/provenance.json) bind QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and Java8 executable/source/class hashes.
The [bounded oracle](../../tools/upstream/time-series-oracle/README.md) observes detached production
objects, including nested decorators, observer notices, raw generic bypass and inherited markers.

- `TimeSeriesSubscriptionSymbol` retains its event symbol and signed64 `fromTime`. Its source
  is DEFAULT zero; equality and hash use only the event symbol, excluding the outer range.
- `DXFeedTimeSeriesSubscription` starts at `Long.MAX_VALUE`, meaning no requested events until
  the property changes. A changed `setFromTime` re-decorates the current raw set. Setting the same
  value is a no-op. Signed extrema, negative values and zero are retained without clamping.
- Raw `getSymbols()` and decorated `getDecoratedSymbols()` are different views. Existing decorators
  are wrapped, not flattened. A range change preserves raw identity; a time-series value's range
  is a demand attribute, not its key. Plain star text and explicit wildcard remain different objects.
- TimeAndSale, Candle, Greeks, TheoPrice and Underlying declare TimeSeriesEvent. NuamTimeAndSale
  and the harness's custom TimeAndSale subclass inherit it. Order and Series are indexed but not
  time-series. All nine currently registered native projection types are neither marker.
- Java's generic type bound rejects unsupported typed use at compile time; its raw-Class constructor
  bypass accepts the tested non-time-series classes. That bypass is not a wire-capability proof.
- Java accepts `setFromTime(42)` after close while retaining an empty symbol set. This is recorded
  separately; the native logical model has no close operation or facade-lifecycle claim.

The [ADR-0012 correction](../../docs/decisions/ADR-0012-decorated-subscription-identity.md) remains
in force. The actual Java IES/TSS asymmetry propagates through a nested TSS event symbol; collection
results can then depend on insertion order. Raw rows retain that discrepancy. Native typed-kind
equality remains symmetric, including inside a time-series decorator, so native raw-set deduplication
does not reproduce the defective collapse. Java reference-driven FilteredSubscriptionSymbol notices
are not model callbacks: this model reports effective value changes only.

## Values and capabilities

`TimeSeriesSubscriptionSymbol<Symbol>` owns its Symbol by value. The default Symbol is the existing
`SubscriptionSelector`, preserving PLAIN, INDEXED, TIME_SERIES and ALL kinds and their attributes.
Another time-series wrapper can itself be a Symbol, so nesting is not limited to a fixed runtime depth
or a Candle-specific class. The concept checks copy/equality/hash syntax. Callers must supply the
semantic laws: symmetric equivalence, equal hashes for equal identities, and identity stability while
used as a key. Ownership/exception behavior of custom Symbol values remains their own contract.
The default native symbol domain retains only text and scalar metadata, with no operational owner.

`TimeSeriesEventDescriptor::declared(stableIdentity, capability)` retains explicit nonempty stable
business identity and one marker: NON_INDEXED, INDEXED or TIME_SERIES. TIME_SERIES includes indexed
semantics. Identity spelling, raw record-time layout and presence of a timestamp field grant no
capability. Different business types must not alias one stable ID. Custom and inherited types use
the same small descriptor, not a closed list of five Java classes or a new subclass hierarchy.
Declarations are caller metadata, not verified wire mappers. `isTimeSeriesEventType(EventType)`
returns false for all current nine native types and unknown values; `hasTimeSeriesFeedReceiver`
returns false even for a valid logical declaration. Current Feed TIME_SERIES selector rejection is unchanged.

## Mutable model and atomic cuts

Creation requires a nonempty finite descriptor batch, all declaring TIME_SERIES. Exact duplicate
types collapse in first-seen order; conflicting markers for one ID are refused. Invalid descriptors,
unsupported categories, invalid limits and allocation failure publish no model. Limits bound supplied
type/input counts, retained symbol identities and exact UTF-16 text units, not process RSS.

The initial range is MAX and the raw set is empty. Set/add/remove/clear use the existing symmetric
typed identity. The last duplicate's raw attributes win. Input order alone is not a new set; a no-op
retains accepted order and revision. A raw inner TIME_SERIES range can change its retained demand
without changing identity. Clear removes symbols while preserving the range and type declarations.
`setFromTime` changes only the shared lower-bound property, including on an empty set. It does not
filter event objects or infer their index encoding. The existing `SubscriptionSelector::qdTime()`
remains a separate generic value conversion, not proof of business or receiver capability.

All model operations share one internal mutex. A snapshot copies the revision, range, type declarations,
raw values and one outer decoration per raw value in one cut. Every outer decoration captures that
snapshot's range; previously saved snapshots stay unchanged. Copies retain no model, schema, callback,
socket or runtime. Reads allocate bounded copies but perform no progress or callback waiting.
Move transfers the unique pure owner; moved-from methods report INVALID_STATE. Move/destruction must
not overlap calls on that object. Detached snapshots can outlive both moved and destroyed owners.

Each effective change receives a checked model-local revision; no-op operations preserve it. Revision
exhaustion refuses a change before mutation. A count/unit/length/allocation refusal preserves the
complete accepted range, raw set, declarations and revision. Temporary preparation and caller-owned
snapshots are separate owning copies; no history or per-symbol monitoring map accumulates.

See the [usage guide](../../docs/TIME-SERIES-SUBSCRIPTIONS.md). The oracle proves the captured Java
facts; native builds, consumers, sanitizers and independent review are separate execution evidence.
