// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "subscription.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::kraken {

namespace {

using namespace std::chrono_literals;

constexpr auto MAX_TIMEOUT = std::chrono::duration_cast<std::chrono::milliseconds>(5min);
constexpr std::size_t MAX_CONSECUTIVE_FAILURES = 64;
constexpr auto MAX_REQUEST_ID = std::numeric_limits<std::int32_t>::max();

/** Test source-level bounds before constructing any transport.
 * @param settings Explicit source settings.
 * @return True when the timing and retry policies fit the supported bounds.
 */
bool valid(const SubscriptionSettings &settings) noexcept {
    const auto validTimeout = [](auto timeout) {
        return timeout > 0ms && timeout <= MAX_TIMEOUT;
    };

    return validTimeout(settings.stageTimeout) && validTimeout(settings.snapshotTimeout) &&
           validTimeout(settings.quoteFreshness) && validTimeout(settings.initialBackoff) &&
           validTimeout(settings.maximumBackoff) && settings.initialBackoff <= settings.maximumBackoff &&
           settings.consecutiveFailureLimit > 0 && settings.consecutiveFailureLimit <= MAX_CONSECUTIVE_FAILURES;
}

/** @param stage Current transport state.
 * @return True if the transport closed or will only process retirement from now on.
 */
bool cuttingOff(WssStage stage) noexcept {
    return stage == WssStage::STOPPING || stage == WssStage::FAILED || stage == WssStage::CLOSED;
}

/** @param transport Exact WSS failure, when one was reported.
 * @return True when repeating the same local inputs cannot restore an attempt.
 */
bool permanent(const std::optional<WssDiagnostic> &transport) noexcept {
    if (!transport) {
        return false;
    }

    return transport->reason == WssError::INVALID_CONFIGURATION || transport->reason == WssError::TRUST ||
           transport->reason == WssError::TLS_POLICY || transport->reason == WssError::RESOURCE;
}

} // namespace

KrakenSubscription::KrakenSubscription(SubscriptionSettings settings)
    : settings_(std::move(settings)), nextBackoff_(settings_.initialBackoff) {
    if (!valid(settings_)) {
        fail({generation_, SubscriptionError::INVALID_CONFIGURATION}, true);
    }
}

void KrakenSubscription::open() {
    if (generation_ == MAX_REQUEST_ID) {
        fail({generation_, SubscriptionError::RESOURCE}, true);

        return;
    }

    ++generation_;
    WssSettings wss;
    wss.routeHost = settings_.routeHost;
    wss.identity = settings_.identity;
    wss.target = settings_.target;
    wss.trustPem = settings_.trustPem;
    wss.port = settings_.port;
    wss.resolveTimeout = settings_.stageTimeout;
    wss.connectTimeout = settings_.stageTimeout;
    wss.tlsTimeout = settings_.stageTimeout;
    wss.upgradeTimeout = settings_.stageTimeout;
    wss.sendTimeout = settings_.stageTimeout;
    wss.receiveTimeout = MAX_TIMEOUT;

    auto started = WssSession::begin(wss);

    if (!started) {
        fail({generation_, SubscriptionError::TRANSPORT, started.error()}, permanent(started.error()));

        return;
    }

    session_ = std::move(*started);
}

void KrakenSubscription::subscribe() {
    constexpr auto request =
        R"({"method":"subscribe","params":{"channel":"ticker","symbol":["BTC/USD"],"event_trigger":"bbo","snapshot":true},"req_id":)";
    const auto payload = request + std::to_string(generation_) + '}';

    if (session_->startSend(payload) != WssAdmission::ACCEPTED) {
        failTransport();

        return;
    }

    phase_ = SubscriptionPhase::AWAITING_SNAPSHOT;
    deadline_ = Clock::now() + settings_.snapshotTimeout;
}

void KrakenSubscription::advance(std::chrono::milliseconds wait) {
    if (phase_ != SubscriptionPhase::CONNECTING) {
        if (session_->stage() == WssStage::READY && session_->startReceive() != WssAdmission::ACCEPTED) {
            failTransport();

            return;
        }

        const auto timeout = phase_ == SubscriptionPhase::ACTIVE ? quoteDeadline_ : deadline_;
        wait = std::min(wait, std::chrono::ceil<std::chrono::milliseconds>(timeout - Clock::now()));
    }

    const auto stage = session_->runOneFor(wait);
    const auto now = Clock::now();

    if (cuttingOff(stage)) {
        failTransport();

        return;
    }

    expire(now);

    if (!session_ || phase_ == SubscriptionPhase::STOPPING || stage != WssStage::READY) {
        return;
    }

    if (phase_ == SubscriptionPhase::CONNECTING) {
        subscribe();

        return;
    }

    if (auto payload = session_->takeMessage()) {
        auto message = decodeTicker(payload->text());

        if (!message) {
            fail({generation_, SubscriptionError::DECODE, {}, message.error()},
                 message.error() == DecodeError::ALLOCATION_FAILURE);

            return;
        }

        expire(Clock::now());

        if (phase_ == SubscriptionPhase::AWAITING_SNAPSHOT || phase_ == SubscriptionPhase::ACTIVE) {
            accept(std::move(*message), now);
        }
    }
}

void KrakenSubscription::accept(Message message, Clock::time_point receivedAt) noexcept {
    switch (message.kind) {
    case MessageKind::ACKNOWLEDGEMENT:
    case MessageKind::REJECTION:

        if (message.requestId != generation_) {
            fail({generation_, SubscriptionError::REQUEST_ID_MISMATCH});

            return;
        }

        if (message.kind == MessageKind::REJECTION) {
            fail({generation_, SubscriptionError::REQUEST_REJECTED});

            return;
        }

        acknowledged_ = true;

        break;
    case MessageKind::SNAPSHOT:

        if (phase_ != SubscriptionPhase::AWAITING_SNAPSHOT || bufferedSnapshot_) {
            return;
        }

        observations_ = std::move(message.entries);
        bufferedSnapshot_ = true;
        quoteDeadline_ = receivedAt + settings_.quoteFreshness;

        break;
    case MessageKind::UPDATE:

        if (phase_ == SubscriptionPhase::ACTIVE) {
            observations_ = std::move(message.entries);
            bufferedSnapshot_ = false;
            quoteDeadline_ = receivedAt + settings_.quoteFreshness;
        }

        return;
    case MessageKind::HEARTBEAT:
    case MessageKind::STATUS:
        return;
    }

    if (phase_ == SubscriptionPhase::AWAITING_SNAPSHOT && acknowledged_ && bufferedSnapshot_) {
        if (Clock::now() >= quoteDeadline_) {
            fail({generation_, SubscriptionError::QUOTE_STALE});

            return;
        }

        phase_ = SubscriptionPhase::ACTIVE;
        consecutiveFailures_ = 0;
        nextBackoff_ = settings_.initialBackoff;
        diagnostic_.reset();
    }
}

void KrakenSubscription::fail(SubscriptionDiagnostic cause, bool isPermanent) noexcept {
    diagnostic_ = std::move(cause);
    ++consecutiveFailures_;
    permanentFailure_ = isPermanent;
    discard();

    if (session_) {
        session_->requestStop();
    }

    retire();
}

void KrakenSubscription::failTransport() noexcept {
    const auto transport = session_->diagnostic();
    fail({generation_, SubscriptionError::TRANSPORT, transport}, permanent(transport));
}

void KrakenSubscription::retire() noexcept {
    if (session_ && session_->stage() == WssStage::STOPPING) {
        phase_ = SubscriptionPhase::STOPPING;

        return;
    }

    session_.reset();

    if (stopRequested_) {
        phase_ = SubscriptionPhase::STOPPED;
    } else if (permanentFailure_ || consecutiveFailures_ == settings_.consecutiveFailureLimit) {
        phase_ = SubscriptionPhase::FAILED;
    } else {
        phase_ = SubscriptionPhase::BACKOFF;
        deadline_ = Clock::now() + nextBackoff_;
        nextBackoff_ = std::min(nextBackoff_ * 2, settings_.maximumBackoff);
    }
}

void KrakenSubscription::discard() noexcept {
    observations_.clear();
    nextObservation_ = 0;
    acknowledged_ = false;
    bufferedSnapshot_ = false;
}

void KrakenSubscription::expire(Clock::time_point now) noexcept {
    if (phase_ == SubscriptionPhase::AWAITING_SNAPSHOT && now >= deadline_) {
        fail({generation_, SubscriptionError::SNAPSHOT_TIMEOUT});
    } else if (phase_ == SubscriptionPhase::ACTIVE && now >= quoteDeadline_) {
        fail({generation_, SubscriptionError::QUOTE_STALE});
    }
}

void KrakenSubscription::runOneFor(std::chrono::milliseconds wait) {
    wait = std::clamp(wait, 0ms, MAX_TIMEOUT);
    expire(Clock::now());

    switch (phase_) {
    case SubscriptionPhase::STOPPED:
    case SubscriptionPhase::FAILED:
        return;
    case SubscriptionPhase::STOPPING:
        (void)session_->runOneFor(wait);
        retire();

        return;
    case SubscriptionPhase::BACKOFF:
        std::this_thread::sleep_until(std::min(deadline_, Clock::now() + wait));

        if (Clock::now() >= deadline_) {
            phase_ = SubscriptionPhase::CONNECTING;
        }

        return;
    case SubscriptionPhase::ACTIVE:

        if (!observations_.empty()) {
            return;
        }

        break;
    case SubscriptionPhase::CONNECTING:
    case SubscriptionPhase::AWAITING_SNAPSHOT:
        break;
    }

    try {
        if (session_) {
            advance(wait);
        } else {
            open();
        }
    } catch (const std::bad_alloc &) {
        fail({generation_, SubscriptionError::RESOURCE}, true);
    }
}

void KrakenSubscription::requestStop() noexcept {
    stopRequested_ = true;
    discard();

    if (session_) {
        session_->requestStop();
    }

    retire();
}

SubscriptionPhase KrakenSubscription::phase() const noexcept {
    return phase_;
}

const std::optional<SubscriptionDiagnostic> &KrakenSubscription::diagnostic() const noexcept {
    return diagnostic_;
}

std::optional<SourceObservation> KrakenSubscription::takeObservation() noexcept {
    expire(Clock::now());

    if (phase_ != SubscriptionPhase::ACTIVE || observations_.empty()) {
        return std::nullopt;
    }

    auto result = SourceObservation{generation_, std::move(observations_[nextObservation_++]), bufferedSnapshot_};

    if (nextObservation_ == observations_.size()) {
        observations_.clear();
        nextObservation_ = 0;
    }

    return result;
}

} // namespace org::ttldtor::qdcxx::kraken
