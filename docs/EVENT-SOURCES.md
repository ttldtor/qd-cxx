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
codes and physical routes; this API does not enable source routing, capabilities
or Order record mapping.
