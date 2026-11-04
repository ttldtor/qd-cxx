// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <iostream>
#include <qd/schedule/schedule.hpp>

/** @param argc Explicit data-root argument count.
 * @param argv Native command line; test deployment roots use portable ASCII paths.
 * @return Zero only when installed public behavior, ownership and snapshot checks pass.
 */
int main(int argc, char **argv) {
    using namespace org::ttldtor::qdcxx::schedule;
    namespace time = org::ttldtor::qdcxx::time;

    if (argc != 2) {
        return 2;
    }

    auto fixed = time::ZoneBinding::create(u"GMT+05:30", nullptr, {0, 128});

    if (!fixed || !fixed->getOffset(0) || fixed->getOffset(0)->totalSeconds != 19800) {
        return 3;
    }

    auto data = ScheduleTimeZoneData::initialize(std::filesystem::path(argv[1]));

    if (!data) {
        return 4;
    }

    auto context = ScheduleDataContext::create(*data, {}, {1793793600000LL, 128});

    if (!context) {
        return 5;
    }

    auto source = Schedule::getInstance(*context, u"XNYS()");

    if (!source) {
        return 6;
    }

    auto day = source->getDayByYearMonthDay(20241224);

    if (!day || day->getDayId() != 20081 || !day->isShortDay() || day->isHoliday() || !day->isTrading() ||
        day->getStartTime() != 1735016400000LL || day->getEndTime() != 1735102800000LL ||
        day->getResetTime() != 1735027200000LL) {
        return 7;
    }

    const auto sessions = day->getSessions();

    if (sessions.size() != 3 || sessions[1].getType() != SessionType::REGULAR ||
        sessions[1].getStartTime() != 1735050600000LL || sessions[1].getEndTime() != 1735063200000LL ||
        !sessions[1].containsTime(1735050600000LL) || sessions[1].containsTime(1735063200000LL)) {
        return 8;
    }

    auto next = day->getNextDay();

    if (!next || !next->isHoliday() || next->isTrading()) {
        return 9;
    }

    const auto oldStart = day->getStartTime();
    const CalendarDefaultsEntry replacement{CalendarEntryKind::VENUE, u"XNYS",
                                            u"name=changed;tz=UTC;td=1234567;0=09001700"};

    if (!context->setDefaults({1794000000000LL, std::span(&replacement, 1)})) {
        return 10;
    }

    auto current = source->getDayByYearMonthDay(20241224);

    if (!current || current->getStartTime() == oldStart || day->getStartTime() != oldStart ||
        day->getSchedule().getName() != u"changed" || sessions[1].getDay().getStartTime() != oldStart) {
        return 11;
    }

    auto raw = data->bind(u"Europe/Dublin", {-2208988800000LL, 128});

    if (!raw || raw->getRawOffsetSeconds() != -1521 || !raw->getOffset(-5364662400000LL) ||
        raw->getOffset(-5364662400000LL)->totalSeconds != 0) {
        return 12;
    }

    std::cout << "PASS installed owning days/sessions, raw metadata and actual Java XNYS values\n";

    return 0;
}
