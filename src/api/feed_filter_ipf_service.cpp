// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed_filter.hpp"

#include "../ipf/connection_sink.hpp"
#include "../ipf/transfer_source.hpp"
#include "../transport/bootstrap_lease.hpp"
#include "feed_filter_ipf_config.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {
using Clock = std::chrono::steady_clock;

/** Shared wake channel; source handles never own the runtime or its worker. */
struct FilterIpfWake {
    std::mutex mutex;
    std::condition_variable condition;
    std::thread::id worker;
    bool stopping = false;
};

/** Reject without retaining caller input or arbitrary exception text.
 * @param code Detached high-level category.
 * @param stage Static library-owned checkpoint.
 */
[[noreturn]] void reject(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** Map the existing transport failure without copying secret-bearing inputs.
 * @param value Detached existing transfer result.
 * @return Static and numeric high-level failure.
 */
Diagnostic transferDiagnostic(const ipf::IpfTransferDiagnostic &value) noexcept {
    auto code = ErrorCode::TRANSPORT_FAILURE;

    switch (value.error) {
    case ipf::IpfTransferError::INVALID_OPTIONS:
    case ipf::IpfTransferError::INVALID_SOURCE:
    case ipf::IpfTransferError::AUTHORIZATION_FAILURE:
        code = ErrorCode::INVALID_ARGUMENT;
        break;
    case ipf::IpfTransferError::UNSUPPORTED_SOURCE:
        code = ErrorCode::UNSUPPORTED_PROFILE;
        break;
    case ipf::IpfTransferError::RESOURCE_LIMIT:
        code = ErrorCode::RESOURCE_LIMIT;
        break;
    case ipf::IpfTransferError::WORK_LIMIT:
        code = ErrorCode::WORK_LIMIT;
        break;
    case ipf::IpfTransferError::ALLOCATION_FAILURE:
        code = ErrorCode::ALLOCATION_FAILURE;
        break;
    case ipf::IpfTransferError::CANCELLED:
        code = ErrorCode::CLOSED;
        break;
    case ipf::IpfTransferError::WRONG_THREAD:
        code = ErrorCode::WRONG_THREAD;
        break;
    case ipf::IpfTransferError::INVALID_STATE:
        code = ErrorCode::INVALID_STATE;
        break;
    default:
        break;
    }

    return {code, value.stage, value.transport.nativeCode, static_cast<std::uint32_t>(value.error)};
}

/** Add a validated positive duration without overflowing the monotonic clock.
 * @param start Explicit monotonic origin.
 * @param delay Positive finite duration.
 * @return Saturating absolute deadline.
 */
Clock::time_point after(Clock::time_point start, std::chrono::milliseconds delay) noexcept {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::time_point::max() - start);

    return delay >= remaining ? Clock::time_point::max() : start + delay;
}
} // namespace

struct FeedFilterIpfSource::Port {
    std::mutex mutex;
    std::condition_variable condition;
    FeedFilterIpfObservation observed;
    std::atomic<bool> closed{false};
    std::stop_source stop;
    std::weak_ptr<FilterIpfWake> wake;
    std::thread::id worker;
    std::shared_ptr<FeedFilterContext> context;
    std::u16string expression;
    std::string source;
    ipf::IpfUrlRequest request;
    detail::FilterIpfConfig config;
    std::vector<std::u16string> schedules;
    Clock::time_point due = Clock::now();
    bool initialAttempt = true;

    /** Publish request-only cutoff and wake waiters; no native or user code executes. */
    void close() noexcept {
        const auto channel = wake.lock();

        if (channel) {
            std::lock_guard lock(channel->mutex);
            closed.store(true);
        } else {
            closed.store(true);
        }

        stop.request_stop();
        {
            std::lock_guard lock(mutex);
            observed.closed = true;
        }
        condition.notify_all();

        if (channel) {
            channel->condition.notify_all();
        }
    }

    /** Preserve the last detached failure without changing an accepted snapshot.
     * @param value Static/numeric cause.
     */
    void fail(Diagnostic value) {
        std::lock_guard lock(mutex);
        observed.diagnostic = value;
    }

    /** Retire provider/credential captures after the attempt and outside all locks. */
    void retire() {
        close();
        request = {};
        config.user.reset();
        config.password.reset();
        {
            std::lock_guard lock(mutex);
            observed.retired = true;
        }
        condition.notify_all();
    }

    /** Wait only for an already registered source, never advance native work.
     * @param deadline Absolute caller deadline.
     * @return Whether a full successful initial snapshot was published.
     */
    bool await(Clock::time_point deadline) {
        std::unique_lock lock(mutex);

        if (!observed.retired && worker == std::this_thread::get_id()) {
            reject(ErrorCode::INVALID_STATE, "filter-ipf-worker-self-wait");
        }

        condition.wait_until(lock, deadline, [&] {
            return observed.initialSnapshotReady || observed.closed || observed.retired;
        });

        return observed.initialSnapshotReady;
    }
};

struct FeedFilterIpfRuntime::Impl {
    /** Shared registration state; the context resolver retains only a weak reference. */
    struct Control {
        std::shared_ptr<FilterIpfWake> wake = std::make_shared<FilterIpfWake>();
        std::vector<std::shared_ptr<FeedFilterIpfSource::Port>> ports;
        std::shared_ptr<FeedFilterContext> context;
        ipf::IpfTransferOptions transfer;
        FeedFilterIpfLimits limits;
        FeedFilterIpfSchedule schedule;
        Clock::time_point deadline;
        std::size_t attempts = 0;
        std::size_t copyWork = 0;
        std::size_t schedulerTurns = 0;

        /** Prepare copied settings and the bounded registration vector before activation.
         * @param options Existing finite lower-layer settings.
         * @param bounds Additional source/scheduler bounds.
         * @param capability Explicit schedule and clock adapter.
         * @param repository Owning resource context.
         */
        Control(const ipf::IpfTransferOptions &options, FeedFilterIpfLimits bounds, FeedFilterIpfSchedule capability,
                std::shared_ptr<FeedFilterContext> repository)
            : context(std::move(repository)), transfer(options), limits(bounds), schedule(std::move(capability)),
              deadline(after(Clock::now(), bounds.lifetime)) {
            ports.reserve(bounds.sources);
        }

        /** Validate and register without native work or calls under the registration lock.
         * @param repository Independent target context.
         * @param expression Exact full resource token.
         * @param request Explicit auth/conditional policy, copied before publication.
         * @param reuse Whether an existing resolver registration may be reused.
         * @return Stable registered source state.
         */
        std::shared_ptr<FeedFilterIpfSource::Port> add(std::shared_ptr<FeedFilterContext> repository,
                                                       std::u16string expression, const ipf::IpfUrlRequest &request,
                                                       bool reuse) {
            if (!repository || !expression.starts_with(u"ipf[") || !expression.ends_with(u']')) {
                reject(ErrorCode::INVALID_ARGUMENT, "filter-ipf-resource-expression");
            }

            auto config = detail::parseFilterIpfConfig(expression, limits.expressionUnits);

            if (config.schedule && (!schedule.currentTimeMillis || !schedule.isTrading)) {
                reject(ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-schedule-capability-required");
            }

            if ((request.authorization && request.provider) ||
                ((config.user || config.password) && (request.authorization || request.provider))) {
                reject(ErrorCode::INVALID_ARGUMENT, "filter-ipf-authorization-conflict");
            }

            auto value = std::make_shared<FeedFilterIpfSource::Port>();
            value->context = std::move(repository);
            value->expression = std::move(expression);
            value->request = request;
            value->config = std::move(config);

            if (value->config.user || value->config.password) {
                auto authorization =
                    ipf::HttpAuthorization::basic(value->config.user.value_or(""), value->config.password.value_or(""),
                                                  transfer.limits.authorizationBytes);

                if (!authorization) {
                    reject(ErrorCode::INVALID_ARGUMENT, "filter-ipf-text-authorization");
                }

                value->request.authorization = std::move(*authorization);
            }

            auto parsed = ipf::detail::parseTransferSource(value->config.source, transfer.limits,
                                                           !value->request.authorization && !value->request.provider);

            if (!parsed) {
                throw Error(transferDiagnostic(parsed.error()));
            }

            if ((value->request.authorization || value->request.provider || parsed->userinfo) && !parsed->secure) {
                reject(ErrorCode::INVALID_ARGUMENT, "filter-ipf-authorization-requires-https");
            }

            if (!value->request.authorization && !value->request.provider) {
                value->request.authorization = std::move(parsed->userinfo);
            }

            value->source = parsed->address();
            value->config.source.clear();
            value->config.user.reset();
            value->config.password.reset();
            value->wake = wake;
            std::lock_guard lock(wake->mutex);

            if (wake->stopping || Clock::now() >= deadline) {
                reject(ErrorCode::CLOSED, "filter-ipf-runtime-closed");
            }

            for (const auto &existing : ports) {
                if (existing->context == value->context && existing->expression == value->expression) {
                    if (reuse) {
                        return existing;
                    }

                    reject(ErrorCode::INVALID_STATE, "filter-ipf-resource-already-started");
                }
            }

            if (ports.size() >= limits.sources) {
                reject(ErrorCode::RESOURCE_LIMIT, "filter-ipf-source-registrations");
            }

            value->worker = wake->worker;
            ports.push_back(value);
            wake->condition.notify_one();

            return value;
        }
    };

    /** Attempt-local full snapshot; controls are evidence, never publication boundaries. */
    struct Sink final : ipf::connection_detail::StreamConsumer {
        Control &state;
        std::shared_ptr<FeedFilterIpfSource::Port> port;
        std::vector<std::u16string> symbols;
        std::vector<std::u16string> schedules;
        std::size_t units = 0;
        std::optional<ipf::IpfTransferDiagnostic> rejected;

        /** Borrow the worker state through exactly one finite transfer.
         * @param state Stable session grants.
         * @param source Stable source registration.
         */
        Sink(Control &state, std::shared_ptr<FeedFilterIpfSource::Port> source)
            : state(state), port(std::move(source)) {
        }

        /** Select full source processing; conditional304 is handled by the transfer host.
         * @param result Borrowed transfer metadata.
         * @param modified Optional source timestamp.
         * @return ACCEPT; the complete parser EOF remains mandatory.
         */
        ipf::connection_detail::HeaderAction
        headers([[maybe_unused]] const ipf::IpfTransferResult &result,
                [[maybe_unused]] std::optional<std::chrono::sys_time<std::chrono::milliseconds>> modified) override {
            return ipf::connection_detail::HeaderAction::ACCEPT;
        }

        /** Debit before any snapshot allocation or copy.
         * @param amount UTF-16/call visits.
         * @return True after admission, false when the cumulative session grant is exhausted.
         */
        bool charge(std::size_t amount) {
            if (amount > state.limits.copyWork || state.copyWork > state.limits.copyWork - amount) {
                rejected = {ipf::IpfTransferError::WORK_LIMIT, "filter-ipf-snapshot-copy-work"};

                return false;
            }

            state.copyWork += amount;

            return true;
        }

        /** Stage symbols and optional per-profile schedule identifiers without publishing a prefix.
         * @param profile Borrowed complete parser value.
         * @return True after bounded owning admission.
         */
        bool profile(const ipf::InstrumentProfile &profile) override {
            const auto symbol = profile.getSymbol();
            const auto schedule = port->config.schedule && port->config.schedule->empty()
                                      ? profile.getField(u"TRADING_HOURS")
                                      : std::u16string_view{};

            if (!checkpoint() || !charge(1) || !charge(symbol.size()) || !charge(schedule.size())) {
                return false;
            }

            if (symbols.size() >= state.limits.snapshotSymbols || symbol.size() > state.limits.snapshotUnits ||
                units > state.limits.snapshotUnits - symbol.size()) {
                rejected = {ipf::IpfTransferError::RESOURCE_LIMIT, "filter-ipf-snapshot-symbols"};

                return false;
            }

            units += symbol.size();

            if (schedule.size() > state.limits.snapshotUnits || units > state.limits.snapshotUnits - schedule.size()) {
                rejected = {ipf::IpfTransferError::RESOURCE_LIMIT, "filter-ipf-snapshot-schedules"};

                return false;
            }

            units += schedule.size();
            symbols.emplace_back(symbol);

            if (!schedule.empty()) {
                schedules.emplace_back(schedule);
            }

            return true;
        }

        /** Ignore advisory FLUSH/COMPLETE for snapshot publication.
         * @param kind Existing parsed control.
         * @return Whether ordinary work remains permitted.
         */
        bool control([[maybe_unused]] ipf::IpfItemKind kind) override {
            return checkpoint();
        }

        /** Observe source and owner deadline between bounded transfer turns.
         * @return True before cutoff, false afterward.
         */
        bool checkpoint() override {
            if (port->closed.load()) {
                return false;
            }

            if (Clock::now() >= state.deadline) {
                rejected = {ipf::IpfTransferError::DEADLINE, "filter-ipf-owner-lifetime"};

                return false;
            }

            return true;
        }

        /** Inspect a detached local refusal.
         * @return First finite snapshot/deadline failure, if any.
         */
        std::optional<ipf::IpfTransferDiagnostic> refusal() const noexcept override {
            return rejected;
        }
    };

    transport::detail::BootstrapLease bootstrap;
    std::shared_ptr<Control> control;
    std::thread worker;
    const std::thread::id owner = std::this_thread::get_id();
    bool joining = false;
    std::vector<std::shared_ptr<FeedFilterIpfSource::Port>> retiringPorts;

    /** Bind the counted bootstrap before every dependent native/source owner.
     * @param lease Existing main-owned process borrow.
     * @param state Shared source registration/control state.
     */
    Impl(transport::detail::BootstrapLease lease, std::shared_ptr<Control> state)
        : bootstrap(std::move(lease)), control(std::move(state)) {
        retiringPorts.reserve(control->limits.sources);
    }

    /** Join an already started worker even if facade allocation failed. */
    ~Impl() {
        if (worker.joinable()) {
            shutdown();
        }
    }

    /** Reject an invalid high-level finite shape before source/configuration copies.
     * @param limits Explicit session limits.
     */
    static void validate(const FeedFilterIpfLimits &limits) {
        if (limits.sources == 0 || limits.attempts == 0 || limits.expressionUnits == 0 || limits.snapshotSymbols == 0 ||
            limits.snapshotUnits == 0 || limits.copyWork == 0 || limits.schedulerTurns == 0 ||
            limits.initialTimeout.count() <= 0 || limits.lifetime.count() <= 0 ||
            limits.schedulerInterval.count() <= 0) {
            reject(ErrorCode::INVALID_ARGUMENT, "filter-ipf-runtime-limits");
        }
    }

    /** Evaluate only explicit schedule capabilities outside registration/context/native locks.
     * @param port Selected source whose initial snapshot already succeeded.
     * @return True when the schedule suppresses this check.
     */
    bool suppressed(const std::shared_ptr<FeedFilterIpfSource::Port> &port) {
        if (!port->config.schedule) {
            return false;
        }

        const auto time = control->schedule.currentTimeMillis();

        if (port->closed.load()) {
            return true;
        }

        if (!port->config.schedule->empty()) {
            return control->schedule.isTrading(*port->config.schedule, time);
        }

        for (const auto &schedule : port->schedules) {
            if (port->closed.load() || control->schedule.isTrading(schedule, time)) {
                return true;
            }
        }

        return false;
    }

    /** Execute one finite source attempt; a failed candidate never replaces last-known-good data.
     * @param host Worker-affine persistent native host.
     * @param port Selected source registration.
     */
    void attempt(ipf::connection_detail::TransferHost &host, const std::shared_ptr<FeedFilterIpfSource::Port> &port) {
        ++control->attempts;
        {
            std::lock_guard lock(port->mutex);
            ++port->observed.attempts;
        }
        Sink sink(*control, port);
        auto result = host.attempt(port->source, port->request, port->stop.get_token(), sink);

        if (result.status == ipf::IpfTransferStatus::SUCCESS && result.nativeRetired && !port->closed.load() &&
            Clock::now() < control->deadline) {
            std::ranges::sort(sink.schedules);
            sink.schedules.erase(std::unique(sink.schedules.begin(), sink.schedules.end()), sink.schedules.end());
            port->context->replaceIpfSymbols(port->expression, sink.symbols, sink.schedules,
                                             port->config.update.count() > 0);
            port->schedules = std::move(sink.schedules);
            {
                std::lock_guard lock(port->mutex);
                port->observed.initialSnapshotReady = true;
                port->observed.diagnostic.reset();
            }
            port->condition.notify_all();
        } else if (result.error) {
            port->fail(transferDiagnostic(*result.error));
        } else if (result.status == ipf::IpfTransferStatus::NOT_MODIFIED && port->initialAttempt) {
            port->fail({ErrorCode::INVALID_STATE, "filter-ipf-initial-not-modified"});
        } else if (result.status != ipf::IpfTransferStatus::NOT_MODIFIED) {
            port->fail({ErrorCode::TRANSPORT_FAILURE, "filter-ipf-full-read-required"});
        }

        const bool initial = std::exchange(port->initialAttempt, false);
        bool ready;
        {
            std::lock_guard lock(port->mutex);
            ready = port->observed.initialSnapshotReady;
        }

        if ((initial && !ready) || port->config.update.count() == 0 || !result.nativeRetired ||
            (result.error && (result.error->error == ipf::IpfTransferError::WORK_LIMIT ||
                              result.error->error == ipf::IpfTransferError::RESOURCE_LIMIT ||
                              result.error->error == ipf::IpfTransferError::ALLOCATION_FAILURE))) {
            port->retire();
        } else {
            port->due = after(Clock::now(), port->config.update);
        }
    }

    /** Own all SDK work on one thread and alternate eligible resources fairly. */
    void run() noexcept {
        std::unique_ptr<ipf::connection_detail::TransferHost> host;
        std::size_t cursor = 0;
        std::vector<std::shared_ptr<FeedFilterIpfSource::Port>> ports;
        std::optional<Diagnostic> terminal;

        try {
            ports.reserve(control->limits.sources);

            while (true) {
                {
                    std::lock_guard lock(control->wake->mutex);

                    if (control->wake->stopping) {
                        break;
                    }

                    ports = control->ports;
                }

                if (Clock::now() >= control->deadline || control->attempts >= control->limits.attempts ||
                    control->schedulerTurns >= control->limits.schedulerTurns) {
                    terminal = Diagnostic{ErrorCode::WORK_LIMIT, "filter-ipf-owner-lifetime-work"};
                    break;
                }

                ++control->schedulerTurns;
                bool advanced = false;
                auto nextWake = std::min(control->deadline, after(Clock::now(), control->limits.schedulerInterval));

                for (std::size_t visited = 0; visited < ports.size(); ++visited) {
                    auto port = ports[cursor++ % ports.size()];

                    if (port->closed.load()) {
                        bool retired;
                        {
                            std::lock_guard lock(port->mutex);
                            retired = port->observed.retired;
                        }

                        if (!retired) {
                            port->retire();
                        }

                        continue;
                    }

                    if (!port->initialAttempt) {
                        if (!port->context->waitForTracking(port->expression, Clock::now(), port->stop.get_token())) {
                            continue;
                        }

                        if (Clock::now() < port->due) {
                            nextWake = std::min(nextWake, port->due);
                            continue;
                        }
                    }

                    try {
                        if (!port->initialAttempt && suppressed(port)) {
                            port->due = after(Clock::now(), port->config.update);
                            nextWake = std::min(nextWake, port->due);
                            continue;
                        }

                        if (port->closed.load()) {
                            continue;
                        }

                        if (!host) {
                            auto prepared = ipf::connection_detail::TransferHost::create(bootstrap, control->transfer);

                            if (!prepared) {
                                terminal = transferDiagnostic(prepared.error());
                                break;
                            }

                            host = std::move(*prepared);
                        }

                        attempt(*host, port);
                    } catch (const Error &error) {
                        port->fail(error.diagnostic());
                        port->retire();
                    } catch (const std::length_error &) {
                        port->fail({ErrorCode::RESOURCE_LIMIT, "filter-ipf-source-capacity"});
                        port->retire();
                    } catch (const std::bad_alloc &) {
                        port->fail({ErrorCode::ALLOCATION_FAILURE, "filter-ipf-source-allocation"});
                        port->retire();
                    } catch (...) {
                        port->fail({ErrorCode::CALLBACK_FAILURE, "filter-ipf-schedule-or-source-callback"});
                        port->retire();
                    }

                    advanced = true;
                    break;
                }

                if (terminal) {
                    break;
                }

                if (!advanced) {
                    std::unique_lock lock(control->wake->mutex);
                    control->wake->condition.wait_until(lock, nextWake, [&] {
                        return control->wake->stopping || control->ports.size() != ports.size() ||
                               std::ranges::any_of(ports, [](const auto &port) {
                                   if (!port->closed.load()) {
                                       return false;
                                   }

                                   std::lock_guard selected(port->mutex);

                                   return !port->observed.retired;
                               });
                    });
                }
            }
        } catch (const std::bad_alloc &) {
            terminal = Diagnostic{ErrorCode::ALLOCATION_FAILURE, "filter-ipf-worker-allocation"};
        } catch (...) {
            terminal = Diagnostic{ErrorCode::INVALID_STATE, "filter-ipf-worker-failure"};
        }

        host.reset();
        ports.clear();
        {
            std::lock_guard lock(control->wake->mutex);
            control->wake->stopping = true;
            ports.swap(control->ports);
        }

        for (const auto &port : ports) {
            bool retired;
            {
                std::lock_guard lock(port->mutex);
                retired = port->observed.retired;
            }

            if (terminal && !retired) {
                port->fail(*terminal);
            }

            port->retire();
        }

        control->wake->condition.notify_all();
    }

    /** Fence admission and request every source before creator-thread join. */
    void shutdown() {
        if (owner != std::this_thread::get_id()) {
            reject(ErrorCode::WRONG_THREAD, "filter-ipf-runtime-join-thread");
        }

        if (joining) {
            reject(ErrorCode::INVALID_STATE, "filter-ipf-runtime-join-reentry");
        }

        joining = true;
        retiringPorts.clear();
        {
            std::lock_guard lock(control->wake->mutex);
            control->wake->stopping = true;
            retiringPorts = control->ports;
        }

        for (const auto &port : retiringPorts) {
            port->close();
        }

        control->wake->condition.notify_all();

        if (worker.joinable()) {
            worker.join();
        }

        joining = false;
    }
};

FeedFilterIpfSource::FeedFilterIpfSource(std::shared_ptr<Port> port) noexcept : port_(std::move(port)) {
}

FeedFilterIpfSource::~FeedFilterIpfSource() {
    close();
}

void FeedFilterIpfSource::close() noexcept {
    port_->close();
}

FeedFilterIpfObservation FeedFilterIpfSource::observation() const {
    std::lock_guard lock(port_->mutex);

    return port_->observed;
}

bool FeedFilterIpfSource::awaitInitialSnapshot(Clock::time_point deadline) const {
    return port_->await(deadline);
}

FeedFilterIpfRuntime::FeedFilterIpfRuntime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

FeedFilterIpfRuntime::~FeedFilterIpfRuntime() {
    if (impl_) {
        impl_->shutdown();
    }
}

std::unique_ptr<FeedFilterIpfRuntime> FeedFilterIpfRuntime::create(transport::RuntimeBootstrap &bootstrap,
                                                                   const ipf::IpfTransferOptions &transfer,
                                                                   FeedFilterIpfLimits limits,
                                                                   FeedFilterIpfSchedule schedule,
                                                                   FeedFilterContextLimits contextLimits) {
    Impl::validate(limits);
    const auto valid = ipf::connection_detail::TransferHost::validate(transfer);

    if (!valid) {
        throw Error(transferDiagnostic(valid.error()));
    }

    auto lease = transport::detail::BootstrapLease::acquireForRuntime(bootstrap);

    if (!lease) {
        reject(ErrorCode::BOOTSTRAP_FAILURE, "filter-ipf-bootstrap-lease");
    }

    try {
        auto state = std::make_shared<Impl::Control>(transfer, limits, std::move(schedule),
                                                     FeedFilterContext::create(contextLimits));
        auto impl = std::make_unique<Impl>(std::move(*lease), state);
        std::weak_ptr<Impl::Control> weak = state;
        state->context->setResourceResolver([weak](std::span<const std::u16string> expressions) {
            auto state = weak.lock();

            if (!state) {
                reject(ErrorCode::CLOSED, "filter-ipf-resource-owner-retired");
            }

            if (state->wake->worker == std::this_thread::get_id()) {
                reject(ErrorCode::INVALID_STATE, "filter-ipf-worker-self-wait");
            }

            std::vector<std::shared_ptr<FeedFilterIpfSource::Port>> sources;
            sources.reserve(expressions.size());
            const auto deadline = std::min(state->deadline, after(Clock::now(), state->limits.initialTimeout));

            for (const auto &expression : expressions) {
                sources.push_back(state->add(state->context, expression, {}, true));
            }

            for (const auto &source : sources) {
                if (!source->await(deadline)) {
                    std::optional<Diagnostic> diagnostic;
                    bool closed;
                    {
                        std::lock_guard lock(source->mutex);
                        diagnostic = source->observed.diagnostic;
                        closed = source->observed.closed;
                    }
                    const auto fallback =
                        closed ? Diagnostic{ErrorCode::CLOSED, "filter-ipf-initial-source-closed"}
                               : Diagnostic{ErrorCode::INVALID_STATE, "filter-ipf-initial-snapshot-deadline"};

                    throw Error(diagnostic.value_or(fallback));
                }
            }
        });
        {
            std::lock_guard lock(state->wake->mutex);
            impl->worker = std::thread([runtime = impl.get()] {
                runtime->run();
            });
            state->wake->worker = impl->worker.get_id();
        }

        return std::unique_ptr<FeedFilterIpfRuntime>(new FeedFilterIpfRuntime(std::move(impl)));
    } catch (const std::bad_alloc &) {
        reject(ErrorCode::ALLOCATION_FAILURE, "filter-ipf-runtime-allocation");
    } catch (const std::length_error &) {
        reject(ErrorCode::RESOURCE_LIMIT, "filter-ipf-runtime-capacity");
    } catch (const std::system_error &) {
        reject(ErrorCode::RESOURCE_LIMIT, "filter-ipf-runtime-worker");
    }
}

std::shared_ptr<FeedFilterContext> FeedFilterIpfRuntime::context() const {
    return impl_->control->context;
}

std::shared_ptr<FeedFilterIpfSource> FeedFilterIpfRuntime::start(std::shared_ptr<FeedFilterContext> context,
                                                                 std::u16string expression,
                                                                 const ipf::IpfUrlRequest &request) {
    std::shared_ptr<FeedFilterIpfSource::Port> port;

    try {
        port = impl_->control->add(std::move(context), std::move(expression), request, false);

        return std::shared_ptr<FeedFilterIpfSource>(new FeedFilterIpfSource(port));
    } catch (const std::bad_alloc &) {
        if (port) {
            port->close();
        }

        reject(ErrorCode::ALLOCATION_FAILURE, "filter-ipf-source-allocation");
    } catch (const std::length_error &) {
        if (port) {
            port->close();
        }

        reject(ErrorCode::RESOURCE_LIMIT, "filter-ipf-source-capacity");
    }
}

void FeedFilterIpfRuntime::closeAndAwaitTermination() {
    impl_->shutdown();
}
} // namespace org::ttldtor::qdcxx::api
