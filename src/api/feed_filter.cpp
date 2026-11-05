// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed_filter.hpp"

#include "feed_filter_coordinates.hpp"
#include "feed_filter_expression.hpp"
#include "feed_filter_ipf_config.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {
[[noreturn]] void refuse(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

[[noreturn]] void expressionFailure(const detail::FilterExpressionError &failure) {
    ErrorCode code = ErrorCode::INVALID_ARGUMENT;

    switch (failure.kind) {
    case detail::FilterExpressionError::Kind::RESOURCE_LIMIT:
        code = ErrorCode::RESOURCE_LIMIT;
        break;
    case detail::FilterExpressionError::Kind::WORK_LIMIT:
        code = ErrorCode::WORK_LIMIT;
        break;
    case detail::FilterExpressionError::Kind::DYNAMIC_CONTEXT_REQUIRED:
        code = ErrorCode::INVALID_STATE;
        break;
    case detail::FilterExpressionError::Kind::SYNTAX:
        break;
    }

    refuse(code, "filter-expression");
}

std::size_t add(std::size_t a, std::size_t b) {
    if (a > std::numeric_limits<std::size_t>::max() - b) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-storage");
    }

    return a + b;
}

std::size_t multiply(std::size_t a, std::size_t b) {
    if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-storage");
    }

    return a * b;
}
} // namespace

struct FeedFilter::State {
    std::shared_ptr<void> retention;
    EventRegistry registry;
    DeliveryContract contract;
    FeedFilterLimits limits;
    std::vector<FeedFilterCategory> categories;
    std::vector<detail::FilterExpression> expressions;
    std::size_t storage = 0;
    bool dynamic = false;

    static std::size_t storageFor(FeedFilterLimits limits, const std::vector<FeedFilterCategory> &categories) {
        std::size_t storage = sizeof(State);

        for (const auto &category : categories) {
            storage =
                add(storage, add(sizeof(FeedFilterCategory),
                                 multiply(add(category.name.size(), category.expression.size()), sizeof(char16_t))));
            storage = add(storage, multiply(add(category.expression.size(), 1), 2048));
        }

        if (storage > limits.storageBytes) {
            refuse(ErrorCode::RESOURCE_LIMIT, "filter-storage");
        }

        return storage;
    }

    static std::shared_ptr<const State>
    compile(const EventRegistry &registry, DeliveryContract contract, FeedFilterLimits limits,
            const std::vector<FeedFilterCategory> &categories, std::span<const detail::FilterSymbolResource> resources,
            std::shared_ptr<void> retention = {}, std::size_t *sharedNormalizationWork = nullptr) {
        const auto storage = storageFor(limits, categories);

        auto state =
            std::make_shared<State>(State{std::move(retention), registry, contract, limits, categories, {}, storage});
        state->expressions.reserve(categories.size());

        for (const auto &category : categories) {
            auto compiled = detail::FilterExpression::compile(category.expression,
                                                              {limits.expressionUnits, limits.expressionNodes,
                                                               limits.expressionDepth, limits.patternPositions,
                                                               limits.finiteSymbols, limits.normalizationWork},
                                                              resources, sharedNormalizationWork);
            state->dynamic = state->dynamic || compiled.isDynamic();
            state->expressions.push_back(std::move(compiled));
        }

        return state;
    }

    std::optional<std::u16string_view> category(std::span<const detail::FilterCoordinate> coordinates) const noexcept {
        // Category order is global across delegates, not the first delegate's local match.
        for (std::size_t index = 0; index < categories.size(); ++index) {
            for (const auto &coordinate : coordinates) {
                if (expressions[index].accepts(coordinate.record, coordinate.symbol, coordinate.wildcard)) {
                    return categories[index].name;
                }
            }
        }

        return std::nullopt;
    }
};

namespace {
struct ContextRetention {
    std::atomic<std::size_t> snapshots{0};
    std::atomic<std::size_t> bytes{0};
    std::size_t maximumSnapshots;
    std::size_t maximumBytes;

    ContextRetention(std::size_t snapshots, std::size_t bytes) : maximumSnapshots(snapshots), maximumBytes(bytes) {
    }
};

struct GenerationCharge {
    std::shared_ptr<ContextRetention> ledger;
    std::size_t bytes;

    GenerationCharge(std::shared_ptr<ContextRetention> ledger, std::size_t bytes)
        : ledger(std::move(ledger)), bytes(bytes) {
    }

    ~GenerationCharge() {
        ledger->bytes.fetch_sub(bytes);
        ledger->snapshots.fetch_sub(1);
    }
};

std::shared_ptr<void> reserveGeneration(const std::shared_ptr<ContextRetention> &ledger, std::size_t bytes) {
    const auto count = ledger->snapshots.fetch_add(1);

    if (count >= ledger->maximumSnapshots) {
        ledger->snapshots.fetch_sub(1);
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-live-snapshots");
    }

    const auto previous = ledger->bytes.fetch_add(bytes);

    if (bytes > ledger->maximumBytes || previous > ledger->maximumBytes - bytes) {
        ledger->bytes.fetch_sub(bytes);
        ledger->snapshots.fetch_sub(1);
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-live-storage");
    }

    try {
        return std::make_shared<GenerationCharge>(ledger, bytes);
    } catch (...) {
        ledger->bytes.fetch_sub(bytes);
        ledger->snapshots.fetch_sub(1);
        throw;
    }
}

struct ResourceGeneration {
    std::shared_ptr<void> retention;
    std::vector<std::u16string> symbols;
    std::vector<std::u16string> tradingHours;
};
} // namespace

struct FeedFilterTrackerState {
    struct ListenerReservation {
        std::shared_ptr<std::atomic<std::size_t>> live;
        bool charged = false;

        explicit ListenerReservation(std::shared_ptr<std::atomic<std::size_t>> live) : live(std::move(live)) {
        }

        ~ListenerReservation() {
            if (charged) {
                live->fetch_sub(1);
            }
        }
    };

    struct ListenerNode {
        // Declared before callback so its charge survives the last user capture destructor.
        ListenerReservation reservation;
        std::uint64_t token;
        FeedFilterTracker::Listener callback;

        ListenerNode(std::uint64_t token, FeedFilterTracker::Listener callback,
                     std::shared_ptr<std::atomic<std::size_t>> live)
            : reservation(std::move(live)), token(token), callback(std::move(callback)) {
        }
    };

    mutable std::mutex mutex;
    std::shared_ptr<const FeedFilter::State> current;
    std::shared_ptr<const FeedFilter::State> latest;
    std::vector<std::shared_ptr<ListenerNode>> listeners;
    std::vector<std::shared_ptr<ListenerNode>> dispatchSelection;
    std::shared_ptr<std::atomic<std::size_t>> liveListeners = std::make_shared<std::atomic<std::size_t>>(0);
    std::shared_ptr<FeedFilterContext> context;
    std::size_t maximumListeners = 0;
    std::uint64_t nextToken = 1;
    std::atomic<std::uint64_t> failures{0};
    bool dispatching = false;
    bool pending = false;

    void select(std::shared_ptr<const FeedFilter::State> candidate) {
        std::shared_ptr<const FeedFilter::State> retiredLatest;
        std::shared_ptr<const FeedFilter::State> retiredCurrent;
        {
            std::lock_guard lock(mutex);
            retiredLatest = std::move(latest);
            latest = std::move(candidate);

            if (!listeners.empty()) {
                retiredCurrent = std::move(current);
                current = latest;
                pending = true;
            }
        }
    }

    void dispatch(const std::shared_ptr<FeedFilterTrackerState> &self) {
        auto &selected = dispatchSelection;
        {
            std::lock_guard lock(mutex);

            if (dispatching || !pending) {
                return;
            }

            // Pre-reserved at build time: selection performs no callback-node allocation.
            dispatching = true;
        }

        for (;;) {
            {
                std::lock_guard lock(mutex);

                if (!pending || listeners.empty()) {
                    pending = false;
                    dispatching = false;

                    return;
                }

                pending = false;
                selected.assign(listeners.begin(), listeners.end());
            }

            for (const auto &node : selected) {
                try {
                    node->callback(FeedFilterTracker(self));
                } catch (...) {
                    auto previous = failures.load();

                    while (previous != std::numeric_limits<std::uint64_t>::max() &&
                           !failures.compare_exchange_weak(previous, previous + 1)) {
                    }
                }
            }

            selected.clear();
        }
    }
};

struct FeedFilterContext::Impl {
    FeedFilterContextLimits limits;
    std::mutex updateMutex;
    std::mutex mutex;
    std::vector<detail::FilterSymbolResource> resources;
    std::vector<std::weak_ptr<FeedFilterTrackerState>> trackers;
    std::shared_ptr<ContextRetention> retention;
    std::size_t remainingWork;
    std::mutex activityMutex;
    std::condition_variable_any activity;
    std::size_t activityRevision = 0;
    std::function<void(std::span<const std::u16string>)> resolver;

    explicit Impl(FeedFilterContextLimits limits)
        : limits(limits), retention(std::make_shared<ContextRetention>(limits.snapshots, limits.storageBytes)),
          remainingWork(limits.updateWork) {
    }
};

void FeedFilterContext::resolveResources(std::span<const FeedFilterCategory> categories,
                                         const FeedFilterLimits &limits) {
    std::vector<std::u16string> referenced;

    for (const auto &category : categories) {
        auto expressions = detail::FilterExpression::referencedResources(
            category.expression, {limits.expressionUnits, limits.expressionNodes, limits.expressionDepth,
                                  limits.patternPositions, limits.finiteSymbols, limits.normalizationWork});

        for (auto &expression : expressions) {
            if (std::ranges::find(referenced, expression) == referenced.end()) {
                referenced.push_back(std::move(expression));
            }
        }
    }

    std::function<void(std::span<const std::u16string>)> resolver;
    {
        std::lock_guard lock(impl_->mutex);
        std::erase_if(referenced, [&](const auto &expression) {
            return std::ranges::find(impl_->resources, expression, &detail::FilterSymbolResource::expression) !=
                   impl_->resources.end();
        });
        resolver = impl_->resolver;
    }

    if (!referenced.empty() && resolver) {
        resolver(referenced);
    }
}

void FeedFilterContext::setResourceResolver(std::function<void(std::span<const std::u16string>)> resolver) {
    std::function<void(std::span<const std::u16string>)> retired;
    {
        std::lock_guard lock(impl_->mutex);
        retired = std::move(impl_->resolver);
        impl_->resolver = std::move(resolver);
    }
}

bool FeedFilterContext::waitForTracking(std::u16string_view expression, std::chrono::steady_clock::time_point deadline,
                                        std::stop_token stop) const {
    auto impl = impl_;

    for (;;) {
        std::size_t revision;
        {
            std::lock_guard lock(impl->activityMutex);
            revision = impl->activityRevision;
        }

        if (stop.stop_requested()) {
            return false;
        }

        // Strong tracker leases and their arbitrary capture destructors retire outside every context lock.
        std::vector<std::shared_ptr<FeedFilterTrackerState>> trackers;
        trackers.reserve(impl->limits.trackers);
        {
            std::lock_guard lock(impl->mutex);

            for (const auto &entry : impl->trackers) {
                if (auto tracker = entry.lock()) {
                    trackers.push_back(std::move(tracker));
                }
            }
        }
        bool tracked = false;

        for (const auto &tracker : trackers) {
            std::lock_guard selected(tracker->mutex);
            tracked = tracked || (!tracker->listeners.empty() &&
                                  std::ranges::any_of(tracker->latest->expressions, [&](const auto &value) {
                                      return value.usesResource(expression);
                                  }));
        }

        trackers.clear();

        if (tracked) {
            return true;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::unique_lock lock(impl->activityMutex);

        if (!impl->activity.wait_until(lock, stop, deadline, [&] {
                return impl->activityRevision != revision;
            })) {
            return false;
        }
    }
}

void FeedFilterContext::notifyTracking() noexcept {
    {
        std::lock_guard lock(impl_->activityMutex);
        ++impl_->activityRevision;
    }
    impl_->activity.notify_all();
}

FeedFilterContext::FeedFilterContext(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

std::shared_ptr<FeedFilterContext> FeedFilterContext::create(FeedFilterContextLimits limits) {
    try {
        return std::shared_ptr<FeedFilterContext>(new FeedFilterContext(std::make_shared<Impl>(limits)));
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-context-allocation");
    }
}

void FeedFilterContext::replaceSymbols(std::u16string expression, std::span<const std::u16string> symbols,
                                       bool dynamic) {
    replaceResourceSymbols(std::move(expression), symbols, std::nullopt, dynamic);
}

void FeedFilterContext::replaceIpfSymbols(std::u16string expression, std::span<const std::u16string> symbols,
                                          std::span<const std::u16string> tradingHours, bool dynamic) {
    replaceResourceSymbols(std::move(expression), symbols, tradingHours, dynamic);
}

void FeedFilterContext::replaceResourceSymbols(std::u16string expression, std::span<const std::u16string> symbols,
                                               std::optional<std::span<const std::u16string>> suppliedHours,
                                               bool dynamic) {
    if (!expression.starts_with(u"ipf[") || !expression.ends_with(u']')) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-resource-expression");
    }

    const auto config = detail::parseFilterIpfConfig(expression, impl_->limits.expressionUnits);

    if (dynamic != (config.update.count() > 0)) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-resource-dynamic-setting");
    }

    auto impl = impl_;
    std::vector<std::shared_ptr<FeedFilterTrackerState>> selected;
    std::vector<std::shared_ptr<const FeedFilter::State>> candidates;
    std::vector<detail::FilterSymbolResource> retired;
    try {
        std::unique_lock updates(impl->updateMutex);
        const auto &limits = impl->limits;

        std::vector<detail::FilterSymbolResource> resources;
        selected.reserve(limits.trackers);
        {
            std::lock_guard lock(impl->mutex);
            resources = impl->resources;
            std::erase_if(impl->trackers, [](const auto &value) {
                return value.expired();
            });

            for (const auto &value : impl->trackers) {
                if (auto tracker = value.lock()) {
                    selected.push_back(std::move(tracker));
                }
            }
        }
        const auto found = std::ranges::find(resources, expression, &detail::FilterSymbolResource::expression);

        const auto hours = suppliedHours ? *suppliedHours
                                         : (found != resources.end() && found->comparisonMetadata
                                                ? std::span<const std::u16string>(*found->comparisonMetadata)
                                                : std::span<const std::u16string>{});

        if (hours.size() > limits.symbols) {
            refuse(ErrorCode::RESOURCE_LIMIT, "filter-resource-metadata-count");
        }

        if (symbols.size() > limits.symbols) {
            refuse(ErrorCode::RESOURCE_LIMIT, "filter-resource-symbols");
        }

        std::size_t units = 0;

        for (const auto &symbol : symbols) {
            if (symbol.size() > limits.symbolUnits) {
                refuse(ErrorCode::RESOURCE_LIMIT, "filter-resource-symbol-units");
            }

            units = add(units, symbol.size());
        }

        for (const auto &description : hours) {
            units = add(units, description.size());
        }

        if (units > limits.totalSymbolUnits) {
            refuse(ErrorCode::RESOURCE_LIMIT, "filter-resource-total-units");
        }

        const auto elements = add(symbols.size(), hours.size());
        const auto controlWork = add(expression.size(), multiply(limits.resources, add(limits.expressionUnits, 1)));
        const auto work = add(add(1, controlWork), multiply(add(units, elements), 4 * (std::bit_width(elements) + 1)));

        if (work > impl->remainingWork) {
            refuse(ErrorCode::WORK_LIMIT, "filter-resource-work");
        }

        impl->remainingWork -= work;
        const auto bytes =
            add(add(512, multiply(expression.size(), 4 * sizeof(char16_t))),
                add(multiply(elements, 2 * sizeof(std::u16string)), multiply(units, 4 * sizeof(char16_t))));
        auto generation = std::make_shared<ResourceGeneration>();
        generation->retention = reserveGeneration(impl->retention, bytes);
        generation->symbols.assign(symbols.begin(), symbols.end());
        std::ranges::sort(generation->symbols);
        generation->symbols.erase(std::unique(generation->symbols.begin(), generation->symbols.end()),
                                  generation->symbols.end());
        generation->tradingHours.assign(hours.begin(), hours.end());
        std::ranges::sort(generation->tradingHours);
        generation->tradingHours.erase(std::unique(generation->tradingHours.begin(), generation->tradingHours.end()),
                                       generation->tradingHours.end());
        auto ownedSymbols = std::shared_ptr<const std::vector<std::u16string>>(generation, &generation->symbols);
        auto ownedHours = std::shared_ptr<const std::vector<std::u16string>>(generation, &generation->tradingHours);

        if (found != resources.end()) {
            if (found->dynamic != dynamic) {
                refuse(ErrorCode::INVALID_ARGUMENT, "filter-resource-dynamic-change");
            }

            if (*found->symbols == generation->symbols &&
                (found->comparisonMetadata ? *found->comparisonMetadata == generation->tradingHours
                                           : generation->tradingHours.empty())) {
                return;
            }

            found->symbols = std::move(ownedSymbols);
            found->comparisonMetadata = std::move(ownedHours);
        } else {
            if (resources.size() == limits.resources) {
                refuse(ErrorCode::RESOURCE_LIMIT, "filter-resource-count");
            }

            resources.push_back({std::move(expression), std::move(ownedSymbols), dynamic, std::move(ownedHours)});
        }

        candidates.reserve(selected.size());

        for (const auto &tracker : selected) {
            std::shared_ptr<const FeedFilter::State> base;
            {
                std::lock_guard lock(tracker->mutex);
                base = tracker->latest;
            }

            if (base->storage > impl->remainingWork) {
                refuse(ErrorCode::WORK_LIMIT, "filter-resource-rebuild-work");
            }

            impl->remainingWork -= base->storage;
            candidates.push_back(
                FeedFilter::State::compile(base->registry, base->contract, base->limits, base->categories, resources,
                                           reserveGeneration(impl->retention, base->storage), &impl->remainingWork));
        }

        {
            std::lock_guard lock(impl->mutex);
            retired = std::move(impl->resources);
            impl->resources = std::move(resources);
        }

        for (std::size_t i = 0; i < selected.size(); ++i) {
            selected[i]->select(std::move(candidates[i]));
        }

        updates.unlock();

        for (const auto &tracker : selected) {
            tracker->dispatch(tracker);
        }
    } catch (const detail::FilterExpressionError &failure) {
        expressionFailure(failure);
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-resource-allocation");
    }
}

FeedFilterTracker::FeedFilterTracker(std::shared_ptr<FeedFilterTrackerState> state) noexcept
    : state_(std::move(state)) {
}

FeedFilter FeedFilterTracker::getCurrentFilter() const {
    std::lock_guard lock(state_->mutex);

    return FeedFilter(state_->current, state_);
}

std::uint64_t FeedFilterTracker::addListener(Listener listener) {
    auto state = state_;
    {
        std::lock_guard lock(state->mutex);

        if (!state->latest->dynamic) {
            return 0;
        }
    }

    if (!listener) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-tracker-listener");
    }

    std::shared_ptr<FeedFilterTrackerState::ListenerNode> node;
    try {
        // Capture construction/retirement occurs before acquiring the selection lock.
        node = std::make_shared<FeedFilterTrackerState::ListenerNode>(0, std::move(listener), state->liveListeners);
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-tracker-listener-allocation");
    }
    std::shared_ptr<const FeedFilter::State> retired;
    std::unique_lock lock(state->mutex);

    if (!state->latest->dynamic) {
        return 0;
    }

    if (state->liveListeners->load() >= state->maximumListeners ||
        state->nextToken == std::numeric_limits<std::uint64_t>::max()) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-tracker-listeners");
    }

    const auto token = state->nextToken;
    node->token = token;
    state->liveListeners->fetch_add(1);
    node->reservation.charged = true;
    state->listeners.push_back(std::move(node));
    ++state->nextToken;
    retired = std::move(state->current);
    state->current = state->latest;
    lock.unlock();

    if (state->context) {
        state->context->notifyTracking();
    }

    return token;
}

bool FeedFilterTracker::removeListener(std::uint64_t token) {
    std::shared_ptr<FeedFilterTrackerState::ListenerNode> retired;
    {
        std::lock_guard lock(state_->mutex);
        const auto found = std::ranges::find(state_->listeners, token, &FeedFilterTrackerState::ListenerNode::token);

        if (found == state_->listeners.end()) {
            return false;
        }

        retired = std::move(*found);
        state_->listeners.erase(found);
    }

    if (state_->context) {
        state_->context->notifyTracking();
    }

    return true;
}

std::uint64_t FeedFilterTracker::callbackFailures() const noexcept {
    return state_->failures.load();
}

FeedFilter::FeedFilter(std::shared_ptr<const State> state, std::shared_ptr<FeedFilterTrackerState> tracker) noexcept
    : state_(std::move(state)), tracker_(std::move(tracker)) {
}

bool FeedFilter::accept(EventType type, const SubscriptionSelector &symbol) const {
    return getCategory(type, symbol).has_value();
}

bool FeedFilter::acceptEvent(const RegisteredEvent &event) const {
    return getEventCategory(event).has_value();
}

std::optional<std::u16string_view> FeedFilter::getCategory(EventType type, const SubscriptionSelector &symbol) const {
    try {
        return state_->category(
            detail::filterSubscriptionCoordinates(state_->registry, state_->contract, type, symbol, state_->limits));
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-query-allocation");
    }
}

std::optional<std::u16string_view> FeedFilter::getEventCategory(const RegisteredEvent &event) const {
    try {
        return state_->category(
            detail::filterEventCoordinates(state_->registry, state_->contract, event, state_->limits));
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-query-allocation");
    }
}

std::span<const FeedFilterCategory> FeedFilter::getCategories() const noexcept {
    return state_->categories;
}

bool FeedFilter::isDynamic() const noexcept {
    return state_->dynamic;
}

FeedFilter::Builder::Builder(EventRegistry registry, DeliveryContract contract, FeedFilterLimits limits)
    : registry_(std::move(registry)), contract_(contract), limits_(limits) {
    if (!registry_.valid() || (contract != DeliveryContract::TICKER && contract != DeliveryContract::STREAM &&
                               contract != DeliveryContract::HISTORY)) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-builder");
    }
}

FeedFilter::Builder &FeedFilter::Builder::withCategory(std::u16string name, std::u16string expression) {
    if (name.empty() || expression.empty() || std::ranges::any_of(categories_, [&](const auto &category) {
            return category.name == name;
        })) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-category");
    }

    if (categories_.size() == limits_.categories || name.size() > limits_.categoryNameUnits ||
        expression.size() > limits_.expressionUnits) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-category-limit");
    }

    try {
        categories_.push_back({std::move(name), std::move(expression)});
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-category-allocation");
    }

    return *this;
}

FeedFilterTracker FeedFilter::getTracker() const {
    return FeedFilterTracker(tracker_);
}

FeedFilter::Builder &FeedFilter::Builder::withResourceContext(std::shared_ptr<FeedFilterContext> context) {
    context_ = std::move(context);

    return *this;
}

FeedFilter FeedFilter::Builder::build() const {
    if (categories_.empty()) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-empty-categories");
    }

    try {
        if (context_) {
            context_->resolveResources(categories_, limits_);
        }

        auto tracker = std::make_shared<FeedFilterTrackerState>();
        std::unique_lock<std::mutex> updates;
        std::vector<detail::FilterSymbolResource> resources;

        if (context_) {
            updates = std::unique_lock(context_->impl_->updateMutex);
            std::lock_guard lock(context_->impl_->mutex);
            resources = context_->impl_->resources;
        }

        auto retention = context_
                             ? reserveGeneration(context_->impl_->retention, State::storageFor(limits_, categories_))
                             : std::shared_ptr<void>{};
        auto state = State::compile(registry_, contract_, limits_, categories_, resources, std::move(retention),
                                    context_ ? &context_->impl_->remainingWork : nullptr);
        tracker->current = state;
        tracker->latest = state;

        if (context_) {
            tracker->context = context_;
            tracker->maximumListeners = context_->impl_->limits.listeners;
            tracker->listeners.reserve(tracker->maximumListeners);
            tracker->dispatchSelection.reserve(tracker->maximumListeners);
            std::lock_guard lock(context_->impl_->mutex);
            std::erase_if(context_->impl_->trackers, [](const auto &value) {
                return value.expired();
            });

            if (context_->impl_->trackers.size() == context_->impl_->limits.trackers) {
                refuse(ErrorCode::RESOURCE_LIMIT, "filter-context-trackers");
            }

            context_->impl_->trackers.push_back(tracker);
        }

        return FeedFilter(std::move(state), std::move(tracker));
    } catch (const detail::FilterExpressionError &failure) {
        expressionFailure(failure);
    } catch (const std::length_error &) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-build-capacity");
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "filter-build-allocation");
    }
}

std::span<const FeedFilterCategory> FeedFilter::Builder::getCategories() const noexcept {
    return categories_;
}
} // namespace org::ttldtor::qdcxx::api
