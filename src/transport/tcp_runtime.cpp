// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tcp_runtime.hpp"

#include "runtime_core.hpp"
#include "tcp_runtime_factory.hpp"

#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

/** Preserve the checked public rejection for an absent moved-from owner.
 * @return Detached static evidence with the existing category and checkpoint.
 */
TcpDiagnostic movedRuntimeDiagnostic() noexcept {
    return {TcpError::INVALID_STATE, "moved-runtime"};
}

} // namespace

TcpRuntime::TcpRuntime(std::unique_ptr<detail::RuntimeCore> impl) noexcept : impl_(std::move(impl)) {
}

TcpRuntime::TcpRuntime(TcpRuntime &&other) noexcept = default;
TcpRuntime &TcpRuntime::operator=(TcpRuntime &&other) noexcept = default;
TcpRuntime::~TcpRuntime() = default;

std::expected<TcpRuntime, TcpDiagnostic> TcpRuntime::create(RuntimeBootstrap &bootstrap, core::LocalSchema &&schema,
                                                            protocol::SubscriptionWriter &&composer,
                                                            protocol::LiveLimits sessionLimits, TcpOptions options,
                                                            protocol::LiveWork &work) {
    return detail::TcpRuntimeFactory::create(bootstrap.state_.get(), std::move(schema), std::move(composer),
                                             sessionLimits, options, work);
}

std::expected<TcpRuntime, TcpDiagnostic>
detail::TcpRuntimeFactory::create(const BootstrapLease &lease, core::LocalSchema &&schema,
                                  protocol::SubscriptionWriter &&composer, protocol::LiveLimits sessionLimits,
                                  TcpOptions options, protocol::LiveWork &work) {
    return create(lease.state_, std::move(schema), std::move(composer), sessionLimits, options, work);
}

std::expected<TcpRuntime, TcpDiagnostic>
detail::TcpRuntimeFactory::create(RuntimeBootstrap::State *bootstrap, core::LocalSchema &&schema,
                                  protocol::SubscriptionWriter &&composer, protocol::LiveLimits sessionLimits,
                                  TcpOptions options, protocol::LiveWork &work) {
    auto impl =
        detail::RuntimeCore::create(bootstrap, std::move(schema), std::move(composer), sessionLimits, options, work);

    if (!impl) {
        return std::unexpected(impl.error());
    }

    return TcpRuntime(std::move(*impl));
}

std::expected<protocol::LiveWork, TcpDiagnostic>
TcpRuntime::dnsPreparationWorkRequirement(std::size_t serverCount) noexcept {
    return detail::RuntimeCore::dnsPreparationWorkRequirement(serverCount);
}

std::expected<void, TcpDiagnostic> TcpRuntime::start() {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->start();
}

std::expected<TcpPollResult, TcpDiagnostic> TcpRuntime::poll(TcpWork &work, protocol::LiveWork &sessionWork) {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->poll(work, sessionWork);
}

std::expected<void, TcpDiagnostic> TcpRuntime::composeProtocol(protocol::LiveProtocolView descriptor,
                                                               protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->composeProtocol(descriptor, work);
}

std::expected<void, TcpDiagnostic> TcpRuntime::composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->composeHeartbeat(heartbeat, work);
}

std::expected<void, TcpDiagnostic> TcpRuntime::describe(std::span<const std::int32_t> senderIds,
                                                        protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->describe(senderIds, work);
}

std::expected<void, TcpDiagnostic> TcpRuntime::subscribe(protocol::SubscriptionMessage message,
                                                         std::span<const core::SubscriptionRequest> requests,
                                                         protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->subscribe(message, requests, work);
}

std::expected<void, TcpDiagnostic> TcpRuntime::requestClose() noexcept {
    if (!impl_) {
        return std::unexpected(movedRuntimeDiagnostic());
    }

    return impl_->requestClose();
}

TcpState TcpRuntime::state() const noexcept {
    return impl_ ? impl_->state() : TcpState::CLOSED;
}

std::optional<TcpTerminal> TcpRuntime::terminal() const noexcept {
    return impl_ ? impl_->terminal() : std::nullopt;
}

protocol::LiveSnapshot TcpRuntime::peer() const noexcept {
    return impl_ ? impl_->peer() : protocol::LiveSnapshot{};
}

protocol::LiveAuthentication TcpRuntime::authentication() const noexcept {
    return impl_ ? impl_->authentication() : protocol::LiveAuthentication::UNCHALLENGED;
}

TcpResources TcpRuntime::resources() const noexcept {
    return impl_ ? impl_->resources() : TcpResources{};
}

std::optional<std::chrono::steady_clock::time_point> TcpRuntime::nextWake() const noexcept {
    return impl_ ? impl_->nextWake() : std::nullopt;
}

TcpWork TcpRuntime::dnsProgressWorkRequirement() const noexcept {
    return impl_ ? impl_->dnsProgressWorkRequirement() : TcpWork{};
}

} // namespace org::ttldtor::qdcxx::transport
