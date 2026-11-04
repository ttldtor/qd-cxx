// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/schedule/schedule.hpp"

#include "schedule_private.hpp"

#include <algorithm>
#include <stdexcept>

namespace org::ttldtor::qdcxx::schedule {
namespace detail {
namespace {
/** @param usage Retention statistics.
 * @param kind Distinct model category.
 * @return The corresponding counter; context fixed backing is accounted separately.
 */
std::size_t &counter(ScheduleUsage &usage, RetainedKind kind) noexcept {
    switch (kind) {
    case RetainedKind::GENERATION:
        return usage.generations;
    case RetainedKind::IDENTITY:
        return usage.identities;
    case RetainedKind::RULES:
        return usage.ruleModels;
    case RetainedKind::BUNDLE:
        return usage.bundles;
    case RetainedKind::DAY:
        return usage.days;
    case RetainedKind::SESSION:
        return usage.sessions;
    case RetainedKind::DAY_CACHE:
        return usage.dayCaches;
    }

    std::terminate();
}

/** @param limits Immutable finite domain limits.
 * @param kind Distinct model category.
 * @return The corresponding count bound.
 */
std::size_t capacity(const ScheduleLimits &limits, RetainedKind kind) noexcept {
    switch (kind) {
    case RetainedKind::GENERATION:
        return limits.retainedGenerations;
    case RetainedKind::IDENTITY:
        return limits.retainedSchedules;
    case RetainedKind::RULES:
        return limits.retainedRuleModels;
    case RetainedKind::BUNDLE:
        return limits.retainedBundles;
    case RetainedKind::DAY:
        return limits.retainedDays;
    case RetainedKind::SESSION:
        return limits.retainedSessions;
    case RetainedKind::DAY_CACHE:
        return limits.retainedDayCaches;
    }

    std::terminate();
}

/** @param context Domain containing the current immutable bundle.
 * @param base Expected unchanged bundle.
 * @param candidate Complete replacement.
 * @param removed Inactive identities omitted by the candidate, borrowed from base.
 * @return Whether publication succeeded; all destruction occurs after the lock scope.
 */
bool publish(const std::shared_ptr<Context> &context, const std::shared_ptr<const Bundle> &base,
             const std::shared_ptr<const Bundle> &candidate, std::span<const Identity *const> removed) {
    std::shared_ptr<const Bundle> retired;

    {
        std::lock_guard lock(context->commitMutex);

        if (context->current.load(std::memory_order_acquire) != base) {
            return false;
        }

        for (const auto *identity : removed) {
            const auto it = std::find_if(base->entries.begin(), base->entries.end(), [&](const Entry &entry) {
                return entry.identity.get() == identity;
            });

            if (it == base->entries.end() || it->identity->liveHandles.load(std::memory_order_acquire) != 0) {
                return false;
            }
        }

        retired = context->current.exchange(candidate, std::memory_order_acq_rel);
    }

    return true;
}

/** @param context Shared owning domain.
 * @param defaults Candidate/current calendar generation.
 * @param entries Fully owning entry candidates.
 * @param construction Operation counters.
 * @return Leased immutable publication candidate.
 */
std::shared_ptr<Bundle> makeBundle(const std::shared_ptr<Context> &context, std::shared_ptr<const Defaults> defaults,
                                   std::vector<Entry> entries, Construction &construction) {
    construction.allocate(sizeof(Bundle));
    auto candidate = std::make_shared<Bundle>();
    candidate->defaults = std::move(defaults);
    candidate->entries = std::move(entries);

    if (candidate->entries.size() > (std::numeric_limits<std::size_t>::max() - sizeof(Bundle)) / sizeof(Entry)) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    candidate->lease = acquire(context->ledger, RetainedKind::BUNDLE,
                               sizeof(Bundle) + candidate->entries.size() * sizeof(Entry), construction);

    return candidate;
}

/** @param context Shared owning domain.
 * @param base Captured complete bundle.
 * @param construction Operation counters.
 * @return Current bundle after bounded inactive-cache collection, or unchanged base.
 */
std::shared_ptr<const Bundle> collectInactive(const std::shared_ptr<Context> &context,
                                              const std::shared_ptr<const Bundle> &base, Construction &construction) {
    std::size_t inactive = 0;

    for (const auto &entry : base->entries) {
        construction.inspect(1);
        inactive += entry.identity->liveHandles.load(std::memory_order_acquire) == 0;
    }

    ScheduleUsage usage;

    {
        std::lock_guard lock(context->ledger->mutex);
        usage = context->ledger->usage;
    }

    const auto cacheExcess =
        inactive > context->limits.inactiveCacheTarget ? inactive - context->limits.inactiveCacheTarget : 0;
    const auto needSlot = usage.identities >= context->limits.retainedSchedules ? std::size_t{1} : std::size_t{0};
    auto remove = std::max(cacheExcess, needSlot);

    if (remove == 0 || inactive == 0) {
        return base;
    }

    std::vector<Entry> entries;
    std::vector<const Identity *> removed;

    for (const auto &entry : base->entries) {
        if (remove != 0 && entry.identity->liveHandles.load(std::memory_order_acquire) == 0) {
            construction.allocate(sizeof(const Identity *));
            removed.push_back(entry.identity.get());
            --remove;
        } else {
            construction.allocate(sizeof(Entry));
            entries.push_back(entry);
        }
    }

    auto candidate = makeBundle(context, base->defaults, std::move(entries), construction);

    if (!publish(context, base, candidate, removed)) {
        throw Failure{ScheduleError::CONCURRENT_CHANGE};
    }

    return candidate;
}

/** @tparam Result Public success value.
 * @tparam Function Bounded candidate-producing operation.
 * @param operation Work whose ownership stays local until publication.
 * @return Complete success or a translated semantic/resource/allocation failure.
 */
template <class Result, class Function> std::expected<Result, ScheduleError> guarded(Function &&operation) {
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

/** @param context Owning shared domain.
 * @param text Complete borrowed instrument text.
 * @param venue Optional exact named venue.
 * @return Owning familiar Schedule handle after bounded selection and publication.
 */
std::expected<Schedule, ScheduleError> instance(const std::shared_ptr<Context> &context, std::u16string_view text,
                                                const std::u16string_view *venue) {
    return guarded<Schedule>([&] {
        Construction construction{context->limits};
        const auto selected = selectDefinition(text, venue, construction);
        std::shared_ptr<const Bundle> base;
        std::shared_ptr<const Identity> cached;

        {
            std::lock_guard lock(context->commitMutex);
            base = context->current.load(std::memory_order_acquire);

            for (const auto &entry : base->entries) {
                construction.inspect(selected.size() + entry.identity->definition.size() + 1);

                if (entry.identity->definition == selected) {
                    // Reserve before unlocking so a concurrent commit cannot retire the returned identity.
                    entry.identity->liveHandles.fetch_add(1, std::memory_order_acq_rel);
                    cached = entry.identity;
                    break;
                }
            }
        }

        if (cached) {
            return Access::makeReserved(context, std::move(cached));
        }

        // Cache collection is an independent complete publication with unchanged calendar/live identities.
        // It may discard unused optimization entries even if a later candidate fails.
        auto collected = collectInactive(context, base, construction);
        base = std::move(collected);
        auto rules = parseRules(selected, base->defaults, construction);

        if (context->timeZones) {
            stampTimeZone(*rules, *context, rawReference(*context), construction);
        }

        construction.allocate(sizeof(Identity));
        auto identity = std::make_shared<Identity>();
        identity->definition = construction.copy(selected);
        identity->lease = acquire(context->ledger, RetainedKind::IDENTITY,
                                  sizeof(Identity) + identity->definition.size() * sizeof(char16_t), construction);
        rules->lease = acquire(context->ledger, RetainedKind::RULES, rules->modelBytes, construction);
        std::vector<Entry> entries;

        for (const auto &entry : base->entries) {
            construction.allocate(sizeof(Entry));
            entries.push_back(entry);
        }

        construction.allocate(sizeof(Entry));
        entries.push_back({identity, std::move(rules)});
        auto candidate = makeBundle(context, base->defaults, std::move(entries), construction);
        auto result = Access::make(context, identity);

        if (!publish(context, base, candidate, {})) {
            throw Failure{ScheduleError::CONCURRENT_CHANGE};
        }

        return result;
    });
}
} // namespace

Lease::~Lease() {
    if (ledger != nullptr) {
        std::lock_guard lock(ledger->mutex);
        counter(ledger->usage, kind) -= units;
        ledger->usage.logicalBytes -= bytes;
    }
}

std::shared_ptr<const Lease> acquire(const std::shared_ptr<Ledger> &ledger, RetainedKind kind, std::size_t bytes,
                                     Construction &construction, std::size_t units) {
    // Allocate the uncharged lease first; all quota-failure destruction then occurs after the lock.
    construction.allocate(sizeof(Lease));
    auto lease = std::make_shared<Lease>();

    if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(Lease)) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    bytes += sizeof(Lease);
    bool admitted = false;

    {
        std::lock_guard lock(ledger->mutex);
        auto &count = counter(ledger->usage, kind);

        if (units <= capacity(ledger->limits, kind) - count &&
            bytes <= ledger->limits.retainedBytes - ledger->usage.logicalBytes) {
            count += units;
            ledger->usage.logicalBytes += bytes;
            admitted = true;
        }
    }

    if (!admitted) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    lease->kind = kind;
    lease->bytes = bytes;
    lease->units = units;
    lease->ledger = ledger;

    return lease;
}

const std::shared_ptr<Context> &Access::context(const ScheduleDataContext &context) noexcept {
    return context.state_;
}

const std::shared_ptr<Context> &Access::context(const Schedule &schedule) noexcept {
    return schedule.context_;
}

std::shared_ptr<const Rules> Access::rules(const Schedule &schedule) {
    const auto bundle = schedule.context_->current.load(std::memory_order_acquire);

    for (const auto &entry : bundle->entries) {
        if (entry.identity == schedule.identity_) {
            return entry.rules;
        }
    }

    throw std::logic_error("Live schedule identity is absent from its published generation");
}

Schedule Access::make(std::shared_ptr<Context> context, std::shared_ptr<const Identity> identity) noexcept {
    return Schedule(std::move(context), std::move(identity));
}

Schedule Access::makeReserved(std::shared_ptr<Context> context, std::shared_ptr<const Identity> identity) noexcept {
    return Schedule(std::move(context), std::move(identity), ReservedHandle{});
}
} // namespace detail

ScheduleDataContext::ScheduleDataContext(std::shared_ptr<detail::Context> state) noexcept : state_(std::move(state)) {
}

ScheduleDataContext::ScheduleDataContext(ScheduleDataContext &&other) noexcept : state_(other.state_) {
}

ScheduleDataContext &ScheduleDataContext::operator=(ScheduleDataContext &&other) noexcept {
    state_ = other.state_;

    return *this;
}

std::expected<ScheduleDataContext, ScheduleError> ScheduleDataContext::create(ScheduleLimits limits) {
    return detail::guarded<ScheduleDataContext>([&] {
        detail::Construction construction{limits};
        constexpr auto FIXED_DOMAIN_BYTES = sizeof(detail::Context) + sizeof(detail::Ledger);

        if (limits.retainedBytes < FIXED_DOMAIN_BYTES) {
            throw detail::Failure{ScheduleError::RESOURCE_LIMIT};
        }

        construction.allocate(FIXED_DOMAIN_BYTES);
        auto context = std::make_shared<detail::Context>();
        context->limits = limits;
        context->ledger = std::make_shared<detail::Ledger>();
        context->ledger->limits = limits;
        context->ledger->usage.logicalBytes = FIXED_DOMAIN_BYTES;
        auto defaults = detail::builtinDefaults(construction);
        defaults->lease =
            detail::acquire(context->ledger, detail::RetainedKind::GENERATION, defaults->modelBytes, construction);
        auto bundle = detail::makeBundle(context, std::move(defaults), {}, construction);
        context->current.store(std::move(bundle), std::memory_order_release);

        return ScheduleDataContext(std::move(context));
    });
}

std::expected<ScheduleDataContext, ScheduleError> ScheduleDataContext::create(const ScheduleTimeZoneData &timeZones,
                                                                              ScheduleLimits limits,
                                                                              ScheduleQueryOptions options) {
    auto result = create(limits);

    if (result) {
        result->state_->timeZones = timeZones;
        result->state_->queryOptions = options;
    }

    return result;
}

std::expected<DefaultsUpdate, ScheduleError> ScheduleDataContext::setDefaults(CalendarDefaults defaults) const {
    return detail::guarded<DefaultsUpdate>([&] {
        detail::Construction construction{state_->limits};
        const auto base = state_->current.load(std::memory_order_acquire);
        auto candidateDefaults = detail::parseDefaults(defaults, construction);

        if (candidateDefaults->version < base->defaults->version) {
            return DefaultsUpdate::OLDER_IGNORED;
        }

        candidateDefaults->lease = detail::acquire(state_->ledger, detail::RetainedKind::GENERATION,
                                                   candidateDefaults->modelBytes, construction);
        std::vector<detail::Entry> entries;
        std::vector<const detail::Identity *> removed;
        const auto reference = state_->timeZones ? detail::rawReference(*state_) : 0;

        for (const auto &entry : base->entries) {
            construction.inspect(1);

            if (entry.identity->liveHandles.load(std::memory_order_acquire) == 0) {
                construction.allocate(sizeof(const detail::Identity *));
                removed.push_back(entry.identity.get());
                continue;
            }

            auto rules = detail::parseRules(entry.identity->definition, candidateDefaults, construction);

            if (state_->timeZones) {
                detail::stampTimeZone(*rules, *state_, reference, construction);
            }

            rules->lease =
                detail::acquire(state_->ledger, detail::RetainedKind::RULES, rules->modelBytes, construction);
            construction.allocate(sizeof(detail::Entry));
            entries.push_back({entry.identity, std::move(rules)});
        }

        auto candidate = detail::makeBundle(state_, std::move(candidateDefaults), std::move(entries), construction);

        if (!detail::publish(state_, base, candidate, removed)) {
            throw detail::Failure{ScheduleError::CONCURRENT_CHANGE};
        }

        return DefaultsUpdate::APPLIED;
    });
}

std::int64_t ScheduleDataContext::getDefaultsVersion() const noexcept {
    return state_->current.load(std::memory_order_acquire)->defaults->version;
}

ScheduleUsage ScheduleDataContext::getUsage() const noexcept {
    std::lock_guard lock(state_->ledger->mutex);

    return state_->ledger->usage;
}

Schedule::Schedule(std::shared_ptr<detail::Context> context, std::shared_ptr<const detail::Identity> identity) noexcept
    : context_(std::move(context)), identity_(std::move(identity)) {
    identity_->liveHandles.fetch_add(1, std::memory_order_acq_rel);
}

Schedule::Schedule(std::shared_ptr<detail::Context> context, std::shared_ptr<const detail::Identity> identity,
                   detail::ReservedHandle) noexcept
    : context_(std::move(context)), identity_(std::move(identity)) {
}

Schedule::Schedule(const Schedule &other) noexcept : Schedule(other.context_, other.identity_) {
}

Schedule::Schedule(Schedule &&other) noexcept : Schedule(static_cast<const Schedule &>(other)) {
}

Schedule &Schedule::operator=(const Schedule &other) noexcept {
    if (this != &other) {
        auto nextContext = other.context_;
        auto nextIdentity = other.identity_;
        nextIdentity->liveHandles.fetch_add(1, std::memory_order_acq_rel);
        auto oldContext = std::move(context_);
        auto oldIdentity = std::move(identity_);
        context_ = std::move(nextContext);
        identity_ = std::move(nextIdentity);
        oldIdentity->liveHandles.fetch_sub(1, std::memory_order_acq_rel);
    }

    return *this;
}

Schedule &Schedule::operator=(Schedule &&other) noexcept {
    return operator=(static_cast<const Schedule &>(other));
}

Schedule::~Schedule() {
    identity_->liveHandles.fetch_sub(1, std::memory_order_acq_rel);
}

std::expected<Schedule, ScheduleError> Schedule::getInstance(const ScheduleDataContext &context,
                                                             std::u16string_view instrumentDefinition) {
    return detail::instance(detail::Access::context(context), instrumentDefinition, nullptr);
}

std::expected<Schedule, ScheduleError> Schedule::getInstance(const ScheduleDataContext &context,
                                                             std::u16string_view instrumentDefinition,
                                                             std::u16string_view venue) {
    return detail::instance(detail::Access::context(context), instrumentDefinition, &venue);
}

std::expected<std::vector<std::u16string>, ScheduleError>
Schedule::getTradingVenues(std::u16string_view instrumentDefinition, ScheduleLimits limits) {
    return detail::guarded<std::vector<std::u16string>>([&] {
        detail::Construction construction{limits};

        if (instrumentDefinition.size() > limits.inputUnits) {
            throw detail::Failure{ScheduleError::RESOURCE_LIMIT};
        }

        std::vector<std::u16string> result;

        for (std::size_t search = 0; search < instrumentDefinition.size();) {
            construction.inspect(instrumentDefinition.size());
            construction.inspect(instrumentDefinition.size());
            construction.inspect(instrumentDefinition.size());
            const auto open = instrumentDefinition.find(u'(', search);

            if (open == instrumentDefinition.npos) {
                break;
            }

            const auto next = instrumentDefinition.find_first_of(u"()", open + 1);

            if (next != instrumentDefinition.npos && instrumentDefinition[next] == u')') {
                auto begin = open;

                while (begin != 0) {
                    const auto ch = instrumentDefinition[begin - 1];
                    const bool asciiWord = (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z') ||
                                           (ch >= u'0' && ch <= u'9') || ch == u'_';

                    if (!asciiWord) {
                        break;
                    }

                    --begin;
                }

                construction.allocate(sizeof(std::u16string));
                result.push_back(construction.copy(instrumentDefinition.substr(begin, open - begin)));
                search = next + 1;
            } else {
                search = open + 1;
            }
        }

        return result;
    });
}

std::u16string_view Schedule::getDefinition() const noexcept {
    return identity_->definition;
}

std::u16string Schedule::getName() const {
    return detail::Access::rules(*this)->name;
}

std::u16string Schedule::getTimeZoneId() const {
    return detail::Access::rules(*this)->zone;
}

bool Schedule::operator==(const Schedule &other) const noexcept {
    return context_ == other.context_ && identity_ == other.identity_;
}
} // namespace org::ttldtor::qdcxx::schedule
