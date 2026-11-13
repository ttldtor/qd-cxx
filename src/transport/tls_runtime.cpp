// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tls_runtime.hpp"

#include "../protocol/live_authorization.hpp"
#include "tls_engine.hpp"
#include "tls_runtime_factory.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

/** Make detached adapter evidence without retaining caller/provider text.
 * @param error Typed category.
 * @param stage Static checkpoint.
 * @return Static diagnostic.
 */
TlsDiagnostic diagnostic(TlsError error, std::string_view stage) noexcept {
    return {error, stage};
}

/** Translate the common native/session cause while retaining all numeric evidence.
 * @param cause Detached common cause.
 * @return Detached TLS-facing cause.
 */
TlsDiagnostic fromTransport(TcpDiagnostic cause) noexcept {
    auto category = TlsError::TRANSPORT_FAILURE;

    switch (cause.error) {
    case TcpError::NONE:
        category = TlsError::NONE;
        break;
    case TcpError::INVALID_STATE:
        category = TlsError::INVALID_STATE;
        break;
    case TcpError::WRONG_THREAD:
        category = TlsError::WRONG_THREAD;
        break;
    case TcpError::INVALID_CONFIGURATION:
        category = TlsError::INVALID_CONFIGURATION;
        break;
    case TcpError::RESOURCE_LIMIT:
        category = TlsError::RESOURCE_LIMIT;
        break;
    case TcpError::WORK_LIMIT:
        category = TlsError::WORK_LIMIT;
        break;
    case TcpError::ALLOCATION_FAILURE:
        category = TlsError::ALLOCATION_FAILURE;
        break;
    case TcpError::DEADLINE:
        category = TlsError::DEADLINE;
        break;
    case TcpError::PROTOCOL_FAILURE:
        category = TlsError::PROTOCOL_FAILURE;
        break;
    case TcpError::CLOSE_FAILURE:
        category = TlsError::CLOSE_FAILURE;
        break;
    default:
        break;
    }

    auto result = diagnostic(category, cause.stage);
    result.transport = cause;

    return result;
}

/** Translate pure resource-domain failure without exposing its inputs.
 * @param cause Static resource rejection.
 * @return Static TLS-facing rejection.
 */
TlsDiagnostic fromResource(TlsResourceDiagnostic cause) noexcept {
    const auto category = cause.error == TlsResourceError::WORK_LIMIT            ? TlsError::WORK_LIMIT
                          : cause.error == TlsResourceError::ARITHMETIC_OVERFLOW ? TlsError::RESOURCE_LIMIT
                                                                                 : TlsError::INVALID_CONFIGURATION;

    return diagnostic(category, cause.stage);
}

/** Checked capacity accumulation used before allocation or configuration transfer.
 * @param total Current sum, updated only on success.
 * @param bytes Additional requested capacity.
 * @return True for a representable sum.
 */
bool add(std::size_t &total, std::size_t bytes) noexcept {
    if (bytes > std::numeric_limits<std::size_t>::max() - total) {
        return false;
    }

    total += bytes;

    return true;
}

/** Map configuration-owned native values explicitly, without aliasing/casting layouts.
 * @param settings Retained immutable snapshot values.
 * @param dns Explicit route resolver values, copied later by the common owner.
 * @return Borrowed common preparation options.
 */
TcpOptions tcpOptions(const client::EndpointSettings &settings, TlsDnsOptions dns) noexcept {
    const auto &limits = settings.budgets.tcp;
    const auto &timeouts = settings.timeouts;

    return {settings.host,
            settings.port,
            dns.servers,
            dns.port,
            timeouts.dns,
            timeouts.connect,
            timeouts.read,
            timeouts.write,
            timeouts.poll,
            {limits.inputBytes, limits.writeBytes, limits.addresses, limits.cnameDepth, limits.contextBytes,
             limits.handlerBytes, limits.residentBytes}};
}

} // namespace

/** Stable operation owner; public-handle liveness is independent of retained operation leases.
 * All fields and SDK release stay serialized on owner. core is destroyed before endpoint/provider
 * ownership, and its destructor drains every native borrower before destroying the engine.
 */
struct TlsRuntime::Impl {
    client::EndpointOptions endpoint; ///< Retained immutable schema/settings/provider owner.
    TlsRequirements requirements;     ///< Exact copied immutable proof shape and all phase vectors.
    TlsWorkVector disposal;           ///< Factory-prepaid owned retirement credit; never aliases caller work.
    TlsResources reservation;         ///< Conservative simultaneous requested backing, unchanged after close.
    detail::TlsCredentialAttempt credentialAttempt;     ///< Optional Feed fence captured at this attempt's factory.
    std::thread::id owner = std::this_thread::get_id(); ///< Immutable serialized owner affinity.
    std::unique_ptr<char16_t[]> token;                  ///< Fresh per-attempt bounded credential backing.
    std::unique_ptr<detail::RuntimeCore> core;          ///< Sole DNS/socket/session/native completion owner.
    detail::TlsEngine *engine = nullptr;                ///< Borrowed from core, valid until core retirement.
    TlsDiagnostic primary{};   ///< First observed adapter/engine/common failure in owner order.
    TlsDiagnostic secondary{}; ///< First distinct later cause.
    std::optional<detail::Clock::time_point> handshakeDeadline{};      ///< Starts at actual native connection.
    std::optional<detail::Clock::time_point> authenticationDeadline{}; ///< Starts after verified handshake.
    std::optional<detail::Clock::time_point> closeDeadline{};          ///< Starts at explicit/peer TLS-close request.
    bool publicOwnerLive = true; ///< Immediate admission cutoff, independent of shared lease count.
    bool busy = false;           ///< Normal public-operation reentrancy guard, retained through provider calls.
    bool abortRequested = false; ///< Request-only cutoff, applied by the next safe poll/destruction.
    bool explicitAbort = false;  ///< User request/public retirement, distinct from an internally observed failure.
    bool initialFunded = false;  ///< Once-only H was copied from a successful admission.
    bool authorizationAttempted = false; ///< Provider is never called again for this attempt.
    bool credentialSuperseded = false;   ///< Feed will apply its newer logical cutoff before another native turn.
    bool verifiedObserved = false;       ///< Authentication deadline was installed exactly once.
    std::size_t pendingPlaintext = 0;    ///< Last known unaccepted application suffix before native retirement.

    /** Retain immutable inputs before any SDK construction.
     * @param source Explicit immutable endpoint.
     * @param need Recomputed qualified requirements.
     * @param resources Fully checked whole-run reservation.
     */
    Impl(const client::EndpointOptions &source, TlsRequirements need, TlsResources resources,
         detail::TlsCredentialAttempt attempt)
        : endpoint(source), requirements(need), disposal(need.retirement), reservation(resources),
          credentialAttempt(attempt) {
    }

    /** Drain native borrowers before the SDK or retained provider can be released. */
    ~Impl() {
        if (owner != std::this_thread::get_id() || busy) {
            std::terminate();
        }

        core.reset();
        disposal = {}; // The separately admitted reserve is spent once by graph retirement.
    }

    /** Check owner/busy/public eligibility without entering the SDK.
     * @param allowBusy True only for request-only abort.
     * @return Detached rejection, or empty for an eligible operation.
     */
    std::optional<TlsDiagnostic> rejection(bool allowBusy = false) const noexcept {
        if (owner != std::this_thread::get_id()) {
            return diagnostic(TlsError::WRONG_THREAD, "tls-owner");
        }

        if (!publicOwnerLive || (!allowBusy && busy)) {
            return diagnostic(TlsError::INVALID_STATE, "tls-owner-retired-or-busy");
        }

        return std::nullopt;
    }

    /** Retain the first cause and first distinct cleanup cause without text formatting.
     * @param cause Already observed detached cause.
     */
    void record(TlsDiagnostic cause) noexcept {
        if (cause.error == TlsError::NONE) {
            return;
        }

        if (primary.error == TlsError::NONE) {
            primary = cause;
        } else if (secondary.error == TlsError::NONE &&
                   (primary.error != cause.error || primary.stage != cause.stage)) {
            secondary = cause;
        }
    }

    /** Request only; no SSL/BIO/native cancellation or user destructor can run here. */
    void cutoff() noexcept {
        abortRequested = true;
        explicitAbort = true;
    }

    /** Record failure before requesting allocation-free ordinary-admission cutoff.
     * @param cause Detached observed cause.
     */
    void fail(TlsDiagnostic cause) noexcept {
        record(cause);
        abortRequested = true;
    }

    /** Install a representable cooperative deadline, or request typed retirement.
     * @param target Owned deadline.
     * @param duration Required positive immutable duration.
     */
    void deadline(std::optional<detail::Clock::time_point> &target, std::chrono::milliseconds duration) noexcept {
        const auto now = detail::Clock::now();

        if (duration <= std::chrono::milliseconds::zero() || duration > detail::Clock::time_point::max() - now) {
            fail(diagnostic(TlsError::RESOURCE_LIMIT, "tls-deadline-range"));

            return;
        }

        target = now + duration;
    }

    /** Observe SDK/common causes before any newer local failure is recorded. */
    void observeFailures() noexcept {
        record(engine->diagnostic());
        record(fromTransport(core->outcome.primary));
        record(fromTransport(core->outcome.secondary));
    }

    /** Observe verified/auth/close transitions and enforce explicit cooperative deadlines. */
    void observePhase() noexcept {
        const auto &tls = *endpoint.settings().tls;
        const auto now = detail::Clock::now();

        if (core->state() == TcpState::CONNECTED && !handshakeDeadline && !engine->verified()) {
            deadline(handshakeDeadline, *tls.handshakeTimeout);
        }

        if (engine->verified() && !verifiedObserved) {
            verifiedObserved = true;
            handshakeDeadline.reset();

            if (tls.credentials) {
                deadline(authenticationDeadline, *tls.authenticationTimeout);
            }
        }

        if (core->authentication() == protocol::LiveAuthentication::ACKNOWLEDGED) {
            authenticationDeadline.reset();
        }

        if (engine->closing() && !closeDeadline) {
            deadline(closeDeadline, *tls.closeTimeout);
        }

        if (!abortRequested &&
            ((handshakeDeadline && now >= *handshakeDeadline) ||
             (authenticationDeadline && now >= *authenticationDeadline) || (closeDeadline && now >= *closeDeadline))) {
            fail(diagnostic(TlsError::DEADLINE, "tls-phase-deadline"));
        }
    }

    /** Authorize at most once, only after whole work/output admission and actual verification.
     * No public-handle member is accessed here. An executing operation and local provider owner
     * survive callback destruction/replacement, and the old owner marker is rechecked afterward.
     * @param work Mutable caller protocol credits, precharged before user code.
     * @return Work/output rejection before callback, otherwise empty with typed failure recorded.
     */
    std::expected<void, TlsDiagnostic> authenticate(protocol::LiveWork &work) {
        if (!engine->verified() || abortRequested || core->authentication() != protocol::LiveAuthentication::REQUIRED) {
            return {};
        }

        if (!credentialAttempt.admit()) {
            credentialSuperseded = true;

            return {};
        }

        const auto &tls = *endpoint.settings().tls;

        if (authorizationAttempted) {
            fail(diagnostic(TlsError::AUTHENTICATION_DENIED, "tls-authentication-denied"));

            return {};
        }

        if (!tls.credentials) {
            fail(diagnostic(TlsError::CREDENTIAL_FAILURE, "tls-credential-unavailable"));

            return {};
        }

        if (core->output) {
            return {};
        }

        const auto capacity = *tls.tokenUnits;
        // authorize scans once, UTFString scans twice, and the adapter initializes/scans once.
        // UTF-8 staging, final compaction and fixed property/envelope prefixes are precharged too.
        const protocol::LiveWork need{1, 1, 4 * capacity + 26, 8 * capacity + 256};

        if (work.operations < need.operations || work.visits < need.visits || work.textUnits < need.textUnits ||
            work.bytes < need.bytes) {
            return std::unexpected(diagnostic(TlsError::WORK_LIMIT, "tls-credential-work"));
        }

        work.operations -= need.operations;
        work.visits -= need.visits;
        work.textUnits -= need.textUnits;
        work.bytes -= need.bytes;
        auto admitted = need;
        authorizationAttempted = true;
        std::fill_n(token.get(), capacity, char16_t{});
        admitted.bytes -= capacity * sizeof(char16_t);
        const auto originalPeer = core->peer();
        const auto originalIdentity = originalPeer.identity();
        const auto provider = tls.credentials;
        std::expected<std::size_t, client::CredentialError> supplied =
            std::unexpected(client::CredentialError::UNAVAILABLE);
        bool threw = false;
        try {
            supplied = provider->basicToken({token.get(), capacity});
        } catch (...) {
            threw = true;
        }

        if (!publicOwnerLive || abortRequested) {
            return {};
        }

        if (!credentialAttempt.admit()) {
            credentialSuperseded = true;

            return {};
        }

        if (!credentialAttempt.enter) {
            observePhase();

            if (abortRequested)
                return {};
        }

        TlsDiagnostic failure{};
        std::expected<protocol::LivePendingOutput, protocol::LiveError> prepared =
            std::unexpected(protocol::LiveError::INVALID_STATE);

        if (threw || !supplied || *supplied == 0 || *supplied > capacity || !engine->verified() ||
            !originalIdentity.matches(core->peer().identity())) {
            failure = diagnostic(TlsError::CREDENTIAL_FAILURE, "tls-credential-result");
        } else {
            const auto value = std::u16string_view(token.get(), *supplied);
            admitted.textUnits -= value.size();

            if (!value.starts_with(u"Basic ") || value.size() <= std::u16string_view(u"Basic ").size()) {
                failure = diagnostic(TlsError::CREDENTIAL_FAILURE, "tls-credential-prefix");
            } else {
                prepared = protocol::detail::LiveAuthorization::prepare(*core->session, value, admitted);
            }
        }

        struct Commit {
            Impl &attempt;
            decltype(prepared) &output;
            TlsDiagnostic failure;
            const decltype(originalIdentity) &identity;
        } commit{*this, prepared, failure, originalIdentity};

        const bool adopted = credentialAttempt.admit(&commit, [](void *context) noexcept {
            auto &value = *static_cast<Commit *>(context);
            auto &attempt = value.attempt;
            attempt.observePhase();

            if (attempt.abortRequested)
                return;

            if (value.failure.error != TlsError::NONE) {
                attempt.fail(value.failure);

                return;
            }

            if (!attempt.publicOwnerLive || !attempt.engine->verified() ||
                !value.identity.matches(attempt.core->peer().identity()))
                return;

            if (value.output)
                protocol::detail::LiveAuthorization::commit(*attempt.core->session);

            if (auto result = attempt.core->adoptOutput(std::move(value.output)); !result)
                attempt.fail(fromTransport(result.error()));
        });

        if (!adopted) {
            credentialSuperseded = true;
        }

        return {};
    }

    /** Reject public composition before touching pure-session state.
     * @return Detached rejection or empty for verified writable TLS.
     */
    std::optional<TlsDiagnostic> outputRejection() const noexcept {
        if (const auto error = rejection()) {
            return error;
        }

        if (abortRequested || core->closeRequested || core->state() == TcpState::CLOSED || engine->closing() ||
            !engine->verified()) {
            return diagnostic(TlsError::INVALID_STATE, "tls-output-not-verified-or-closing");
        }

        return std::nullopt;
    }
};

TlsRuntime::TlsRuntime(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TlsRuntime::TlsRuntime(TlsRuntime &&other) noexcept : impl_(std::move(other.impl_)) {
}

TlsRuntime &TlsRuntime::operator=(TlsRuntime &&other) noexcept {
    if (this != &other) {
        if (impl_) {
            impl_->publicOwnerLive = false;
            impl_->cutoff();
        }

        impl_ = std::move(other.impl_);
    }

    return *this;
}

TlsRuntime::~TlsRuntime() {
    if (impl_) {
        impl_->publicOwnerLive = false;
        impl_->cutoff();
    }
}

std::expected<TlsRuntime, TlsDiagnostic> TlsRuntime::create(RuntimeBootstrap &bootstrap,
                                                            const client::EndpointOptions &endpoint,
                                                            protocol::SubscriptionWriter &&composer, TlsOptions options,
                                                            TlsWork &tlsWork, protocol::LiveWork &sessionWork) {
    return detail::TlsRuntimeFactory::create(bootstrap.state_.get(), endpoint, std::move(composer), options, tlsWork,
                                             sessionWork);
}

std::expected<TlsRuntime, TlsDiagnostic>
detail::TlsRuntimeFactory::create(const BootstrapLease &lease, const client::EndpointOptions &endpoint,
                                  protocol::SubscriptionWriter &&composer, TlsOptions options, TlsWork &tlsWork,
                                  protocol::LiveWork &sessionWork, TlsCredentialAttempt credentials) {
    return create(lease.state_, endpoint, std::move(composer), options, tlsWork, sessionWork, credentials);
}

std::expected<TlsRuntime, TlsDiagnostic>
detail::TlsRuntimeFactory::create(RuntimeBootstrap::State *bootstrap, const client::EndpointOptions &endpoint,
                                  protocol::SubscriptionWriter &&composer, TlsOptions options, TlsWork &tlsWork,
                                  protocol::LiveWork &sessionWork, TlsCredentialAttempt credentials) {
    using Impl = TlsRuntime::Impl;
    const auto &settings = endpoint.settings();

    if (settings.security != client::EndpointSecurity::TLS || !settings.tls ||
        settings.runtime != client::EndpointRuntime::CALLER_DRIVEN_TCP || settings.retry.maximumAttempts != 1) {
        return std::unexpected(diagnostic(TlsError::INVALID_CONFIGURATION, "tls-endpoint-profile"));
    }

    const auto &tls = *settings.tls;
    const auto requirements = tlsRequirements(options.limits, options.host);

    if (!requirements) {
        return std::unexpected(fromResource(requirements.error()));
    }

    if (options.sdkInstanceBytes < requirements->sdk.instanceBytes ||
        options.sdkSharedBytes < requirements->sdk.sharedBytes || options.residentBytes == 0 ||
        tls.trustPem.size() > options.limits.trustBytes ||
        tls.referenceIdentity.size() > options.limits.identityBytes) {
        return std::unexpected(diagnostic(TlsError::RESOURCE_LIMIT, "tls-sdk-allowance"));
    }

    const auto tokenUnits = tls.tokenUnits.value_or(0);
    constexpr auto MAXIMUM_TOKEN_UNITS = (std::numeric_limits<std::size_t>::max() - 256) / 8;

    if (tokenUnits > MAXIMUM_TOKEN_UNITS ||
        (tls.credentials && (3 * tokenUnits + 64 > settings.budgets.session.outputBytes ||
                             3 * tokenUnits + 64 > settings.budgets.session.frame.maximumBodyBytes))) {
        return std::unexpected(diagnostic(TlsError::RESOURCE_LIMIT, "tls-token-envelope"));
    }

    TlsResources reservation;
    reservation.sdk = requirements->sdk;
    reservation.adapterBytes =
        sizeof(Impl) + 4 * sizeof(void *) + sizeof(detail::TlsEngine) + sizeof(detail::TlsPolicy);
    reservation.snapshotBytes = endpoint.schema().storageBytes() + sizeof(client::EndpointSettings) +
                                alignof(std::max_align_t) + 4 * sizeof(void *);

    if (!add(reservation.adapterBytes, tokenUnits * sizeof(char16_t)) ||
        !add(reservation.snapshotBytes, settings.host.capacity() + 1) ||
        !add(reservation.snapshotBytes, tls.referenceIdentity.capacity() + 1) ||
        !add(reservation.snapshotBytes, tls.trustPem.capacity() + 1)) {
        return std::unexpected(diagnostic(TlsError::RESOURCE_LIMIT, "tls-resident-arithmetic"));
    }

    reservation.totalBytes = reservation.adapterBytes;

    if (!add(reservation.totalBytes, reservation.snapshotBytes) ||
        !add(reservation.totalBytes, settings.budgets.tcp.residentBytes) ||
        !add(reservation.totalBytes, options.sdkInstanceBytes) ||
        !add(reservation.totalBytes, options.sdkSharedBytes) || reservation.totalBytes > options.residentBytes) {
        return std::unexpected(diagnostic(TlsError::RESOURCE_LIMIT, "tls-resident-allowance"));
    }

    reservation.peakBytes = reservation.totalBytes;
    const auto admission = admitTlsWork(tlsWork, requirements->shape, requirements->factoryAdmission);

    if (!admission) {
        return std::unexpected(fromResource(admission.error()));
    }

    try {
        auto impl = std::make_shared<Impl>(endpoint, *requirements, reservation, credentials);

        if (tokenUnits != 0) {
            impl->token = std::make_unique<char16_t[]>(tokenUnits);
        }

        auto schema = endpoint.schema().boundedCopy(settings.budgets.schema);

        if (!schema) {
            return std::unexpected(diagnostic(TlsError::RESOURCE_LIMIT, "tls-schema-copy"));
        }

        auto engine = detail::TlsEngine::create(tls, options.limits);

        if (!engine) {
            return std::unexpected(engine.error());
        }

        impl->engine = engine->get();
        auto sessionLimits = settings.budgets.session;
        sessionLimits.credentials = protocol::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
        auto core = detail::RuntimeCore::create(bootstrap, std::move(*schema), std::move(composer), sessionLimits,
                                                tcpOptions(settings, options.dns), sessionWork, std::move(*engine));

        if (!core) {
            return std::unexpected(fromTransport(core.error()));
        }

        impl->core = std::move(*core);
        impl->reservation.transport = impl->core->resources();

        return TlsRuntime(std::move(impl));
    } catch (const std::bad_alloc &) {
        return std::unexpected(diagnostic(TlsError::ALLOCATION_FAILURE, "tls-wrapper-storage"));
    }
}

std::expected<void, TlsDiagnostic> TlsRuntime::start() {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->rejection()) {
        return std::unexpected(*error);
    }

    if (lease->abortRequested) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-start-retired"));
    }

    const auto started = lease->core->start();

    return started ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(started.error()));
}

std::expected<TlsPollResult, TlsDiagnostic> TlsRuntime::poll(TcpWork &runtimeWork, TlsWork &tlsWork,
                                                             protocol::LiveWork &sessionWork) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    auto &state = *lease;

    if (const auto error = state.rejection()) {
        return std::unexpected(*error);
    }

    if (state.core->terminal()) {
        return TlsPollResult{TcpPollStatus::CLOSED, std::nullopt};
    }

    state.busy = true;

    /** Restore entry/busy state before releasing the operation's stable lifetime lease. */
    struct Operation {
        Impl &state; ///< Borrowed from the enclosing retained operation lease.

        ~Operation() {
            state.engine->endEntry();
            state.busy = false;
        }
    } operation{state};

    state.observeFailures();
    state.observePhase();

    if (!state.abortRequested) {
        if (const auto auth = state.authenticate(sessionWork); !auth) {
            return std::unexpected(auth.error());
        }

        if (state.credentialSuperseded)
            return TlsPollResult{TcpPollStatus::IDLE, std::nullopt};
    }

    if (!state.abortRequested && state.core->state() == TcpState::CONNECTED &&
        state.engine->needsStep(state.core->inputBegin != state.core->inputEnd, state.core->output.has_value())) {
        // The published close vector already includes the ordinary entry coordinates.
        const auto need = !state.initialFunded      ? state.requirements.initialStart
                          : state.engine->closing() ? state.requirements.close
                                                    : state.requirements.entry;

        if (const auto admitted = admitTlsWork(tlsWork, state.requirements.shape, need); !admitted) {
            return std::unexpected(fromResource(admitted.error()));
        }

        if (!state.initialFunded) {
            state.initialFunded = true;
            state.engine->fundInitial();
        }

        state.engine->fundEntry(state.engine->closing());
    }

    if (state.abortRequested) {
        state.pendingPlaintext =
            state.core->output ? state.core->output->pendingBytes().size() : state.pendingPlaintext;
        state.core->outcome.aborted |= state.explicitAbort;
        state.core->closeRequested = true; // Native cancellation is deferred to this safe common poll.
    }

    auto result = state.core->poll(runtimeWork, sessionWork);

    if (!result) {
        return std::unexpected(fromTransport(result.error()));
    }

    state.observeFailures();
    state.observePhase();

    if (state.credentialAttempt.enter && result->event && result->event->kind() == protocol::LiveEventKind::DATA &&
        state.core->authentication() != protocol::LiveAuthentication::ACKNOWLEDGED) {
        const auto denied = diagnostic(TlsError::AUTHENTICATION_DENIED, "tls-authentication-ack-required");
        state.fail(denied);

        return std::unexpected(denied);
    }

    if (!state.abortRequested && !result->event) {
        if (const auto auth = state.authenticate(sessionWork); !auth) {
            return std::unexpected(auth.error());
        }

        if (state.credentialSuperseded)
            return TlsPollResult{TcpPollStatus::IDLE, std::nullopt};
    }

    return TlsPollResult{result->status, std::move(result->event)};
}

std::expected<void, TlsDiagnostic> TlsRuntime::composeProtocol(protocol::LiveProtocolView descriptor,
                                                               protocol::LiveWork &work) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->outputRejection()) {
        return std::unexpected(*error);
    }

    const auto result = lease->core->composeProtocol(descriptor, work);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsRuntime::composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                protocol::LiveWork &work) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->outputRejection()) {
        return std::unexpected(*error);
    }

    const auto result = lease->core->composeHeartbeat(heartbeat, work);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsRuntime::describe(std::span<const std::int32_t> senderIds,
                                                        protocol::LiveWork &work) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->outputRejection()) {
        return std::unexpected(*error);
    }

    const auto result = lease->core->describe(senderIds, work);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsRuntime::subscribe(protocol::SubscriptionMessage message,
                                                         std::span<const core::SubscriptionRequest> requests,
                                                         protocol::LiveWork &work) {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->outputRejection()) {
        return std::unexpected(*error);
    }

    const auto result = lease->core->subscribe(message, requests, work);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsRuntime::requestClose() noexcept {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->rejection(true)) {
        return std::unexpected(*error);
    }

    lease->cutoff();

    return {};
}

std::expected<void, TlsDiagnostic> TlsRuntime::requestTlsClose() noexcept {
    const auto lease = impl_;

    if (!lease) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-moved-runtime"));
    }

    if (const auto error = lease->outputRejection()) {
        return std::unexpected(*error);
    }

    if (lease->core->output || lease->engine->pendingWrite()) {
        return std::unexpected(diagnostic(TlsError::INVALID_STATE, "tls-close-pending-output"));
    }

    lease->engine->requestTlsClose();
    lease->deadline(lease->closeDeadline, *lease->endpoint.settings().tls->closeTimeout);

    return {};
}

TlsState TlsRuntime::state() const noexcept {
    if (!impl_) {
        return TlsState::CLOSED;
    }

    const auto &state = *impl_;

    switch (state.core->state()) {
    case TcpState::CONFIGURED:
        return state.abortRequested ? TlsState::CLOSING : TlsState::CONFIGURED;
    case TcpState::RESOLVING:
        return state.abortRequested ? TlsState::CLOSING : TlsState::RESOLVING;
    case TcpState::CONNECTING:
        return state.abortRequested ? TlsState::CLOSING : TlsState::CONNECTING;
    case TcpState::CLOSING:
        return TlsState::CLOSING;
    case TcpState::CLOSED:
        return TlsState::CLOSED;
    case TcpState::CONNECTED:
        break;
    }

    if (state.abortRequested || state.engine->closing()) {
        return TlsState::CLOSING;
    }

    if (!state.engine->verified()) {
        return TlsState::HANDSHAKING;
    }

    const auto auth = state.core->authentication();

    if (auth == protocol::LiveAuthentication::REQUIRED || auth == protocol::LiveAuthentication::AUTHORIZATION_EMITTED ||
        (state.endpoint.settings().tls->credentials && auth != protocol::LiveAuthentication::ACKNOWLEDGED)) {
        return TlsState::AUTHENTICATING;
    }

    return TlsState::CONNECTED;
}

std::optional<TlsTerminal> TlsRuntime::terminal() const noexcept {
    if (!impl_) {
        return std::nullopt;
    }

    const auto wire = impl_->core->terminal();

    if (!wire) {
        return std::nullopt;
    }

    return TlsTerminal{impl_->primary,
                       impl_->secondary,
                       *wire,
                       wire->aborted,
                       impl_->engine->peerClosed(),
                       impl_->engine->finished() && impl_->primary.error == TlsError::NONE && !wire->aborted,
                       impl_->engine->dataPending(),
                       impl_->engine->acceptedPlaintext(),
                       impl_->core->pendingApplicationAtClose,
                       impl_->engine->pendingCiphertext()};
}

protocol::LiveSnapshot TlsRuntime::peer() const noexcept {
    return impl_ ? impl_->core->peer() : protocol::LiveSnapshot{};
}

protocol::LiveAuthentication TlsRuntime::authentication() const noexcept {
    return impl_ ? impl_->core->authentication() : protocol::LiveAuthentication::UNCHALLENGED;
}

TlsResources TlsRuntime::resources() const noexcept {
    if (!impl_) {
        return {};
    }

    auto result = impl_->reservation;
    result.transport = impl_->core->resources();

    return result;
}

ConnectionStatistics detail::TlsRuntimeFactory::statistics(const TlsRuntime &runtime) noexcept {
    const auto &owner = runtime.impl_;
    auto result = owner->core->statistics();

    if (!owner->verifiedObserved) {
        result.establishedConnections.value = 0;
        result.closedConnections.value = 0;
        result.activeConnections = 0;
        result.sourceEpoch.reset();
    }

    return result;
}

ConnectionStatistics detail::TlsRuntimeFactory::retireStatistics(TlsRuntime &runtime) noexcept {
    runtime.impl_->core->retire();

    return statistics(runtime);
}

TlsRequirements TlsRuntime::requirements() const noexcept {
    return impl_ ? impl_->requirements : TlsRequirements{};
}

TcpWork TlsRuntime::dnsProgressWorkRequirement() const noexcept {
    return impl_ ? impl_->core->dnsProgressWorkRequirement() : TcpWork{};
}

std::optional<std::chrono::steady_clock::time_point> TlsRuntime::nextWake() const noexcept {
    if (!impl_ || impl_->core->state() == TcpState::CLOSED) {
        return std::nullopt;
    }

    auto result = impl_->core->nextWake();

    for (const auto &deadline : {impl_->handshakeDeadline, impl_->authenticationDeadline, impl_->closeDeadline}) {
        if (deadline && (!result || *deadline < *result)) {
            result = deadline;
        }
    }

    return result;
}

} // namespace org::ttldtor::qdcxx::transport
