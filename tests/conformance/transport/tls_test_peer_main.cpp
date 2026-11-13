// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_test_peer.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/provider.h>
#include <openssl/ssl.h>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace {
namespace t = org::ttldtor::qdcxx::test;
using Tcp = asio::ip::tcp;      ///< Ordinary IPv4 loopback transport.
using Error = asio::error_code; ///< Asio errors are reduced to numeric flags, never printed.
constexpr std::uint64_t CONTROL_MAGIC_HTTP =
    0x5144435854503036ULL; ///< Bounded HTTP header extension; ordinary v5 remains unchanged.
constexpr std::uint64_t CONTROL_MAGIC = 0x5144435854503035ULL;       ///< Internal binary control version 5.
constexpr std::uint64_t CONTROL_MAGIC_V4 = 0x5144435854503034ULL;    ///< Accepted original ticket-control input.
constexpr std::uint64_t CONTROL_MAGIC_V3 = 0x5144435854503033ULL;    ///< Accepted policy-control compatibility input.
constexpr std::uint64_t CONTROL_MAGIC_V2 = 0x5144435854503032ULL;    ///< Accepted ordinary-script compatibility input.
constexpr std::uint64_t REPORT_MAGIC = 0x5144435852503033ULL;        ///< Internal numeric report version 3.
constexpr std::size_t MAX_SCRIPT_BYTES = 4U * 1024U * 1024U;         ///< Finite application bytes per script.
constexpr std::size_t MAX_PATH_BYTES = 4096;                         ///< Finite UTF-8 ordinary fixture path ceiling.
constexpr std::size_t MAX_PEM_BYTES = 128U * 1024U;                  ///< Finite ordinary certificate/key fixture input.
constexpr auto REPORT_REPLACE_WAIT = std::chrono::milliseconds(100); ///< Finite transient file-sharing retry.
constexpr auto REPORT_REPLACE_POLL = std::chrono::milliseconds(1);   ///< Local report replacement retry cadence.
constexpr auto STOP_POLL = std::chrono::milliseconds(20);            ///< Finite cooperative stop polling cadence.

/** Reject invalid owned input with fixed text; exceptions never include file paths or payloads.
 * @param condition Required input or initialization condition.
 */
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("internal peer failure");
    }
}

/** Read one exact portable little-endian word.
 * @param input Owned binary control stream.
 * @return Numeric field; malformed/truncated control throws a static failure.
 */
std::uint64_t readWord(std::istream &input) {
    std::array<unsigned char, 8> bytes{};
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(input));
    std::uint64_t result{};

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }

    return result;
}

/** Decode a bounded explicit UTF-8 path.
 * @param input Owned binary control stream.
 * @return Absolute ordinary fixture path without embedded NUL bytes.
 */
std::filesystem::path readPath(std::istream &input) {
    const auto size = readWord(input);
    require(size != 0 && size <= MAX_PATH_BYTES);
    std::u8string bytes(static_cast<std::size_t>(size), u8'\0');
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(input) && bytes.find(u8'\0') == std::u8string::npos);
    std::filesystem::path result(bytes);
    require(result.is_absolute() && std::filesystem::is_regular_file(result));

    return result;
}

/** Decode one finite positive timeout.
 * @param input Owned binary control stream.
 * @return Timeout in milliseconds, bounded to 1..60000.
 */
std::chrono::milliseconds readTimeout(std::istream &input) {
    const auto value = readWord(input);
    require(value > 0 && value <= 60000);

    return std::chrono::milliseconds(value);
}

/** Decode one bounded script without interpreting its application bytes.
 * @param file Exact parent-owned control file.
 * @return Owned explicit configuration; malformed/trailing input is rejected before sockets start.
 */
t::PeerOptions readControl(const std::filesystem::path &file) {
    const auto fileSize = std::filesystem::file_size(file);
    constexpr std::size_t MAX_CONTROL_BYTES = 10 * MAX_SCRIPT_BYTES + 2 * MAX_PATH_BYTES + 32 * sizeof(std::uint64_t);
    require(fileSize <= MAX_CONTROL_BYTES);
    std::ifstream input(file, std::ios::binary);
    const auto magic = readWord(input);
    require(magic == CONTROL_MAGIC_HTTP || magic == CONTROL_MAGIC || magic == CONTROL_MAGIC_V4 ||
            magic == CONTROL_MAGIC_V3 || magic == CONTROL_MAGIC_V2);
    t::PeerOptions options;
    const auto version = readWord(input);
    const auto mode = readWord(input);
    const auto policy = magic != CONTROL_MAGIC_V2 ? readWord(input) : 0;
    const auto tickets =
        (magic == CONTROL_MAGIC_HTTP || magic == CONTROL_MAGIC || magic == CONTROL_MAGIC_V4) ? readWord(input) : 0;
    require(version >= 10 && version <= 13 && mode <= 2 && policy <= 3 &&
            ((policy != 1 && policy != 3) || version == 12) && (version >= 12 || policy == 0) && tickets <= 2 &&
            (tickets == 0 || version == 13));
    options.tlsVersion = static_cast<t::PeerTlsVersion>(version);
    options.closeMode = static_cast<t::PeerCloseMode>(mode);
    options.policy = static_cast<t::PeerPolicy>(policy);
    options.tickets = static_cast<std::uint32_t>(tickets);
    options.handshakeTimeout = readTimeout(input);
    options.readTimeout = readTimeout(input);
    options.writeTimeout = readTimeout(input);
    options.overallTimeout = readTimeout(input);
    const auto pause = readWord(input);
    const auto delay = readWord(input);
    require(pause <= static_cast<std::uint64_t>(options.overallTimeout.count()) &&
            delay <= static_cast<std::uint64_t>(options.overallTimeout.count()));
    options.pauseBeforeHandshake = std::chrono::milliseconds(pause);
    options.readDelay = std::chrono::milliseconds(delay);
    const auto hasExpected = readWord(input);
    const auto expectedSize = readWord(input);
    const auto replySize = readWord(input);
    const auto initialSize = readWord(input);
    require(hasExpected <= 1 && expectedSize <= MAX_SCRIPT_BYTES && replySize <= MAX_SCRIPT_BYTES &&
            initialSize <= MAX_SCRIPT_BYTES && (hasExpected != 0 || expectedSize == 0));
    options.certificateFile = readPath(input);
    options.privateKeyFile = readPath(input);

    if (hasExpected != 0) {
        options.expectedClientBytes.emplace(static_cast<std::size_t>(expectedSize));
        input.read(reinterpret_cast<char *>(options.expectedClientBytes->data()),
                   static_cast<std::streamsize>(expectedSize));
    }

    options.replyBytes.resize(static_cast<std::size_t>(replySize));
    input.read(reinterpret_cast<char *>(options.replyBytes.data()), static_cast<std::streamsize>(replySize));
    options.initialBytes.resize(static_cast<std::size_t>(initialSize));
    input.read(reinterpret_cast<char *>(options.initialBytes.data()), static_cast<std::streamsize>(initialSize));

    if (magic == CONTROL_MAGIC || magic == CONTROL_MAGIC_HTTP) {
        const auto capture = readWord(input);
        require(capture <= 1);
        const auto hasReplyExpected = readWord(input);
        const auto replyExpectedSize = readWord(input);
        const auto finalSize = readWord(input);
        require(hasReplyExpected <= 1 && replyExpectedSize <= MAX_SCRIPT_BYTES && finalSize <= MAX_SCRIPT_BYTES &&
                (hasReplyExpected != 0 || replyExpectedSize == 0));
        require(capture == 0 || replyExpectedSize <= MAX_SCRIPT_BYTES - expectedSize);

        if (hasReplyExpected != 0) {
            options.expectedReplyClientBytes.emplace(static_cast<std::size_t>(replyExpectedSize));
            input.read(reinterpret_cast<char *>(options.expectedReplyClientBytes->data()),
                       static_cast<std::streamsize>(replyExpectedSize));
        }

        options.finalBytes.resize(static_cast<std::size_t>(finalSize));
        input.read(reinterpret_cast<char *>(options.finalBytes.data()), static_cast<std::streamsize>(finalSize));
        const auto successor = readWord(input);
        require(capture <= 1 && successor <= 1 && (successor == 0 || capture != 0));
        options.captureUntilClose = capture != 0;

        if (successor != 0) {
            options.successor.emplace();
            auto &next = *options.successor;
            const auto nextPause = readWord(input);
            const auto nextExpected = readWord(input);
            const auto nextExpectedSize = readWord(input);
            const auto nextReplySize = readWord(input);
            const auto nextInitialSize = readWord(input);
            require(nextPause <= static_cast<std::uint64_t>(options.overallTimeout.count()) && nextExpected <= 1 &&
                    nextExpectedSize <= MAX_SCRIPT_BYTES && nextReplySize <= MAX_SCRIPT_BYTES &&
                    nextInitialSize <= MAX_SCRIPT_BYTES && (nextExpected != 0 || nextExpectedSize == 0));
            next.pauseBeforeHandshake = std::chrono::milliseconds(nextPause);

            if (nextExpected != 0) {
                next.expectedClientBytes.emplace(static_cast<std::size_t>(nextExpectedSize));
                input.read(reinterpret_cast<char *>(next.expectedClientBytes->data()),
                           static_cast<std::streamsize>(nextExpectedSize));
            }

            next.replyBytes.resize(static_cast<std::size_t>(nextReplySize));
            input.read(reinterpret_cast<char *>(next.replyBytes.data()), static_cast<std::streamsize>(nextReplySize));
            next.initialBytes.resize(static_cast<std::size_t>(nextInitialSize));
            input.read(reinterpret_cast<char *>(next.initialBytes.data()),
                       static_cast<std::streamsize>(nextInitialSize));
            const auto nextHasReplyExpected = readWord(input);
            const auto nextReplyExpectedSize = readWord(input);
            const auto nextFinalSize = readWord(input);
            require(nextHasReplyExpected <= 1 && nextReplyExpectedSize <= MAX_SCRIPT_BYTES &&
                    nextFinalSize <= MAX_SCRIPT_BYTES && (nextHasReplyExpected != 0 || nextReplyExpectedSize == 0));
            require(nextReplyExpectedSize <= MAX_SCRIPT_BYTES - nextExpectedSize);

            if (nextHasReplyExpected != 0) {
                next.expectedReplyClientBytes.emplace(static_cast<std::size_t>(nextReplyExpectedSize));
                input.read(reinterpret_cast<char *>(next.expectedReplyClientBytes->data()),
                           static_cast<std::streamsize>(nextReplyExpectedSize));
            }

            next.finalBytes.resize(static_cast<std::size_t>(nextFinalSize));
            input.read(reinterpret_cast<char *>(next.finalBytes.data()), static_cast<std::streamsize>(nextFinalSize));
        }
    }

    if (magic == CONTROL_MAGIC_HTTP) {
        require(readWord(input) == 1 && !options.captureUntilClose && !options.successor &&
                !options.expectedReplyClientBytes && options.initialBytes.empty() &&
                (!options.expectedClientBytes || options.expectedClientBytes->size() <= 32768));
        options.readHttpHeaders = true;
    }

    require(static_cast<bool>(input) && input.peek() == std::char_traits<char>::eof());

    return options;
}

/** Read a bounded ordinary PEM fixture through the native filesystem path API.
 * @param path Explicit test fixture, never a discovered user key path.
 * @return Owned PEM bytes used only by child TLS initialization.
 */
std::vector<char> readPem(const std::filesystem::path &path) {
    const auto size = std::filesystem::file_size(path);
    require(size > 0 && size <= MAX_PEM_BYTES);
    std::vector<char> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(input));

    return bytes;
}

/** Own the explicit built-in provider longer than all child SSL contexts and callbacks. */
class Crypto {
    std::unique_ptr<OSSL_LIB_CTX, decltype(&OSSL_LIB_CTX_free)> library_{nullptr, OSSL_LIB_CTX_free};
    std::unique_ptr<OSSL_PROVIDER, decltype(&OSSL_PROVIDER_unload)> provider_{nullptr, OSSL_PROVIDER_unload};

    public:
    /** Initialize without configuration autoload; no custom provider, allocator or RNG hooks. */
    Crypto() {
        require(OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) == 1);
        library_.reset(OSSL_LIB_CTX_new());
        require(library_ != nullptr);
        provider_.reset(OSSL_PROVIDER_load(library_.get(), "default"));
        require(provider_ != nullptr);
    }

    /** Create one ordinary exact-version server context owned by Asio.
     * @param options Validated explicit fixture/version options.
     * @return Context independent of every parent/client SDK object.
     */
    asio::ssl::context makeContext(const t::PeerOptions &options) {
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> native(
            SSL_CTX_new_ex(library_.get(), nullptr, TLS_server_method()), SSL_CTX_free);
        require(native != nullptr);
        int version = TLS1_3_VERSION;

        switch (options.tlsVersion) {
        case t::PeerTlsVersion::TLS_1_0:
            version = TLS1_VERSION;
            break;
        case t::PeerTlsVersion::TLS_1_1:
            version = TLS1_1_VERSION;
            break;
        case t::PeerTlsVersion::TLS_1_2:
            version = TLS1_2_VERSION;
            break;
        case t::PeerTlsVersion::TLS_1_3:
            break;
        }

        require(SSL_CTX_set_min_proto_version(native.get(), version) == 1 &&
                SSL_CTX_set_max_proto_version(native.get(), version) == 1);
        require(SSL_CTX_set_cipher_list(native.get(), "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256") ==
                1);
        require(SSL_CTX_set_ciphersuites(native.get(), "TLS_AES_128_GCM_SHA256") == 1);
        require(SSL_CTX_set1_groups_list(native.get(), "X25519:P-256") == 1);

        if (version < TLS1_2_VERSION || options.policy == t::PeerPolicy::SHA1_ONLY) {
            // Isolated negative-test server only; the production client's policy remains unchanged.
            SSL_CTX_set_security_level(native.get(), 0);
        }

        if (version < TLS1_2_VERSION) {
            require(SSL_CTX_set_cipher_list(native.get(), "AES128-SHA") == 1);
        } else if (options.policy == t::PeerPolicy::CBC_ONLY) {
            require(SSL_CTX_set_cipher_list(native.get(), "ECDHE-RSA-AES128-SHA256") == 1);
        } else if (options.policy == t::PeerPolicy::P521_ONLY) {
            require(SSL_CTX_set1_groups_list(native.get(), "P-521") == 1);
        } else if (options.policy == t::PeerPolicy::SHA1_ONLY) {
            require(SSL_CTX_set1_sigalgs_list(native.get(), "rsa_pkcs1_sha1") == 1);
        }

        SSL_CTX_set_options(native.get(), SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
        require(SSL_CTX_set_num_tickets(native.get(), options.tickets) == 1);
        asio::ssl::context context(native.release());
        const auto certificate = readPem(options.certificateFile);
        const auto key = readPem(options.privateKeyFile);
        context.use_certificate_chain(asio::buffer(certificate));
        context.use_private_key(asio::buffer(key), asio::ssl::context::pem);
        require(SSL_CTX_check_private_key(context.native_handle()) == 1);

        return context;
    }
};

/** One serialized event-loop-owned connection; callbacks cannot outlive run() and this object. */
class PeerSession {
    asio::io_context &io_;    ///< Shared child loop, restarted only after all previous session callbacks drain.
    Tcp::acceptor &acceptor_; ///< The listener and backlog survive between the two independent TLS sessions.
    asio::ssl::stream<Tcp::socket> stream_;
    asio::steady_timer overall_;
    asio::steady_timer operation_;
    asio::steady_timer pause_;
    asio::steady_timer stopPoll_;
    t::PeerOptions options_;
    std::filesystem::path directory_;
    std::string httpHeaders_; ///< Opt-in fixed maximum32768-byte request framing capture.
    std::vector<std::uint8_t> received_;
    std::vector<std::uint8_t> captured_; ///< Opt-in synthetic plaintext, bounded independently of every read.
    std::array<std::uint8_t, 4096> captureBuffer_{}; ///< Async-read storage retained through cancellation drain.
    unsigned ordinal_{};                             ///< One or two, selecting the exact owned capture file.
    std::chrono::steady_clock::time_point absoluteDeadline_; ///< Shared child lifetime across both fresh sessions.
    t::PeerSnapshot report_;
    bool finished_{};
    bool publicationDisabled_{}; ///< Exception cleanup never retries a failed report operation.

    /** Observe numeric ticket/signature metadata in SDK-owned outgoing messages without retaining their bytes.
     * @param outgoing Nonzero for an outgoing SDK message.
     * @param version SDK protocol version, selecting the ordinary signature-message grammar.
     * @param contentType SDK message category; only handshake messages are inspected.
     * @param bytes SDK-owned bytes, borrowed only during this callback.
     * @param size Readable byte count; zero forbids inspecting the message type.
     * @param ssl Live SDK owner, unused because the session is the explicit argument.
     * @param argument This live session, cleared before stream/member destruction.
     */
    static void observeMessage(int outgoing, int version, int contentType, const void *bytes, std::size_t size,
                               SSL *ssl, void *argument) noexcept {
        (void)ssl;

        if (outgoing == 0 || contentType != SSL3_RT_HANDSHAKE || size < 4) {
            return;
        }

        const auto *message = static_cast<const unsigned char *>(bytes);
        auto &session = *static_cast<PeerSession *>(argument);

        if (message[0] == SSL3_MT_NEWSESSION_TICKET) {
            if (session.report_.ticketsSent != std::numeric_limits<std::uint32_t>::max()) {
                ++session.report_.ticketsSent;
            }

            return;
        }

        std::size_t offset = 4; // SDK callback includes the ordinary four-byte handshake header.

        if (message[0] == SSL3_MT_SERVER_KEY_EXCHANGE && version == TLS1_2_VERSION) {
            if (size < 8 || message[4] != 3) { // Named-curve ECDHE: type, group, point length, point.
                return;
            }

            offset = 8 + static_cast<std::size_t>(message[7]);
        } else if (message[0] != SSL3_MT_CERTIFICATE_VERIFY || version != TLS1_3_VERSION) {
            return;
        }

        if (offset > size || size - offset < 4) {
            return;
        }

        const auto scheme = static_cast<std::uint32_t>((message[offset] << 8) | message[offset + 1]);
        const auto length = static_cast<std::uint32_t>((message[offset + 2] << 8) | message[offset + 3]);

        if (length == size - offset - 4) {
            session.report_.signatureScheme = scheme;
            session.report_.signatureBytes = length;
        }
    }

    /** Atomically replace a fixed numeric report; never emit application or SDK diagnostic text. */
    void publish() {
        if (publicationDisabled_) {
            return;
        }

        const auto temporary = directory_ / "report.next";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << REPORT_MAGIC << ' ' << report_.port << ' ' << report_.tlsVersion << ' ' << report_.bytesRead << ' '
               << report_.bytesWritten << ' ' << report_.ready << ' ' << report_.accepted << ' '
               << report_.handshakeComplete << ' ' << report_.expectedBytesMatched << ' ' << report_.closeNotifyComplete
               << ' ' << report_.completed << ' ' << report_.timedOut << ' ' << report_.failed << ' '
               << report_.sniPresent << ' ' << report_.sniLocalhost << ' ' << report_.cipherSuite << ' '
               << report_.groupId << ' ' << report_.signatureHashNid << ' ' << report_.ticketsSent << ' '
               << report_.signatureScheme << ' ' << report_.signatureBytes << ' ' << report_.completedSessions << ' '
               << report_.successfulHandshakes << ' ' << report_.matchedSessions << ' ' << report_.firstCaptureEnd
               << ' ' << report_.secondCaptureEnd << '\n';
        output.close();
        require(static_cast<bool>(output));
#ifdef _WIN32
        // Preserve old readers; retry only finite filesystem sharing conflicts during snapshot replacement.
        const auto deadline = std::chrono::steady_clock::now() + REPORT_REPLACE_WAIT;

        while (!MoveFileExW(temporary.c_str(), (directory_ / "report").c_str(), MOVEFILE_REPLACE_EXISTING)) {
            const auto error = GetLastError();
            const bool transient =
                error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION;
            require(transient && std::chrono::steady_clock::now() < deadline);
            std::this_thread::sleep_for(REPORT_REPLACE_POLL);
        }

#else
        std::error_code error;
        std::filesystem::rename(temporary, directory_ / "report", error);
        require(!error);
#endif
    }

    /** Cancel one timer without allowing an exception to skip cancellation of another owner.
     * @param timer Session-owned timer whose handlers retain this session.
     */
    static void cancelTimer(asio::steady_timer &timer) noexcept {
        try {
            timer.cancel();
        } catch (...) {
            // A failed cancellation leaves the existing finite timer available to the drain.
        }
    }

    /** Cancel owned operations without reporting or destroying their borrowed buffers. */
    void cancelOwned() noexcept {
        Error ignored;
        acceptor_.cancel(ignored);
        stream_.lowest_layer().cancel(ignored);
        stream_.lowest_layer().close(ignored);
        cancelTimer(overall_);
        cancelTimer(operation_);
        cancelTimer(pause_);
        cancelTimer(stopPoll_);
    }

    /** Close the owned network operation and cancel all timers; run() drains their callbacks.
     * @param failed Whether a script, TLS or transport operation failed.
     * @param timedOut Whether a configured operation/overall deadline expired.
     */
    void finish(bool failed, bool timedOut = false) {
        if (finished_) {
            return;
        }

        finished_ = true;
        report_.completed = true;
        report_.failed = failed;
        report_.timedOut = timedOut;

        if (!failed) {
            ++report_.completedSessions;
        }

        if (options_.captureUntilClose || options_.readHttpHeaders) {
            std::ofstream capture(directory_ / (ordinal_ == 1 ? "capture-1" : "capture-2"),
                                  std::ios::binary | std::ios::trunc);
            capture.write(reinterpret_cast<const char *>(captured_.data()),
                          static_cast<std::streamsize>(captured_.size()));
            capture.close();
            require(static_cast<bool>(capture));
        }

        cancelOwned();
        publish();
    }

    /** Retire handlers after any initiating/completion exception while all borrowed owners remain alive. */
    void drainFailure() noexcept {
        finished_ = true;
        report_.completed = true;
        report_.failed = true;
        publicationDisabled_ = true;
        cancelOwned();

        for (;;) {
            try {
                if (io_.stopped()) {
                    io_.restart();
                }

                io_.run();

                return;
            } catch (...) {
                // Asio permits reentering run() after a handler throws; no handler may start new work now.
                cancelOwned();
            }
        }
    }

    /** Arm the next serialized TLS operation deadline, canceling the previous timer.
     * @param timeout Finite validated duration.
     */
    void armOperation(std::chrono::milliseconds timeout) {
        operation_.expires_after(timeout);
        operation_.async_wait([this](Error error) {
            if (!error && !finished_) {
                finish(true, true);
            }
        });
    }

    /** Poll only the parent-owned stop marker within the absolute lifetime deadline. */
    void pollStop() {
        stopPoll_.expires_after(STOP_POLL);
        stopPoll_.async_wait([this](Error error) {
            if (error || finished_) {
                return;
            }

            std::error_code filesystemError;
            const bool requested = std::filesystem::exists(directory_ / "stop", filesystemError);

            if (filesystemError) {
                finish(true);
            } else if (requested) {
                finish(false);
            } else {
                pollStop();
            }
        });
    }

    /** Capture synthetic application bytes until peer EOF, retaining storage through cancellation. */
    void captureToClose() {
        armOperation(options_.readTimeout);
        stream_.async_read_some(asio::buffer(captureBuffer_), [this](Error error, std::size_t size) {
            if (finished_) {
                return;
            }

            require(size <= MAX_SCRIPT_BYTES - captured_.size());
            captured_.insert(captured_.end(), captureBuffer_.begin(),
                             captureBuffer_.begin() + static_cast<std::ptrdiff_t>(size));
            report_.bytesRead += size;

            if (error) {
                const unsigned end = error == asio::error::eof                     ? 1U
                                     : error == asio::ssl::error::stream_truncated ? 2U
                                     : error == asio::error::connection_reset      ? 3U
                                                                                   : 0U;
                (ordinal_ == 1 ? report_.firstCaptureEnd : report_.secondCaptureEnd) = end;
                finish(end == 0);
            } else {
                captureToClose();
            }
        });
    }

    /** Apply the selected script-completion close behavior. */
    void closeScript() {
        if (options_.captureUntilClose) {
            captureToClose();

            return;
        }

        if (options_.closeMode == t::PeerCloseMode::HOLD) {
            operation_.cancel();
            publish();

            return;
        }

        if (options_.closeMode == t::PeerCloseMode::ABRUPT) {
            finish(false);

            return;
        }

        armOperation(options_.writeTimeout);
        stream_.async_shutdown([this](Error error) {
            if (finished_) {
                return;
            }

            report_.closeNotifyComplete = !error;
            finish(static_cast<bool>(error));
        });
    }

    /** Write the final acknowledgement after a separately checked authorization read. */
    void writeFinal() {
        if (options_.finalBytes.empty()) {
            closeScript();

            return;
        }

        armOperation(options_.writeTimeout);
        asio::async_write(stream_, asio::buffer(options_.finalBytes), [this](Error error, std::size_t size) {
            report_.bytesWritten += size;

            if (finished_) {
                return;
            }

            if (error) {
                finish(true);
            } else {
                closeScript();
            }
        });
    }

    /** Check the optional second prefix before publishing the final acknowledgement. */
    void readReplyExpected() {
        if (!options_.expectedReplyClientBytes) {
            writeFinal();

            return;
        }

        require(!options_.captureUntilClose ||
                options_.expectedReplyClientBytes->size() <= MAX_SCRIPT_BYTES - captured_.size());
        received_.resize(options_.expectedReplyClientBytes->size());
        armOperation(options_.readTimeout);
        asio::async_read(stream_, asio::buffer(received_), [this](Error error, std::size_t size) {
            report_.bytesRead += size;

            if (finished_) {
                return;
            }

            require(size <= MAX_SCRIPT_BYTES - captured_.size());

            if (options_.captureUntilClose) {
                captured_.insert(captured_.end(), received_.begin(),
                                 received_.begin() + static_cast<std::ptrdiff_t>(size));
            }

            if (error || received_ != *options_.expectedReplyClientBytes) {
                finish(true);
            } else {
                writeFinal();
            }
        });
    }

    /** Write the exact bounded application reply after the optional read succeeds. */
    void writeReply() {
        if (options_.replyBytes.empty()) {
            readReplyExpected();

            return;
        }

        armOperation(options_.writeTimeout);
        asio::async_write(stream_, asio::buffer(options_.replyBytes), [this](Error error, std::size_t size) {
            report_.bytesWritten += size;

            if (finished_) {
                publish();

                return;
            }

            publish();

            if (error) {
                finish(true);
            } else {
                readReplyExpected();
            }
        });
    }

    /** Read one finite HTTP request header before replying, preserving normal TLS shutdown. */
    void readHttpHeaders() {
        constexpr std::size_t MAX_HTTP_HEADERS = 32768;
        httpHeaders_.assign(received_.begin(), received_.end());
        const auto prefix = httpHeaders_.size();
        armOperation(options_.readTimeout);
        asio::async_read_until(stream_, asio::dynamic_buffer(httpHeaders_, MAX_HTTP_HEADERS), "\r\n\r\n",
                               [this, prefix](Error error, std::size_t size) {
                                   if (finished_) {
                                       return;
                                   }

                                   report_.bytesRead += httpHeaders_.size() - prefix;
                                   captured_.assign(httpHeaders_.begin(), httpHeaders_.end());
                                   publish();

                                   if (error || size > MAX_HTTP_HEADERS) {
                                       finish(true);
                                   } else {
                                       writeReply();
                                   }
                               });
    }

    /** Read and compare only the configured exact byte count; no protocol interpretation occurs. */
    void readExpected() {
        if (!options_.expectedClientBytes) {
            if (options_.readHttpHeaders) {
                readHttpHeaders();
            } else {
                writeReply();
            }

            return;
        }

        received_.resize(options_.expectedClientBytes->size());

        if (received_.empty()) {
            report_.expectedBytesMatched = true;
            ++report_.matchedSessions;

            if (options_.readHttpHeaders) {
                readHttpHeaders();
            } else {
                writeReply();
            }

            return;
        }

        armOperation(options_.readTimeout);
        asio::async_read(stream_, asio::buffer(received_), [this](Error error, std::size_t size) {
            report_.bytesRead = size;

            if (finished_) {
                publish();

                return;
            }

            report_.expectedBytesMatched = !error && received_ == *options_.expectedClientBytes;

            if (options_.captureUntilClose) {
                captured_.insert(captured_.end(), received_.begin(),
                                 received_.begin() + static_cast<std::ptrdiff_t>(size));
            }

            if (report_.expectedBytesMatched) {
                ++report_.matchedSessions;
            }

            publish();

            if (!report_.expectedBytesMatched) {
                finish(true);
            } else {
                if (options_.readHttpHeaders) {
                    readHttpHeaders();
                } else {
                    writeReply();
                }
            }
        });
    }

    /** Apply the optional read delay after the initial write has completed. */
    void delayRead() {
        operation_.cancel();

        if (options_.readDelay.count() == 0) {
            readExpected();

            return;
        }

        pause_.expires_after(options_.readDelay);
        pause_.async_wait([this](Error error) {
            if (!error && !finished_) {
                readExpected();
            }
        });
    }

    /** Send the optional challenge before starting the expected client read. */
    void writeInitial() {
        if (options_.initialBytes.empty()) {
            delayRead();

            return;
        }

        armOperation(options_.writeTimeout);
        asio::async_write(stream_, asio::buffer(options_.initialBytes), [this](Error error, std::size_t size) {
            report_.bytesWritten += size;
            publish();

            if (finished_) {
                return;
            }

            if (error) {
                finish(true);
            } else {
                delayRead();
            }
        });
    }

    /** Handshake after the explicit optional pre-handshake pause. */
    void handshake() {
        armOperation(options_.handshakeTimeout);
        stream_.async_handshake(asio::ssl::stream_base::server, [this](Error error) {
            if (finished_) {
                return;
            }

            const char *name = SSL_get_servername(stream_.native_handle(), TLSEXT_NAMETYPE_host_name);
            report_.sniPresent = name != nullptr;
            report_.sniLocalhost = name != nullptr && std::strcmp(name, "localhost") == 0;

            if (error) {
                finish(true);

                return;
            }

            operation_.cancel();
            report_.handshakeComplete = true;
            ++report_.successfulHandshakes;
            const auto *cipher = SSL_get_current_cipher(stream_.native_handle());
            report_.cipherSuite = cipher ? SSL_CIPHER_get_protocol_id(cipher) : 0;
            const auto group = SSL_get_negotiated_group(stream_.native_handle());
            report_.groupId = group > 0 ? static_cast<std::uint32_t>(group) : 0;
            int signature = 0;

            if (SSL_get_signature_nid(stream_.native_handle(), &signature) == 1 && signature > 0) {
                report_.signatureHashNid = static_cast<std::uint32_t>(signature);
            }

            const auto version = SSL_version(stream_.native_handle());
            report_.tlsVersion = version == TLS1_VERSION     ? 10U
                                 : version == TLS1_1_VERSION ? 11U
                                 : version == TLS1_2_VERSION ? 12U
                                                             : 13U;
            publish();
            writeInitial();
        });
    }

    public:
    /** Construct one ordinary listener, with no remote resolution or external endpoint.
     * @param io Child-owned loop with no callbacks from any previous session.
     * @param acceptor Stable child listener retained through both sessions.
     * @param context Child-owned SSL context, retained beyond run().
     * @param options Validated bounded owned script.
     * @param directory Exact parent-created directory holding control/report/stop files.
     */
    PeerSession(asio::io_context &io, Tcp::acceptor &acceptor, asio::ssl::context &context, t::PeerOptions options,
                std::filesystem::path directory, unsigned ordinal, t::PeerSnapshot previous,
                std::chrono::steady_clock::time_point absoluteDeadline)
        : io_(io), acceptor_(acceptor), stream_(io_, context), overall_(io_), operation_(io_), pause_(io_),
          stopPoll_(io_), options_(std::move(options)), directory_(std::move(directory)) {
        ordinal_ = ordinal;
        absoluteDeadline_ = absoluteDeadline;
        report_.completedSessions = previous.completedSessions;
        report_.successfulHandshakes = previous.successfulHandshakes;
        report_.matchedSessions = previous.matchedSessions;
        report_.firstCaptureEnd = previous.firstCaptureEnd;
        report_.secondCaptureEnd = previous.secondCaptureEnd;
        report_.port = acceptor_.local_endpoint().port();
        report_.ready = true;
        SSL_set_msg_callback(stream_.native_handle(), observeMessage);
        SSL_set_msg_callback_arg(stream_.native_handle(), this);
    }

    /** Remove the borrowed callback argument before any report/buffer/stream member is destroyed. */
    ~PeerSession() {
        SSL_set_msg_callback(stream_.native_handle(), nullptr);
        SSL_set_msg_callback_arg(stream_.native_handle(), nullptr);
    }

    /** Copy numeric observations after the event loop drained.
     * @return Detached report; no socket, stream or callback borrow escapes.
     */
    t::PeerSnapshot snapshot() const {
        return report_;
    }

    /** Run until script completion, parent stop or a deadline; drain canceled operations before returning.
     * @return Zero for ordinary script/stop completion, one for bounded TLS/I/O/script failure.
     */
    int run() {
        try {
            overall_.expires_at(absoluteDeadline_);
            overall_.async_wait([this](Error error) {
                if (!error && !finished_) {
                    finish(true, true);
                }
            });
            pollStop();
            acceptor_.async_accept(stream_.lowest_layer(), [this](Error error) {
                if (finished_) {
                    return;
                }

                if (error) {
                    finish(true);

                    return;
                }

                report_.accepted = true;
                publish();

                if (options_.pauseBeforeHandshake.count() == 0) {
                    handshake();

                    return;
                }

                pause_.expires_after(options_.pauseBeforeHandshake);
                pause_.async_wait([this](Error pauseError) {
                    if (!pauseError && !finished_) {
                        handshake();
                    }
                });
            });
            publish();
            io_.run();
        } catch (...) {
            drainFailure();
        }

        return report_.failed ? 1 : 0;
    }
};

/** Print standalone internal-test help before any input, SDK initialization or socket operation.
 * @return Zero on successful stdout output, one on output failure.
 */
int printHelp() {
    std::cout
        << "Internal isolated localhost TLS test peer\n"
           "Usage: qdcxx_tls_test_peer [--help] | --control ABSOLUTE_OWNED_FILE\n"
           "No arguments and --help print this help without files, TLS initialization or sockets.\n"
           "--control reads parent-owned binary v5 (or compatible v2/v3/v4); at most two fresh TLS connections.\n"
           "Input includes explicit ordinary PEM fixtures, exact TLS 1.0/1.1/1.2/1.3, scripts and deadlines.\n"
           "Legacy versions and CBC/P-521/SHA-1 policies are isolated negative-test server modes only.\n"
           "Numeric report flags expose only SNI presence and exact localhost match, never the name.\n"
           "Report v3 carries cipher/group/signature-hash IDs, ticket count and up to two session counters.\n"
           "TLS 1.3 initial ticket count is explicitly 0..2, default 0; other versions require zero.\n"
           "Limits: 4 MiB/script, 4096 UTF-8 bytes/path, 128 KiB/PEM fixture, 1..60000 ms/timeout.\n"
           "Scripts: initial write/read, reply write/read, final write; each at most 4 MiB, 12 MiB writes/session.\n"
           "Opt-in captureUntilClose retains at most 4 MiB synthetic client bytes/session until EOF or reset.\n"
           "A successor uses the same endpoint and fresh SSL owners after the first session drains.\n"
           "API defaults: TLS 1.3; clean close_notify; empty writes; no reads/capture/successor; zero delays;\n"
           "startup/handshake/read/write 5000 ms; overall 15000 ms; parent stop grace 250 ms.\n"
           "Modes: clean close_notify, abrupt TCP EOF, hold; optional handshake/read delays.\n"
           "Side effects: owned loopback listener, numeric report/report.next and opt-in capture-1/capture-2 files;\n"
           "polls its stop marker; both sessions share the absolute overall deadline after control validation.\n"
           "Only opt-in synthetic client bytes enter captures; no keys, payloads or SDK errors are printed.\n"
           "Exit: 0 normal script/stop or help; 1 invalid control/TLS/I/O/deadline; 2 invalid arguments.\n";
    std::cout.flush();

    return std::cout ? 0 : 1;
}

/** Validate the exact owned control location and run only its bounded ordinary TLS script.
 * @param control Explicit parent-owned file, never interpreted as command text.
 * @return One on fixed diagnostic failure; otherwise the session result.
 */
int runControl(const std::filesystem::path &control) {
    try {
        require(control.is_absolute() && control.filename() == "control" &&
                control.parent_path().filename().string().find("qdcxx-tls-peer-") == 0);
        auto options = readControl(control);
        const auto absoluteDeadline = std::chrono::steady_clock::now() + options.overallTimeout;
        Crypto crypto;
        auto next = std::move(options.successor);
        options.successor.reset();
        asio::io_context io;
        Tcp::acceptor acceptor(io, {asio::ip::address_v4::loopback(), 0}, options.captureUntilClose);
        t::PeerSnapshot first;
        {
            auto context = crypto.makeContext(options);
            PeerSession session(io, acceptor, context, options, control.parent_path(), 1, {}, absoluteDeadline);
            const auto result = session.run();
            first = session.snapshot();

            if (result != 0 || !next) {
                return result;
            }
        }
        options.initialBytes = std::move(next->initialBytes);
        options.replyBytes = std::move(next->replyBytes);
        options.expectedClientBytes = std::move(next->expectedClientBytes);
        options.pauseBeforeHandshake = next->pauseBeforeHandshake;
        options.expectedReplyClientBytes = std::move(next->expectedReplyClientBytes);
        options.finalBytes = std::move(next->finalBytes);
        auto context = crypto.makeContext(options);
        io.restart();
        PeerSession session(io, acceptor, context, std::move(options), control.parent_path(), 2, first,
                            absoluteDeadline);

        return session.run();
    } catch (...) {
        std::cerr << "Internal TLS peer failed.\n";

        return 1;
    }
}
} // namespace

#ifdef _WIN32
/** Parse Windows native paths without locale-dependent argv conversion.
 * @param argc Argument count.
 * @param argv Native Windows arguments; only the owned control path can be supplied.
 * @return Documented standalone internal-helper exit status.
 */
int wmain(int argc, wchar_t **argv) {
    if (argc == 1 || (argc == 2 && std::wstring(argv[1]) == L"--help")) {
        return printHelp();
    }

    if (argc != 3 || std::wstring(argv[1]) != L"--control") {
        std::cerr << "Invalid internal TLS peer arguments; use --help.\n";

        return 2;
    }

    return runControl(std::filesystem::path(argv[2]));
}
#else
/** Parse POSIX native paths without shell invocation.
 * @param argc Argument count.
 * @param argv Arguments; only the owned control path can be supplied.
 * @return Documented standalone internal-helper exit status.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        return printHelp();
    }

    if (argc != 3 || std::string(argv[1]) != "--control") {
        std::cerr << "Invalid internal TLS peer arguments; use --help.\n";

        return 2;
    }

    return runControl(std::filesystem::path(argv[2]));
}
#endif
