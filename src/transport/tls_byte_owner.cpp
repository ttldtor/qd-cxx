// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_byte_owner.hpp"

#include "deadline.hpp"

#include <climits>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

/** Translate existing common evidence without retaining dynamic text.
 * @param cause Detached common native/owner cause.
 * @return TLS-facing category with complete original common evidence.
 */
TlsDiagnostic fromTransport(TcpDiagnostic cause) noexcept {
    auto error = TlsError::TRANSPORT_FAILURE;

    switch (cause.error) {
    case TcpError::NONE:
        error = TlsError::NONE;
        break;
    case TcpError::WRONG_THREAD:
        error = TlsError::WRONG_THREAD;
        break;
    case TcpError::INVALID_STATE:
        error = TlsError::INVALID_STATE;
        break;
    case TcpError::INVALID_CONFIGURATION:
        error = TlsError::INVALID_CONFIGURATION;
        break;
    case TcpError::RESOURCE_LIMIT:
        error = TlsError::RESOURCE_LIMIT;
        break;
    case TcpError::WORK_LIMIT:
        error = TlsError::WORK_LIMIT;
        break;
    case TcpError::ALLOCATION_FAILURE:
        error = TlsError::ALLOCATION_FAILURE;
        break;
    case TcpError::DEADLINE:
        error = TlsError::DEADLINE;
        break;
    case TcpError::CLOSE_FAILURE:
        error = TlsError::CLOSE_FAILURE;
        break;
    default:
        break;
    }

    auto result = TlsDiagnostic{error, cause.stage};
    result.transport = cause;

    return result;
}

/** Translate the existing pure same-image requirement/admission refusal.
 * @param cause Existing resource-domain refusal.
 * @return Detached work, arithmetic or configuration evidence.
 */
TlsDiagnostic fromResource(TlsResourceDiagnostic cause) noexcept {
    const auto error = cause.error == TlsResourceError::WORK_LIMIT            ? TlsError::WORK_LIMIT
                       : cause.error == TlsResourceError::ARITHMETIC_OVERFLOW ? TlsError::RESOURCE_LIMIT
                                                                              : TlsError::INVALID_CONFIGURATION;

    return {error, cause.stage};
}

/** Add one bounded requested allocation without wrapping.
 * @param total Current reservation, modified only on success.
 * @param bytes Additional requested bytes.
 * @return True for a representable sum.
 */
bool add(std::size_t &total, std::size_t bytes) noexcept {
    if (bytes > std::numeric_limits<std::size_t>::max() - total) {
        return false;
    }

    total += bytes;

    return true;
}

/** Validate an explicitly supplied cooperative phase timeout before allocation.
 * @param value Optional positive millisecond duration.
 * @return True for an explicit representable duration within the existing profile.
 */
bool validTimeout(std::optional<std::chrono::milliseconds> value) noexcept {
    return value && value->count() > 0 && value->count() <= INT_MAX && deadlineAfter(Clock::now(), *value);
}

} // namespace

TlsByteOwner::TlsByteOwner(const client::EndpointTls &settings, TlsRequirements requirements, TlsResources resources)
    : settings_(settings), requirements_(requirements), disposal_(requirements.retirement), resources_(resources) {
}

TlsByteOwner::~TlsByteOwner() {
    if (owner_ != std::this_thread::get_id() || busy_) {
        std::terminate();
    }

    bytes_.reset();
    disposal_ = {};
}

std::optional<TlsDiagnostic> TlsByteOwner::rejection(bool allowBusy) const noexcept {
    if (owner_ != std::this_thread::get_id()) {
        return TlsDiagnostic{TlsError::WRONG_THREAD, "tls-byte-owner"};
    }

    if (busy_ && !allowBusy) {
        return TlsDiagnostic{TlsError::INVALID_STATE, "tls-byte-busy"};
    }

    return std::nullopt;
}

std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
TlsByteOwner::create(RuntimeBootstrap &bootstrap, TcpOptions native, RawByteLimits limits,
                     const client::EndpointTls &settings, TlsOptions options, protocol::LiveWork &byteWork,
                     TlsWork &tlsWork) {
    return createImpl(&bootstrap, nullptr, native, limits, settings, options, byteWork, tlsWork);
}

std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
TlsByteOwner::create(const BootstrapLease &bootstrap, TcpOptions native, RawByteLimits limits,
                     const client::EndpointTls &settings, TlsOptions options, protocol::LiveWork &byteWork,
                     TlsWork &tlsWork) {
    return createImpl(nullptr, &bootstrap, native, limits, settings, options, byteWork, tlsWork);
}

std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
TlsByteOwner::createImpl(RuntimeBootstrap *facade, const BootstrapLease *lease, TcpOptions native, RawByteLimits limits,
                         const client::EndpointTls &settings, TlsOptions options, protocol::LiveWork &byteWork,
                         TlsWork &tlsWork) {
    if (settings.credentials || settings.credentials.use_count() != 0 || settings.tokenUnits ||
        settings.authenticationTimeout || !validTimeout(settings.handshakeTimeout) ||
        !validTimeout(settings.closeTimeout)) {
        return std::unexpected(TlsDiagnostic{TlsError::INVALID_CONFIGURATION, "tls-byte-configuration"});
    }

    const auto requirements = tlsRequirements(options.limits, options.host);

    if (!requirements) {
        return std::unexpected(fromResource(requirements.error()));
    }

    if (settings.trustPem.size() > options.limits.trustBytes ||
        settings.referenceIdentity.size() > options.limits.identityBytes ||
        options.sdkInstanceBytes < requirements->sdk.instanceBytes ||
        options.sdkSharedBytes < requirements->sdk.sharedBytes) {
        return std::unexpected(TlsDiagnostic{TlsError::RESOURCE_LIMIT, "tls-byte-sdk-allowance"});
    }

    TlsResources reservation;
    reservation.sdk = requirements->sdk;
    reservation.adapterBytes = sizeof(TlsByteOwner) + sizeof(TlsEngine) + sizeof(TlsPolicy);
    // Include inline-string slack conservatively; inspect the qualified copies before any SDK allocation.
    const auto inlineBytes = std::string{}.capacity();
    reservation.snapshotBytes = settings.trustPem.size() + 1;

    if (!add(reservation.snapshotBytes, settings.referenceIdentity.size() + 1) ||
        !add(reservation.snapshotBytes, 2 * inlineBytes)) {
        return std::unexpected(TlsDiagnostic{TlsError::RESOURCE_LIMIT, "tls-byte-resident-arithmetic"});
    }

    reservation.totalBytes = reservation.adapterBytes;

    if (!add(reservation.totalBytes, reservation.snapshotBytes) ||
        !add(reservation.totalBytes, native.limits.residentBytes) ||
        !add(reservation.totalBytes, options.sdkInstanceBytes) ||
        !add(reservation.totalBytes, options.sdkSharedBytes) || reservation.totalBytes > options.residentBytes) {
        return std::unexpected(TlsDiagnostic{TlsError::RESOURCE_LIMIT, "tls-byte-resident-allowance"});
    }

    const auto copyBytes = reservation.adapterBytes + reservation.snapshotBytes;
    constexpr std::size_t PREPARATION_PASSES = 3; // Owned settings copy, zeroed fields and setup traversal.

    if (copyBytes > std::numeric_limits<std::size_t>::max() / PREPARATION_PASSES ||
        byteWork.bytes < copyBytes * PREPARATION_PASSES || byteWork.operations == 0) {
        return std::unexpected(TlsDiagnostic{TlsError::WORK_LIMIT, "tls-byte-preparation-work"});
    }

    const auto admission = admitTlsWork(tlsWork, requirements->shape, requirements->factoryAdmission);

    if (!admission) {
        return std::unexpected(fromResource(admission.error()));
    }

    --byteWork.operations;
    byteWork.bytes -= copyBytes * PREPARATION_PASSES;
    reservation.peakBytes = reservation.totalBytes;

    try {
        auto result = std::unique_ptr<TlsByteOwner>(new TlsByteOwner(settings, *requirements, reservation));

        if (result->settings_.trustPem.capacity() + 1 > reservation.snapshotBytes ||
            result->settings_.referenceIdentity.capacity() + 1 >
                reservation.snapshotBytes - (result->settings_.trustPem.capacity() + 1)) {
            return std::unexpected(TlsDiagnostic{TlsError::RESOURCE_LIMIT, "tls-byte-copy-capacity"});
        }

        auto engine = TlsEngine::create(result->settings_, options.limits);

        if (!engine) {
            return std::unexpected(engine.error());
        }

        result->engine_ = engine->get();
        auto bytes = lease ? RawByteOwner::create(*lease, native, limits, byteWork, std::move(*engine))
                           : RawByteOwner::create(*facade, native, limits, byteWork, std::move(*engine));

        if (!bytes) {
            return std::unexpected(fromTransport(bytes.error()));
        }

        result->bytes_ = std::move(*bytes);
        result->resources_.transport = result->bytes_->core().resources();

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(TlsDiagnostic{TlsError::ALLOCATION_FAILURE, "tls-byte-storage"});
    }
}

void TlsByteOwner::observeFailure() noexcept {
    if (failure_) {
        return;
    }

    const auto engineFailure = engine_->diagnostic();
    const auto cause =
        engineFailure.error != TlsError::NONE ? engineFailure : fromTransport(bytes_->core().outcome.primary);

    if (cause.error != TlsError::NONE) {
        failure_ = cause;
    }
}

bool TlsByteOwner::installDeadline(std::optional<Clock::time_point> &target,
                                   std::chrono::milliseconds duration) noexcept {
    target = deadlineAfter(Clock::now(), duration);

    if (!target) {
        if (!failure_) {
            failure_ = TlsDiagnostic{TlsError::RESOURCE_LIMIT, "tls-byte-deadline-range"};
        }

        abortRequested_ = true;
    }

    return target.has_value();
}

void TlsByteOwner::observePhase() noexcept {
    if (bytes_->core().terminal()) {
        handshakeDeadline_.reset();
        closeDeadline_.reset();

        return;
    }

    const auto now = Clock::now();

    if (bytes_->core().state() == TcpState::CONNECTED && !handshakeDeadline_ && !engine_->verified()) {
        (void)installDeadline(handshakeDeadline_, *settings_.handshakeTimeout);
    }

    if (engine_->verified()) {
        handshakeDeadline_.reset();
    }

    if (engine_->closing() && !closeDeadline_) {
        (void)installDeadline(closeDeadline_, *settings_.closeTimeout);
    }

    if ((handshakeDeadline_ && now >= *handshakeDeadline_) || (closeDeadline_ && now >= *closeDeadline_)) {
        if (!failure_) {
            failure_ = TlsDiagnostic{TlsError::DEADLINE, "tls-byte-phase-deadline"};
        }

        abortRequested_ = true;
    }
}

std::expected<void, TlsDiagnostic> TlsByteOwner::start() {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (abortRequested_) {
        return std::unexpected(TlsDiagnostic{TlsError::INVALID_STATE, "tls-byte-start-cutoff"});
    }

    const auto result = bytes_->start();

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<TcpPollResult, TlsDiagnostic> TlsByteOwner::poll(TcpWork &nativeWork, protocol::LiveWork &byteWork,
                                                               TlsWork &tlsWork) {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (bytes_->core().terminal()) {
        return TcpPollResult{TcpPollStatus::CLOSED, std::nullopt};
    }

    busy_ = true;

    /** Expire the copied per-entry permit on every exit, including owner/native/work refusal. */
    struct Turn {
        TlsByteOwner &state; ///< Stable owner, alive through the enclosing serialized call.

        ~Turn() {
            state.engine_->endEntry();
            state.busy_ = false;
        }
    } turn{*this};

    observeFailure();
    observePhase();
    auto &core = bytes_->core();

    if (!abortRequested_ && !core.closeRequested && core.state() == TcpState::CONNECTED &&
        engine_->needsStep(core.inputBegin != core.inputEnd, bytes_->pendingOutput() != 0)) {
        const auto need = !initialFunded_      ? requirements_.initialStart
                          : engine_->closing() ? requirements_.close
                                               : requirements_.entry;
        const auto admitted = admitTlsWork(tlsWork, requirements_.shape, need);

        if (!admitted) {
            return std::unexpected(fromResource(admitted.error()));
        }

        if (!initialFunded_) {
            initialFunded_ = true;
            engine_->fundInitial();
        }

        engine_->fundEntry(engine_->closing());
    }

    if (abortRequested_) {
        (void)bytes_->requestClose();
    }

    auto result = bytes_->poll(nativeWork, byteWork);
    observeFailure();
    observePhase();

    return result ? std::expected<TcpPollResult, TlsDiagnostic>{std::move(*result)}
                  : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsByteOwner::write(std::span<const std::byte> bytes, protocol::LiveWork &work) {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (abortRequested_ || engine_->closing()) {
        return std::unexpected(TlsDiagnostic{TlsError::INVALID_STATE, "tls-byte-output-cutoff"});
    }

    const auto result = bytes_->write(bytes, work);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<std::span<const std::byte>, TlsDiagnostic> TlsByteOwner::input() const noexcept {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    const auto result = bytes_->input();

    return result ? std::expected<std::span<const std::byte>, TlsDiagnostic>{*result}
                  : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsByteOwner::consumeInput(std::size_t bytes) noexcept {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    const auto result = bytes_->consumeInput(bytes);

    return result ? std::expected<void, TlsDiagnostic>{} : std::unexpected(fromTransport(result.error()));
}

std::expected<void, TlsDiagnostic> TlsByteOwner::requestClose() noexcept {
    if (const auto error = rejection(true)) {
        return std::unexpected(*error);
    }

    if (!bytes_->core().terminal()) {
        abortRequested_ = true;
    }

    return {};
}

std::expected<void, TlsDiagnostic> TlsByteOwner::requestTlsClose() noexcept {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (abortRequested_ || bytes_->core().closeRequested || engine_->closing() || bytes_->pendingOutput() != 0 ||
        engine_->pendingWrite()) {
        return std::unexpected(TlsDiagnostic{TlsError::INVALID_STATE, "tls-byte-close-output"});
    }

    engine_->requestTlsClose();

    if (!installDeadline(closeDeadline_, *settings_.closeTimeout)) {
        return std::unexpected(*failure_);
    }

    return {};
}

RawByteOwner &TlsByteOwner::bytes() noexcept {
    return *bytes_;
}

TlsDiagnostic TlsByteOwner::diagnostic() const noexcept {
    if (failure_) {
        return *failure_;
    }

    const auto engineFailure = engine_->diagnostic();

    if (engineFailure.error != TlsError::NONE) {
        return engineFailure;
    }

    return fromTransport(bytes_->core().outcome.primary);
}

TlsResources TlsByteOwner::resources() noexcept {
    resources_.transport = bytes_->core().resources();

    return resources_;
}

} // namespace org::ttldtor::qdcxx::transport::detail
