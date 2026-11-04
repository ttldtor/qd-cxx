// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_private.hpp"

#include "qd/events/time_helpers.hpp"
#include "schedule_data.hpp"

#include <algorithm>
#include <bit>
#include <iterator>
#include <optional>

namespace org::ttldtor::qdcxx::schedule::detail {
namespace {
constexpr std::int64_t HOUR_MILLIS = 3600000;
constexpr std::int64_t MINUTE_MILLIS = 60000;
constexpr std::int64_t SECOND_MILLIS = 1000;
constexpr std::size_t ZONE_LOOKUP_TEXT_PASSES = 16;

/** @param error Typed owning-boundary failure. */
[[noreturn]] void fail(ScheduleError error = ScheduleError::INVALID_DEFINITION) {
    throw Failure{error};
}

/** @param count Logical element count.
 * @param size One element's byte contribution.
 * @return Checked byte count.
 */
std::size_t bytesFor(std::size_t count, std::size_t size) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    return count * size;
}

/** @param value UTF-16 code unit.
 * @return Whether it belongs to Java Pattern's default ASCII word class.
 */
bool word(char16_t value) noexcept {
    return (value >= u'a' && value <= u'z') || (value >= u'A' && value <= u'Z') || (value >= u'0' && value <= u'9') ||
           value == u'_';
}

/** @param value UTF-16 code unit.
 * @return Whether it belongs to the session/strategy ASCII letter-or-underscore class.
 */
bool letter(char16_t value) noexcept {
    return word(value) && !(value >= u'0' && value <= u'9');
}

/** @param value UTF-16 code unit.
 * @return Whether it is one decimal ASCII digit.
 */
bool digit(char16_t value) noexcept {
    return value >= u'0' && value <= u'9';
}

/** @param value Borrowed ASCII token.
 * @param expected Lowercase expected token.
 * @return Case-insensitive ASCII equality, as used only by session group prefixes.
 */
bool samePrefix(std::u16string_view value, std::u16string_view expected) noexcept {
    if (value.size() != expected.size()) {
        return false;
    }

    for (std::size_t i = 0; i < value.size(); ++i) {
        auto ch = value[i];

        if (ch >= u'A' && ch <= u'Z') {
            ch += u'a' - u'A';
        }

        if (ch != expected[i]) {
            return false;
        }
    }

    return true;
}

/** @param text Exact eight-unit token.
 * @return Source YMD value, or -1; only length/digits/month are validated.
 */
std::int32_t yearMonthDay(std::u16string_view text) noexcept {
    if (text.size() != 8) {
        return -1;
    }

    std::int32_t value = 0;

    for (auto ch : text) {
        if (!digit(ch)) {
            return -1;
        }

        value = value * 10 + ch - u'0';
    }

    const auto month = value / 100 % 100;

    return month >= 1 && month <= 12 ? value : -1;
}

/** @param value Borrowed exact timezone identity.
 * @return Whether the pinned Java compatibility ID profile accepts it.
 */
bool acceptedZone(std::u16string_view value) {
    if (std::binary_search(data::ZONE_IDS.begin(), data::ZONE_IDS.end(), value)) {
        return true;
    }

    if (value.size() != 9 || value.substr(0, 3) != u"GMT" || (value[3] != u'+' && value[3] != u'-') ||
        value[6] != u':' || !digit(value[4]) || !digit(value[5]) || !digit(value[7]) || !digit(value[8])) {
        return false;
    }

    return (value[4] - u'0') * 10 + value[5] - u'0' < 24 && (value[7] - u'0') * 10 + value[8] - u'0' < 60;
}

/** @param props Ordered property list.
 * @param key Exact case-sensitive name.
 * @param construction Operation counters.
 * @return Borrowed value or null.
 */
const std::u16string *property(const Properties &props, std::u16string_view key, Construction &construction) {
    for (const auto &entry : props) {
        construction.inspect(key.size() + entry.first.size() + 1);

        if (entry.first == key) {
            return &entry.second;
        }
    }

    return nullptr;
}

/** @param props Mutable ordered property candidate.
 * @param key Borrowed key to replace/bump.
 * @param value Borrowed owning-copy source.
 * @param construction Operation counters.
 */
void putProperty(Properties &props, std::u16string_view key, std::u16string_view value, Construction &construction) {
    auto newKey = construction.copy(key);
    auto newValue = construction.copy(value);

    for (auto it = props.begin(); it != props.end(); ++it) {
        construction.inspect(key.size() + it->first.size() + 1);

        if (it->first == key) {
            props.erase(it);
            break;
        }
    }

    if (props.size() >= construction.limits.properties) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    construction.allocate(sizeof(Properties::value_type));
    props.emplace_back(std::move(newKey), std::move(newValue));
}

/** @param props Destination preserving bump-last ordering.
 * @param text Raw semicolon/chained-equals property text.
 * @param construction Operation counters.
 */
void readProperties(Properties &props, std::u16string_view text, Construction &construction) {
    construction.inspect(text.size());

    for (std::size_t begin = 0; begin <= text.size();) {
        auto end = text.find(u';', begin);

        if (end == text.npos) {
            end = text.size();
        }

        const auto cell = text.substr(begin, end - begin);
        const auto last = cell.rfind(u'=');

        if (last != cell.npos) {
            const auto value = cell.substr(last + 1);

            for (std::size_t keyBegin = 0; keyBegin <= last;) {
                const auto keyEnd = cell.find(u'=', keyBegin);
                putProperty(props, cell.substr(keyBegin, keyEnd - keyBegin), value, construction);
                keyBegin = keyEnd + 1;
            }
        }

        if (end == text.size()) {
            break;
        }

        begin = end + 1;
    }
}

/** @param lists Ordered named date lists.
 * @param name Exact requested name.
 * @param construction Operation counters.
 * @return Borrowed list or null.
 */
const std::vector<std::int32_t> *namedDays(const std::vector<NamedDays> &lists, std::u16string_view name,
                                           Construction &construction) {
    for (const auto &entry : lists) {
        construction.inspect(name.size() + entry.name.size() + 1);

        if (entry.name == name) {
            return &entry.days;
        }
    }

    return nullptr;
}

/** @param construction Operation counters.
 * @param count Date-value construction credits and logical backing to reserve.
 */
void chargeDates(Construction &construction, std::size_t count) {
    if (count > construction.limits.dateValues - construction.dateValues) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    construction.dateValues += count;
    construction.allocate(bytesFor(count, sizeof(std::int32_t)));
}

/** @param left Current sorted unique set.
 * @param right Referenced sorted unique set.
 * @param operation Union (0), subtraction ('-') or intersection ('*').
 * @param construction Operation counters.
 * @return Complete new sorted set; temporary/model work is charged before growth.
 */
std::vector<std::int32_t> mergeDays(const std::vector<std::int32_t> &left, const std::vector<std::int32_t> &right,
                                    char16_t operation, Construction &construction) {
    const auto count = operation == 0 ? left.size() + right.size() : left.size();

    if (count < left.size()) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    chargeDates(construction, count);
    construction.inspect(left.size() + right.size() + count);
    std::vector<std::int32_t> result;
    result.reserve(count);

    if (operation == u'-') {
        std::set_difference(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    } else if (operation == u'*') {
        std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    } else {
        std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    }

    return result;
}

/** @param days Mutable sorted unique set.
 * @param value Source YMD literal.
 * @param operation Union (0), subtraction or intersection.
 * @param construction Operation counters.
 */
void literalDay(std::vector<std::int32_t> &days, std::int32_t value, char16_t operation, Construction &construction) {
    auto first = days.begin();
    auto last = days.end();

    while (first != last) {
        construction.inspect(1);
        const auto middle = first + (last - first) / 2;

        if (*middle < value) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }

    const bool exists = first != days.end() && *first == value;
    construction.inspect(static_cast<std::size_t>(days.end() - first));

    if (operation == u'*') {
        days.clear();

        if (exists) {
            days.push_back(value);
        }
    } else if (operation == u'-') {
        if (exists) {
            days.erase(first);
        }
    } else if (!exists) {
        chargeDates(construction, 1);
        days.insert(first, value);
    }
}

/** @param text Optional date-list expression.
 * @param lists Previously available named lists in this namespace.
 * @param construction Operation counters.
 * @return Owned sorted date values, without calendar normalization.
 */
std::vector<std::int32_t> readDays(std::u16string_view text, const std::vector<NamedDays> &lists,
                                   Construction &construction) {
    construction.inspect(text.size());
    std::vector<std::int32_t> result;

    for (std::size_t begin = 0; begin <= text.size();) {
        auto end = text.find(u',', begin);

        if (end == text.npos) {
            end = text.size();
        }

        auto token = text.substr(begin, end - begin);

        if (!token.empty()) {
            char16_t operation = 0;

            if (token.front() == u'-' || token.front() == u'*') {
                operation = token.front();
                token.remove_prefix(1);
            }

            if (const auto *reference = namedDays(lists, token, construction)) {
                result = mergeDays(result, *reference, operation, construction);
            } else {
                const auto value = yearMonthDay(token);

                if (value < 0) {
                    fail();
                }

                literalDay(result, value, operation, construction);
            }
        }

        if (end == text.size()) {
            break;
        }

        begin = end + 1;
    }

    return result;
}

/** @param text Exact direct TimeDef field, including source's non-digit arithmetic quirks.
 * @param construction Operation counters.
 * @return Unnormalized local fields.
 */
TimeDefinition timeDefinition(std::u16string_view text, Construction &construction) {
    construction.inspect(text.size());
    TimeDefinition result;
    std::size_t i = 0;
    std::uint32_t day = 0;

    while (i < text.size() && (text[i] == u'+' || text[i] == u'-')) {
        day += text[i] == u'+' ? std::uint32_t{1} : std::numeric_limits<std::uint32_t>::max();
        ++i;
    }

    result.day = std::bit_cast<std::int32_t>(day);

    if (text.size() - i != 4 && text.size() - i != 6) {
        fail();
    }

    const auto pair = [&](std::size_t at) {
        return (static_cast<std::int32_t>(text[at]) - u'0') * 10 + static_cast<std::int32_t>(text[at + 1]) - u'0';
    };
    result.hour = pair(i);
    result.minute = pair(i + 2);
    result.second = text.size() - i == 6 ? pair(i + 4) : 0;

    if (!(result.hour == 24 && result.minute == 0 && result.second == 0) &&
        (result.hour >= 24 || result.minute >= 60 || result.second >= 60)) {
        fail();
    }

    return result;
}

/** @param text One non-letter session group.
 * @param width Exact digits per endpoint (four before six, as in the reference).
 * @param at Current input position, advanced on success.
 * @return Endpoint end position or npos on mismatch.
 */
std::size_t endpointEnd(std::u16string_view text, std::size_t width, std::size_t &at) noexcept {
    while (at < text.size() && (text[at] == u'+' || text[at] == u'-')) {
        ++at;
    }

    if (width > text.size() - at) {
        return text.npos;
    }

    for (std::size_t i = 0; i < width; ++i) {
        if (!digit(text[at + i])) {
            return text.npos;
        }
    }

    at += width;

    return at;
}

/** @param text One complete session group.
 * @param width Endpoint digit width.
 * @return Whether the entire group matches repeated endpoint pairs.
 */
bool sessionGroupMatches(std::u16string_view text, std::size_t width) noexcept {
    std::size_t at = 0;

    while (at < text.size()) {
        if (endpointEnd(text, width, at) == text.npos) {
            return false;
        }

        if (at < text.size() && text[at] == u'/') {
            ++at;
        }

        if (endpointEnd(text, width, at) == text.npos) {
            return false;
        }
    }

    return true;
}

/** @param sessions Mutable abstract-session candidate.
 * @param session Complete local fields/type.
 * @param construction Operation counters.
 */
void appendSession(std::vector<SessionDefinition> &sessions, SessionDefinition session, Construction &construction) {
    if (sessions.size() >= construction.limits.sessionsPerDay) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    construction.allocate(sizeof(SessionDefinition));
    sessions.push_back(session);
}

/** @param text Complete abstract day definition.
 * @param start Inherited local day start.
 * @param end Inherited local day end.
 * @param reset Optional inherited reset.
 * @param construction Operation counters.
 * @return Fully validated local day/session definition with inserted gaps.
 */
DayDefinition dayDefinition(std::u16string_view text, TimeDefinition start, TimeDefinition end,
                            std::optional<TimeDefinition> reset, Construction &construction) {
    if (construction.dayDefinitions == construction.limits.dayDefinitions) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    ++construction.dayDefinitions;
    construction.allocate(sizeof(DayDefinition));
    construction.inspect(bytesFor(text.size(), 3));
    std::vector<SessionDefinition> sessions;

    for (std::size_t at = 0; at < text.size();) {
        const auto keyStart = at;

        while (at < text.size() && letter(text[at])) {
            ++at;
        }

        const auto key = text.substr(keyStart, at - keyStart);
        const auto dataStart = at;

        while (at < text.size() && !letter(text[at])) {
            ++at;
        }

        const auto value = text.substr(dataStart, at - dataStart);

        if (value.empty()) {
            fail();
        }

        if (samePrefix(key, u"rt")) {
            reset = timeDefinition(value, construction);
            continue;
        }

        SessionKind kind;

        if (samePrefix(key, u"d")) {
            kind = SessionKind::NO_TRADING;
        } else if (samePrefix(key, u"p")) {
            kind = SessionKind::PRE_MARKET;
        } else if (samePrefix(key, u"a")) {
            kind = SessionKind::AFTER_MARKET;
        } else if (key.empty() || samePrefix(key, u"r")) {
            kind = SessionKind::REGULAR;
        } else {
            fail();
        }

        const std::size_t width = sessionGroupMatches(value, 4) ? 4 : sessionGroupMatches(value, 6) ? 6 : 0;

        if (width == 0) {
            fail();
        }

        for (std::size_t pairAt = 0; pairAt < value.size();) {
            const auto first = pairAt;
            const auto firstEnd = endpointEnd(value, width, pairAt);
            const auto firstTime = timeDefinition(value.substr(first, firstEnd - first), construction);

            if (pairAt < value.size() && value[pairAt] == u'/') {
                ++pairAt;
            }

            const auto second = pairAt;
            const auto secondEnd = endpointEnd(value, width, pairAt);
            const auto secondTime = timeDefinition(value.substr(second, secondEnd - second), construction);

            if (firstTime > secondTime) {
                fail();
            }

            if (kind == SessionKind::NO_TRADING) {
                start = firstTime;
                end = secondTime;
            } else {
                appendSession(sessions, {kind, firstTime, secondTime}, construction);
            }

            kind = SessionKind::REGULAR;
        }
    }

    const auto resetTime = reset.value_or(start);

    if (resetTime < start || (start == end ? resetTime > end : resetTime >= end)) {
        fail();
    }

    DayDefinition result{start, end, resetTime, {}};

    if (sessions.empty()) {
        appendSession(result.sessions, {SessionKind::NO_TRADING, start, end}, construction);

        return result;
    }

    auto previous = start;

    for (const auto &session : sessions) {
        if (previous > session.start) {
            fail();
        }

        if (previous < session.start) {
            appendSession(result.sessions, {SessionKind::NO_TRADING, previous, session.start}, construction);
        }

        appendSession(result.sessions, session, construction);
        previous = session.end;
    }

    if (previous > end) {
        fail();
    }

    if (previous < end) {
        appendSession(result.sessions, {SessionKind::NO_TRADING, previous, end}, construction);
    }

    return result;
}

/** @param rules Mutable complete rule candidate.
 * @param day Owning abstract definition.
 * @param construction Operation counters.
 * @return Stable definition index inside this rule model.
 */
std::size_t appendDefinition(Rules &rules, DayDefinition day, Construction &construction) {
    construction.allocate(sizeof(DayDefinition));
    rules.definitions.push_back(std::move(day));

    return rules.definitions.size() - 1;
}

/** @param props Merged ordered properties.
 * @param key Strategy property name.
 * @param name Recognized case-sensitive strategy name.
 * @param digits Exact decimal parameter width.
 * @param construction Operation counters.
 * @return Accepted parameter view, or empty for absent/unknown strategies.
 */
std::u16string_view genericStrategy(const Properties &props, std::u16string_view key, std::u16string_view name,
                                    std::size_t digits, Construction &construction) {
    const auto *value = property(props, key, construction);

    if (value == nullptr || value->empty()) {
        return {};
    }

    std::size_t at = 0;

    while (at < value->size() && letter((*value)[at])) {
        ++at;
    }

    if (at == 0) {
        fail();
    }

    // Java's optional parameter starts with any non-letter, but its remaining dot does not cross line terminators.
    for (std::size_t i = at + 1; i < value->size(); ++i) {
        if ((*value)[i] == u'\n' || (*value)[i] == u'\r' || (*value)[i] == u'\u0085' || (*value)[i] == u'\u2028' ||
            (*value)[i] == u'\u2029') {
            fail();
        }
    }

    if (std::u16string_view(*value).substr(0, at) != name) {
        return {};
    }

    const auto argument = std::u16string_view(*value).substr(at);

    if (argument.size() != digits || !std::all_of(argument.begin(), argument.end(), digit)) {
        fail();
    }

    return argument;
}

/** @param rule Ordered comma-separated per-date strategy text.
 * @param rules Owning destination for any custom abstract definitions.
 * @param start Global inherited start.
 * @param end Global inherited end.
 * @param reset Global optional reset.
 * @param construction Operation counters.
 * @return Composed source strategy, before day/default population.
 */
DayStrategy parseStrategy(std::u16string_view rule, Rules &rules, TimeDefinition start, TimeDefinition end,
                          std::optional<TimeDefinition> reset, Construction &construction) {
    construction.inspect(rule.size());
    DayStrategy strategy;

    for (std::size_t begin = 0; begin <= rule.size();) {
        auto finish = rule.find(u',', begin);

        if (finish == rule.npos) {
            finish = rule.size();
        }

        const auto part = rule.substr(begin, finish - begin);

        if (part == u"closed") {
            if (strategy.definition != NO_DEFINITION || strategy.earlyClose) {
                strategy.definition = NO_DEFINITION;
                strategy.earlyClose = false;
                strategy.earlyCloseMillis = 0;
            }

            strategy.closed = true;
        } else if (part == u"normal") {
            if (strategy.definition != NO_DEFINITION || strategy.closed) {
                strategy.definition = NO_DEFINITION;
                strategy.closed = false;
            }

            strategy.forcedTrading = true;
        } else if (part.starts_with(u"def:")) {
            if (strategy.closed || strategy.forcedTrading) {
                strategy.closed = false;
                strategy.forcedTrading = false;
            }

            strategy.definition =
                appendDefinition(rules, dayDefinition(part.substr(4), start, end, reset, construction), construction);
        } else if (part.starts_with(u"ec:")) {
            strategy.closed = false;
            strategy.earlyClose = true;
            strategy.earlyCloseMillis = timeDefinition(part.substr(3), construction).offset();
        }

        if (finish == rule.size()) {
            break;
        }

        begin = finish + 1;
    }

    return strategy;
}

/** @param key Exact ds. suffix.
 * @param defaults Calendar lists.
 * @param construction Operation counters.
 * @return Owned referenced YMD set; unknown reference families intentionally yield empty.
 */
std::vector<std::int32_t> referencedDays(std::u16string_view key, const Defaults &defaults,
                                         Construction &construction) {
    const auto date = yearMonthDay(key);

    if (date > 0) {
        chargeDates(construction, 1);

        return {date};
    }

    const auto *lists = key.starts_with(u"hd.")   ? &defaults.holidays
                        : key.starts_with(u"sd.") ? &defaults.shortDays
                        : key.starts_with(u"xd.") ? &defaults.specialDays
                                                  : nullptr;

    if (lists == nullptr) {
        return {};
    }

    const auto *days = namedDays(*lists, key.substr(3), construction);

    if (days == nullptr) {
        fail();
    }

    chargeDates(construction, days->size());
    construction.inspect(days->size());

    return *days;
}

/** @param rules Complete owned rules model.
 * @return Logical owned-field byte count, excluding allocator/control-block overhead.
 */
std::size_t ruleBytes(const Rules &rules) {
    std::size_t bytes =
        sizeof(Rules) + (rules.definition.size() + rules.name.size() + rules.zone.size()) * sizeof(char16_t);

    for (const auto &entry : rules.properties) {
        bytes += sizeof(Properties::value_type) + (entry.first.size() + entry.second.size()) * sizeof(char16_t);
    }

    bytes += (rules.holidays.size() + rules.shortDays.size()) * sizeof(std::int32_t);

    for (const auto &day : rules.definitions) {
        bytes += sizeof(DayDefinition) + day.sessions.size() * sizeof(SessionDefinition);
    }

    return bytes + rules.overrides.size() * sizeof(DayStrategy);
}
} // namespace

void Construction::inspect(std::size_t amount) {
    if (amount > limits.work - work) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    work += amount;
}

void Construction::allocate(std::size_t amount) {
    if (amount > limits.stagingBytes - bytes) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    bytes += amount;
}

std::u16string Construction::copy(std::u16string_view value) {
    inspect(value.size());
    allocate(bytesFor(value.size(), sizeof(char16_t)));

    return std::u16string(value);
}

std::int64_t TimeDefinition::offset() const noexcept {
    return day * DAY_MILLIS + hour * HOUR_MILLIS + minute * MINUTE_MILLIS + second * SECOND_MILLIS;
}

std::strong_ordering TimeDefinition::operator<=>(const TimeDefinition &other) const noexcept {
    for (const auto pair : {std::pair{day, other.day}, std::pair{hour, other.hour}, std::pair{minute, other.minute},
                            std::pair{second, other.second}}) {
        if (pair.first != pair.second) {
            const auto difference = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(pair.first) -
                                                                static_cast<std::uint32_t>(pair.second));

            return difference < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
        }
    }

    return std::strong_ordering::equal;
}

std::u16string_view selectDefinition(std::u16string_view text, const std::u16string_view *venue,
                                     Construction &construction) {
    if (text.size() > construction.limits.inputUnits ||
        (venue != nullptr && venue->size() > construction.limits.inputUnits)) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    construction.inspect(bytesFor(text.size(), 3));

    for (std::size_t search = 0;;) {
        const auto open = text.find(u'(', search);

        if (open == text.npos) {
            fail(venue == nullptr ? ScheduleError::INVALID_DEFINITION : ScheduleError::MISSING_VENUE);
        }

        construction.inspect(bytesFor(text.size(), 3));

        const auto close = text.find(u')', open + 1);

        if (close == text.npos) {
            fail();
        }

        auto start = std::size_t{0};

        if (open != 0) {
            const auto previousClose = text.rfind(u')', open - 1);
            const auto previousSeparator = text.rfind(u';', open - 1);

            if (previousClose != text.npos) {
                start = previousClose + 1;
            }

            if (previousSeparator != text.npos) {
                start = std::max(start, previousSeparator + 1);
            }
        }

        if (venue == nullptr || text.substr(start, open - start) == *venue) {
            return text.substr(start, close - start + 1);
        }

        search = open + 1;
    }
}

std::shared_ptr<Rules> parseRules(std::u16string_view text, const std::shared_ptr<const Defaults> &defaults,
                                  Construction &construction) {
    construction.inspect(text.size());
    const auto open = text.find(u'(');

    if (open == text.npos || text.empty() || text.back() != u')' ||
        !std::all_of(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(open), word) ||
        text.find_first_of(u"()", open + 1) != text.size() - 1) {
        fail();
    }

    construction.allocate(sizeof(Rules));
    auto result = std::make_shared<Rules>();
    result->defaults = defaults;
    result->definition = construction.copy(text);
    const auto venue = text.substr(0, open);

    for (const auto &entry : defaults->venues) {
        construction.inspect(entry.name.size() + venue.size() + 1);

        if (entry.name == venue) {
            for (const auto &prop : entry.properties) {
                putProperty(result->properties, prop.first, prop.second, construction);
            }

            break;
        }
    }

    readProperties(result->properties, text.substr(open + 1, text.size() - open - 2), construction);
    const auto *name = property(result->properties, u"name", construction);
    result->name = construction.copy(name == nullptr ? venue : std::u16string_view(*name));
    const auto *zone = property(result->properties, u"tz", construction);

    if (zone == nullptr || zone->empty()) {
        fail();
    }

    construction.inspect(bytesFor(zone->size(), ZONE_LOOKUP_TEXT_PASSES));
    construction.inspect(data::ZONE_IDS.size());

    if (!acceptedZone(*zone)) {
        fail(ScheduleError::UNKNOWN_TIME_ZONE);
    }

    result->zone = construction.copy(*zone);
    const auto *hd = property(result->properties, u"hd", construction);
    const auto *sd = property(result->properties, u"sd", construction);
    result->holidays = readDays(hd == nullptr ? std::u16string_view{} : *hd, defaults->holidays, construction);
    result->shortDays = readDays(sd == nullptr ? std::u16string_view{} : *sd, defaults->shortDays, construction);
    const auto ec = genericStrategy(result->properties, u"sds", u"ec", 4, construction);
    const auto jntd = genericStrategy(result->properties, u"hds", u"jntd", 1, construction);
    result->earlyClose = ec.empty() ? 0 : timeDefinition(ec, construction).offset();
    result->joinNextTradingDay = jntd.empty() ? 0 : jntd[0] - u'0';
    const auto *trading = property(result->properties, u"td", construction);
    const auto td = trading == nullptr ? std::u16string_view(u"12345") : *trading;
    const auto *dayEnd = property(result->properties, u"de", construction);
    const auto end = timeDefinition(dayEnd == nullptr ? std::u16string_view(u"+0000") : *dayEnd, construction);
    auto start = end;
    start.day = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(start.day) - 1U);
    result->dayOffset = start.offset();
    const auto *resetProperty = property(result->properties, u"rt", construction);
    const auto reset =
        resetProperty == nullptr ? std::optional<TimeDefinition>{} : timeDefinition(*resetProperty, construction);
    result->weekDays.fill(NO_DEFINITION);

    for (std::size_t i = 0; i <= 8; ++i) {
        const char16_t key[] = {static_cast<char16_t>(u'0' + i)};

        if (const auto *value = property(result->properties, std::u16string_view(key, 1), construction)) {
            result->weekDays[i] =
                appendDefinition(*result, dayDefinition(*value, start, end, reset, construction), construction);
        }
    }

    if (result->weekDays[8] == NO_DEFINITION) {
        const auto zero = result->weekDays[0];

        if (zero == NO_DEFINITION) {
            result->weekDays[8] =
                appendDefinition(*result, dayDefinition({}, start, end, reset, construction), construction);
        } else {
            const auto &day = result->definitions[zero];
            result->weekDays[8] =
                appendDefinition(*result, dayDefinition({}, day.start, day.end, day.reset, construction), construction);
        }
    }

    for (std::size_t i = 1; i <= 7; ++i) {
        if (result->weekDays[i] == NO_DEFINITION) {
            result->weekDays[i] = result->weekDays[td.find(static_cast<char16_t>(u'0' + i)) == td.npos ? 8 : 0];

            if (result->weekDays[i] == NO_DEFINITION) {
                fail();
            }
        }
    }

    for (const auto &prop : result->properties) {
        std::vector<std::int32_t> days;
        DayStrategy strategy;
        const auto ymd = yearMonthDay(prop.first);

        if (ymd > 0) {
            chargeDates(construction, 1);
            days.push_back(ymd);
            construction.allocate(sizeof(std::u16string) + bytesFor(prop.second.size() + 4, sizeof(char16_t)));
            strategy = parseStrategy(u"def:" + prop.second, *result, start, end, reset, construction);
        } else if (prop.first.starts_with(u"ds.")) {
            days = referencedDays(std::u16string_view(prop.first).substr(3), *defaults, construction);

            if (days.empty()) {
                continue;
            }

            strategy = parseStrategy(prop.second, *result, start, end, reset, construction);
        } else {
            continue;
        }

        for (const auto date : days) {
            construction.inspect(result->overrides.size() + 1);
            auto populated = strategy;
            populated.yearMonthDay = date;

            if (populated.definition == NO_DEFINITION) {
                const auto id = events::getDayIdByYearMonthDay(date).value();
                const auto weekday = static_cast<std::size_t>(((id + 3) % 7 + 7) % 7 + 1);
                populated.definition = result->weekDays[weekday];
            }

            if (!populated.earlyClose && result->earlyClose != 0 &&
                std::binary_search(result->shortDays.begin(), result->shortDays.end(), date)) {
                populated.earlyClose = true;
                populated.earlyCloseMillis = result->earlyClose;
            }

            const auto it = std::lower_bound(result->overrides.begin(), result->overrides.end(), date,
                                             [](const DayStrategy &item, std::int32_t value) {
                                                 return item.yearMonthDay < value;
                                             });

            if (it != result->overrides.end() && it->yearMonthDay == date) {
                *it = populated;
            } else {
                construction.allocate(sizeof(DayStrategy));
                result->overrides.insert(it, populated);
            }
        }
    }

    result->modelBytes = ruleBytes(*result);

    return result;
}

std::shared_ptr<Defaults> parseDefaults(CalendarDefaults input, Construction &construction) {
    if (input.entries.size() > construction.limits.defaultEntries) {
        fail(ScheduleError::RESOURCE_LIMIT);
    }

    std::size_t units = 0;

    for (const auto &entry : input.entries) {
        for (const auto count : {entry.name.size(), entry.definition.size()}) {
            if (count > construction.limits.defaultUnits - units) {
                fail(ScheduleError::RESOURCE_LIMIT);
            }

            units += count;
        }
    }

    construction.inspect(units);
    construction.allocate(sizeof(Defaults));
    auto result = std::make_shared<Defaults>();
    result->version = input.versionMillis;

    for (const auto &entry : input.entries) {
        if (entry.kind == CalendarEntryKind::VENUE) {
            for (const auto &venue : result->venues) {
                construction.inspect(venue.name.size() + entry.name.size() + 1);

                if (venue.name == entry.name) {
                    fail(ScheduleError::INVALID_DEFAULTS);
                }
            }

            construction.allocate(sizeof(VenueDefaults));
            VenueDefaults venue{construction.copy(entry.name), {}};
            readProperties(venue.properties, entry.definition, construction);
            result->venues.push_back(std::move(venue));
        } else {
            auto *lists = entry.kind == CalendarEntryKind::HOLIDAY_DAYS   ? &result->holidays
                          : entry.kind == CalendarEntryKind::SHORT_DAYS   ? &result->shortDays
                          : entry.kind == CalendarEntryKind::SPECIAL_DAYS ? &result->specialDays
                                                                          : nullptr;

            if (lists == nullptr || namedDays(*lists, entry.name, construction) != nullptr) {
                fail(ScheduleError::INVALID_DEFAULTS);
            }

            construction.allocate(sizeof(NamedDays));
            auto name = construction.copy(entry.name);
            auto days = readDays(entry.definition, *lists, construction);
            lists->push_back({std::move(name), std::move(days)});
        }
    }

    for (const auto &venue : result->venues) {
        construction.allocate(bytesFor(venue.name.size() + 4, sizeof(char16_t)));
        (void)parseRules(venue.name + u"(0=)", result, construction);
    }

    std::size_t bytes = sizeof(Defaults);

    for (const auto *lists : {&result->holidays, &result->shortDays, &result->specialDays}) {
        for (const auto &list : *lists) {
            bytes += sizeof(NamedDays) + list.name.size() * sizeof(char16_t) + list.days.size() * sizeof(std::int32_t);
        }
    }

    for (const auto &venue : result->venues) {
        bytes += sizeof(VenueDefaults) + venue.name.size() * sizeof(char16_t);

        for (const auto &prop : venue.properties) {
            bytes += sizeof(Properties::value_type) + (prop.first.size() + prop.second.size()) * sizeof(char16_t);
        }
    }

    result->modelBytes = bytes;

    return result;
}
} // namespace org::ttldtor::qdcxx::schedule::detail
