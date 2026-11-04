// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_query_projection.hpp"
#include "schedule_private.hpp"

#include <charconv>
#include <stdexcept>

namespace org::ttldtor::qdcxx::schedule::conformance {
namespace {
constexpr std::int64_t REFERENCE_MILLIS = 1793793600000LL;

/** Preserve product failures as explicit markers; resource failures are never relabelled as Java validation.
 * @param error Actual product refusal.
 * @return Semantic source class where applicable, otherwise a distinct native failure marker.
 */
std::string errorName(ScheduleError error) {
    switch (error) {
    case ScheduleError::OUT_OF_RANGE:
    case ScheduleError::INVALID_DEFINITION:
    case ScheduleError::UNKNOWN_TIME_ZONE:
    case ScheduleError::INVALID_DEFAULTS:
        return "IllegalArgumentException";
    case ScheduleError::NO_SESSION:
    case ScheduleError::NO_MATCH:
        return "NoSuchElementException";
    default:
        return "NATIVE_ERROR:" + std::to_string(static_cast<int>(error));
    }
}

/** Internal fixture propagation; caught only by the projection boundary. */
struct ProjectionFailure {
    ScheduleError error;
};

/** @tparam Value Owning product success type.
 * @param value Actual product result.
 * @return Owning success value, or preserve its actual typed refusal for projection.
 */
template <class Value> Value requireValue(std::expected<Value, ScheduleError> value) {
    if (!value) {
        throw ProjectionFailure{value.error()};
    }

    return *value;
}

/** @param text Exact signed decimal fixture input.
 * @return Complete signed value, rejecting partial/malformed fixtures.
 */
std::int64_t number(std::string_view text) {
    std::int64_t result = 0;
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);

    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid numeric fixture");
    }

    return result;
}

/** @param text Owning/borrowed public metadata.
 * @return Exact hexadecimal UTF-16 code units.
 */
std::string encodeHex(std::u16string_view text) {
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (const auto value : text) {
        for (const auto shift : {12, 8, 4, 0}) {
            result += DIGITS[(value >> shift) & 15];
        }
    }

    return result;
}

/** @param value Actual immutable day.
 * @return Scalar public query projection, matching the oracle's field order.
 */
std::string core(const Day &value) {
    return std::to_string(value.getDayId()) + "," + std::to_string(value.getYearMonthDay()) + "," +
           std::to_string(value.getYear()) + "," + std::to_string(value.getMonthOfYear()) + "," +
           std::to_string(value.getDayOfMonth()) + "," + std::to_string(value.getDayOfWeek()) + "," +
           std::to_string(value.isHoliday()) + "," + std::to_string(value.isShortDay()) + "," +
           std::to_string(value.isTrading()) + "," + std::to_string(value.getStartTime()) + "," +
           std::to_string(value.getEndTime()) + "," + std::to_string(value.getResetTime());
}

/** @param type Public session kind.
 * @return Source enum name, without inferring trading from interval length.
 */
std::string_view typeName(SessionType type) {
    switch (type) {
    case SessionType::NO_TRADING:
        return "NO_TRADING";
    case SessionType::PRE_MARKET:
        return "PRE_MARKET";
    case SessionType::REGULAR:
        return "REGULAR";
    case SessionType::AFTER_MARKET:
        return "AFTER_MARKET";
    }

    throw std::runtime_error("Invalid public session type");
}

/** @param value Actual immutable session handle.
 * @return Public scalar session projection, without a second session model.
 */
std::string core(const Session &value) {
    return std::to_string(value.getDay().getDayId()) + ":" + std::string(typeName(value.getType())) + ":" +
           std::to_string(value.getStartTime()) + ":" + std::to_string(value.getEndTime()) + ":" +
           std::to_string(value.isTrading()) + ":" + std::to_string(value.isEmpty());
}

/** @param result Actual bounded navigation result.
 * @return Public projection or exact semantic failure family.
 */
template <class Value> std::string projected(std::expected<Value, ScheduleError> result) {
    return result ? core(*result) : "ERROR:" + errorName(result.error());
}

std::string optionalDay(std::expected<std::optional<Day>, ScheduleError> result) {
    const auto match = requireValue(std::move(result));

    return match ? std::to_string(match->getYearMonthDay()) : "NONE";
}

std::string optionalSession(const std::optional<Session> &result) {
    return result ? core(*result) : "NONE";
}

std::string optionalSession(std::expected<std::optional<Session>, ScheduleError> result) {
    return optionalSession(requireValue(std::move(result)));
}

std::string projectFilter(const Day &day) {
    const DayFilter neverDay{1};
    const SessionFilter neverSession{SessionType::REGULAR, false};
    const auto first = day.getFirstSession();
    const auto last = day.getLastSession();

    return std::to_string(DayFilter::TRADING.accept(day)) + "," + std::to_string(DayFilter::HOLIDAY.accept(day)) + "," +
           std::to_string(DayFilter::SHORT_DAY.accept(day)) + "," + std::to_string(DayFilter::WEEK_DAY.accept(day)) +
           "\t" + optionalDay(day.findPrevDay(DayFilter::TRADING)) + "\t" +
           optionalDay(day.findNextDay(DayFilter::TRADING)) + "\t" + optionalDay(day.findNextDay(neverDay)) + "\t" +
           projected(day.getNextDay(neverDay)) + "\t" + optionalSession(day.findFirstSession(SessionFilter::TRADING)) +
           "\t" + optionalSession(day.findLastSession(SessionFilter::TRADING)) + "\t" +
           optionalSession(day.findFirstSession(neverSession)) + "\t" + projected(day.getFirstSession(neverSession)) +
           "\t" + optionalSession(first.findNextSession(SessionFilter::REGULAR)) + "\t" +
           optionalSession(last.findPrevSession(SessionFilter::REGULAR)) + "\t" +
           optionalSession(last.findNextSession(neverSession));
}

/** @param value Actual owning day.
 * @return All scalar/session/unfiltered navigation observations.
 */
std::string projectDay(const Day &value) {
    std::string sessions;

    for (const auto &session : value.getSessions()) {
        sessions += core(session) + ":" + std::to_string(session.containsTime(session.getStartTime())) + ":" +
                    std::to_string(session.containsTime(session.getEndTime())) + ";";
    }

    const auto first = value.getFirstSession();
    const auto last = value.getLastSession();

    return core(value) + "\t" + sessions + "\t" + projected(value.getPrevDay()) + "\t" + projected(value.getNextDay()) +
           "\t" + core(first) + "\t" + core(last) + "\t" + projected(first.getPrevSession()) + "\t" +
           projected(last.getNextSession()) + "\t" + std::to_string(value.containsTime(value.getStartTime())) + "," +
           std::to_string(value.containsTime(value.getEndTime()));
}

/** @param value Actual owning session.
 * @return Public session/day/navigation observations.
 */
std::string projectSession(const Session &value) {
    return core(value) + "\t" + core(value.getDay()) + "\t" + projected(value.getPrevSession()) + "\t" +
           projected(value.getNextSession());
}

/** @param context Actual public calendar domain.
 * @param encoded Ordered typed input entries, with borrowed views held only through this call.
 * @param version Explicit typed comparison version.
 */
void update(const ScheduleDataContext &context, std::string_view encoded, std::int64_t version) {
    struct Entry {
        CalendarEntryKind kind;
        std::u16string name;
        std::u16string value;
    };

    std::vector<Entry> owned;

    if (encoded != "-") {
        for (const auto token : splitCells(encoded, '|')) {
            const auto parts = splitCells(token, '/');

            if (parts.size() != 3) {
                throw std::runtime_error("Invalid typed entry fixture");
            }

            CalendarEntryKind kind;

            if (parts[0] == "H") {
                kind = CalendarEntryKind::HOLIDAY_DAYS;
            } else if (parts[0] == "S") {
                kind = CalendarEntryKind::SHORT_DAYS;
            } else if (parts[0] == "X") {
                kind = CalendarEntryKind::SPECIAL_DAYS;
            } else if (parts[0] == "T") {
                kind = CalendarEntryKind::VENUE;
            } else {
                throw std::runtime_error("Unknown typed entry fixture");
            }

            owned.push_back({kind, decodeHex(parts[1]), decodeHex(parts[2])});
        }
    }

    std::vector<CalendarDefaultsEntry> entries;

    for (const auto &entry : owned) {
        entries.push_back({entry.kind, entry.name, entry.value});
    }

    (void)requireValue(context.setDefaults({version, entries}));
}
} // namespace

std::vector<std::string_view> splitCells(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;

    for (std::size_t first = 0;;) {
        const auto next = text.find(delimiter, first);
        result.push_back(text.substr(first, next == text.npos ? next : next - first));

        if (next == text.npos) {
            return result;
        }

        first = next + 1;
    }
}

std::u16string decodeHex(std::string_view text) {
    if (text == "-") {
        return {};
    }

    if (text.size() % 4 != 0) {
        throw std::runtime_error("Invalid UTF16 fixture width");
    }

    std::u16string result;

    for (std::size_t index = 0; index < text.size(); index += 4) {
        unsigned value = 0;
        const auto converted = std::from_chars(text.data() + index, text.data() + index + 4, value, 16);

        if (converted.ec != std::errc{} || converted.ptr != text.data() + index + 4) {
            throw std::runtime_error("Invalid UTF16 fixture digits");
        }

        result.push_back(static_cast<char16_t>(value));
    }

    return result;
}

QueryProjection::QueryProjection(ScheduleTimeZoneData data) : data_(std::move(data)) {
}

const ScheduleDataContext &QueryProjection::context(std::int64_t reference) {
    if (!context_ || reference_ != reference) {
        ScheduleQueryOptions options;
        options.rawOffsetReferenceTime = reference;
        context_.emplace(requireValue(ScheduleDataContext::create(data_, {}, options)));
        reference_ = reference;
    }

    return *context_;
}

std::string QueryProjection::observe(std::string_view line) {
    const auto cells = splitCells(line, '\t');

    if (cells.size() != 6) {
        throw std::runtime_error("Expected six query fixture cells");
    }

    const std::string id(cells[0]);
    try {
        const auto argument = number(cells[4]);
        const auto definition = decodeHex(cells[3]);

        if (cells[1] == "R") {
            const auto schedule =
                requireValue(Schedule::getInstance(context(argument), u"(tz=" + definition + u";td=;0=)"));

            return "RAW\t" + id + "\t" + encodeHex(definition) + "\t" +
                   std::to_string(detail::Access::rules(schedule)->rawOffsetSeconds * 1000);
        }

        if (cells[1] == "H") {
            const auto updates = splitCells(cells[5], '~');
            ScheduleQueryOptions options;
            options.rawOffsetReferenceTime = REFERENCE_MILLIS;
            auto domain = requireValue(ScheduleDataContext::create(data_, {}, options));
            update(domain, updates[0], 1780000000000LL);
            const auto schedule = requireValue(Schedule::getInstance(domain, definition));
            const auto old = requireValue(schedule.getDayById(static_cast<std::int32_t>(argument)));
            const auto oldSession = old.getFirstSession();
            const auto before = core(old);
            const auto beforeNext = projected(old.getNextDay());
            update(domain, updates[1], 1780000001000LL);
            const auto current = requireValue(schedule.getDayById(static_cast<std::int32_t>(argument)));

            return "HOT\t" + id + "\t" + before + "\t" + core(old) + "\t" + core(current) + "\t" + core(oldSession) +
                   "\t" + std::to_string(old == current) + "\t" +
                   std::to_string(oldSession == current.getFirstSession()) + "\t" +
                   encodeHex(old.getSchedule().getName()) + "\t" + beforeNext + "\t" + projected(old.getNextDay()) +
                   "\t" + projected(current.getNextDay());
        }

        if (cells[1] == "F") {
            ScheduleLimits limits;
            limits.queryWork = 10000000;
            limits.queryStagingBytes = 64000000;
            auto domain = requireValue(ScheduleDataContext::create(data_, limits));
            const auto schedule = requireValue(Schedule::getInstance(domain, definition));

            const auto day = requireValue(schedule.getDayByYearMonthDay(static_cast<std::int32_t>(argument)));

            if (cells[5] == "B") {
                return "BOUNDARY\t" + id + "\t" + optionalDay(day.findNextDay(DayFilter::HOLIDAY));
            }

            return "FILTER\t" + id + "\t" + projectFilter(day);
        }

        const auto schedule = requireValue(Schedule::getInstance(context(REFERENCE_MILLIS), definition));

        if (cells[1] == "D") {
            return "DAY\t" + id + "\t" +
                   projectDay(requireValue(schedule.getDayById(static_cast<std::int32_t>(argument))));
        }

        if (cells[1] == "Y") {
            return "DAY\t" + id + "\t" +
                   projectDay(requireValue(schedule.getDayByYearMonthDay(static_cast<std::int32_t>(argument))));
        }

        if (cells[1] == "T") {
            return "DAY\t" + id + "\t" + projectDay(requireValue(schedule.getDayByTime(argument)));
        }

        if (cells[1] == "S") {
            return "SESSION\t" + id + "\t" + projectSession(requireValue(schedule.getSessionByTime(argument)));
        }

        throw std::runtime_error("Unknown query fixture mode");
    } catch (const ProjectionFailure &failure) {
        return "ERROR\t" + id + "\t" + errorName(failure.error);
    }
}
} // namespace org::ttldtor::qdcxx::schedule::conformance
