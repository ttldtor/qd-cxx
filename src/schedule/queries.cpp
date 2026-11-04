// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_private.hpp"
#include "time_zone_private.hpp"

#include "qd/events/time_helpers.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace org::ttldtor::qdcxx::schedule {
namespace detail {
namespace {
constexpr std::int32_t MIN_DAY_ID = -719160;
constexpr std::int32_t MAX_DAY_ID = 2932894;
constexpr std::int32_t MIN_YMD = 10103;
constexpr std::int32_t MAX_YMD = 99991229;
constexpr std::int64_t MIN_QUERY_TIME = -62135424000000LL;
constexpr std::int64_t MAX_QUERY_TIME = 253402128000000LL;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::int64_t NOON_MILLIS = DAY_MILLIS / 2;
constexpr int FILTER_SEARCH_DAYS = 366;

/** @param id Source signed label.
 * @param shift Signed source arithmetic increment.
 * @return Java int addition without C++ signed-overflow undefined behavior.
 */
std::int32_t advanceDay(std::int32_t id, std::int32_t shift) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(id) + static_cast<std::uint32_t>(shift));
}

/** @param value Signed numerator.
 * @param divisor Positive divisor.
 * @return Mathematical floor quotient without signed overflow.
 */
std::int64_t floorDivide(std::int64_t value, std::int64_t divisor) noexcept {
    return value / divisor - (value % divisor < 0 ? 1 : 0);
}

/** @param dayId Consecutive DayUtil label.
 * @return ISO weekday1..7, including negative day identifiers.
 */
std::size_t weekday(std::int32_t dayId) noexcept {
    return static_cast<std::size_t>((static_cast<std::int64_t>(dayId) + 3) % 7 + 7) % 7 + 1;
}

/** @param count Number of modeled fields.
 * @param size Bytes in each field.
 * @return Checked byte product before allocation/growth.
 */
std::size_t modelBytes(std::size_t count, std::size_t size) {
    if (count > std::numeric_limits<std::size_t>::max() / size) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    return count * size;
}

/** @tparam Result Public success type.
 * @tparam Function One finite owning operation.
 * @param operation Candidate construction outside publication locks.
 * @return Complete success or typed semantic/allocation/resource failure.
 */
template <class Result, class Function> std::expected<Result, ScheduleError> queryGuard(Function &&operation) {
    try {
        return operation();
    } catch (const Failure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(ScheduleError::ALLOCATION);
    } catch (const std::length_error &) {
        return std::unexpected(ScheduleError::RESOURCE_LIMIT);
    }
}

/** @param result Completed filtered search, including any typed refusal.
 * @return Matching value, NO_MATCH for an exhausted search, or the original refusal.
 */
template <class T> std::expected<T, ScheduleError> requireMatch(std::expected<std::optional<T>, ScheduleError> result) {
    if (!result) {
        return std::unexpected(result.error());
    }

    if (!*result) {
        return std::unexpected(ScheduleError::NO_MATCH);
    }

    return **result;
}

/** @param context Explicit parser/query domain.
 * @return Query-specific construction bounds, sharing the same retained-model ledger.
 */
ScheduleLimits queryLimits(const Context &context) {
    if (!context.timeZones) {
        throw Failure{ScheduleError::NOT_READY};
    }

    auto result = context.limits;
    result.work = result.queryWork;
    result.stagingBytes = result.queryStagingBytes;

    return result;
}

/** @param dates Sorted immutable date values.
 * @param ymd Packed label to locate.
 * @param construction Operation counters, charged per binary-search comparison.
 * @return Whether the exact label occurs.
 */
bool contains(const std::vector<std::int32_t> &dates, std::int32_t ymd, Construction &construction) {
    std::size_t first = 0;
    auto count = dates.size();

    while (count != 0) {
        construction.inspect(1);
        const auto step = count / 2;

        if (dates[first + step] < ymd) {
            first += step + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    return first < dates.size() && dates[first] == ymd;
}

/** @param rules Pinned complete parsed rules.
 * @param ymd Exact packed day label.
 * @param construction Operation counters.
 * @return Borrowed override inside the owning Rules pin, or null.
 */
const DayStrategy *overrideFor(const Rules &rules, std::int32_t ymd, Construction &construction) {
    std::size_t first = 0;
    auto count = rules.overrides.size();

    while (count != 0) {
        construction.inspect(1);
        const auto step = count / 2;

        if (rules.overrides[first + step].yearMonthDay < ymd) {
            first += step + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    return first < rules.overrides.size() && rules.overrides[first].yearMonthDay == ymd ? &rules.overrides[first]
                                                                                        : nullptr;
}

/** @param rules Pinned parsed model.
 * @param id DayUtil label.
 * @param forceClosedHoliday Whether ordinary holiday closure applies.
 * @param construction Finite inspection counters.
 * @return Effective copied strategy, without changing the parsed generation.
 */
DayStrategy strategyFor(const Rules &rules, std::int32_t id, bool forceClosedHoliday, Construction &construction) {
    construction.inspect(1);
    const auto ymd = events::getYearMonthDayByDayId(id);
    DayStrategy result;

    if (const auto *override = overrideFor(rules, ymd, construction)) {
        result = *override;
    } else {
        result.definition = rules.weekDays[weekday(id)];

        if (rules.earlyClose != 0 && contains(rules.shortDays, ymd, construction)) {
            result.earlyClose = true;
            result.earlyCloseMillis = rules.earlyClose;
        }
    }

    if (forceClosedHoliday && contains(rules.holidays, ymd, construction) && !result.forcedTrading && !result.closed) {
        result.closed = true;
    }

    return result;
}

/** @param definition Complete abstract day.
 * @param construction Finite inspection counters.
 * @return Whether any abstract session has a trading type, regardless of duration.
 */
bool hasTrading(const DayDefinition &definition, Construction &construction) {
    for (const auto &session : definition.sessions) {
        construction.inspect(1);

        if (session.kind != SessionKind::NO_TRADING) {
            return true;
        }
    }

    return false;
}

/** JNTD source classification, independent of an override's forced-trading flag. */
enum class JoinMatch { NONE, CANDIDATE, TRADING };

/** @param rules Pinned parsed rules.
 * @param id Candidate label.
 * @param construction Operation counters.
 * @return Exact source join-next-trading-day classification.
 */
JoinMatch classify(const Rules &rules, std::int32_t id, Construction &construction) {
    const auto ymd = events::getYearMonthDayByDayId(id);
    const auto *override = overrideFor(rules, ymd, construction);
    const auto definition = override == nullptr ? rules.weekDays[weekday(id)] : override->definition;

    if (!hasTrading(rules.definitions[definition], construction)) {
        return JoinMatch::NONE;
    }

    return contains(rules.holidays, ymd, construction) || (override != nullptr && override->closed)
               ? JoinMatch::CANDIDATE
               : JoinMatch::TRADING;
}

/** @param rules Pinned parsed generation.
 * @param id Requested label.
 * @param construction Finite operation counters.
 * @return Inclusive joined label range, or {id,id} for an ordinary day.
 */
std::pair<std::int32_t, std::int32_t> joinedRange(const Rules &rules, std::int32_t id, Construction &construction) {
    if (rules.joinNextTradingDay == 0) {
        return {id, id};
    }

    for (std::int64_t forward = 0; forward <= rules.joinNextTradingDay; ++forward) {
        if (classify(rules, advanceDay(id, static_cast<std::int32_t>(forward)), construction) != JoinMatch::TRADING) {
            continue;
        }

        const auto end = advanceDay(id, static_cast<std::int32_t>(forward));
        std::int32_t earliest = -1;

        for (std::int64_t back = 1; back <= rules.joinNextTradingDay; ++back) {
            const auto match = classify(rules, advanceDay(end, -static_cast<std::int32_t>(back)), construction);

            if (match == JoinMatch::TRADING) {
                break;
            }

            if (match == JoinMatch::CANDIDATE) {
                earliest = static_cast<std::int32_t>(back);
            }
        }

        return earliest >= forward ? std::pair{advanceDay(end, -earliest), end} : std::pair{id, id};
    }

    return {id, id};
}

/** Local-field conversion state for one immutable day construction. */
struct DayBuilder {
    const Rules &rules;
    time::ZoneBinding zone;
    Construction &construction;
    std::vector<SessionRecord> sessions;

    /** @param id Consecutive label.
     * @return Source noon anchor, using the captured constructor raw offset.
     */
    std::int64_t anchor(std::int32_t id) const noexcept {
        return static_cast<std::int64_t>(id) * DAY_MILLIS - rules.rawOffsetSeconds * MILLIS_PER_SECOND + NOON_MILLIS;
    }

    /** @param definition Lenient Calendar local fields and day shift.
     * @param id Label whose noon anchor supplies the physical local date.
     * @return Converted signed instant, preserving gap/later-overlap and skipped-date behavior.
     */
    std::int64_t time(const TimeDefinition &definition, std::int32_t id) {
        construction.inspect(1);
        const auto noon = anchor(id);
        const auto offset = zone.getOffset(noon);

        if (!offset) {
            throw Failure{timeZoneError(offset.error())};
        }

        const auto localNoon = noon + static_cast<std::int64_t>(offset->totalSeconds) * MILLIS_PER_SECOND;
        const auto localDay = floorDivide(localNoon, DAY_MILLIS) * DAY_MILLIS;

        construction.inspect(1);
        const auto converted = time::detail::localToUtc(zone, localDay + definition.offset());

        if (!converted) {
            throw Failure{timeZoneError(converted.error())};
        }

        return *converted;
    }

    /** @param type Source session kind.
     * @param start Exact inclusive instant.
     * @param end Exact exclusive instant; direct source no-trading records can retain inverted endpoints.
     */
    void append(SessionType type, std::int64_t start, std::int64_t end) {
        if (sessions.size() >= construction.limits.querySessionsPerDay) {
            throw Failure{ScheduleError::RESOURCE_LIMIT};
        }

        construction.allocate(sizeof(SessionRecord));
        sessions.push_back({type, start, end});
    }

    /** @param id Label supplying the source noon anchor.
     * @param strategy Complete effective strategy for that label.
     */
    void add(std::int32_t id, const DayStrategy &strategy) {
        const auto &definition = rules.definitions[strategy.definition];

        if (strategy.closed) {
            append(SessionType::NO_TRADING, time(definition.start, id), time(definition.end, id));

            return;
        }

        auto lastRegular = definition.sessions.size();

        if (strategy.earlyClose && strategy.earlyCloseMillis != 0) {
            for (auto i = definition.sessions.size(); i != 0; --i) {
                construction.inspect(1);

                if (definition.sessions[i - 1].kind == SessionKind::REGULAR) {
                    lastRegular = i - 1;
                    break;
                }
            }

            if (lastRegular < definition.sessions.size()) {
                const auto &regular = definition.sessions[lastRegular];

                if (time(regular.start, id) >= time(regular.end, id) - strategy.earlyCloseMillis) {
                    lastRegular = definition.sessions.size();
                }
            }
        }

        const auto shift = -strategy.earlyCloseMillis;

        for (std::size_t i = 0; i < definition.sessions.size(); ++i) {
            construction.inspect(1);
            const auto &session = definition.sessions[i];
            const auto startShift = i > lastRegular ? shift : 0;
            const auto endShift =
                i >= lastRegular && (i == lastRegular || i + 1 < definition.sessions.size()) ? shift : 0;
            const auto start = time(session.start, id) + startShift;
            const auto end = time(session.end, id) + endShift;

            if (start > end) {
                throw Failure{ScheduleError::INVALID_DEFINITION};
            }

            append(static_cast<SessionType>(session.kind), start, end);
        }

        if (lastRegular + 1 == definition.sessions.size()) {
            const auto end = sessions.back().end;
            append(SessionType::NO_TRADING, end, end - shift);
        }
    }
};

/** @param context Owning explicit domain.
 * @param rules Complete generation pin.
 * @param id Requested/internal day label.
 * @param construction Query-wide counters.
 * @return Leased immutable day with all records initialized, without cache/handle publication.
 */
std::shared_ptr<const DayModel> constructDay(const std::shared_ptr<Context> &context,
                                             const std::shared_ptr<const Rules> &rules, std::int32_t id,
                                             Construction &construction) {
    construction.allocate(sizeof(DayModel));
    auto result = std::make_shared<DayModel>();
    result->rules = rules;
    result->dayId = id;
    result->yearMonthDay = events::getYearMonthDayByDayId(id);
    result->holiday = contains(rules->holidays, result->yearMonthDay, construction);
    result->shortDay = contains(rules->shortDays, result->yearMonthDay, construction);
    DayBuilder builder{*rules, *rules->timeZoneBinding, construction, {}};
    const auto [start, end] = joinedRange(*rules, id, construction);

    if (start != end) {
        const auto first = strategyFor(*rules, start, false, construction);
        const auto &firstDefinition = rules->definitions[first.definition];

        if (id != end) {
            result->reset = builder.time(firstDefinition.start, start);
            builder.append(SessionType::NO_TRADING, result->reset, result->reset);
        } else {
            result->holiday = false;
            result->reset = builder.time(firstDefinition.reset, start);
            builder.add(start, first);

            for (auto middle = advanceDay(start, 1); middle < end; middle = advanceDay(middle, 1)) {
                const auto strategy = strategyFor(*rules, middle, false, construction);
                const auto &definition = rules->definitions[strategy.definition];
                builder.append(SessionType::NO_TRADING, builder.time(definition.start, middle),
                               builder.time(definition.end, middle));
            }

            builder.add(end, strategyFor(*rules, end, false, construction));
        }
    } else {
        const auto strategy = strategyFor(*rules, id, true, construction);
        result->reset = builder.time(rules->definitions[strategy.definition].reset, id);
        builder.add(id, strategy);
    }

    if (builder.sessions.empty()) {
        throw Failure{ScheduleError::INVALID_DEFINITION};
    }

    result->sessions = std::move(builder.sessions);
    result->lease =
        acquire(context->ledger, RetainedKind::DAY,
                sizeof(DayModel) + modelBytes(result->sessions.size(), sizeof(SessionRecord)), construction);
    result->sessionLease = acquire(context->ledger, RetainedKind::SESSION, 0, construction, result->sessions.size());

    return result;
}

/** @param context Shared query domain.
 * @param rules Pinned generation, which need not be the current metadata generation.
 * @param id Requested/internal day label.
 * @param construction Operation counters.
 * @return Immutable owning model, optionally reused through a finite weak cache.
 */
std::shared_ptr<const DayModel> queryDay(const std::shared_ptr<Context> &context,
                                         const std::shared_ptr<const Rules> &rules, std::int32_t id,
                                         Construction &construction) {
    const auto base = context->dayCache.load(std::memory_order_acquire);

    if (base) {
        for (const auto &entry : base->entries) {
            construction.inspect(1);

            if (const auto model = entry.lock(); model && model->rules == rules && model->dayId == id) {
                return model;
            }
        }
    }

    const auto result = constructDay(context, rules, id, construction);

    if (context->limits.dayCacheEntries == 0) {
        return result;
    }

    construction.allocate(sizeof(DayCache));
    auto candidate = std::make_shared<DayCache>();

    if (base) {
        for (const auto &entry : base->entries) {
            construction.inspect(1);

            if (!entry.expired() && candidate->entries.size() + 1 < context->limits.dayCacheEntries) {
                construction.allocate(sizeof(entry));
                candidate->entries.push_back(entry);
            }
        }
    }

    construction.allocate(sizeof(std::weak_ptr<const DayModel>));
    candidate->entries.emplace_back(result);
    candidate->lease = acquire(
        context->ledger, RetainedKind::DAY_CACHE,
        sizeof(DayCache) + modelBytes(candidate->entries.size(), sizeof(std::weak_ptr<const DayModel>)), construction);
    std::shared_ptr<const DayCache> desired = std::move(candidate);
    auto expected = base;
    // Cache contention changes only an optimization; a complete leased result remains valid.
    (void)context->dayCache.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);

    return result;
}

/** @param schedule Live identity used to own the returned snapshot.
 * @param rules Pinned complete calendar generation.
 * @param id Label admitted by the caller's particular factory policy.
 * @param construction Query-wide counters.
 * @return Owning immutable day without changing live calendar metadata.
 */
Day ownedDay(const Schedule &schedule, const std::shared_ptr<const Rules> &rules, std::int32_t id,
             Construction &construction) {
    return Access::makeDay(schedule, queryDay(Access::context(schedule), rules, id, construction));
}

/** @param schedule Owning live identity.
 * @param rules Snapshot whose navigation must stay stable.
 * @param id Direct-label request to check.
 * @return Owning snapshot or source range/resource failure.
 */
std::expected<Day, ScheduleError> labelledDay(const Schedule &schedule, const std::shared_ptr<const Rules> &rules,
                                              std::int32_t id) {
    return queryGuard<Day>([&] {
        if (id < MIN_DAY_ID || id > MAX_DAY_ID) {
            throw Failure{ScheduleError::OUT_OF_RANGE};
        }

        const auto limits = queryLimits(*Access::context(schedule));
        Construction construction{limits};

        return ownedDay(schedule, rules, id, construction);
    });
}
} // namespace

Day Access::makeDay(Schedule schedule, std::shared_ptr<const DayModel> model) noexcept {
    return Day(std::move(schedule), std::move(model));
}

Session Access::makeSession(Day day, std::size_t index) noexcept {
    return Session(std::move(day), index);
}

const std::shared_ptr<const DayModel> &Access::model(const Day &day) noexcept {
    return day.model_;
}

const Schedule &Access::schedule(const Day &day) noexcept {
    return day.schedule_;
}

const Day &Access::day(const Session &session) noexcept {
    return session.day_;
}

std::size_t Access::index(const Session &session) noexcept {
    return session.index_;
}
} // namespace detail

bool isTrading(SessionType type) noexcept {
    return type != SessionType::NO_TRADING;
}

bool DayFilter::accept(const Day &day) const noexcept {
    return (weekdays_ == 0 || (weekdays_ & (1U << day.getDayOfWeek())) != 0) &&
           (!holiday_ || *holiday_ == day.isHoliday()) && (!shortDay_ || *shortDay_ == day.isShortDay()) &&
           (!trading_ || *trading_ == day.isTrading());
}

constinit const DayFilter DayFilter::ANY{};
constinit const DayFilter DayFilter::TRADING{0, {}, {}, true};
constinit const DayFilter DayFilter::NON_TRADING{0, {}, {}, false};
constinit const DayFilter DayFilter::HOLIDAY{0, true};
constinit const DayFilter DayFilter::SHORT_DAY{0, {}, true};
constinit const DayFilter DayFilter::MONDAY{1U << 1};
constinit const DayFilter DayFilter::TUESDAY{1U << 2};
constinit const DayFilter DayFilter::WEDNESDAY{1U << 3};
constinit const DayFilter DayFilter::THURSDAY{1U << 4};
constinit const DayFilter DayFilter::FRIDAY{1U << 5};
constinit const DayFilter DayFilter::SATURDAY{1U << 6};
constinit const DayFilter DayFilter::SUNDAY{1U << 7};
constinit const DayFilter DayFilter::WEEK_DAY{0x3e};
constinit const DayFilter DayFilter::WEEK_END{0xc0};

bool SessionFilter::accept(const Session &session) const noexcept {
    return (!type_ || *type_ == session.getType()) && (!trading_ || *trading_ == session.isTrading());
}

constinit const SessionFilter SessionFilter::ANY{};
constinit const SessionFilter SessionFilter::TRADING{{}, true};
constinit const SessionFilter SessionFilter::NON_TRADING{{}, false};
constinit const SessionFilter SessionFilter::NO_TRADING{SessionType::NO_TRADING};
constinit const SessionFilter SessionFilter::PRE_MARKET{SessionType::PRE_MARKET};
constinit const SessionFilter SessionFilter::REGULAR{SessionType::REGULAR};
constinit const SessionFilter SessionFilter::AFTER_MARKET{SessionType::AFTER_MARKET};

std::expected<Day, ScheduleError> Schedule::getDayById(std::int32_t id) const {
    return detail::labelledDay(*this, detail::Access::rules(*this), id);
}

std::expected<Day, ScheduleError> Schedule::getDayByYearMonthDay(std::int32_t ymd) const {
    return detail::queryGuard<Day>([&] {
        if (ymd < detail::MIN_YMD || ymd > detail::MAX_YMD) {
            throw detail::Failure{ScheduleError::OUT_OF_RANGE};
        }

        const auto limits = detail::queryLimits(*detail::Access::context(*this));
        detail::Construction construction{limits};
        const auto rules = detail::Access::rules(*this);
        auto year = ymd / 10000;
        auto month = ymd / 100 % 100;
        auto day = ymd % 100;

        if (day > 31) {
            ++month;
            day = 1;
        } else if (day < 1) {
            day = 1;
        }

        if (month > 12) {
            ++year;
            month = day = 1;
        } else if (month < 1) {
            month = day = 1;
        }

        auto id = *events::getDayIdByYearMonthDay(year, month, day);
        auto result = detail::ownedDay(*this, rules, id, construction);

        while (result.getYearMonthDay() > ymd) {
            id = detail::advanceDay(id, -1);
            result = detail::ownedDay(*this, rules, id, construction);
        }

        while (result.getYearMonthDay() < ymd) {
            id = detail::advanceDay(id, 1);
            result = detail::ownedDay(*this, rules, id, construction);
        }

        return result;
    });
}

std::expected<Day, ScheduleError> Schedule::getDayByTime(std::int64_t time) const {
    return detail::queryGuard<Day>([&] {
        if (time < detail::MIN_QUERY_TIME || time > detail::MAX_QUERY_TIME) {
            throw detail::Failure{ScheduleError::OUT_OF_RANGE};
        }

        const auto limits = detail::queryLimits(*detail::Access::context(*this));
        detail::Construction construction{limits};
        const auto rules = detail::Access::rules(*this);
        auto id = static_cast<std::int32_t>(
            detail::floorDivide(time + rules->rawOffsetSeconds * 1000 - rules->dayOffset, detail::DAY_MILLIS));
        auto result = detail::ownedDay(*this, rules, id, construction);

        while (result.getStartTime() > time) {
            id = detail::advanceDay(id, -1);
            result = detail::ownedDay(*this, rules, id, construction);
        }

        while (result.getEndTime() <= time) {
            id = detail::advanceDay(id, 1);
            result = detail::ownedDay(*this, rules, id, construction);
        }

        // The containing-day contract also applies to degenerate definitions with empty days.
        if (!result.containsTime(time)) {
            throw detail::Failure{ScheduleError::NO_SESSION};
        }

        return result;
    });
}

std::expected<Session, ScheduleError> Schedule::getSessionByTime(std::int64_t time) const {
    auto day = getDayByTime(time);

    if (!day) {
        return std::unexpected(day.error());
    }

    return day->getSessionByTime(time);
}

Day::Day(Schedule schedule, std::shared_ptr<const detail::DayModel> model) noexcept
    : schedule_(std::move(schedule)), model_(std::move(model)) {
}

Day::Day(Day &&other) noexcept : Day(static_cast<const Day &>(other)) {
}

Day &Day::operator=(Day &&other) noexcept {
    return operator=(static_cast<const Day &>(other));
}

Schedule Day::getSchedule() const noexcept {
    return schedule_;
}

std::int64_t Day::getDefaultsVersion() const noexcept {
    return model_->rules->defaults->version;
}

std::int32_t Day::getDayId() const noexcept {
    return model_->dayId;
}

std::int32_t Day::getYearMonthDay() const noexcept {
    return model_->yearMonthDay;
}

std::int32_t Day::getYear() const noexcept {
    return model_->yearMonthDay / 10000;
}

std::int32_t Day::getMonthOfYear() const noexcept {
    return model_->yearMonthDay / 100 % 100;
}

std::int32_t Day::getDayOfMonth() const noexcept {
    return model_->yearMonthDay % 100;
}

std::int32_t Day::getDayOfWeek() const noexcept {
    return static_cast<std::int32_t>(detail::weekday(model_->dayId));
}

bool Day::isHoliday() const noexcept {
    return model_->holiday;
}

bool Day::isShortDay() const noexcept {
    return model_->shortDay;
}

bool Day::isTrading() const noexcept {
    return std::any_of(model_->sessions.begin(), model_->sessions.end(), [](const auto &record) {
        return schedule::isTrading(record.type);
    });
}

std::int64_t Day::getStartTime() const noexcept {
    return model_->sessions.front().start;
}

std::int64_t Day::getEndTime() const noexcept {
    return model_->sessions.back().end;
}

std::int64_t Day::getResetTime() const noexcept {
    return model_->reset;
}

bool Day::containsTime(std::int64_t time) const noexcept {
    return time >= getStartTime() && time < getEndTime();
}

std::vector<Session> Day::getSessions() const {
    std::vector<Session> result;
    result.reserve(model_->sessions.size());

    for (std::size_t i = 0; i < model_->sessions.size(); ++i) {
        result.push_back(detail::Access::makeSession(*this, i));
    }

    return result;
}

std::expected<Session, ScheduleError> Day::getSessionByTime(std::int64_t time) const {
    for (std::size_t i = 0; i < model_->sessions.size(); ++i) {
        const auto &record = model_->sessions[i];

        if (time >= record.start && time < record.end) {
            return detail::Access::makeSession(*this, i);
        }
    }

    return std::unexpected(ScheduleError::NO_SESSION);
}

Session Day::getFirstSession() const noexcept {
    return detail::Access::makeSession(*this, 0);
}

Session Day::getLastSession() const noexcept {
    return detail::Access::makeSession(*this, model_->sessions.size() - 1);
}

std::expected<Day, ScheduleError> Day::getPrevDay() const {
    return detail::labelledDay(schedule_, model_->rules, detail::advanceDay(model_->dayId, -1));
}

std::expected<Day, ScheduleError> Day::getNextDay() const {
    return detail::labelledDay(schedule_, model_->rules, detail::advanceDay(model_->dayId, 1));
}

std::optional<Session> Day::findFirstSession(const SessionFilter &filter) const noexcept {
    for (std::size_t i = 0; i < model_->sessions.size(); ++i) {
        auto session = detail::Access::makeSession(*this, i);

        if (filter.accept(session)) {
            return session;
        }
    }

    return std::nullopt;
}

std::optional<Session> Day::findLastSession(const SessionFilter &filter) const noexcept {
    for (std::size_t i = model_->sessions.size(); i > 0; --i) {
        auto session = detail::Access::makeSession(*this, i - 1);

        if (filter.accept(session)) {
            return session;
        }
    }

    return std::nullopt;
}

std::expected<std::optional<Day>, ScheduleError> Day::findPrevDay(const DayFilter &filter) const {
    return detail::queryGuard<std::optional<Day>>([&]() -> std::optional<Day> {
        const auto limits = detail::queryLimits(*detail::Access::context(schedule_));
        detail::Construction construction{limits};
        auto id = getDayId();

        for (int i = 0; i < detail::FILTER_SEARCH_DAYS; ++i) {
            id = detail::advanceDay(id, -1);

            if (id < detail::MIN_DAY_ID || id > detail::MAX_DAY_ID) {
                throw detail::Failure{ScheduleError::OUT_OF_RANGE};
            }

            construction.inspect(1);
            auto day = detail::ownedDay(schedule_, model_->rules, id, construction);
            construction.inspect(detail::Access::model(day)->sessions.size());

            if (filter.accept(day)) {
                return day;
            }
        }

        return std::nullopt;
    });
}

std::expected<std::optional<Day>, ScheduleError> Day::findNextDay(const DayFilter &filter) const {
    return detail::queryGuard<std::optional<Day>>([&]() -> std::optional<Day> {
        const auto limits = detail::queryLimits(*detail::Access::context(schedule_));
        detail::Construction construction{limits};
        auto id = getDayId();

        for (int i = 0; i < detail::FILTER_SEARCH_DAYS; ++i) {
            id = detail::advanceDay(id, 1);

            if (id < detail::MIN_DAY_ID || id > detail::MAX_DAY_ID) {
                throw detail::Failure{ScheduleError::OUT_OF_RANGE};
            }

            construction.inspect(1);
            auto day = detail::ownedDay(schedule_, model_->rules, id, construction);
            construction.inspect(detail::Access::model(day)->sessions.size());

            if (filter.accept(day)) {
                return day;
            }
        }

        return std::nullopt;
    });
}

std::expected<Session, ScheduleError> Day::getFirstSession(const SessionFilter &filter) const {
    auto result = findFirstSession(filter);

    return result ? std::expected<Session, ScheduleError>(*result) : std::unexpected(ScheduleError::NO_MATCH);
}

std::expected<Session, ScheduleError> Day::getLastSession(const SessionFilter &filter) const {
    auto result = findLastSession(filter);

    return result ? std::expected<Session, ScheduleError>(*result) : std::unexpected(ScheduleError::NO_MATCH);
}

std::expected<Day, ScheduleError> Day::getPrevDay(const DayFilter &filter) const {
    return detail::requireMatch(findPrevDay(filter));
}

std::expected<Day, ScheduleError> Day::getNextDay(const DayFilter &filter) const {
    return detail::requireMatch(findNextDay(filter));
}

bool Day::operator==(const Day &other) const noexcept {
    return schedule_ == other.schedule_ && getDayId() == other.getDayId();
}

Session::Session(Day day, std::size_t index) noexcept : day_(std::move(day)), index_(index) {
}

Day Session::getDay() const noexcept {
    return day_;
}

SessionType Session::getType() const noexcept {
    return detail::Access::model(day_)->sessions[index_].type;
}

bool Session::isTrading() const noexcept {
    return schedule::isTrading(getType());
}

bool Session::isEmpty() const noexcept {
    return getStartTime() >= getEndTime();
}

std::int64_t Session::getStartTime() const noexcept {
    return detail::Access::model(day_)->sessions[index_].start;
}

std::int64_t Session::getEndTime() const noexcept {
    return detail::Access::model(day_)->sessions[index_].end;
}

bool Session::containsTime(std::int64_t time) const noexcept {
    return time >= getStartTime() && time < getEndTime();
}

std::expected<Session, ScheduleError> Session::getPrevSession() const {
    if (index_ != 0) {
        return detail::Access::makeSession(day_, index_ - 1);
    }

    auto day = day_.getPrevDay();

    if (!day) {
        return std::unexpected(day.error());
    }

    return day->getLastSession();
}

std::expected<Session, ScheduleError> Session::getNextSession() const {
    if (index_ + 1 < detail::Access::model(day_)->sessions.size()) {
        return detail::Access::makeSession(day_, index_ + 1);
    }

    auto day = day_.getNextDay();

    if (!day) {
        return std::unexpected(day.error());
    }

    return day->getFirstSession();
}

std::expected<std::optional<Session>, ScheduleError> Session::findPrevSession(const SessionFilter &filter) const {
    return detail::queryGuard<std::optional<Session>>([&]() -> std::optional<Session> {
        const auto schedule = day_.getSchedule();
        const auto limits = detail::queryLimits(*detail::Access::context(schedule));
        detail::Construction construction{limits};

        for (std::size_t i = index_; i > 0; --i) {
            construction.inspect(1);
            auto session = detail::Access::makeSession(day_, i - 1);

            if (filter.accept(session)) {
                return session;
            }
        }

        auto id = day_.getDayId();

        for (int i = 0; i < detail::FILTER_SEARCH_DAYS; ++i) {
            id = detail::advanceDay(id, -1);

            if (id < detail::MIN_DAY_ID || id > detail::MAX_DAY_ID) {
                throw detail::Failure{ScheduleError::OUT_OF_RANGE};
            }

            auto day = detail::ownedDay(schedule, detail::Access::model(day_)->rules, id, construction);
            const auto &records = detail::Access::model(day)->sessions;

            for (std::size_t j = records.size(); j > 0; --j) {
                construction.inspect(1);
                auto session = detail::Access::makeSession(day, j - 1);

                if (filter.accept(session)) {
                    return session;
                }
            }
        }

        return std::nullopt;
    });
}

std::expected<std::optional<Session>, ScheduleError> Session::findNextSession(const SessionFilter &filter) const {
    return detail::queryGuard<std::optional<Session>>([&]() -> std::optional<Session> {
        const auto schedule = day_.getSchedule();
        const auto limits = detail::queryLimits(*detail::Access::context(schedule));
        detail::Construction construction{limits};

        for (std::size_t i = index_ + 1; i < detail::Access::model(day_)->sessions.size(); ++i) {
            construction.inspect(1);
            auto session = detail::Access::makeSession(day_, i);

            if (filter.accept(session)) {
                return session;
            }
        }

        auto id = day_.getDayId();

        for (int i = 0; i < detail::FILTER_SEARCH_DAYS; ++i) {
            id = detail::advanceDay(id, 1);

            if (id < detail::MIN_DAY_ID || id > detail::MAX_DAY_ID) {
                throw detail::Failure{ScheduleError::OUT_OF_RANGE};
            }

            auto day = detail::ownedDay(schedule, detail::Access::model(day_)->rules, id, construction);
            const auto &records = detail::Access::model(day)->sessions;

            for (std::size_t j = 0; j < records.size(); ++j) {
                construction.inspect(1);
                auto session = detail::Access::makeSession(day, j);

                if (filter.accept(session)) {
                    return session;
                }
            }
        }

        return std::nullopt;
    });
}

std::expected<Session, ScheduleError> Session::getPrevSession(const SessionFilter &filter) const {
    return detail::requireMatch(findPrevSession(filter));
}

std::expected<Session, ScheduleError> Session::getNextSession(const SessionFilter &filter) const {
    return detail::requireMatch(findNextSession(filter));
}

bool Session::operator==(const Session &other) const noexcept {
    return day_ == other.day_ && getType() == other.getType() && getStartTime() == other.getStartTime() &&
           getEndTime() == other.getEndTime();
}
} // namespace org::ttldtor::qdcxx::schedule
