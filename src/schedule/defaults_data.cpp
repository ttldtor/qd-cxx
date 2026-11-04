// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_private.hpp"

#include "schedule_data.hpp"

#include <deque>

namespace org::ttldtor::qdcxx::schedule::detail {
namespace {
/** @param construction Operation counters.
 * @return Exact embedded UTF-8 resource decoded to owning UTF-16; invalid pinned bytes fail closed.
 */
std::u16string resourceText(Construction &construction) {
    const auto size = sizeof(data::CALENDAR_BYTES);
    construction.inspect(size);

    if (size > construction.limits.defaultUnits || size > construction.limits.stagingBytes / sizeof(char16_t)) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    construction.allocate(size * sizeof(char16_t));
    std::u16string result;
    result.reserve(size);

    for (std::size_t i = 0; i < size;) {
        const auto first = data::CALENDAR_BYTES[i++];
        std::uint32_t scalar = first;
        std::size_t continuation = 0;

        if (first >= 0xC2 && first <= 0xDF) {
            scalar = first & 0x1FU;
            continuation = 1;
        } else if (first >= 0xE0 && first <= 0xEF) {
            scalar = first & 0x0FU;
            continuation = 2;
        } else if (first >= 0xF0 && first <= 0xF4) {
            scalar = first & 0x07U;
            continuation = 3;
        } else if (first >= 0x80) {
            throw Failure{ScheduleError::INVALID_DEFAULTS};
        }

        if (continuation > size - i) {
            throw Failure{ScheduleError::INVALID_DEFAULTS};
        }

        for (std::size_t n = 0; n < continuation; ++n) {
            const auto byte = data::CALENDAR_BYTES[i++];

            if ((byte & 0xC0U) != 0x80U) {
                throw Failure{ScheduleError::INVALID_DEFAULTS};
            }

            scalar = (scalar << 6) | (byte & 0x3FU);
        }

        if ((continuation == 1 && scalar < 0x80) || (continuation == 2 && scalar < 0x800) ||
            (continuation == 3 && scalar < 0x10000) || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF)) {
            throw Failure{ScheduleError::INVALID_DEFAULTS};
        }

        if (scalar < 0x10000) {
            result.push_back(static_cast<char16_t>(scalar));
        } else {
            scalar -= 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FFU)));
        }
    }

    return result;
}

/** @param resource Complete pinned UTF-16 resource.
 * @param at Next physical-line position.
 * @return Borrowed physical line, without Java BufferedReader's CR/LF separators.
 */
std::u16string_view lineAt(std::u16string_view resource, std::size_t &at) {
    const auto start = at;
    const auto end = resource.find_first_of(u"\r\n", at);

    if (end == resource.npos) {
        at = resource.size();

        return resource.substr(start);
    }

    at = end + 1;

    if (resource[end] == u'\r' && at < resource.size() && resource[at] == u'\n') {
        ++at;
    }

    return resource.substr(start, end - start);
}
} // namespace

std::shared_ptr<Defaults> builtinDefaults(Construction &construction) {
    const auto resource = resourceText(construction);
    std::deque<std::u16string> retainedLines;
    std::vector<CalendarDefaultsEntry> entries;
    bool sawVersion = false;

    for (std::size_t at = 0; at < resource.size();) {
        auto line = construction.copy(lineAt(resource, at));

        while (line.ends_with(u'\\')) {
            line.pop_back();

            if (at == resource.size()) {
                break;
            }

            const auto next = lineAt(resource, at);
            construction.inspect(next.size());
            construction.allocate(next.size() * sizeof(char16_t));
            line.append(next);
        }

        if (line.starts_with(u"date=")) {
            if (std::u16string_view(line).substr(5) != data::CALENDAR_DATE_TEXT) {
                throw Failure{ScheduleError::INVALID_DEFAULTS};
            }

            sawVersion = true;
            continue;
        }

        const auto dot = line.find(u'.');
        const auto equals = line.find(u'=');

        if (line.empty() || line.starts_with(u'#') || dot == line.npos || equals == line.npos || dot == 0 ||
            equals == 0 || dot > equals) {
            continue;
        }

        CalendarEntryKind kind;

        if (line.starts_with(u"hd.")) {
            kind = CalendarEntryKind::HOLIDAY_DAYS;
        } else if (line.starts_with(u"sd.")) {
            kind = CalendarEntryKind::SHORT_DAYS;
        } else if (line.starts_with(u"xd.")) {
            kind = CalendarEntryKind::SPECIAL_DAYS;
        } else if (line.starts_with(u"tv.")) {
            kind = CalendarEntryKind::VENUE;
        } else {
            continue;
        }

        if (entries.size() >= construction.limits.defaultEntries) {
            throw Failure{ScheduleError::RESOURCE_LIMIT};
        }

        construction.allocate(sizeof(std::u16string) + sizeof(CalendarDefaultsEntry));
        retainedLines.push_back(std::move(line));
        const std::u16string_view retained = retainedLines.back();
        entries.push_back({kind, retained.substr(dot + 1, equals - dot - 1), retained.substr(equals + 1)});
    }

    if (!sawVersion) {
        throw Failure{ScheduleError::INVALID_DEFAULTS};
    }

    return parseDefaults({data::CALENDAR_VERSION_MILLIS, entries}, construction);
}
} // namespace org::ttldtor::qdcxx::schedule::detail
