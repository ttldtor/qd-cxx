# Use indexed and order event sources

[Русская версия](EVENT-SOURCES.ru.md). English is authoritative.

Link the independent SDK-free target qdcxx_event_sources, or qdcxx::feed for
its public transitive dependency.
~~~cmake
find_package(qdcxx CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE qdcxx_event_sources)
~~~

~~~cpp
#include <qd/events/order_source.hpp>

namespace events = org::ttldtor::qdcxx::events;
auto source = events::OrderSource::valueOf(u"NTV");
auto restored = events::OrderSource::valueOf(source.id());
bool same = restored == source; // true; ID-only equality
events::IndexedEventSource base(source.id(), std::nullopt);
bool sameFamilyIndependent = base == source; // true
~~~
Names use exact case-sensitive UTF-16. Base values accept any signed ID and
nullable owned name. Order sources resolve built-ins before validating one to four
ASCII letters/digits. Null and malformed order inputs throw std::invalid_argument;
allocation can throw standard string/allocation errors.
The [full contract](../spec/events/EVENT-SOURCES.md) defines invalid-ID handling.

Values own names. Getter views and indexedSource() references borrow the owner
until assignment, move-from or destruction. Java reference identity is not promised.
hashCode() is a signed ID; std::hash converts it to size_t. Java lowercase names
use _LOWER constants, such as NTV_LOWER for ntv. Source IDs differ from exchange
codes and physical routes; this API does not enable source routing or Order record mapping.

## Query capabilities and select sources

~~~cpp
#include <qd/events/order_source.hpp>
#include <qd/events/source_selection.hpp>

namespace events = org::ttldtor::qdcxx::events;
auto publishable = events::OrderSource::publishable(events::OrderEventType::ORDER);
bool fullBook = events::OrderSource::NTV.isFullOrderBook();
auto all = events::SourceSelection::all();
auto defaultOnly = events::SourceSelection::specific(events::IndexedEventSource::DEFAULT);
auto custom = events::SourceSelection::specific(events::IndexedEventSource(-1, std::nullopt));
bool acceptsCustom = all.matches(*custom.source()); // true
bool same = all == defaultOnly; // false
~~~

The [capability matrix and selector contract](../spec/events/EVENT-SOURCES.md#publication-capabilities-and-source-selection)
define all six event families, exact list order and failures. Specific selectors own
their source, accept arbitrary base IDs, and compare IDs only. source() borrows the
selector until assignment, move-from or destruction; nullptr means ALL. Publication
flags do not restrict specific selection. This pure value API does not subscribe,
receive Order events or integrate selection into Feed.
