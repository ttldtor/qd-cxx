// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {
struct IpfTransferOptions;
struct IpfUrlRequest;
} // namespace org::ttldtor::qdcxx::ipf

namespace org::ttldtor::qdcxx::api {
class FeedFilterContext;
class FeedFilterTracker;

/** One owned named classification rule; order determines the first matching category. */
struct FeedFilterCategory {
    std::u16string name;       ///< Nonempty exact category identity, without normalization.
    std::u16string expression; ///< Nonempty QD expression, copied into the immutable filter.
};

/** Finite construction/query bounds; zero is an actual bound, never an unlimited sentinel. */
struct FeedFilterLimits {
    std::size_t categories = 32;         ///< Maximum ordered category count.
    std::size_t categoryNameUnits = 128; ///< Maximum UTF-16 units per category name.
    std::size_t expressionUnits = 4096;  ///< Maximum UTF-16 units per expression.
    std::size_t expressionNodes = 1024;  ///< Maximum compiled nodes per expression.
    std::size_t expressionDepth = 64;    ///< Maximum recursive expression/compiled-tree depth.
    std::size_t patternPositions = 4096; ///< Maximum compiled character positions per pattern.
    std::size_t finiteSymbols = 100000;  ///< Maximum finite candidate emissions during one expression normalization.
    std::size_t normalizationWork = 100000000; ///< Construction-only finite enumeration and predicate credits.
    std::size_t symbolUnits = 4096;            ///< Maximum UTF-16 units per query symbol.
    std::size_t coordinates = 64;              ///< Maximum selected registered routes in one query.
    std::size_t storageBytes =
        16 * 1024 * 1024; ///< Conservative complete immutable generation backing, not builder inputs or RSS.
};

/** Immutable ordered query classifier, independent of subscriptions and native runtime ownership.
 * Copies share owned immutable state. Concurrent const queries are supported; caller input must remain
 * alive and unmodified during a call. Category views remain valid while this value or an owning copy lives.
 * A query never opens a source, changes subscription demand or advances network work.
 */
class FeedFilter final {
    struct State;
    std::shared_ptr<const State> state_;
    std::shared_ptr<struct FeedFilterTrackerState> tracker_;
    friend class FeedFilterContext;
    friend class FeedFilterTracker;
    friend struct FeedFilterTrackerState;
    explicit FeedFilter(std::shared_ptr<const State> state,
                        std::shared_ptr<FeedFilterTrackerState> tracker = {}) noexcept;

    public:
    class Builder;
    /** @param type Existing registered business projection.
     * @param symbol Explicit plain/indexed/time-series/ALL query, borrowed only during this call.
     * @return True if any selected subscription delegate matches any category.
     * @throws Error for an invalid enum or a finite query capacity refusal.
     */
    [[nodiscard]] bool accept(EventType type, const SubscriptionSelector &symbol) const;
    /** @param event Existing owning registered event, borrowed only during this call.
     * @return True if any publisher delegate matches any category; uses data routing, not subscription routing.
     * @throws Error if a finite query capacity is exceeded.
     */
    [[nodiscard]] bool acceptEvent(const RegisteredEvent &event) const;
    /** @param type Existing registered business projection.
     * @param symbol Explicit typed subscription query, borrowed only during this call.
     * @return First category matching through any selected delegate, or nullopt if none match.
     * @throws Error for invalid enum or a finite query capacity refusal.
     */
    [[nodiscard]] std::optional<std::u16string_view> getCategory(EventType type,
                                                                 const SubscriptionSelector &symbol) const;
    /** @param event Existing owning registered event, borrowed only during this call.
     * @return First category matching through any selected publisher delegate, or nullopt.
     * @throws Error if a finite query capacity is exceeded.
     */
    [[nodiscard]] std::optional<std::u16string_view> getEventCategory(const RegisteredEvent &event) const;
    /** @return Ordered owned configuration, borrowed while this value or a copy lives. */
    [[nodiscard]] std::span<const FeedFilterCategory> getCategories() const noexcept;
    /** @return Whether any category is backed by an explicitly configured changing resource. */
    [[nodiscard]] bool isDynamic() const noexcept;
    /** @return Stable owning tracker; static filters retain a tracker with inert listener admission. */
    [[nodiscard]] FeedFilterTracker getTracker() const;
};

/** Single-threaded configuration builder; query state remains independent of subscriptions/native owners.
 * An explicitly bound resource context may resolve missing sources during build under its finite bootstrap policy.
 */
class FeedFilter::Builder final {
    EventRegistry registry_;
    DeliveryContract contract_;
    FeedFilterLimits limits_;
    std::vector<FeedFilterCategory> categories_;
    std::shared_ptr<FeedFilterContext> context_;

    public:
    /** @param registry Independently owned immutable validated routes; moved-from registries are invalid.
     * @param contract Explicit existing endpoint delivery contract.
     * @param limits Finite construction/query bounds, retained independently.
     * @throws Error for invalid registry or delivery contract.
     */
    explicit Builder(EventRegistry registry, DeliveryContract contract = DeliveryContract::TICKER,
                     FeedFilterLimits limits = {});
    /** @param name Nonempty exact category identity, copied/transferred without normalization.
     * @param expression Nonempty QD expression, parsed at build time.
     * @return This builder; duplicate names or finite bounds throw Error before configuration changes.
     */
    Builder &withCategory(std::u16string name, std::u16string expression);
    /** @param context Explicit owning resource repository; binding starts no I/O or callbacks.
     * @return This builder, retaining an independent owning reference.
     */
    Builder &withResourceContext(std::shared_ptr<FeedFilterContext> context);
    /** Build an independent immutable classifier. A configured resource resolver may perform finite initial I/O.
     * @return Independent classifier; no caller-owned string or registry storage is borrowed.
     * @throws Error for empty configuration, invalid grammar or finite bounds; allocation failure is detached.
     */
    [[nodiscard]] FeedFilter build() const;
    /** @return Borrowed ordered configuration, invalidated by successful mutation or builder destruction. */
    [[nodiscard]] std::span<const FeedFilterCategory> getCategories() const noexcept;
};

/** Explicit finite resource/tracker ownership bounds shared by a context and retained old snapshots. */
struct FeedFilterContextLimits {
    std::size_t expressionUnits = 8192; ///< Maximum complete named resource token units.
    std::size_t resources = 8;          ///< Maximum named immutable symbol resources.
    std::size_t symbols = 100000;       ///< Maximum input symbol count in one replacement, including duplicates.
    std::size_t symbolUnits = 4096;     ///< Maximum UTF-16 units in one symbol.
    std::size_t totalSymbolUnits = 8 * 1024 * 1024; ///< Maximum symbol and source-metadata units in one replacement.
    std::size_t snapshots = 64; ///< Live resource/filter generations, including externally retained old values.
    std::size_t storageBytes = 64 * 1024 * 1024; ///< Conservative shared live-generation backing, excluding captures.
    std::size_t trackers = 32;                   ///< Simultaneous live trackers registered with this context.
    std::size_t listeners = 16; ///< Registered, selected and capture-retiring listener nodes per tracker.
    std::size_t updateWork = 1024 * 1024 * 1024; ///< Lifetime conservative symbol-copy/comparison and rebuild units.
};

/** Explicit thread-safe symbol-resource owner; it performs no I/O or native bootstrap.
 * Resource names are exact IPF expression tokens. An owning source service supplies complete replacements.
 * Every replacement is prepared atomically for all registered classifiers before resource publication.
 * Existing immutable filters remain valid. Callbacks and capture retirement occur outside context locks.
 */
class FeedFilterContext final {
    struct Impl;
    std::shared_ptr<Impl> impl_;
    explicit FeedFilterContext(std::shared_ptr<Impl> impl) noexcept;
    friend class FeedFilter;
    friend class FeedFilter::Builder;
    friend class FeedFilterTracker;
    friend class FeedFilterIpfRuntime;
    void resolveResources(std::span<const FeedFilterCategory> categories, const FeedFilterLimits &limits);
    void setResourceResolver(std::function<void(std::span<const std::u16string>)> resolver);
    [[nodiscard]] bool waitForTracking(std::u16string_view expression, std::chrono::steady_clock::time_point deadline,
                                       std::stop_token stop) const;
    void notifyTracking() noexcept;
    void replaceIpfSymbols(std::u16string expression, std::span<const std::u16string> symbols,
                           std::span<const std::u16string> tradingHours, bool dynamic);
    void replaceResourceSymbols(std::u16string expression, std::span<const std::u16string> symbols,
                                std::optional<std::span<const std::u16string>> tradingHours, bool dynamic);
    friend struct FeedFilterTrackerState;

    public:
    /** @param limits Explicit finite retained generations, source values, trackers and callback bounds.
     * @return Independent owning context; no resource is initially configured and no I/O starts.
     */
    [[nodiscard]] static std::shared_ptr<FeedFilterContext> create(FeedFilterContextLimits limits = {});
    /** Configure or atomically replace one exact symbol set; equal content preserves snapshot identity.
     * @param expression Exact complete `ipf[...]` token identifying explicit source configuration.
     * @param symbols Borrowed complete snapshot, copied before publication; attributes are matched by base prefix.
     * @param dynamic Whether future replacements are expected; this property is immutable for an existing resource.
     * @throws Error on invalid input, finite refusal or allocation failure; no snapshot is partly published.
     */
    void replaceSymbols(std::u16string expression, std::span<const std::u16string> symbols, bool dynamic);
};

/** Stable tracker handle independent of the immutable filter generations it observes.
 * Tracking is lazy. Without listeners, getCurrentFilter may retain the previous version; first admission
 * catches up without an initial callback. A removed listener may finish one already selected invocation.
 * Reentrant replacements coalesce; callbacks never nest and callback exceptions are contained/countable.
 * Dispatch is serialized on the publishing dispatcher thread; selected capture destruction runs outside locks.
 */
class FeedFilterTracker final {
    std::shared_ptr<FeedFilterTrackerState> state_;
    explicit FeedFilterTracker(std::shared_ptr<FeedFilterTrackerState> state) noexcept;
    friend class FeedFilter;
    friend struct FeedFilterTrackerState;

    public:
    using Listener = std::function<void(FeedFilterTracker)>; ///< Owning advisory callback with stable tracker.
    /** @return Immutable current generation; obtaining it starts no source/network work. */
    [[nodiscard]] FeedFilter getCurrentFilter() const;
    /** @param listener Nonempty owned callback; captures retire outside tracker/context locks.
     * @return Unique nonzero token, or zero for a static filter; finite refusal throws Error.
     */
    [[nodiscard]] std::uint64_t addListener(Listener listener);
    /** @param token Previously admitted token; unknown/zero tokens are harmless.
     * @return Whether an active registration was removed; selected callbacks may finish once.
     */
    [[nodiscard]] bool removeListener(std::uint64_t token);
    /** @return Saturating count of contained advisory callback exceptions. */
    [[nodiscard]] std::uint64_t callbackFailures() const noexcept;
};

/** Explicit owning-source limits, separate from the existing native/HTTP/parser grants. */
struct FeedFilterIpfLimits {
    std::chrono::milliseconds schedulerInterval{10};  ///< Internal listener-activation cadence; strictly positive.
    std::size_t schedulerTurns = 10000000;            ///< Lifetime worker scheduling turns, including inactive checks.
    std::size_t sources = 8;                          ///< Maximum lifetime source registrations in one owning session.
    std::chrono::milliseconds initialTimeout{300000}; ///< Finite synchronous bootstrap wait used by builder resolution.
    std::size_t attempts = 64;            ///< Session-wide full-source attempts, including failed/unchanged checks.
    std::size_t expressionUnits = 8192;   ///< Complete configured source token including properties.
    std::size_t snapshotSymbols = 100000; ///< Complete staged profiles per attempt before atomic replacement.
    std::size_t snapshotUnits = 8 * 1024 * 1024; ///< Combined staged symbol/schedule UTF-16 units.
    std::size_t copyWork = 64 * 1024 * 1024;     ///< Session-wide staged symbol/schedule copy units and profile visits.
    std::chrono::milliseconds lifetime{86400000}; ///< Cooperative owner lifetime, never an unlimited sentinel.
};

/** Explicit wall-clock and schedule capability resolved at the owning source boundary. */
struct FeedFilterIpfSchedule {
    std::function<std::int64_t()> currentTimeMillis; ///< Explicit Unix-millisecond clock, required with schedule.
    std::function<bool(std::u16string_view, std::int64_t)> isTrading;
    ///< Existing schedule-query adapter; true suppresses an update check. Required with schedule.
};

/** Detached source observation; it contains no address, credentials or arbitrary exception text. */
struct FeedFilterIpfObservation {
    bool initialSnapshotReady = false; ///< At least one complete successful source snapshot was published.
    bool closed = false;               ///< Request-only cutoff; native/callback retirement may still be pending.
    bool retired = false;     ///< This source native work and selected callbacks finished; shared worker may continue.
    std::size_t attempts = 0; ///< Lifetime source checks already admitted.
    std::optional<Diagnostic> diagnostic; ///< Last detached source or context refusal.
};

/** Producer-safe handle; close/destruction requests cutoff without joining native work. */
class FeedFilterIpfSource final {
    struct Port;
    std::shared_ptr<Port> port_;
    explicit FeedFilterIpfSource(std::shared_ptr<Port> port) noexcept;
    friend class FeedFilterIpfRuntime;

    public:
    /** Request cutoff; a runtime owner still performs creator-thread join/drain. */
    ~FeedFilterIpfSource();
    FeedFilterIpfSource(const FeedFilterIpfSource &) = delete;
    FeedFilterIpfSource &operator=(const FeedFilterIpfSource &) = delete;
    /** Request source/callback cutoff from any thread, including a tracker callback. */
    void close() noexcept;
    /** @return Detached finite progress/lifetime evidence; no native work is performed. */
    [[nodiscard]] FeedFilterIpfObservation observation() const;
    /** @param deadline Absolute steady-clock deadline; no polling or native work is performed by the waiter.
     * @return True after initial publication, false after close/deadline. Refuses worker self-wait.
     */
    [[nodiscard]] bool awaitInitialSnapshot(std::chrono::steady_clock::time_point deadline) const;
};

/** Creator-thread owner of one finite SDK worker and bounded lifetime source slots; no manual client polling loop.
 * Prepare on the bootstrap creator thread before application threads. The explicit bootstrap outlives
 * this owner and final drain. Destroy/join only on the creating thread, never from a source callback.
 * A source handle may outlive this owner as a closed retired handle. Classifier snapshots remain independent.
 * The optional IPF backend is selected at build/package time; unavailable capability is an actionable error.
 */
class FeedFilterIpfRuntime final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit FeedFilterIpfRuntime(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Creator-thread request/drain/join before releasing its explicit bootstrap lease. */
    ~FeedFilterIpfRuntime();
    FeedFilterIpfRuntime(const FeedFilterIpfRuntime &) = delete;
    FeedFilterIpfRuntime &operator=(const FeedFilterIpfRuntime &) = delete;
    /** @param bootstrap Explicit existing same-linked-instance main/pre-thread SDK owner.
     * @param transfer Existing complete finite source/native/HTTP/TLS/parser grants; copied before return.
     * @param limits Additional finite lifetime/staging grants.
     * @param schedule Explicit schedule/clock capability; required only when the expression configures schedule.
     * @param contextLimits Finite shared resources, generations, classifier trackers and callbacks.
     * @return Prepared owner with one idle worker, without source/native work; preparation failures throw detached
     * Error.
     */
    [[nodiscard]] static std::unique_ptr<FeedFilterIpfRuntime> create(transport::RuntimeBootstrap &bootstrap,
                                                                      const ipf::IpfTransferOptions &transfer,
                                                                      FeedFilterIpfLimits limits = {},
                                                                      FeedFilterIpfSchedule schedule = {},
                                                                      FeedFilterContextLimits contextLimits = {});
    /** Obtain the common high-level resource context once and pass it to filter builders.
     * Builder build resolves missing IPF tokens through this session, waiting at most initialTimeout.
     * No per-expression start/await/shutdown is required. The creator retains this session through common shutdown.
     * @return Producer-safe context; after session retirement existing snapshots remain usable, new sources refuse.
     */
    [[nodiscard]] std::shared_ptr<FeedFilterContext> context() const;
    /** @param context Explicit owning resource repository, retained through final source retirement.
     * @param expression Full Java-compatible `ipf[address,update=...,schedule=...,user=...,password=...]` token.
     * @param request Existing explicit authorization/provider policy; conflicting textual credentials are refused.
     * @return Started producer-safe source handle. Initial loading is asynchronous; await before building its filter.
     * @throws Error on invalid settings/capability, finite refusal or a previously used lifetime source slot.
     */
    [[nodiscard]] std::shared_ptr<FeedFilterIpfSource>
    start(std::shared_ptr<FeedFilterContext> context, std::u16string expression, const ipf::IpfUrlRequest &request);
    /** Creator-thread cutoff/drain/join; refuses worker/self/wrong-thread waits before blocking. */
    void closeAndAwaitTermination();
};
} // namespace org::ttldtor::qdcxx::api
