// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tls_recovery.hpp"

#include "recovery_copy_profile.hpp"
#include "recovery_core.hpp"
#include "recovery_core_support.hpp"
#include "tls_recovery_factory.hpp"
#include "tls_runtime_factory.hpp"

#include <exception>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

constexpr std::uint64_t OWN_PRIVATE_PREFIXES = 3; ///< One possible terminal failed-preparation path, not per attempt.
constexpr std::uint64_t OWN_AMBIENT_PREFIXES = 4; ///< Same one-failed-factory source-bound reserve.

/** Preserve actual TLS failure in the common coarse/native view.
 * @param diagnostic Existing static/numeric cause, without peer/credential text.
 * @return Common category plus exact nested native/pure evidence.
 */
RecoveryDiagnostic common(TlsDiagnostic diagnostic) noexcept {
    auto error = RecoveryError::PROTOCOL_FAILURE;

    switch (diagnostic.error) {
    case TlsError::NONE:
        error = RecoveryError::NONE;
        break;
    case TlsError::INVALID_STATE:
        error = RecoveryError::INVALID_STATE;
        break;
    case TlsError::WRONG_THREAD:
        error = RecoveryError::WRONG_THREAD;
        break;
    case TlsError::INVALID_CONFIGURATION:
        error = RecoveryError::INVALID_CONFIGURATION;
        break;
    case TlsError::RESOURCE_LIMIT:
        error = RecoveryError::RESOURCE_LIMIT;
        break;
    case TlsError::WORK_LIMIT:
        error = RecoveryError::WORK_LIMIT;
        break;
    case TlsError::ALLOCATION_FAILURE:
        error = RecoveryError::ALLOCATION_FAILURE;
        break;
    case TlsError::TLS_POLICY_FAILURE:
        error = RecoveryError::UNSUPPORTED_PROFILE;
        break;
    case TlsError::CREDENTIAL_FAILURE:
    case TlsError::AUTHENTICATION_DENIED:
        error = RecoveryError::AUTHENTICATION;
        break;
    case TlsError::TRANSPORT_FAILURE:
    case TlsError::DEADLINE:
    case TlsError::CLOSE_FAILURE:
        error = RecoveryError::TRANSPORT_FAILURE;
        break;
    default:
        break;
    }

    return {error, diagnostic.stage, diagnostic.transport};
}

/** Classify only source-backed typed/static TLS/native causes, never formatted SDK text.
 * @param diagnostic Actual primary TLS cause.
 * @return Conservative finite-retry policy and exact nested evidence.
 */
detail::RecoveryAttemptFailure classify(TlsDiagnostic diagnostic) noexcept {
    if (diagnostic.error == TlsError::NONE) {
        return detail::recoveryTransportFailure({});
    }

    auto result = detail::RecoveryAttemptFailure{common(diagnostic), detail::RecoveryCause::PROTOCOL, false};

    if (diagnostic.error == TlsError::CREDENTIAL_FAILURE || diagnostic.error == TlsError::AUTHENTICATION_DENIED) {
        result.cause = detail::RecoveryCause::AUTHENTICATION;
    } else if (diagnostic.error == TlsError::TRANSPORT_FAILURE ||
               (diagnostic.error == TlsError::HANDSHAKE_FAILURE && diagnostic.stage == "tls-truncated-eof")) {
        result.cause = detail::RecoveryCause::TRANSPORT;
        result.retryable = diagnostic.transport.protocolError == protocol::LiveError::NONE;
    } else if (diagnostic.error == TlsError::DEADLINE) {
        result.cause = detail::RecoveryCause::TRANSPORT;
        // Native operation deadlines retain an exact TcpDiagnostic. Opaque TLS-phase timers are terminal.
        result.retryable = diagnostic.transport.error == TcpError::DEADLINE;
    } else if (diagnostic.error == TlsError::RESOURCE_LIMIT || diagnostic.error == TlsError::ALLOCATION_FAILURE) {
        result.cause = detail::RecoveryCause::RESOURCE;
    } else if (diagnostic.error == TlsError::INVALID_CONFIGURATION ||
               diagnostic.error == TlsError::TLS_POLICY_FAILURE) {
        result.cause = detail::RecoveryCause::CONFIGURATION;
    }

    return result;
}

/** Compare exact fixed cause fields without retaining or formatting source data.
 * @param left Earlier cause.
 * @param right Later cause.
 * @return Whether both report the same numeric/static evidence.
 */
bool same(TlsDiagnostic left, TlsDiagnostic right) noexcept {
    return left.error == right.error && left.stage == right.stage && left.sdkCode == right.sdkCode &&
           left.verifyCode == right.verifyCode && left.transport.error == right.transport.error &&
           left.transport.stage == right.transport.stage && left.transport.nativeCode == right.transport.nativeCode &&
           left.transport.protocolError == right.transport.protocolError;
}

/** Embedded real TLS adapter; no SDK operation occurs in its constructor. */
class TlsRecoveryBackend final : public detail::RecoveryBackend {
    detail::BootstrapLease bootstrap_;      ///< First member: stable process borrow released after native/SDK children.
    TlsOptions options_;                    ///< Fixed immutable qualification scalars; never retains caller DNS spans.
    detail::TlsCredentialGate credentials_; ///< Optional stable Feed credential fence and revision source.
    std::optional<TlsRuntime> runtime_;     ///< At most one actual active-or-retiring private SDK owner.
    TlsWork *turn_ = nullptr;       ///< Borrowed only in a checked public poll scope; cleared before its lease ends.
    TlsDiagnostic last_{};          ///< Actual direct failure, without a fabricated SDK return value.
    mutable TlsDiagnostic first_{}; ///< First actual terminal failure, including failed entered factory.
    mutable TlsDiagnostic secondary_{}; ///< First distinct subsequent actual failure.

    /** Retain bounded TLS evidence, excluding actual transient admission gates.
     * @param diagnostic Actual library-produced cause.
     * @param factory Whether an entered failed factory makes even WORK_LIMIT terminal.
     */
    void record(TlsDiagnostic diagnostic, bool factory = false) const noexcept {
        if (diagnostic.error == TlsError::NONE)
            return;

        if (!factory &&
            (diagnostic.error == TlsError::WORK_LIMIT || detail::recoveryOutputBlocked(common(diagnostic)) ||
             diagnostic.transport.protocolError == protocol::LiveError::NOT_READY ||
             diagnostic.transport.protocolError == protocol::LiveError::AUTHENTICATION_REQUIRED))
            return;

        if (first_.error == TlsError::NONE)
            first_ = diagnostic;
        else if (secondary_.error == TlsError::NONE && !same(first_, diagnostic))
            secondary_ = diagnostic;
    }

    /** Preserve actual direct composition/start results.
     * @param value Synchronous real result.
     * @return Same admission or common diagnostic.
     */
    std::expected<void, RecoveryDiagnostic> result(std::expected<void, TlsDiagnostic> value) {
        last_ = value ? TlsDiagnostic{} : value.error();

        if (!value) {
            record(value.error());

            return std::unexpected(common(value.error()));
        }

        return {};
    }

    public:
    /** Copy qualification scalars and explicitly discard borrowed DNS presentation.
     * @param bootstrap Counted process borrow spanning every attempt and idle gap.
     * @param options Qualified controlled-host/resource values.
     */
    TlsRecoveryBackend(detail::BootstrapLease &&bootstrap, const TlsOptions &options,
                       detail::TlsCredentialGate credentials) noexcept
        : bootstrap_(std::move(bootstrap)), options_(options), credentials_(credentials) {
        options_.dns = {};
    }

    /** Own one actual caller TLS-work binding for this checked public operation. */
    class Turn {
        TlsRecoveryBackend &backend_; ///< Enclosing stable Impl's adapter.
        public:
        /** Bind only after the Core's owner/reentry rejection check succeeds.
         * @param backend Stable adapter protected by the outer operation lease.
         * @param work Actual caller vector, borrowed until this guard leaves.
         */
        Turn(TlsRecoveryBackend &backend, TlsWork &work) noexcept : backend_(backend) {
            if (backend_.turn_)
                std::terminate();

            backend_.turn_ = &work;
        }

        /** Clear the borrowed pointer before the enclosing public operation lease can retire. */
        ~Turn() {
            backend_.turn_ = nullptr;
        }

        /** The scoped caller reference cannot be copied.
         * @param other Unused source.
         */
        Turn(const Turn &other) = delete;
        /** Scoped caller references cannot be assigned.
         * @param other Unused source.
         * @return No result; assignment is unavailable.
         */
        Turn &operator=(const Turn &other) = delete;
    };

    std::expected<void, RecoveryDiagnostic> prepare(detail::RecoveryConfiguration &configuration,
                                                    protocol::SubscriptionWriter &&writer,
                                                    RecoveryTurnWork &work) override {
        if (!turn_)
            return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "tls-recovery-turn"});

        auto options = options_;
        const auto &dns = configuration.options.dns;
        options.dns = {{dns.servers.data(), dns.serverCount}, dns.port};
        auto prepared = detail::TlsRuntimeFactory::create(bootstrap_, configuration.endpoint, std::move(writer),
                                                          options, *turn_, work.session, credentials_.forAttempt());

        if (!prepared) {
            last_ = prepared.error();
            record(last_, true);

            return std::unexpected(common(last_));
        }

        runtime_.emplace(std::move(*prepared));
        last_ = {};

        return {};
    }

    std::expected<void, RecoveryDiagnostic> start() override {
        return result(runtime_->start());
    }

    std::expected<TcpPollResult, RecoveryDiagnostic> poll(RecoveryTurnWork &work, bool decodeInput) override {
        if (!turn_)
            return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_STATE, "tls-recovery-turn"});

        detail::RecoveryDecodeGate gate(work.session, decodeInput);
        auto polled = runtime_->poll(work.runtime, *turn_, work.session);
        last_ = polled ? TlsDiagnostic{} : polled.error();

        if (!polled) {
            record(last_);

            return std::unexpected(common(last_));
        }

        return TcpPollResult{polled->status, std::move(polled->event)};
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

    void dispose() noexcept override {
        runtime_.reset();
    }

    ConnectionStatistics statistics() const noexcept override {
        return runtime_ ? detail::TlsRuntimeFactory::statistics(*runtime_) : detail::RecoveryBackend::statistics();
    }

    ConnectionStatistics retireStatistics() noexcept override {
        const auto result = runtime_ ? detail::TlsRuntimeFactory::retireStatistics(*runtime_) : statistics();
        dispose();

        return result;
    }

    detail::RecoveryAttemptFacts facts() const noexcept override {
        if (!runtime_)
            return {};

        const auto state = runtime_->state();
        const auto auth = runtime_->authentication();
        const bool verified = state == TlsState::CONNECTED || state == TlsState::AUTHENTICATING;
        const bool eligible = auth == protocol::LiveAuthentication::ACKNOWLEDGED ||
                              (!credentials_.enter && auth == protocol::LiveAuthentication::UNCHALLENGED);

        return {true,
                verified,
                verified && eligible,
                runtime_->terminal().has_value(),
                runtime_->peer().identity().generation(),
                auth,
                runtime_->resources().transport.pendingOperations};
    }

    detail::RecoveryAttemptFailure failure() const noexcept override {
        if (!runtime_)
            return {};

        if (const auto terminal = runtime_->terminal()) {
            record(terminal->primary, true);
            record(terminal->secondary, true);
            auto failure = classify(terminal->primary);

            if (terminal->secondary.error != TlsError::NONE)
                failure.secondary = common(terminal->secondary);

            return failure;
        }

        return last_.error == TlsError::NONE ? detail::RecoveryAttemptFailure{} : classify(last_);
    }

    std::optional<TcpTerminal> wireTerminal() const noexcept override {
        if (runtime_) {
            if (const auto terminal = runtime_->terminal())
                return terminal->wire;
        }

        return std::nullopt;
    }

    std::optional<detail::RecoveryClock::time_point> nextWake() const noexcept override {
        return runtime_ ? runtime_->nextWake() : std::nullopt;
    }

    /** Copy exact saved first/secondary TLS causes without entering a native engine.
     * @param status Actual common state.
     * @return Bounded complete public TLS recovery inspection.
     */
    TlsRecoveryStatus status(RecoveryStatus status) const noexcept {
        return {status, first_, secondary_};
    }
};

/** Refuse conflicting borrowed DNS input instead of silently choosing another route.
 * @param tls Existing optional explicit TLS DNS handoff.
 * @param recovery Authoritative bounded owned route.
 * @return Whether no conflicting presentation was supplied.
 */
bool compatibleDns(const TlsDnsOptions &tls, const RecoveryDns &recovery) noexcept {
    if (recovery.serverCount > recovery.servers.size() || tls.servers.size() > recovery.servers.size())
        return false;

    if (tls.servers.empty())
        return tls.port == 0 || tls.port == recovery.port;

    if (tls.servers.size() != recovery.serverCount || tls.port != recovery.port)
        return false;

    for (std::size_t index = 0; index != tls.servers.size(); ++index) {
        if (tls.servers[index].address != recovery.servers[index].address)
            return false;
    }

    return true;
}

/** Static moved-from refusal without accessing the SDK.
 * @return Detached owner checkpoint.
 */
RecoveryDiagnostic moved() noexcept {
    return {RecoveryError::INVALID_STATE, "tls-recovery-moved-owner"};
}

} // namespace

struct TlsRecovery::Impl {
    TlsRecoveryBackend backend; ///< Declared first, so it survives Core's actual private disposal.
    detail::RecoveryCore core;  ///< Stable final origin protected through all user callbacks.

    /** Construct fixed state before the final input adoption.
     * @param bootstrap Counted stable process borrow, released after backend/Core retirement.
     * @param options Fixed qualification, without retaining caller DNS pointers.
     * @param configuration Complete still-uncommitted configuration.
     * @param retirement Copied final Core/one-event reserve.
     */
    Impl(detail::BootstrapLease &&bootstrap, const TlsOptions &options, detail::RecoveryConfiguration &&configuration,
         RecoveryWork retirement, detail::TlsCredentialGate credentials) noexcept
        : backend(std::move(bootstrap), options, credentials), core(std::move(configuration), backend, retirement) {
    }
};

TlsRecovery::TlsRecovery(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TlsRecovery::TlsRecovery(TlsRecovery &&other) noexcept : impl_(std::move(other.impl_)) {
}

TlsRecovery &TlsRecovery::operator=(TlsRecovery &&other) noexcept {
    if (this != &other) {
        if (impl_)
            impl_->core.cutoff();

        impl_ = std::move(other.impl_);
    }

    return *this;
}

TlsRecovery::~TlsRecovery() {
    if (impl_)
        impl_->core.cutoff();
}

std::expected<TlsRecovery, RecoveryDiagnostic>
TlsRecovery::create(RuntimeBootstrap &bootstrap, const client::EndpointOptions &endpoint,
                    std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues,
                    const RecoveryOptions &options, const TlsOptions &tls, RecoveryPreparationWork &work) {
    return detail::TlsRecoveryFactory::create(&bootstrap, nullptr, endpoint, std::move(blueprint), std::move(queues),
                                              options, tls, work);
}

std::expected<TlsRecovery, RecoveryDiagnostic>
detail::TlsRecoveryFactory::create(const BootstrapLease &bootstrap, const client::EndpointOptions &endpoint,
                                   std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues,
                                   const RecoveryOptions &options, const TlsOptions &tls, RecoveryPreparationWork &work,
                                   TlsCredentialGate credentials) {
    return create(nullptr, &bootstrap, endpoint, std::move(blueprint), std::move(queues), options, tls, work,
                  credentials);
}

std::expected<TlsRecovery, RecoveryDiagnostic> detail::TlsRecoveryFactory::create(
    RuntimeBootstrap *bootstrap, const BootstrapLease *lease, const client::EndpointOptions &endpoint,
    std::vector<protocol::OutgoingRecord> &&blueprint, RecoveryQueues &&queues, const RecoveryOptions &options,
    const TlsOptions &tls, RecoveryPreparationWork &work, TlsCredentialGate credentials) {
    if (detail::recoveryCopyProfile() == 0) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::UNSUPPORTED_PROFILE, "recovery-copy-profile"});
    }

    if (tls.host.failedPrivatePrefixes < OWN_PRIVATE_PREFIXES ||
        tls.host.failedAmbientPrefixes < OWN_AMBIENT_PREFIXES || !compatibleDns(tls.dns, options.dns)) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::INVALID_CONFIGURATION, "tls-recovery-host-or-route"});
    }

    auto candidate = detail::RecoveryConfiguration::prepare(endpoint, blueprint, queues, options,
                                                            client::EndpointSecurity::TLS, work);

    if (!candidate)
        return std::unexpected(candidate.error());

    const auto retirement = detail::admitRecoveryOwner(*candidate, sizeof(TlsRecovery::Impl), work.recovery);

    if (!retirement)
        return std::unexpected(retirement.error());

    auto bootstrapLease = lease ? BootstrapLease::acquire(*lease) : BootstrapLease::acquire(*bootstrap);

    if (!bootstrapLease) {
        return std::unexpected(detail::recoveryTransportDiagnostic(bootstrapLease.error()));
    }

    try {
        auto impl = std::make_shared<TlsRecovery::Impl>(std::move(*bootstrapLease), tls, std::move(*candidate),
                                                        *retirement, credentials);

        if (!impl->core.adopt(std::move(blueprint), std::move(queues)))
            std::terminate();

        return TlsRecovery(std::move(impl));
    } catch (const std::bad_alloc &) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::ALLOCATION_FAILURE, "tls-recovery-owner-storage"});
    }
}

detail::RecoveryCore &detail::TlsRecoveryFactory::core(TlsRecovery &recovery) noexcept {
    return recovery.impl_->core;
}

std::expected<void, RecoveryDiagnostic> TlsRecovery::start() noexcept {
    const auto lease = impl_;

    return lease ? lease->core.start() : std::unexpected(moved());
}

std::expected<TlsRecoveryStatus, RecoveryDiagnostic> TlsRecovery::poll(RecoveryTurnWork &work, TlsWork &tlsWork) {
    const auto lease = impl_; // Taken before even installing a caller-work scope or entering provider code.

    if (!lease)
        return std::unexpected(moved());

    if (const auto rejection = lease->core.rejection())
        return std::unexpected(*rejection);

    TlsRecoveryBackend::Turn turn(lease->backend, tlsWork);
    const auto progressed = lease->core.poll(work);

    if (!progressed)
        return std::unexpected(progressed.error());

    return lease->backend.status(*progressed);
}

std::expected<void, RecoveryDiagnostic> TlsRecovery::requestStop() noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestStop() : std::unexpected(moved());
}

std::expected<void, RecoveryDiagnostic>
TlsRecovery::requestDrain(std::chrono::steady_clock::time_point deadline) noexcept {
    const auto lease = impl_;

    return lease ? lease->core.requestDrain(deadline) : std::unexpected(moved());
}

TlsRecoveryStatus TlsRecovery::status() const noexcept {
    if (impl_)
        return impl_->backend.status(impl_->core.status());

    TlsRecoveryStatus result;
    result.recovery.phase = RecoveryPhase::CLOSED;
    result.recovery.stopped = true;

    return result;
}

RecoveryCapacity TlsRecovery::capacity() const noexcept {
    return impl_ ? impl_->core.capacity() : RecoveryCapacity{};
}

std::expected<mux::QueueRangeStatus, RecoveryDiagnostic>
TlsRecovery::historyRangeStatus(const mux::MuxHandle &handle, core::SubscriptionKey key, mux::QueueWork &work) const {
    const auto lease = impl_;

    return lease ? lease->core.historyRangeStatus(handle, key, work) : std::unexpected(moved());
}

std::expected<mux::MuxHandle, RecoveryReceiverFailure> TlsRecovery::openSubscriptionReceiver(
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

std::optional<std::chrono::steady_clock::time_point> TlsRecovery::nextWake() const noexcept {
    return impl_ ? impl_->core.nextWake() : std::nullopt;
}

} // namespace org::ttldtor::qdcxx::transport
