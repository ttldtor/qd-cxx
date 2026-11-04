// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#ifdef _WIN32
#    include <winsock2.h>

#    include <Windows.h>
#endif
#include "qd/events/event_text.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/summary.hpp"
#include "qd/time/time_zone_data.hpp"

#include <bit>
#include <string>

/** @param argc One for the file-free profile, two for an explicit installed timezone-data root.
 * @param argv Optional absolute controlled data root; no ambient path is consulted.
 * @return Zero only when the installed UTF16/UTF8 event API and typed failures retain their contracts.
 */
int main(int argc, char **argv) {
    namespace events = org::ttldtor::qdcxx::events;
    events::Quote quote{};
    quote.symbol = u"DEMO/\u03b1";
    quote.eventTime = 1;
    const auto context = events::EventTextContext::fixedOffset(19800000);
    const events::EventTextLimits limits{1024, 4096, 16384, 65536};
    const auto text = events::toStringUtf8(quote, context, limits, events::MalformedUtf16::REJECT);
    const auto subnormal = events::formatEventDouble(std::bit_cast<double>(std::uint64_t{2}), limits);
    const auto flags = events::formatEventFlags(-1, limits);
    const auto deletion = events::toString(events::OrderAction::DELETE_ORDER, limits);
    const auto invalid = events::toString(static_cast<events::Direction>(7), limits);

    if (!text || text->find("DEMO/\xce\xb1") == std::string::npos ||
        text->find("eventTime=19700101-053000.001") == std::string::npos || subnormal != std::u16string(u"9.9E-324") ||
        flags != std::u16string(u"ffffffff") || deletion != std::u16string(u"DELETE") || invalid ||
        invalid.error() != events::EventTextError::INVALID_ENUM_CODE) {
        return 1;
    }

    events::Summary summary{};
    summary.openInterest = 3000000000LL;
    const auto summaryText = events::toStringUtf8(summary, context, limits, events::MalformedUtf16::REJECT);

    if (!summaryText || summaryText->find("openInterest=3000000000") == std::string::npos) {
        return 3;
    }

    if (events::EventTextContext::timeZone(nullptr, u"America/New_York", 0).error() !=
        events::EventTextError::TIME_ZONE_NOT_READY) {
        return 4;
    }

    if (argc == 2) {
        namespace time = org::ttldtor::qdcxx::time;
        auto data = time::TimeZoneData::initialize(argv[1]);

        if (!data) {
            return 5;
        }

        auto named = events::EventTextContext::timeZone(&*data, u"America/New_York", 0);

        if (!named || events::formatEventTime(1, true, *named, limits) != std::u16string(u"19691231-190000.001")) {
            return 6;
        }
    } else if (argc != 1) {
        return 7;
    }

    auto reducedLimits = limits;
    reducedLimits.outputUnits = 0;

    return events::toString(quote, context, reducedLimits).error() == events::EventTextError::RESOURCE_LIMIT ? 0 : 2;
}
