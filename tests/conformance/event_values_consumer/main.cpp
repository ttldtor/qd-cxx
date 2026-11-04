// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/events/event_flags.hpp>
#include <qd/events/market_enums.hpp>
#include <qd/events/short_string.hpp>
#include <qd/events/time_helpers.hpp>

#include <cstdint>
#include <optional>
#include <string>

/** @return Zero when the documented installed public value APIs retain their contracts. */
int main() {
    namespace events = org::ttldtor::qdcxx::events;
    const auto raw = events::marketEnumFromCode<events::Direction>(7);
    const auto direction = events::marketEnumValue(raw);
    const auto flags =
        events::setBits(std::int32_t{-1}, std::int32_t{7}, 1, events::marketEnumCode(events::Direction::UP));
    const auto day = events::getDayIdByYearMonthDay(2024, 2, 29);
    const auto text = events::encodeShortString(u"XNYS");
    const auto time = events::withTimeSequenceSequence(events::withTimeSequenceTime(0, -1), 42);

    if (!direction || *direction != events::Direction::UNDEFINED || events::marketEnumCode(raw) != 7 ||
        events::isKnownMarketEnum(raw) || events::getBits(flags, std::int32_t{7}, 1) != 5 ||
        (static_cast<std::uint32_t>(flags) & ~std::uint32_t{14}) != (~std::uint32_t{0} & ~std::uint32_t{14}) || !day ||
        events::getYearMonthDayByDayId(*day) != 20240229 || !text ||
        events::decodeShortString(*text) != std::optional<std::u16string>(u"XNYS") || !time ||
        events::getTimeFromTimeSequence(*time) != -1 || events::getSequenceFromTimeSequence(*time) != 42 ||
        events::getMillisFromNanos(-1) != -1 || events::getNanoPartFromNanos(-1) != 999999 ||
        events::getNanosFromMillisAndNanoPart(-1, 999999) != -1) {
        return 1;
    }

    return events::getDayIdByYearMonthDay(2024, 13, 1) || events::encodeShortString(u"123456789") ||
                   events::withTimeSequenceSequence(0, -1)
               ? 2
               : 0;
}
