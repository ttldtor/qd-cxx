// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#ifndef ORG_TTLDTOR_QDCXX_KRAKEN_WSS_SESSION_HPP
#define ORG_TTLDTOR_QDCXX_KRAKEN_WSS_SESSION_HPP

#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::kraken {

/** Borrowed settings used during synchronous admission and copied where needed for the attempt. */
struct WssSettings {
    std::string_view routeHost;      ///< DNS name or numeric address passed to the system resolver.
    std::string_view identity;       ///< DNS SAN reference name and SNI, independently configured from the route.
    std::string_view target = "/v2"; ///< Explicit HTTP request target, including optional query.
    std::string_view trustPem;       ///< Explicit, ordinary CERTIFICATE blocks. System trust is not loaded.
    std::uint16_t port = 443;        ///< Numeric connect port; included in the HTTP authority when non-default.
    std::chrono::milliseconds resolveTimeout{5000};  ///< Time available for admitting the resolver result.
    std::chrono::milliseconds connectTimeout{5000};  ///< Shared budget across all admitted endpoints.
    std::chrono::milliseconds tlsTimeout{5000};      ///< Verified TLS handshake budget.
    std::chrono::milliseconds upgradeTimeout{5000};  ///< HTTP/WebSocket handshake budget.
    std::chrono::milliseconds receiveTimeout{30000}; ///< Budget for a complete admitted receive operation.
    std::chrono::milliseconds sendTimeout{5000};     ///< Budget for writing one admitted text message.
    std::chrono::milliseconds closeTimeout{5000};    ///< Budget for local close and transport teardown.
    std::size_t maxMessageBytes = 64 * 1024;         ///< Positive inbound and outbound text payload limit.
};

/** Current owner-thread state, or the phase associated with a failure. */
enum class WssStage {
    CONFIGURATION, ///< Synchronous TLS policy and option admission.
    RESOLVE,       ///< Native system resolver operation outstanding.
    CONNECT,       ///< Native TCP connect outstanding.
    TLS,           ///< TLS handshake outstanding.
    UPGRADE,       ///< Beast WebSocket upgrade outstanding.
    READY,         ///< Verified upgraded connection with no active operation and at most one result.
    RECEIVING,     ///< One read, including Beast's automatic control traffic, is outstanding.
    SENDING,       ///< One owned text write is outstanding.
    CLOSING,       ///< A local WebSocket close and its transport teardown are outstanding.
    STOPPING,      ///< Cutoff imposed, waiting for an outstanding completion.
    CLOSED,        ///< An orderly WebSocket close completed and native backing was retired.
    FAILED,        ///< Terminal failure or cancellation with native handlers retired.
};

/** Reason an attempt did not reach or retain READY, including orderly closure. */
enum class WssError {
    INVALID_CONFIGURATION, ///< A finite setting or input syntax check failed.
    TRUST,                 ///< Explicit trust admission or loading failed.
    TLS_POLICY,            ///< A private provider, context or TLS invariant could not be established.
    TLS_VERIFICATION,      ///< OpenSSL refused the peer certificate, chain or reference identity.
    RESOURCE,              ///< An application resource limit or ordinary allocation failed.
    NATIVE,                ///< The network, TLS stack or HTTP/WebSocket parser failed.
    MESSAGE_LIMIT,         ///< The peer exceeded the admitted WebSocket message budget.
    PROTOCOL,              ///< Beast rejected a WebSocket protocol violation.
    UNSUPPORTED_EXTENSION, ///< The peer advertised an extension the client did not offer.
    UNSUPPORTED_BINARY,    ///< An otherwise valid binary message is outside the text profile.
    PEER_EOF,              ///< The transport ended without a completed WebSocket close.
    GRACEFUL_CLOSE,        ///< An orderly WebSocket close completed.
    TIMEOUT,               ///< A phase deadline expired before the result was admitted.
    CANCELLED,             ///< The owner cut off the attempt.
};

/** Bounded failure receipt with no copied peer text or credential content. */
struct WssDiagnostic {
    WssStage stage;                     ///< Phase at cutoff or error, before STOPPING/FAILED.
    WssError reason;                    ///< Stable classification for the private adapter.
    boost::system::error_code native{}; ///< Optional exact library error code.
    long verifyError = 0;               ///< OpenSSL verification code for TLS_VERIFICATION.
    unsigned httpStatus = 0;            ///< HTTP status reported for a completed upgrade refusal.
    std::uint16_t closeCode = 0;        ///< Validated peer WebSocket close code when available.
};

/** Outcome of attempting to start one post-upgrade operation. */
enum class WssAdmission {
    ACCEPTED,        ///< The operation is armed against the stable owner.
    NOT_READY,       ///< The connection has not upgraded or is terminal.
    BUSY,            ///< An operation or an unread result occupies the one available slot.
    INVALID_PAYLOAD, ///< Output is empty, exceeds the limit or is not UTF-8.
    FAILED,          ///< Native initiation failed and the attempt is stopping or terminal.
};

/** Immutable text bytes owned independently of the session and the provider decoder. */
class WssMessage final {
    friend class WssSession;

    std::string bytes_;

    /** Copy the exact bytes of a verified complete text message.
     * @param text Borrowed admitted payload.
     */
    explicit WssMessage(std::string_view text);

    public:
    /** View the message without transferring its storage.
     * @return UTF-8 bytes held by this value until moved from or destroyed.
     */
    std::string_view text() const noexcept;
};

/** One private, single-thread-driven connection attempt in the dedicated WSS process.
 * The caller serializes creation, progress, stop and destruction on its owner thread.
 */
class WssSession final {
    class Impl;
    std::unique_ptr<Impl> impl_;

    /** Adopt the stable owner before exposing an attempt.
     * @param impl Owned state retaining native operation backing.
     */
    explicit WssSession(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Admit settings and initiate resolution for one attempt.
     * @param settings Borrowed inputs that may be released when begin returns.
     * @return Owned attempt on admission or a synchronous diagnostic.
     */
    static std::expected<std::unique_ptr<WssSession>, WssDiagnostic> begin(const WssSettings &settings);

    /** Close the native socket and drain outstanding callbacks before destroying backing.
     * The native system DNS worker may still need to return before teardown can finish.
     */
    ~WssSession();

    /** Copying an executor/socket owner is unsupported. */
    WssSession(const WssSession &) = delete;
    /** Assigning an executor/socket owner is unsupported. */
    WssSession &operator=(const WssSession &) = delete;

    /** Drive at most one handler and advance the state machine.
     * @param wait Nonnegative maximum executor wait, clipped to the active phase deadline.
     * @return Current stage after admitting the completion, deadline or stop.
     */
    WssStage runOneFor(std::chrono::milliseconds wait);

    /** Arm one bounded Beast read. An empty message completes without creating a result.
     * @return Admission, including BUSY while an unread message occupies the slot.
     */
    WssAdmission startReceive();

    /** Transfer the one completed nonempty text message, if any.
     * @return Owning payload independent of this session, or no value.
     */
    std::optional<WssMessage> takeMessage() noexcept;

    /** Copy one complete output message and arm a bounded Beast write.
     * @param text Borrowed nonempty UTF-8, which may be released when this call returns.
     * @return Admission or refusal. Completion does not acknowledge receipt by the peer.
     */
    WssAdmission startSend(std::string_view text);

    /** Start an orderly normal close from an idle ready connection.
     * @return Admission. Drive the owner to CLOSED or FAILED for the actual receipt.
     */
    WssAdmission startClose();

    /** Cut off further operations and delivery, cancel resolution and close the native socket. */
    void requestStop() noexcept;

    /** Inspect the current phase without driving the executor.
     * @return Current stage.
     */
    WssStage stage() const noexcept;

    /** Inspect the cutoff or failure retained by this attempt.
     * @return Diagnostic after cutoff or error, including during retirement, or no value.
     */
    const std::optional<WssDiagnostic> &diagnostic() const noexcept;

    /** Read back the Beast message limit while native state is live, or the retained setting after retirement.
     * @return The configured positive receive-message limit in bytes.
     */
    std::size_t readMessageLimit() const noexcept;
};

} // namespace org::ttldtor::qdcxx::kraken

#endif // ORG_TTLDTOR_QDCXX_KRAKEN_WSS_SESSION_HPP
