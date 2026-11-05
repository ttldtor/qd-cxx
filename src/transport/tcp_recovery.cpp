// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tcp_recovery.hpp"

#include "recovery_copy_profile.hpp"
#include "recovery_core.hpp"
#include "recovery_core_support.hpp"
#include "tcp_recovery_factory.hpp"
#include "tcp_runtime_factory.hpp"

#include <exception>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

/** Embedded real TCP adapter; constructing it creates no native or protocol owner. */
class PlainRecoveryBackend final : public detail::RecoveryBackend {
    detail::BootstrapLease bootstrap_;  ///< First member: stable process borrow released after every native attempt.
    std::optional<TcpRuntime> runtime_; ///< At most one prepared/active/retiring attempt.
    TcpDiagnostic last_{};              ///< Actual latest direct failure, without native formatted text.

    /** Preserve actual direct method evidence.
     * @param value Actual synchronous result.
     * @return Same admission or common typed diagnostic.
     */
    std::expected<void, RecoveryDiagnostic> result(std::expected<void, TcpDiagnostic> value) {
        last_ = value ? TcpDiagnostic{} : value.error();

        if (!value)
            return std::unexpected(detail::recoveryTransportDiagnostic(value.error()));

        return {};
    }

    public:
    /** Retain stable process state across every native attempt and idle gap.
     * @param bootstrap Counted process borrow, released after all dependent children.
     */
    explicit PlainRecoveryBackend(detail::BootstrapLease &&bootstrap) noexcept : bootstrap_(std::move(bootstrap)) {
    }

    std::expected<void, RecoveryDiagnostic> prepare(detail::RecoveryConfiguration &configuration,
                                                    protocol::SubscriptionWriter &&writer,
                                                    RecoveryTurnWork &work) override {
        const auto &settings = configuration.endpoint.settings();
        auto schema = configuration.endpoint.schema().boundedCopy(settings.budgets.schema);

        if (!schema) {
            return std::unexpected(RecoveryDiagnostic{schema.error() == core::SchemaError::ALLOCATION_FAILURE
                                                          ? RecoveryError::ALLOCATION_FAILURE
                                                          : RecoveryError::RESOURCE_LIMIT,
                                                      "recovery-attempt-schema"});
        }

        const auto peak = configuration.capacity.schemaCopies;

        if (!detail::recoverySchemaFits(*schema,
                                        {peak.records / 3, peak.fields / 3, peak.lookups / 3, peak.nameUnits / 3})) {
            return std::unexpected(
                RecoveryDiagnostic{RecoveryError::UNSUPPORTED_PROFILE, "recovery-attempt-copy-profile"});
        }

        const auto &dns = configuration.options.dns;
        const auto &limits = settings.budgets.tcp;
        const TcpOptions options{settings.host,
                                 settings.port,
                                 {dns.servers.data(), dns.serverCount},
                                 dns.port,
                                 settings.timeouts.dns,
                                 settings.timeouts.connect,
                                 settings.timeouts.read,
                                 settings.timeouts.write,
                                 settings.timeouts.poll,
                                 {limits.inputBytes, limits.writeBytes, limits.addresses, limits.cnameDepth,
                                  limits.contextBytes, limits.handlerBytes, limits.residentBytes}};
        auto prepared = detail::TcpRuntimeFactory::create(bootstrap_, std::move(*schema), std::move(writer),
                                                          settings.budgets.session, options, work.session);

        if (!prepared) {
            last_ = prepared.error();

            return std::unexpected(detail::recoveryTransportDiagnostic(prepared.error()));
        }

        runtime_.emplace(std::move(*prepared));
        last_ = {};

        return {};
    }

    std::expected<void, RecoveryDiagnostic> start() override {
        return result(runtime_->start());
    }

    std::expected<TcpPollResult, RecoveryDiagnostic> poll(RecoveryTurnWork &work, bool decodeInput) override {
        detail::RecoveryDecodeGate gate(work.session, decodeInput);
        auto polled = runtime_->poll(work.runtime, work.session);
        last_ = polled ? TcpDiagnostic{} : polled.error();

        if (!polled)
            return std::unexpected(detail::recoveryTransportDiagnostic(polled.error()));

        return std::move(*polled);
    }

    std::expected<void, RecoveryDiagnostic> protocol(protocol::LiveProtocolView descriptor,
                                                     protocol::LiveWork &work) override {
        return result(runtime_->composeProtocol(descriptor, work));
    }

    std::expected<void, RecoveryDiagnostic> describe(std::span<const std::int32_t> ids,
                                                     protocol::LiveWork &work) override {
        return result(runtime_->describe(ids, work));
    }

    std::expected<void, RecoveryDiagnostic> subscribe(protocol::SubscriptionMessage message,
                                                      std::span<const core::SubscriptionRequest> requests,
                                                      protocol::LiveWork &work) override {
        return result(runtime_->subscribe(message, requests, work));
    }

    std::expected<void, RecoveryDiagnostic> heartbeat(protocol::LiveWork &work) override {
        return result(runtime_->composeHeartbeat(protocol::LiveHeartbeat{false}, work));
    }

    void abort() noexcept override {
        if (runtime_)
            (void)runtime_->requestClose();
    }

    void suspend() noexcept override {
        if (runtime_) {
            (void)detail::TcpRuntimeFactory::requestPause(*runtime_);
        }
    }

    void dispose() noexcept override {
        runtime_.reset();
    }

    ConnectionStatistics statistics() const noexcept override {
        return runtime_ ? runtime_->statistics() : ConnectionStatistics{};
    }

    ConnectionStatistics retireStatistics() noexcept override {
        const auto result = runtime_ ? detail::TcpRuntimeFactory::retireStatistics(*runtime_) : ConnectionStatistics{};
        dispose();

        return result;
    }

    detail::RecoveryAttemptFacts facts() const noexcept override {
        if (!runtime_)
            return {};

        const auto authentication = runtime_->authentication();

        return {true,
                runtime_->state() == TcpState::CONNECTED,
                authentication == protocol::LiveAuthentication::UNCHALLENGED ||
                    authentication == protocol::LiveAuthentication::ACKNOWLEDGED,
                runtime_->terminal().has_value(),
                runtime_->peer().identity().generation(),
                authentication,
                runtime_->resources().pendingOperations};
    }

    detail::RecoveryAttemptFailure failure() const noexcept override {
        if (!runtime_)
            return {};

        if (runtime_->authentication() == protocol::LiveAuthentication::REQUIRED ||
            runtime_->authentication() == protocol::LiveAuthentication::AUTHORIZATION_EMITTED) {
            return {{RecoveryError::AUTHENTICATION, "recovery-plain-authentication-required"},
                    detail::RecoveryCause::AUTHENTICATION,
                    false};
        }

        if (const auto terminal = runtime_->terminal()) {
            auto failure = detail::recoveryTransportFailure(terminal->primary);

            if (terminal->secondary.error != TcpError::NONE) {
                failure.secondary = detail::recoveryTransportDiagnostic(terminal->secondary);
            }

            return failure;
        }

        return last_.error == TcpError::NONE ? detail::RecoveryAttemptFailure{}
                                             : detail::recoveryTransportFailure(last_);
    }

    std::optional<TcpTerminal> wireTerminal() const noexcept override {
        return runtime_ ? runtime_->terminal() : std::nullopt;
    }

    std::optional<detail::RecoveryClock::time_point> nextWake() const noexcept override {
        return runtime_ ? runtime_->nextWake() : std::nullopt;
    }
};

/** Detached moved-from rejection.
 * @return Static owner evidence without native access.
 */
RecoveryDiagnostic moved() noexcept {
    return {RecoveryError::INVALID_STATE, "recovery-moved-owner"};
}

} // namespace

struct TcpRecovery::Impl {
    PlainRecoveryBackend backend; ///< Declared first, so it outlives Core's fallback disposal.
    detail::RecoveryCore core;    ///< Stable final origin and callback continuation owner.

    /** Construct before the final nonthrowing input adoption.
     * @param bootstrap Counted stable process borrow, released after backend/Core retirement.
     * @param configuration Complete uncommitted candidate.
     * @param retirement Copied final Core/event retirement allowance.
     */
    Impl(detail::BootstrapLease &&bootstrap, detail::RecoveryConfiguration &&configuration,
         RecoveryWork retirement) noexcept
        : backend(std::move(bootstrap)), core(std::move(configuration), backend, retirement) {
    }
};

TcpRecovery::TcpRecovery(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TcpRecovery::TcpRecovery(TcpRecovery &&other) noexcept : impl_(std::move(other.impl_)) {
}

TcpRecovery &TcpRecovery::operator=(TcpRecovery &&other) noexcept {
    if (this != &other) {
        if (impl_)
            impl_->core.cutoff();

        impl_ = std::move(other.impl_);
    }

    return *this;
}

TcpRecovery::~TcpRecovery() {
    if (impl_)
        impl_->core.cutoff();
}

std::expected<TcpRecovery, RecoveryDiagnostic>
TcpRecovery::create(RuntimeBootstrap &bootstrap, const client::EndpointOptions &endpoint,
                    std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues,
                    const RecoveryOptions &options, RecoveryPreparationWork &work) {
    return detail::TcpRecoveryFactory::create(bootstrap.state_.get(), endpoint, std::move(blueprint), std::move(queues),
                                              options, work);
}

std::expected<TcpRecovery, RecoveryDiagnostic>
detail::TcpRecoveryFactory::create(const BootstrapLease &bootstrap, const client::EndpointOptions &endpoint,
                                   std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues,
                                   const RecoveryOptions &options, RecoveryPreparationWork &work) {
    return create(bootstrap.state(), endpoint, std::move(blueprint), std::move(queues), options, work);
}

std::expected<TcpRecovery, RecoveryDiagnostic>
detail::TcpRecoveryFactory::create(RuntimeBootstrap::State *bootstrap, const client::EndpointOptions &endpoint,
                                   std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues,
                                   const RecoveryOptions &options, RecoveryPreparationWork &work) {
    auto candidate = detail::RecoveryConfiguration::prepare(endpoint, blueprint, queues, options,
                                                            client::EndpointSecurity::CREDENTIAL_FREE_PLAIN, work);

    if (!candidate)
        return std::unexpected(candidate.error());

    const auto retirement = detail::admitRecoveryOwner(*candidate, sizeof(TcpRecovery::Impl), work.recovery);

    if (!retirement)
        return std::unexpected(retirement.error());

    auto bootstrapLease = detail::BootstrapLease::acquire(bootstrap);

    if (!bootstrapLease) {
        return std::unexpected(detail::recoveryTransportDiagnostic(bootstrapLease.error()));
    }

    try {
        auto impl = std::make_shared<TcpRecovery::Impl>(std::move(*bootstrapLease), std::move(*candidate), *retirement);

        if (!impl->core.adopt(std::move(blueprint), std::move(queues)))
            std::terminate();

        return TcpRecovery(std::move(impl));
    } catch (const std::bad_alloc &) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::ALLOCATION_FAILURE, "recovery-owner-storage"});
    }
}

std::expected<void, RecoveryDiagnostic> TcpRecovery::start() noexcept {
    const auto lease = impl_;

    return lease ? lease->core.start() : std::unexpected(moved());
}

std::expected<RecoveryStatus, RecoveryDiagnostic> TcpRecovery::poll(RecoveryTurnWork &work) {
    const auto lease = impl_; // Taken before any callback can destroy or replace the public handle.

    return lease ? lease->core.poll(work) : std::unexpected(moved());
}

std::expected<void, RecoveryDiagnostic> TcpRecovery::requestDisconnect(bool clear) noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestDisconnect(clear) : std::unexpected(moved());
}

std::expected<void, RecoveryDiagnostic> TcpRecovery::requestReconnect() noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestReconnect() : std::unexpected(moved());
}

std::expected<void, RecoveryDiagnostic> TcpRecovery::requestStop() noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestStop() : std::unexpected(moved());
}

std::expected<void, RecoveryDiagnostic>
TcpRecovery::requestDrain(std::chrono::steady_clock::time_point deadline) noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestDrain(deadline) : std::unexpected(moved());
}

RecoveryStatus TcpRecovery::status() const noexcept {
    if (impl_)
        return impl_->core.status();

    RecoveryStatus result;
    result.phase = RecoveryPhase::CLOSED;
    result.stopped = true;

    return result;
}

ConnectionStatistics TcpRecovery::statistics() const noexcept {
    return impl_ ? impl_->core.statistics() : ConnectionStatistics{};
}

ConnectionStatistics detail::TcpRecoveryFactory::retireStatistics(TcpRecovery &recovery) noexcept {
    return recovery.impl_ ? recovery.impl_->core.retireStatistics() : ConnectionStatistics{};
}

std::expected<void, RecoveryDiagnostic>
detail::TcpRecoveryFactory::requestAggregationPeriod(TcpRecovery &recovery,
                                                     std::optional<std::chrono::milliseconds> period) noexcept {
    return recovery.impl_ ? recovery.impl_->core.requestAggregationPeriod(period) : std::unexpected(moved());
}

std::optional<AggregationPeriodInfo>
detail::TcpRecoveryFactory::aggregationPeriodInfo(const TcpRecovery &recovery) noexcept {
    return recovery.impl_ ? recovery.impl_->core.aggregationPeriodInfo() : std::nullopt;
}

RecoveryCapacity TcpRecovery::capacity() const noexcept {
    return impl_ ? impl_->core.capacity() : RecoveryCapacity{};
}

std::expected<mux::QueueRangeStatus, RecoveryDiagnostic>
TcpRecovery::historyRangeStatus(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work) const {
    const auto lease = impl_;

    return lease ? lease->core.historyRangeStatus(handle, key, work) : std::unexpected(moved());
}

std::expected<std::vector<mux::QueueRangeObservation>, RecoveryDiagnostic>
TcpRecovery::historyRangeSnapshot(const mux::MuxHandle &handle, mux::QueueWork &work) const {
    const auto lease = impl_;

    return lease ? lease->core.historyRangeSnapshot(handle, work) : std::unexpected(moved());
}

std::expected<mux::MuxHandle, RecoveryReceiverFailure> TcpRecovery::openSubscriptionReceiver(
    core::SubscriptionFamily family, std::span<const client::PreparedSubscriptionBinding> bindings,
    const std::shared_ptr<client::SubscriptionConsumer> &consumer, client::ReceiverLimits receiverLimits,
    mux::ClientLimits clientLimits, client::ReceiverWork &receiverWork, mux::QueueWork &queueWork) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(RecoveryReceiverFailure{moved()});
    }

    return lease->core.openSubscriptionReceiver(family, bindings, consumer, receiverLimits, clientLimits, receiverWork,
                                                queueWork);
}

std::optional<std::chrono::steady_clock::time_point> TcpRecovery::nextWake() const noexcept {
    return impl_ ? impl_->core.nextWake() : std::nullopt;
}

} // namespace org::ttldtor::qdcxx::transport
