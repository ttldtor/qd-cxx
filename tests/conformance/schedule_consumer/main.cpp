// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal caller: installed/relocated consumer CTest; uses only the public installed boundary.
#include <qd/schedule/schedule.hpp>

#include <iostream>

using namespace org::ttldtor::qdcxx::schedule;

int main() {
    auto context = ScheduleDataContext::create();

    if (!context) {
        return 2;
    }

    auto venue = Schedule::getInstance(*context, u"XNYS()XNAS()", u"XNAS");

    if (!venue || venue->getName() != u"NASDAQ" || venue->getTimeZoneId() != u"America/New_York") {
        return 2;
    }

    auto list = Schedule::getTradingVenues(u"XNYS()XNAS()");

    if (!list || list->size() != 2 || (*list)[0] != u"XNYS" || (*list)[1] != u"XNAS") {
        return 2;
    }

    const CalendarDefaultsEntry entries[] = {{CalendarEntryKind::VENUE, u"XNYS", u"name=updated;tz=UTC;0=09001700"},
                                             {CalendarEntryKind::VENUE, u"XNAS", u"name=changed;tz=UTC;0=10001800"}};
    auto updated = context->setDefaults({1780000000000LL, entries});

    if (!updated || venue->getName() != u"changed" || venue->getTimeZoneId() != u"UTC") {
        return 2;
    }

    const CalendarDefaultsEntry invalid[] = {{CalendarEntryKind::VENUE, u"XNAS", u"tz=UNKNOWN;0=09001700"}};

    if (context->setDefaults({1780000001000LL, invalid}) || venue->getName() != u"changed") {
        return 2;
    }

    std::cout << "PASS installed schedule factory, metadata, typed updates and unchanged-on-update-failure\n";

    return 0;
}
