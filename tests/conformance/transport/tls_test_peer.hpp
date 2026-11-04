// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace org::ttldtor::qdcxx::test {

/** Exact ordinary protocol version selected for the isolated peer. */
enum class PeerTlsVersion : std::uint32_t {
    TLS_1_0 = 10, ///< TLS 1.0 only, for an ordinary unsupported-version rejection test.
    TLS_1_1 = 11, ///< TLS 1.1 only, for an ordinary unsupported-version rejection test.
    TLS_1_2 = 12, ///< TLS 1.2 only.
    TLS_1_3 = 13  ///< TLS 1.3 only.
};

/** Finite isolated-server policies used to test client negotiation refusals; never production settings. */
enum class PeerPolicy : std::uint32_t {
    MODERN = 0,    ///< Ordinary selected AEAD/group defaults.
    CBC_ONLY = 1,  ///< TLS 1.2 ECDHE-RSA CBC, absent from the production client's cipher list.
    P521_ONLY = 2, ///< P-521 ephemeral group, absent from the production client's group list.
    SHA1_ONLY = 3  ///< TLS 1.2 RSA/SHA-1 handshake signature, absent from the client's signature list.
};

/** Action after the finite application-byte script. */
enum class PeerCloseMode : std::uint32_t {
    CLEAN = 0,  ///< Exchange TLS close_notify within the write deadline.
    ABRUPT = 1, ///< Close the TCP socket without close_notify.
    HOLD = 2    ///< Keep the connection until stop or the overall deadline.
};

/** Static, payload-free failure categories for parent-side setup. */
enum class PeerError {
    INVALID_OPTIONS, ///< Invalid paths, limits or enum values; no child launched.
    CONTROL_IO,      ///< Owned directory/control/report I/O failed.
    PROCESS_START,   ///< The exact executable could not be started, or POSIX child waiting is disabled.
    START_TIMEOUT,   ///< The child did not publish a listening port before its deadline.
    CHILD_FAILED     ///< The child exited or rejected its input before becoming ready.
};

/** Optional second fresh TLS connection on the same owned loopback endpoint. */
struct PeerSuccessor {
    std::optional<std::vector<std::uint8_t>> expectedReplyClientBytes; ///< Exact post-reply authorization prefix.
    std::vector<std::uint8_t> finalBytes;   ///< Final acknowledgement after the second exact read.
    std::vector<std::uint8_t> initialBytes; ///< Independent second challenge or protocol prefix.
    std::vector<std::uint8_t> replyBytes;   ///< Independent second reply after the exact read.
    std::optional<std::vector<std::uint8_t>> expectedClientBytes; ///< Exact second pre-reply prefix.
    std::chrono::milliseconds pauseBeforeHandshake{0};            ///< Explicit second handshake pause.
};

/** Explicit owned inputs for one or two loopback connections; no ambient SDK configuration is used.
 * Scripts are bounded to 4 MiB each, paths to 4096 UTF-8 bytes, and each timeout to 1..60000 ms.
 * Delays may be zero, otherwise they must not exceed the overall timeout (maximum 60000 ms).
 * Certificate/key paths name ordinary test fixtures; their contents never enter diagnostics.
 */
struct PeerOptions {
    std::optional<std::vector<std::uint8_t>> expectedReplyClientBytes; ///< Optional exact read after replyBytes.
    std::vector<std::uint8_t> finalBytes;  ///< Bytes sent after the optional second exact read.
    std::filesystem::path childExecutable; ///< Absolute internal peer executable path.
    std::filesystem::path certificateFile; ///< Absolute ordinary PEM server certificate fixture.
    std::filesystem::path privateKeyFile;  ///< Absolute ordinary PEM private-key fixture.
    std::filesystem::path controlRoot;     ///< Existing explicit parent directory for owned temporary files.
    PeerTlsVersion tlsVersion{PeerTlsVersion::TLS_1_3}; ///< Exact selected protocol version.
    PeerPolicy policy{PeerPolicy::MODERN};              ///< Explicit finite server-only negotiation test policy.
    std::uint32_t tickets{0};                           ///< TLS 1.3 initial ticket count, bounded to 0..2.
    PeerCloseMode closeMode{PeerCloseMode::CLEAN};      ///< Finite script completion behavior.
    std::vector<std::uint8_t> initialBytes;             ///< Bytes sent after handshake, before the optional exact read.
    std::vector<std::uint8_t> replyBytes;               ///< Bytes written after the optional exact read; never logged.
    std::optional<std::vector<std::uint8_t>> expectedClientBytes; ///< Exact prefix to read and compare, if supplied.
    std::chrono::milliseconds startupTimeout{5000};               ///< Parent readiness deadline.
    std::chrono::milliseconds handshakeTimeout{5000};             ///< Child handshake deadline.
    std::chrono::milliseconds readTimeout{5000};                  ///< Child exact application-read deadline.
    std::chrono::milliseconds writeTimeout{5000};                 ///< Child write and close_notify deadline.
    std::chrono::milliseconds overallTimeout{15000};              ///< Child absolute lifetime after input validation.
    std::chrono::milliseconds pauseBeforeHandshake{0};            ///< Delay after accept, before handshake.
    std::chrono::milliseconds readDelay{0};   ///< Delay after handshake, before reading; allows backpressure tests.
    std::chrono::milliseconds stopGrace{250}; ///< Parent grace before terminating only its owned child.
    bool readHttpHeaders{false};   ///< Opt-in32768-byte CRLFCRLF request read before reply; ordinary CLEAN shutdown.
    bool captureUntilClose{false}; ///< Opt-in bounded synthetic plaintext capture through client TLS EOF.
    std::optional<PeerSuccessor> successor; ///< At most one independent successor, requiring captureUntilClose.
};

/** Numeric observations and explicitly enabled bounded synthetic client captures, retained after stop. */
struct PeerSnapshot {
    std::uint32_t firstCaptureEnd{};  ///< Zero incomplete; 1 TLS EOF, 2 truncated TLS EOF, 3 TCP reset.
    std::uint32_t secondCaptureEnd{}; ///< Independent successor receive termination, using the same tags.
    std::uint16_t port{};             ///< Listening IPv4 loopback port, or zero before readiness.
    std::uint32_t tlsVersion{};       ///< Negotiated version: 12, 13, or zero before successful handshake.
    std::uint64_t processId{};        ///< Exact owned child identifier; never used to find foreign processes.
    std::uint64_t bytesRead{};        ///< Successfully read application bytes.
    std::uint64_t bytesWritten{};     ///< Successfully written application bytes.
    bool ready{};                     ///< The child published its listening port.
    bool accepted{};                  ///< One loopback TCP connection was accepted.
    bool handshakeComplete{};         ///< TLS handshake completed successfully.
    bool sniPresent{};                ///< The client supplied a server-name extension; no name is exposed.
    bool sniLocalhost{};              ///< The supplied server name was exactly the ordinary localhost fixture identity.
    std::uint32_t cipherSuite{};      ///< Negotiated IANA cipher-suite number, zero before completion.
    std::uint32_t groupId{};          ///< Raw qualified SDK negotiated-group identifier, zero when unavailable.
    std::uint32_t signatureHashNid{}; ///< Raw qualified SDK local signature digest NID, zero when unavailable.
    std::uint32_t ticketsSent{};      ///< Observed outgoing NewSessionTicket messages, never ticket bytes.
    std::uint32_t signatureScheme{};  ///< Observed outgoing TLS signature scheme, zero for an unsigned exchange.
    std::uint32_t signatureBytes{};   ///< Observed outgoing handshake signature length, never signature bytes.
    bool expectedBytesMatched{};      ///< A supplied exact expected byte sequence matched completely.
    bool closeNotifyComplete{};       ///< TLS shutdown completed without error.
    bool completed{};                 ///< The child completed its script or failed in a bounded operation.
    bool timedOut{};                  ///< An operation or overall deadline expired.
    bool failed{};                    ///< Control, TLS, socket or script comparison failed.
    bool childReportedFailure{};      ///< Failure bit from the last complete child report, excluding process status.
    bool exitObserved{};              ///< A terminal native process status was successfully obtained.
    std::uint32_t exitCode{};         ///< Native Windows exit code; POSIX exit code or 128 plus terminating signal.
    std::uint32_t exitQueryError{};   ///< Native process-status query error; zero after a successful query.
    bool parentTerminationRequested{};    ///< Parent attempted forced termination after its finite cooperative wait.
    bool reportReadable{};                ///< At least one complete validated numeric report was observed.
    bool childRunning{};                  ///< Child was still running when this snapshot was taken.
    std::uint32_t completedSessions{};    ///< Successfully retired scripts, bounded to two.
    std::uint32_t successfulHandshakes{}; ///< Independent completed handshakes, bounded to two.
    std::uint32_t matchedSessions{};      ///< Completely matched configured prefixes, bounded to two.
    std::vector<std::uint8_t> firstClientBytes;  ///< Opt-in synthetic first-session plaintext after completion.
    std::vector<std::uint8_t> secondClientBytes; ///< Opt-in synthetic successor plaintext after completion.
};

/** Move-only owner of an isolated test peer process and its exact temporary directory.
 * Parent operations are serialized by the caller. No SSL context is created in the parent.
 * On POSIX the caller preserves waitable SIGCHLD semantics (neither SIG_IGN nor SA_NOCLDWAIT)
 * throughout this owner's lifetime, and no other thread or handler may reap this child.
 * start() rejects a known incompatible SIGCHLD disposition before launching a child.
 * Destruction requests cooperative stop, waits for stopGrace, then terminates only this child.
 * Cleanup never scans for processes or recursively removes a caller-supplied directory.
 */
class TlsTestPeer {
    struct State;
    std::unique_ptr<State> state_;

    /** Adopt already-created process state.
     * @param state Sole owned implementation state.
     */
    explicit TlsTestPeer(std::unique_ptr<State> state) noexcept;

    public:
    /** Start one child and wait for its ephemeral loopback listener.
     * @param options Explicit paths, bounded script and deadlines; moved into owned control input.
     * @return Move-only ready peer, or a static error with all started resources retired.
     */
    static std::expected<TlsTestPeer, PeerError> start(PeerOptions options);

    /** Transfer sole process/directory ownership.
     * @param other Previous owner; becomes inert.
     */
    TlsTestPeer(TlsTestPeer &&other) noexcept;

    /** Stop the current child before transferring ownership.
     * @param other Previous owner; becomes inert.
     * @return This owner.
     */
    TlsTestPeer &operator=(TlsTestPeer &&other) noexcept;

    TlsTestPeer(const TlsTestPeer &) = delete;
    TlsTestPeer &operator=(const TlsTestPeer &) = delete;

    /** Retire only the owned child and exact owned files within finite waits. */
    ~TlsTestPeer();

    /** Return the ready peer's listening port, or zero for a moved-from object.
     * @return IPv4 127.0.0.1 ephemeral TCP port.
     */
    std::uint16_t port() const noexcept;

    /** Refresh the validated numeric report and process liveness.
     * @return Last complete numeric observations; transient report replacement keeps the previous value.
     */
    PeerSnapshot snapshot() noexcept;

    /** Request stop and retire the exact child; idempotent and valid after peer completion.
     * @return Final retained numeric observations. Forced termination does not imply a completed TLS script.
     */
    PeerSnapshot stop() noexcept;
};

} // namespace org::ttldtor::qdcxx::test
