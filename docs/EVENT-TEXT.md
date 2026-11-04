# Print event and value text

[Русская версия](EVENT-TEXT.ru.md). English is authoritative.

Include qd/events/event_text.hpp and the event's header; link qdcxx_event_text. The [console example](../examples/event_text/README.md) prints an owned UTF-8 string directly through std::cout.

~~~cpp
using namespace org::ttldtor::qdcxx::events;
Quote quote{};
quote.symbol = u"DEMO";
const auto context = EventTextContext::utc();
const EventTextLimits limits{1024, 4096, 16384, 65536};
const auto text = toStringUtf8(quote, context, limits, MalformedUtf16::REJECT);
if (text) {
    std::cout << *text << '\n';
}
~~~

toString returns an independently owned std::u16string; toStringUtf8 renders and converts under one shared work allowance. Both return std::expected and expose no partial result. Events/contexts are borrowed only during the synchronous call and must remain unchanged. A returned string survives destruction or replacement of the inputs.

Choose UTC or EventTextContext::fixedOffset(offsetMillis) explicitly. Fixed offsets accept every signed32 millisecond value, including sub-second offsets, without timezone initialization. Named contexts bind to the shared explicit timezone-data capability with a caller-supplied raw-offset reference instant. Formatting performs no clock, home/OS timezone, filesystem, network or initialization work. A null provider for an accepted named identity returns TIME_ZONE_NOT_READY.

Complete event overloads support Quote, Profile, Trade, TradeETH and Summary. Enum overloads return Java names, including DELETE for OrderAction::DELETE_ORDER; unknown raw C++ enum codes fail explicitly. Source overloads preserve exact names; IndexedEventSource distinguishes null, empty and literal null. formatEventFlags renders every raw signed32 bit as unsigned lowercase hexadecimal.

Symbols and Profile strings remain literal UTF-16, including NUL and individual surrogates. Exchange codes use encodeEventChar. UTF-8 conversion explicitly rejects unpaired surrogates or replaces each with U+FFFD; it changes no process encoding or console settings.

See the [representation and budgets](../spec/events/EVENT-TEXT.md), [reference decision](decisions/ADR-0011-java-event-text-reference.md) and [verification limits](../tests/conformance/EVENT-TEXT-VALIDATION.md). Existing canonical Candle/period/attribute/source helpers and their established round trips remain unchanged.

For named zones, include qd/time/time_zone_data.hpp. Enable ORG_TTLDTOR_QDCXX_BUILD_TIME_ZONE_DATA explicitly when building the producer, and request the installed time_zone_data_backend package component. Linking EventText alone keeps the backend disabled. Initialize the [controlled data](TIME-ZONE-DATA.md) once, then bind:

~~~cpp
auto data = org::ttldtor::qdcxx::time::TimeZoneData::initialize(dataRoot);
if (data) {
    auto context = EventTextContext::timeZone(&*data, u"America/New_York", 0);
    // A successful context owns its binding; data and the input ID may now be destroyed.
}
~~~

The explicit reference instant selects constructor raw-offset metadata; formatting queries the actual offset at each event instant. Historical actual offsets can differ from reference metadata. maxIdUnits bounds copied identity units (128 by default), separately from per-format limits. Provider range/data failures become TIME_ZONE_RANGE/TIME_ZONE_DATA; unknown identities and absent ready capabilities remain distinct. UTC and normalized fixed GMT identities work through the factory without a ready provider; fixedOffset also accepts subsecond and full signed32 offsets. Named profile semantics and their separately classified dataset differences follow the shared provider.