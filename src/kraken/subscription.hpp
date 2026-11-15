// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "ticker_decoder.hpp"
#include "wss_session.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace org::ttldtor::qdcxx::kraken {

/** Explicit inputs for one process-owned BTC/USD source. Durations use a monotonic clock. */
struct SubscriptionSettings {
    std::string routeHost = "ws.kraken.com";          ///< Host passed to the resolver.
    std::string identity = "ws.kraken.com";           ///< DNS certificate identity, SNI and upgrade authority.
    std::string target = "/v2";                       ///< WebSocket upgrade request target.
    std::string trustPem;                             ///< Explicit, owned trust anchors.
    std::uint16_t port = 443;                         ///< Numeric route port.
    std::chrono::milliseconds stageTimeout{5000};     ///< Per-stage setup and subscribe write deadline.
    std::chrono::milliseconds snapshotTimeout{10000}; ///< Deadline for both matching ACK and snapshot.
    std::chrono::milliseconds quoteFreshness{30000};  ///< Maximum age since the last admitted quote message.
    std::chrono::milliseconds initialBackoff{250};    ///< Delay after the first failed attempt.
    std::chrono::milliseconds maximumBackoff{5000};   ///< Cap on the exponential retry delay.
    std::size_t consecutiveFailureLimit = 8;          ///< Terminal limit, reset by an admitted initial snapshot.
};

/** Phase of source admission, recovery or termination. */
enum class SubscriptionPhase {
    CONNECTING,        ///< Not yet subscribed. The next drive may start a new WSS attempt.
    AWAITING_SNAPSHOT, ///< Both the correlated ACK and a snapshot are required.
    ACTIVE,            ///< The current generation can supply observations.
    BACKOFF,           ///< No WSS attempt is retained. A retry delay is in effect.
    STOPPING,          ///< Delivery cut off while the current WSS attempt retires.
    STOPPED,           ///< Owner requested termination and transport retired.
    FAILED             ///< Recovery exhausted or setup permanently refused.
};

/** Reason for ending an attempt or refusing the source configuration. */
enum class SubscriptionError {
    INVALID_CONFIGURATION, ///< Source timers or failure budget are outside the supported range.
    TRANSPORT,             ///< A transport phase or native operation failed.
    DECODE,                ///< The ticker decoder rejected a message.
    REQUEST_REJECTED,      ///< Provider refused the correlated subscribe request.
    REQUEST_ID_MISMATCH,   ///< An acknowledgement lacked the identifier of this attempt.
    SNAPSHOT_TIMEOUT,      ///< Matching ACK and initial snapshot did not both arrive in time.
    QUOTE_STALE,           ///< The quote deadline expired without a new admitted data message.
    RESOURCE,              ///< Source allocation failed or the generation space was exhausted.
};

/** Last failed generation and its bounded causality information. */
struct SubscriptionDiagnostic {
    std::uint64_t generation;                 ///< Failed attempt number, or zero before the first attempt.
    SubscriptionError reason;                 ///< Source-level reason for the cutoff.
    std::optional<WssDiagnostic> transport{}; ///< Exact native transport receipt when available.
    std::optional<DecodeError> decode{};      ///< Exact ticker decode result when applicable.
};

/** An owned BBO row from the current generation. */
struct SourceObservation {
    std::uint64_t generation; ///< Attempt identity. Immutable after the row is transferred to the caller.
    Observation observation;  ///< Owned row from the fixed BTC/USD ticker subscription.
    bool initialSnapshot;     ///< True for rows of the first accepted snapshot after reconnect.
};

/** Owner-thread source driver. Serially drive, stop and destroy it from the same thread. */
class KrakenSubscription final {
    using Clock = std::chrono::steady_clock;

    SubscriptionSettings settings_;
    std::unique_ptr<WssSession> session_;
    SubscriptionPhase phase_ = SubscriptionPhase::CONNECTING;
    std::optional<SubscriptionDiagnostic> diagnostic_;
    std::vector<Observation> observations_;
    std::size_t nextObservation_ = 0;
    std::uint64_t generation_ = 0;
    std::size_t consecutiveFailures_ = 0;
    std::chrono::milliseconds nextBackoff_;
    Clock::time_point deadline_{};
    Clock::time_point quoteDeadline_{};
    bool acknowledged_ = false;
    bool bufferedSnapshot_ = false;
    bool stopRequested_ = false;
    bool permanentFailure_ = false;

    /** Open one attempt with a new generation and explicit WSS settings. */
    void open();
    /** Send the exact public ticker subscription after the verified upgrade. */
    void subscribe();
    /** @param wait Maximum wait for transport work, clipped to the next source deadline. */
    void advance(std::chrono::milliseconds wait);
    /** Admit one owned decoder result under the active generation.
     * @param message Validated provider message with its owned payload.
     * @param receivedAt Monotonic receive time for freshness calculation.
     */
    void accept(Message message, Clock::time_point receivedAt) noexcept;
    /** Invalidate delivery and request retirement of the old transport.
     * @param cause Diagnostic for the attempt being cut off.
     * @param permanent True for configuration, local resource or policy failures.
     */
    void fail(SubscriptionDiagnostic cause, bool permanent = false) noexcept;
    /** Handle a completed or stopping transport operation. */
    void failTransport() noexcept;
    /** Complete stop or schedule a retry once no native stages remain. */
    void retire() noexcept;
    /** Invalidate every unconsumed row and handshake flag of the attempt. */
    void discard() noexcept;
    /** Enforce admission and freshness even when the caller leaves a row unconsumed.
     * @param now Current monotonic time.
     */
    void expire(Clock::time_point now) noexcept;

    public:
    /** Construct a deferred source. The first progress call starts route resolution.
     * @param settings Owned configuration, including trust bytes retained for retries.
     */
    explicit KrakenSubscription(SubscriptionSettings settings);

    KrakenSubscription(const KrakenSubscription &) = delete;
    KrakenSubscription &operator=(const KrakenSubscription &) = delete;

    /** Drive at most one bounded wait. A zero or negative value polls.
     * @param wait Maximum owner-thread wait, clipped to the next stage or retry deadline.
     */
    void runOneFor(std::chrono::milliseconds wait);

    /** Cut off delivery and initiate cancellation; drive until STOPPED to observe retirement. */
    void requestStop() noexcept;

    /** @return Current phase on the owner thread. */
    SubscriptionPhase phase() const noexcept;

    /** @return Borrowed last failure, cleared after successful source admission. */
    const std::optional<SubscriptionDiagnostic> &diagnostic() const noexcept;

    /** @return Next owned current-generation row, or no value outside active delivery. */
    std::optional<SourceObservation> takeObservation() noexcept;
};

} // namespace org::ttldtor::qdcxx::kraken
