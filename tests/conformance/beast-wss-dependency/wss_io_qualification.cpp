// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "wss_session.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#    include <windows.h>
#endif

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;
namespace ws = beast::websocket;
namespace wss = org::ttldtor::qdcxx::kraken;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto DRIVER_LIMIT = 5s;
constexpr auto POLL_SLICE = 2ms;
constexpr auto STALLED_WRITE_INTERVAL = 100ms;
constexpr std::size_t MESSAGE_LIMIT = 32;
constexpr std::size_t LARGE_MESSAGE_LIMIT = 256 * 1024;
constexpr int PEER_RECEIVE_WINDOW = 1024;
constexpr int MAX_BACKPRESSURE_WRITES = 64;
constexpr int REOPEN_ATTEMPTS = 8;

/** Stop the fixture on a failed check.
 * @param ok Condition required by the scenario.
 * @param step Static failure name.
 */
void require(bool ok, const char *step) {
    if (!ok) {
        throw std::runtime_error(step);
    }
}

/** Standard Beast/TLS peer with independently driven, bounded fixture operations. */
class Peer final {
    using Socket = ws::stream<ssl::stream<beast::tcp_stream>, false>;

    asio::io_context io_;
    Tcp::acceptor acceptor_{io_, Tcp::endpoint{asio::ip::address_v4::loopback(), 0}};
    ssl::context context_{ssl::context::tls_server};
    std::optional<Socket> socket_;
    beast::flat_buffer received_{LARGE_MESSAGE_LIMIT + 1};
    std::string outgoing_;
    ws::ping_data ping_;
    std::string pong_;
    std::array<char, 128> abortBuffer_{};
    std::optional<Error> readResult_;
    std::optional<Error> writeResult_;
    bool abortFrameReceived_ = false;
    bool ready_ = false;

    /** @return TCP socket owned below the peer's TLS and WebSocket layers. */
    Tcp::socket &tcp() noexcept {
        return beast::get_lowest_layer(*socket_).socket();
    }

    public:
    /** Start listening and arm a verified connection fixture.
     * @param files Explicit directory of local synthetic TLS fixtures.
     * @param constrainReceive Whether to use a small native receive window before accept.
     */
    Peer(const std::filesystem::path &files, bool constrainReceive) {
        if (constrainReceive) {
            acceptor_.set_option(asio::socket_base::receive_buffer_size{PEER_RECEIVE_WINDOW});
        }

        context_.use_certificate_chain_file((files / "server.pem").string());
        context_.use_private_key_file((files / "server.key").string(), ssl::context::pem);
        socket_.emplace(io_, context_);
        socket_->control_callback([this](ws::frame_type kind, beast::string_view payload) {
            if (kind == ws::frame_type::pong) {
                pong_.assign(payload.data(), payload.size());
            }
        });
        acceptor_.async_accept(tcp(), [this](Error error) {
            if (!error) {
                tcp().set_option(Tcp::no_delay{true}, error);
            }

            if (!error) {
                socket_->next_layer().async_handshake(ssl::stream_base::server, [this](Error tlsError) {
                    if (!tlsError) {
                        socket_->async_accept([this](Error upgradeError) {
                            ready_ = !upgradeError;
                        });
                    }
                });
            }
        });
    }

    /** Close all fixture handles and drain their completions before destroying fixture storage. */
    ~Peer() {
        Error ignored;
        acceptor_.close(ignored);
        tcp().close(ignored);
        io_.restart();
        (void)io_.run();
    }

    /** Drive only immediately available peer work. */
    void poll() {
        io_.restart();
        (void)io_.poll();
    }

    /** @return Locally allocated numeric listener port. */
    std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    /** @return True once the normal Beast accept operation has succeeded. */
    bool ready() const noexcept {
        return ready_;
    }

    /** Arm a read to receive application bytes and service control frames. */
    void read() {
        received_.consume(received_.size());
        readResult_.reset();
        socket_->async_read(received_, [this](Error error, std::size_t) {
            readResult_ = error;
        });
    }

    /** Read the client's failure close, then complete TLS teardown without sending another frame. */
    void answerFailureClose() {
        auto &tls = socket_->next_layer();

        tls.async_read_some(asio::buffer(abortBuffer_), [this](Error error, std::size_t bytes) {
            if (!error && bytes != 0) {
                abortFrameReceived_ = true;
                socket_->next_layer().async_shutdown([this](Error) {
                    Error ignored;
                    tcp().close(ignored);
                });
            }
        });
    }

    /** @return True once the failing client's bounded closing bytes reached the fixture. */
    bool abortFrameReceived() const noexcept {
        return abortFrameReceived_;
    }

    /** Deliberately return an unsolicited WebSocket extension in a valid response. */
    void advertiseCompression() {
        socket_->set_option(ws::stream_base::decorator([](ws::response_type &response) {
            response.set(beast::http::field::sec_websocket_extensions, "permessage-deflate");
        }));
    }

    /** @return Most recent read completion, or no value while pending. */
    const std::optional<Error> &readResult() const noexcept {
        return readResult_;
    }

    /** @return Most recent write completion, or no value while pending. */
    const std::optional<Error> &writeResult() const noexcept {
        return writeResult_;
    }

    /** @return Borrowed text received by the fixture. */
    std::string_view receivedText() const noexcept {
        const auto bytes = received_.data();

        return {static_cast<const char *>(bytes.data()), bytes.size()};
    }

    /** @return Last pong payload observed through Beast's peer callback. */
    std::string_view echoedPong() const noexcept {
        return pong_;
    }

    /** Send one data frame using Beast's message fragmentation support.
     * @param fin True for the final fragment.
     * @param bytes Payload held by this peer through the completion.
     * @param text Whether to start a text message rather than binary.
     */
    void frame(bool fin, std::string_view bytes, bool text = true) {
        outgoing_.assign(bytes);
        writeResult_.reset();
        socket_->text(text);
        socket_->async_write_some(fin, asio::buffer(outgoing_), [this](Error error, std::size_t) {
            writeResult_ = error;
        });
    }

    /** Send a ping that can interleave with data fragments.
     * @param bytes Ping payload retained through completion.
     */
    void ping(std::string_view bytes) {
        pong_.clear();
        ping_.assign(bytes.data(), bytes.size());
        writeResult_.reset();
        socket_->async_ping(ping_, [this](Error error) {
            writeResult_ = error;
        });
    }

    /** Send one synthetic invalid frame over the verified TLS channel.
     * @param bytes Fixed test bytes selected by the fixture.
     */
    void invalidFrame(std::string_view bytes) {
        outgoing_.assign(bytes);
        writeResult_.reset();
        asio::async_write(socket_->next_layer(), asio::buffer(outgoing_), [this](Error error, std::size_t) {
            writeResult_ = error;
        });
    }

    /** Begin a standard peer close with a bounded test reason. */
    void close() {
        const ws::close_reason reason{ws::close_code::going_away, "fixture"};
        writeResult_.reset();
        socket_->async_close(reason, [this](Error error) {
            writeResult_ = error;
        });
    }

    /** Abruptly cut the route without a WebSocket or TLS close.
     * @param reset True to choose an abortive TCP reset rather than an ordinary FIN.
     */
    void disconnect(bool reset) {
        if (reset) {
            tcp().set_option(asio::socket_base::linger{true, 0});
        }

        tcp().close();
    }

    /** @return Whether encrypted client data is queued on a non-reading peer. */
    bool unreadBytes() {
        Error error;
        const auto bytes = tcp().available(error);

        return !error && bytes != 0;
    }
};

/** One independently verified client and loopback peer on the fixture owner thread. */
class Fixture final {
    Peer peer_;
    std::unique_ptr<wss::WssSession> client_;

    public:
    /** Open the selected finite WSS test profile.
     * @param files Directory of local synthetic TLS inputs.
     * @param trust Explicit trusted CA bytes.
     * @param limit Configured byte limit for both message directions.
     * @param blockedPeer True for the reduced-window fixture that withholds reads.
     * @param receiveTimeout Optional operation deadline for a deliberately idle read.
     * @param sendTimeout Optional deadline for a deliberately blocked write.
     * @param closeTimeout Optional deadline for a deliberately unacknowledged close.
     */
    Fixture(const std::filesystem::path &files, std::string_view trust, std::size_t limit = MESSAGE_LIMIT,
            bool blockedPeer = false, std::chrono::milliseconds receiveTimeout = 5s,
            std::chrono::milliseconds sendTimeout = 5s, std::chrono::milliseconds closeTimeout = 5s)
        : peer_(files, blockedPeer) {
        const wss::WssSettings settings{.routeHost = "127.0.0.1",
                                        .identity = "localhost",
                                        .trustPem = trust,
                                        .port = peer_.port(),
                                        .receiveTimeout = receiveTimeout,
                                        .sendTimeout = sendTimeout,
                                        .closeTimeout = closeTimeout,
                                        .maxMessageBytes = limit};
        auto opened = wss::WssSession::begin(settings);
        require(opened.has_value(), "fixture-open");
        client_ = std::move(*opened);
        require(client_->startReceive() == wss::WssAdmission::NOT_READY, "pre-upgrade-no-receive");
        drive(
            [&] {
                return client_->stage() == wss::WssStage::READY && peer_.ready();
            },
            "fixture-upgrade");
    }

    /** @return Borrowed client owner for the current case. */
    wss::WssSession &client() noexcept {
        return *client_;
    }

    /** @return Borrowed loopback peer for the current case. */
    Peer &peer() noexcept {
        return peer_;
    }

    /** Release a session while separately preserving its caller-owned result. */
    void releaseClient() noexcept {
        client_.reset();
    }

    /** Drive at most one client handler with one nonblocking peer poll. */
    void step() {
        peer_.poll();
        (void)client_->runOneFor(POLL_SLICE);
    }

    /** Drive the fixture under its external watchdog.
     * @tparam Done Predicate on fixture state.
     * @param done Completion condition.
     * @param label Static failure name.
     */
    template <typename Done> void drive(Done done, const char *label) {
        const auto end = Clock::now() + DRIVER_LIMIT;

        while (!done() && Clock::now() < end) {
            step();
        }

        require(done(), label);
    }

    /** Drive through successful completion of the current peer write. */
    void finishPeerWrite() {
        drive(
            [&] {
                return peer_.writeResult().has_value();
            },
            "fixture-write-completion");
        require(!*peer_.writeResult(), "fixture-write-success");
    }

    /** Require a terminal result and absence of any staged text.
     * @param phase Original operation that produced the receipt.
     * @param reason Expected typed result.
     * @return Borrowed diagnostic, valid until releasing the client.
     */
    const wss::WssDiagnostic &terminal(wss::WssStage phase, wss::WssError reason) {
        const auto state = reason == wss::WssError::GRACEFUL_CLOSE ? wss::WssStage::CLOSED : wss::WssStage::FAILED;
        drive(
            [&] {
                return client_->stage() == wss::WssStage::CLOSED || client_->stage() == wss::WssStage::FAILED;
            },
            "terminal-retired");
        const auto &diagnostic = client_->diagnostic();

        if (client_->stage() != state || !diagnostic || diagnostic->stage != phase || diagnostic->reason != reason) {
            if (diagnostic) {
                std::cerr << "expected_reason=" << static_cast<int>(reason)
                          << " actual_reason=" << static_cast<int>(diagnostic->reason)
                          << " native=" << diagnostic->native.message() << '\n';
            }

            throw std::runtime_error("terminal-receipt");
        }

        require(!client_->takeMessage() && client_->startReceive() == wss::WssAdmission::NOT_READY,
                "terminal-no-new-work");

        return *diagnostic;
    }
};

/** Exercise exact-limit fragmentation, pong and unchanged owning payload through teardown.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void fragmentAndControl(const std::filesystem::path &files, std::string_view ca) {
    Fixture fixture{files, ca};
    auto &client = fixture.client();
    auto &peer = fixture.peer();
    require(client.startReceive() == wss::WssAdmission::ACCEPTED && client.startReceive() == wss::WssAdmission::BUSY &&
                client.startSend("busy") == wss::WssAdmission::BUSY && client.startClose() == wss::WssAdmission::BUSY,
            "one-active-operation");
    peer.read();

    const auto first = std::string(MESSAGE_LIMIT - 3, 'a') + "\xE2\x82";
    const std::string_view finalByte{"\xAC"};
    const auto expected = first + finalByte.data();
    peer.frame(false, first);
    fixture.finishPeerWrite();
    peer.frame(false, finalByte);
    fixture.finishPeerWrite();
    peer.ping("interleave");
    fixture.finishPeerWrite();
    fixture.drive(
        [&] {
            return peer.echoedPong() == "interleave";
        },
        "masked-pong-echo");
    require(client.stage() == wss::WssStage::RECEIVING && !client.takeMessage(), "no-partial-or-control-payload");

    peer.frame(true, "");
    fixture.finishPeerWrite();
    fixture.drive(
        [&] {
            return client.stage() == wss::WssStage::READY;
        },
        "exact-cap-empty-final-fragment");
    require(client.startClose() == wss::WssAdmission::BUSY && client.startReceive() == wss::WssAdmission::BUSY,
            "one-result-backpressure");
    const auto held = client.takeMessage();
    require(held && held->text() == expected && held->text().size() == MESSAGE_LIMIT && !client.takeMessage(),
            "take-exact-utf8-once");

    require(client.startSend("") == wss::WssAdmission::INVALID_PAYLOAD &&
                client.startSend(std::string(MESSAGE_LIMIT + 1, 'x')) == wss::WssAdmission::INVALID_PAYLOAD &&
                client.startSend("\xFF") == wss::WssAdmission::INVALID_PAYLOAD,
            "output-admission-limits");
    auto original = std::string(MESSAGE_LIMIT, 's');
    const auto expectedOutput = original;
    require(client.startSend(original) == wss::WssAdmission::ACCEPTED, "write-admitted");
    original.assign("caller changed its buffer");
    fixture.drive(
        [&] {
            return client.stage() == wss::WssStage::READY && peer.readResult();
        },
        "copied-output");
    require(!*peer.readResult() && peer.receivedText() == expectedOutput, "exact-masked-output");

    require(client.startReceive() == wss::WssAdmission::ACCEPTED, "empty-receive");
    peer.frame(true, "");
    fixture.finishPeerWrite();
    fixture.drive(
        [&] {
            return client.stage() == wss::WssStage::READY;
        },
        "empty-completed");
    require(!client.takeMessage(), "empty-not-delivered");

    require(client.startReceive() == wss::WssAdmission::ACCEPTED, "second-text-receive");
    peer.frame(true, R"({"channel":"status"})");
    fixture.finishPeerWrite();
    fixture.drive(
        [&] {
            return client.stage() == wss::WssStage::READY;
        },
        "second-text-completed");
    client.requestStop();
    (void)fixture.terminal(wss::WssStage::READY, wss::WssError::CANCELLED);
    fixture.releaseClient();
    require(held->text() == expected, "taken-owner-live-after-teardown");
}

/** Verify an interrupted transport yields explicit EOF or reset, without a graceful receipt.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 * @param reset True for an abortive reset.
 */
void transportEnd(const std::filesystem::path &files, std::string_view ca, bool reset) {
    Fixture fixture{files, ca};
    require(fixture.client().startReceive() == wss::WssAdmission::ACCEPTED, "transport-end-read-armed");
    fixture.step();
    fixture.peer().disconnect(reset);
    const auto &receipt =
        fixture.terminal(wss::WssStage::RECEIVING, reset ? wss::WssError::NATIVE : wss::WssError::PEER_EOF);
    require(receipt.closeCode == 0 &&
                (reset ? receipt.native == asio::error::connection_reset
                       : receipt.native == asio::error::eof || receipt.native == ssl::error::stream_truncated),
            "transport-end-native-receipt");
}

/** Verify both directions of an orderly WebSocket and TLS close.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 * @param local True if the client starts the close.
 */
void orderlyClose(const std::filesystem::path &files, std::string_view ca, bool local) {
    Fixture fixture{files, ca};

    if (local) {
        fixture.peer().read();
        require(fixture.client().startClose() == wss::WssAdmission::ACCEPTED, "local-close-arm");
    } else {
        require(fixture.client().startReceive() == wss::WssAdmission::ACCEPTED, "peer-close-read");
        fixture.peer().close();
    }

    const auto &receipt =
        fixture.terminal(local ? wss::WssStage::CLOSING : wss::WssStage::RECEIVING, wss::WssError::GRACEFUL_CLOSE);
    require(receipt.closeCode == (local ? ws::close_code::normal : ws::close_code::going_away), "exact-close-code");
}

/** Cut off an admitted close whose peer never processes the sent close frame.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void timeOutClose(const std::filesystem::path &files, std::string_view ca) {
    Fixture silent{files, ca, MESSAGE_LIMIT, false, 5s, 5s, 350ms};
    require(silent.client().startClose() == wss::WssAdmission::ACCEPTED, "silent-close-arm");
    silent.drive(
        [&] {
            return silent.peer().unreadBytes();
        },
        "silent-close-reaches-peer");
    require(silent.client().stage() == wss::WssStage::CLOSING, "silent-close-outstanding");
    (void)silent.terminal(wss::WssStage::CLOSING, wss::WssError::TIMEOUT);
}

/** Verify read refusal for a payload beyond the per-message limit.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 * @param fragmented True to check the cumulative limit across data fragments.
 */
void oversize(const std::filesystem::path &files, std::string_view ca, bool fragmented) {
    Fixture fixture{files, ca};
    require(fixture.client().startReceive() == wss::WssAdmission::ACCEPTED, "oversize-read");
    fixture.peer().answerFailureClose();

    if (fragmented) {
        fixture.peer().frame(false, std::string(MESSAGE_LIMIT, 'a'));
        fixture.finishPeerWrite();
        fixture.peer().frame(true, "b");
    } else {
        fixture.peer().frame(true, std::string(MESSAGE_LIMIT + 1, 'a'));
    }

    fixture.finishPeerWrite();
    const auto &receipt = fixture.terminal(wss::WssStage::RECEIVING, wss::WssError::MESSAGE_LIMIT);
    require(receipt.native == ws::error::message_too_big && fixture.peer().abortFrameReceived(),
            "native-message-limit");
}

/** Verify pinned Beast rejects compressed or malformed input and owns the protocol close.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 * @param firstByte Synthetic RFC 6455 header byte for the selected invalid case.
 * @param expected Specific Beast violation.
 * @param data Payload byte used by the synthetic test frame.
 */
void protocolError(const std::filesystem::path &files, std::string_view ca, char firstByte, Error expected,
                   char data = 'x') {
    Fixture fixture{files, ca};
    const char frame[]{firstByte, 1, data};
    fixture.peer().invalidFrame({frame, sizeof(frame)});
    fixture.finishPeerWrite();
    fixture.peer().answerFailureClose();
    require(fixture.client().startReceive() == wss::WssAdmission::ACCEPTED, "protocol-read");
    const auto &receipt = fixture.terminal(wss::WssStage::RECEIVING, wss::WssError::PROTOCOL);
    require(receipt.native == expected && fixture.peer().abortFrameReceived(), "exact-protocol-receipt");
}

/** Reject an unsolicited compression selection before exposing readiness.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void unsolicitedCompression(const std::filesystem::path &files, std::string_view ca) {
    Peer peer{files, false};
    peer.advertiseCompression();
    const wss::WssSettings options{
        .routeHost = "127.0.0.1", .identity = "localhost", .trustPem = ca, .port = peer.port()};
    const auto client = wss::WssSession::begin(options);
    require(client.has_value(), "extension-attempt-admitted");
    const auto end = Clock::now() + DRIVER_LIMIT;

    while (!(*client)->diagnostic() && Clock::now() < end) {
        peer.poll();
        require((*client)->runOneFor(POLL_SLICE) != wss::WssStage::READY, "extension-never-ready");
    }

    require((*client)->stage() == wss::WssStage::FAILED && (*client)->diagnostic() &&
                (*client)->diagnostic()->stage == wss::WssStage::UPGRADE &&
                (*client)->diagnostic()->reason == wss::WssError::UNSUPPORTED_EXTENSION,
            "unsolicited-extension-receipt");
}

/** Require rejection of a bounded but unsupported binary message.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void binary(const std::filesystem::path &files, std::string_view ca) {
    Fixture fixture{files, ca};
    require(fixture.client().startReceive() == wss::WssAdmission::ACCEPTED, "binary-read");
    fixture.peer().frame(true, "binary", false);
    fixture.finishPeerWrite();
    (void)fixture.terminal(wss::WssStage::RECEIVING, wss::WssError::UNSUPPORTED_BINARY);
}

/** Cancel a genuinely active receive after servicing control, and check an independent idle deadline.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void cancelAndTimeOutRead(const std::filesystem::path &files, std::string_view ca) {
    Fixture stuck{files, ca};
    require(stuck.client().startReceive() == wss::WssAdmission::ACCEPTED, "cancel-read-arm");
    stuck.peer().read();
    stuck.peer().frame(false, "unfinished");
    stuck.finishPeerWrite();
    stuck.peer().ping("before-cutoff");
    stuck.finishPeerWrite();
    stuck.drive(
        [&] {
            return stuck.peer().echoedPong() == "before-cutoff";
        },
        "read-observed-by-peer");
    stuck.client().requestStop();
    require(stuck.client().stage() == wss::WssStage::STOPPING && !stuck.client().takeMessage(), "pending-read-cutoff");
    (void)stuck.terminal(wss::WssStage::RECEIVING, wss::WssError::CANCELLED);

    Fixture idle{files, ca, MESSAGE_LIMIT, false, 150ms};
    require(idle.client().startReceive() == wss::WssAdmission::ACCEPTED, "idle-read-arm");
    (void)idle.terminal(wss::WssStage::RECEIVING, wss::WssError::TIMEOUT);
}

/** Fill the peer's bounded native receive window until one admitted write remains outstanding.
 * @param fixture Ready reduced-window fixture that deliberately does not read.
 */
void stallWrite(Fixture &fixture) {
    const std::string payload(LARGE_MESSAGE_LIMIT, 'w');

    for (int index = 0; index < MAX_BACKPRESSURE_WRITES; ++index) {
        require(fixture.client().startSend(payload) == wss::WssAdmission::ACCEPTED, "blocked-send-arm");
        const auto end = Clock::now() + STALLED_WRITE_INTERVAL;

        while (fixture.client().stage() == wss::WssStage::SENDING && Clock::now() < end) {
            fixture.step();
        }

        if (fixture.client().stage() == wss::WssStage::SENDING) {
            require(fixture.peer().unreadBytes(), "blocked-send-route-has-bytes");

            return;
        }

        require(fixture.client().stage() == wss::WssStage::READY, "blocked-send-not-failed");
    }

    throw std::runtime_error("expected-native-backpressure");
}

/** Verify cancellation and deadline of an actual delayed write to a non-reading peer.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void cancelAndTimeOutWrite(const std::filesystem::path &files, std::string_view ca) {
    Fixture cancelled{files, ca, LARGE_MESSAGE_LIMIT, true};
    stallWrite(cancelled);
    cancelled.client().requestStop();
    require(cancelled.client().stage() == wss::WssStage::STOPPING, "pending-write-cutoff");
    (void)cancelled.terminal(wss::WssStage::SENDING, wss::WssError::CANCELLED);

    Fixture delayed{files, ca, LARGE_MESSAGE_LIMIT, true, 5s, 700ms};
    stallWrite(delayed);
    (void)delayed.terminal(wss::WssStage::SENDING, wss::WssError::TIMEOUT);
}

/** Observe one warmed process's native resources between fully destroyed attempts.
 * @return Windows handle count or Linux open descriptor count, including the enumeration descriptor.
 */
std::size_t nativeOpenCount() {
#if defined(_WIN32)
    DWORD count = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0, "fixture-handle-count");

    return count;
#else
    const auto entries = std::filesystem::directory_iterator{"/proc/self/fd"};

    return static_cast<std::size_t>(std::distance(entries, std::filesystem::directory_iterator{}));
#endif
}

/** Reopen independently after each canceled attempt and verify warmed native resource quiescence.
 * @param files Directory of local synthetic TLS inputs.
 * @param ca Explicit trusted CA.
 */
void reopenAfterRetirement(const std::filesystem::path &files, std::string_view ca) {
    const auto initial = nativeOpenCount();
    std::optional<wss::WssMessage> firstMessage;

    for (int attempt = 0; attempt < REOPEN_ATTEMPTS; ++attempt) {
        const bool blockWrite = attempt % 2 != 0;
        Fixture fixture{files, ca, LARGE_MESSAGE_LIMIT, blockWrite};
        auto &client = fixture.client();

        if (blockWrite) {
            stallWrite(fixture);
        } else {
            const auto expected = "attempt-" + std::to_string(attempt);
            require(client.startReceive() == wss::WssAdmission::ACCEPTED, "reopen-receive-arm");
            fixture.peer().frame(true, expected);
            fixture.finishPeerWrite();
            fixture.drive(
                [&] {
                    return client.stage() == wss::WssStage::READY;
                },
                "reopen-message");
            auto message = client.takeMessage();
            require(message && message->text() == expected, "reopen-fresh-text");

            if (!firstMessage) {
                firstMessage = std::move(message);
            }

            require(client.startReceive() == wss::WssAdmission::ACCEPTED, "reopen-idle-read-arm");
            fixture.step();
        }

        require(client.stage() == (blockWrite ? wss::WssStage::SENDING : wss::WssStage::RECEIVING),
                "reopen-outstanding-io");
        client.requestStop();
        require(client.stage() == wss::WssStage::STOPPING, "reopen-cutoff-before-retirement");
        (void)fixture.terminal(blockWrite ? wss::WssStage::SENDING : wss::WssStage::RECEIVING,
                               wss::WssError::CANCELLED);
        fixture.releaseClient();
    }

    const auto final = nativeOpenCount();
    require(firstMessage && firstMessage->text() == "attempt-0", "reopen-retained-text");
    std::cout << "Independent attempts=" << REOPEN_ATTEMPTS << " native_handles_or_fds_before=" << initial
              << " after=" << final << '\n';
    require(initial == final, "reopen-native-quiescence");
}

/** Verify a missing trust configuration and a TCP endpoint with no listening provider.
 * @param ca Explicit trusted CA used only for the connection-refusal case.
 */
void missingTrustAndPeer(std::string_view ca) {
    asio::io_context io;
    Tcp::acceptor port{io};

    port.open(Tcp::v4());
    port.bind(Tcp::endpoint{asio::ip::address_v4::loopback(), 0});
    wss::WssSettings settings{.routeHost = "127.0.0.1", .identity = "localhost", .port = port.local_endpoint().port()};
    const auto noTrust = wss::WssSession::begin(settings);
    require(!noTrust && noTrust.error().stage == wss::WssStage::CONFIGURATION &&
                noTrust.error().reason == wss::WssError::TRUST,
            "missing-explicit-trust");

    settings.trustPem = ca;
    const auto missingPeer = wss::WssSession::begin(settings);
    require(missingPeer.has_value(), "missing-peer-admitted-config");

    auto &client = **missingPeer;
    const auto end = Clock::now() + DRIVER_LIMIT;

    while (client.stage() != wss::WssStage::FAILED && Clock::now() < end) {
        require(client.runOneFor(POLL_SLICE) != wss::WssStage::READY, "missing-peer-never-ready");
    }

    const auto &diagnostic = client.diagnostic();
    require(client.stage() == wss::WssStage::FAILED && diagnostic && diagnostic->stage == wss::WssStage::CONNECT &&
                diagnostic->reason == wss::WssError::NATIVE && diagnostic->native == asio::error::connection_refused &&
                !client.takeMessage(),
            "missing-peer-connection-refused");
}

} // namespace

/** Run the established connection's finite message, control and cancellation matrix.
 * @param argc Number of arguments including the executable.
 * @param argv Expected local synthetic TLS fixtures folder as the sole argument.
 * @return Zero if the whole fixture matrix passed.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 2, "fixture-directory-argument");
        require(OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) == 1, "fixture-init");
        const std::filesystem::path files{argv[1]};
        std::ifstream input{files / "ca.pem", std::ios::binary};
        require(static_cast<bool>(input), "fixture-ca-read");
        const std::string ca{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};

        fragmentAndControl(files, ca);
        oversize(files, ca, false);
        oversize(files, ca, true);
        protocolError(files, ca, static_cast<char>(0xC1), ws::error::bad_reserved_bits);
        protocolError(files, ca, static_cast<char>(0x83), ws::error::bad_opcode);
        protocolError(files, ca, static_cast<char>(0x81), ws::error::bad_frame_payload, static_cast<char>(0xFF));
        unsolicitedCompression(files, ca);
        binary(files, ca);
        orderlyClose(files, ca, false);
        orderlyClose(files, ca, true);
        timeOutClose(files, ca);
        transportEnd(files, ca, false);
        transportEnd(files, ca, true);
        cancelAndTimeOutRead(files, ca);
        cancelAndTimeOutWrite(files, ca);
        reopenAfterRetirement(files, ca);
        missingTrustAndPeer(ca);
        std::cout << "Bounded WSS I/O: fragments, pong, limits, close, EOF/reset, delayed write and retirement\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
