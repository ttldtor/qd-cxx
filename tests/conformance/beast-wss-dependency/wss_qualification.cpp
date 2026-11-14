// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "wss_session.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <openssl/x509_vfy.h>

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

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
namespace wss = org::ttldtor::qdcxx::kraken;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto DRIVER_LIMIT = 5s;
constexpr auto POLL_SLICE = 2ms;
constexpr std::size_t FRAGMENT_BYTES = 13;
constexpr std::size_t MESSAGE_LIMIT = 200;
constexpr std::string_view TARGET = "/v2?profile=fixture";

/** Stop the test on a failed stage.
 * @param ok Condition that must hold.
 * @param stage Static failure description.
 */
void require(bool ok, const char *stage) {
    if (!ok) {
        throw std::runtime_error(stage);
    }
}

/** Read one tracked local TLS fixture.
 * @param path Explicit fixture path.
 * @return Complete byte text.
 */
std::string readFixture(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    require(static_cast<bool>(input), "fixture-read");

    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

/** Test-only stream that splits the normal Beast response across TLS writes. */
class FragmentingTlsStream {
    using Tls = ssl::stream<beast::tcp_stream>;
    Tls transport_;
    std::size_t fragments_ = 0;

    public:
    using executor_type = Tls::executor_type; ///< Executor type required by the Beast stream concept.

    /** Construct the local test transport at its final address.
     * @param io Peer executor.
     * @param context Prepared TLS server context.
     */
    FragmentingTlsStream(asio::io_context &io, ssl::context &context) : transport_(io, context) {
    }

    /** @return Executor associated with the local transport. */
    executor_type get_executor() noexcept {
        return transport_.get_executor();
    }

    /** @return Borrowed TLS layer for fixture setup and TCP access. */
    Tls &next_layer() noexcept {
        return transport_;
    }

    /** @return Borrowed TLS layer for inspecting the native test socket. */
    const Tls &next_layer() const noexcept {
        return transport_;
    }

    /** @return Number of encrypted write initiations from Beast or the test response driver. */
    std::size_t fragments() const noexcept {
        return fragments_;
    }

    /** Forward the peer read without changing the caller's buffer.
     * @tparam Buffers Mutable buffer sequence.
     * @tparam Handler Asynchronous completion token.
     * @param buffers Borrowed storage retained by the operation's caller.
     * @param handler Forwarded completion token.
     * @return The lower stream's initiation result.
     */
    template <typename Buffers, typename Handler>
    decltype(auto) async_read_some(const Buffers &buffers, Handler &&handler) {
        return transport_.async_read_some(buffers, std::forward<Handler>(handler));
    }

    /** Limit each SSL write so the client sees a fragmented response.
     * @tparam Buffers Constant buffer sequence.
     * @tparam Handler Asynchronous completion token.
     * @param buffers Borrowed response storage retained by Beast or the fixture.
     * @param handler Forwarded completion token.
     * @return The lower stream's initiation result.
     */
    template <typename Buffers, typename Handler>
    decltype(auto) async_write_some(const Buffers &buffers, Handler &&handler) {
        ++fragments_;

        return transport_.async_write_some(beast::buffers_prefix(FRAGMENT_BYTES, buffers),
                                           std::forward<Handler>(handler));
    }
};

/** Select one deterministic behavior for the loopback peer. */
enum class PeerMode {
    VALID,        ///< Produce a normal Beast WebSocket accept response.
    INVALID,      ///< Answer 101 with an incorrect accept token.
    REJECTED,     ///< Answer with 403 and a short body.
    LARGE_HEADER, ///< Reject an over-limit HTTP header before upgrade validation.
    LARGE_BODY,   ///< Refuse a declared body larger than the pinned parser limit.
    HOLD_TLS,     ///< Accept TCP, leave the peer TLS handshake unarmed.
    HOLD_UPGRADE, ///< Read the WebSocket request and withhold a response.
};

/** Isolated peer using the same certificate fixtures as native TLS conformance. */
class Peer final {
    using Socket = beast::websocket::stream<FragmentingTlsStream, false>;

    asio::io_context io_;
    Tcp::acceptor acceptor_{io_, Tcp::endpoint{asio::ip::address_v4::loopback(), 0}};
    ssl::context context_{ssl::context::tls_server};
    std::optional<Socket> websocket_;
    beast::flat_static_buffer<4096> requestBuffer_;
    http::request<http::empty_body> request_;
    std::string response_;
    std::string sni_;
    PeerMode mode_;
    bool accepted_ = false;
    bool upgradeReceived_ = false;
    bool ready_ = false;

    /** Emit a valid Beast response or a bounded negative HTTP fixture. */
    void sendResponse() {
        switch (mode_) {
        case PeerMode::VALID:
            websocket_->async_accept(request_, [this](Error error) {
                ready_ = !error;
            });

            return;
        case PeerMode::INVALID:
            response_ = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
                        "Sec-WebSocket-Accept: invalid\r\n\r\n";
            break;
        case PeerMode::REJECTED:
            response_ = "HTTP/1.1 403 Forbidden\r\nContent-Length: 3\r\n\r\nno!";
            break;
        case PeerMode::LARGE_HEADER:
            response_ = "HTTP/1.1 101 Switching Protocols\r\nX-Large: ";
            response_.append(9 * 1024, 'x').append("\r\n\r\n");
            break;
        case PeerMode::LARGE_BODY:
            response_ = "HTTP/1.1 403 Forbidden\r\nContent-Length: 8388609\r\n\r\n";
            break;
        default:
            return;
        }

        asio::async_write(websocket_->next_layer().next_layer(), asio::buffer(response_), [](Error, std::size_t) {
        });
    }

    /** Complete TLS and drive the synthetic upgrade when the selected case permits it. */
    void acceptTls() {
        auto &transport = websocket_->next_layer().next_layer();

        transport.async_handshake(ssl::stream_base::server, [this](Error error) {
            if (error) {
                return;
            }

            const auto *seen =
                SSL_get_servername(websocket_->next_layer().next_layer().native_handle(), TLSEXT_NAMETYPE_host_name);

            if (seen != nullptr) {
                sni_ = seen;
            }

            http::async_read(websocket_->next_layer(), requestBuffer_, request_, [this](Error error, std::size_t) {
                if (!error) {
                    upgradeReceived_ = true;
                    sendResponse();
                }
            });
        });
    }

    public:
    /** Open one loopback listener and prepare its explicit synthetic credentials.
     * @param fixtures Local folder containing TLS conformance inputs.
     * @param mode Response behavior for the current case.
     * @param leaf Server certificate fixture, with key server.key.
     * @param tlsVersion Fixed server protocol version for the case.
     */
    Peer(const std::filesystem::path &fixtures, PeerMode mode, const char *leaf = "server.pem",
         int tlsVersion = TLS1_3_VERSION)
        : mode_(mode) {
        auto *native = context_.native_handle();
        require(SSL_CTX_set_min_proto_version(native, tlsVersion) == 1 &&
                    SSL_CTX_set_max_proto_version(native, tlsVersion) == 1,
                "fixture-tls-version");
        context_.use_certificate_chain_file((fixtures / leaf).string());
        context_.use_private_key_file((fixtures / "server.key").string(), ssl::context::pem);
        websocket_.emplace(io_, context_);

        acceptor_.async_accept(beast::get_lowest_layer(*websocket_).socket(), [this](Error error) {
            if (!error) {
                beast::get_lowest_layer(*websocket_).socket().set_option(Tcp::no_delay{true}, error);
            }

            if (!error) {
                accepted_ = true;

                if (mode_ != PeerMode::HOLD_TLS) {
                    acceptTls();
                }
            }
        });
    }

    /** Close fixture I/O and retire its borrowed callbacks before releasing backing. */
    ~Peer() {
        Error ignored;
        acceptor_.close(ignored);
        beast::get_lowest_layer(*websocket_).socket().close(ignored);
        io_.restart();
        (void)io_.run();
    }

    /** @return Current numeric route port on the isolated listener. */
    std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    /** Drive the ready peer work without sleeping the test thread. */
    void poll() {
        io_.restart();
        (void)io_.poll();
    }

    /** @return True after the peer has accepted TCP and received TLS bytes in the silent case. */
    bool clientHelloQueued() noexcept {
        if (!accepted_) {
            return false;
        }

        Error error;
        const auto waiting = beast::get_lowest_layer(*websocket_).socket().available(error);

        return !error && waiting != 0;
    }

    /** @return True after the client has sent a complete WebSocket request. */
    bool upgradeReceived() const noexcept {
        return upgradeReceived_;
    }

    /** @return True after the peer sent its successful Beast upgrade response. */
    bool ready() const noexcept {
        return ready_;
    }

    /** @return Observed SNI value, independently read from the peer TLS engine. */
    std::string_view sni() const noexcept {
        return sni_;
    }

    /** @return Received WebSocket request for cross-checking HTTP routing. */
    const http::request<http::empty_body> &request() const noexcept {
        return request_;
    }

    /** @return Fragment count recorded by the peer transport. */
    std::size_t fragments() const noexcept {
        return websocket_->next_layer().fragments();
    }
};

/** Construct the basic explicit client options for one loopback fixture.
 * @param peer Listener providing the route port.
 * @param ca Borrowed certificate text.
 * @return Local test options.
 */
wss::WssSettings options(const Peer &peer, std::string_view ca) {
    return {.routeHost = "127.0.0.1",
            .identity = "localhost",
            .target = TARGET,
            .trustPem = ca,
            .port = peer.port(),
            .maxMessageBytes = MESSAGE_LIMIT};
}

/** Start a valid configuration and return its stable owner.
 * @param settings Expected admissible input.
 * @return Attempt with resolution already pending.
 */
std::unique_ptr<wss::WssSession> open(const wss::WssSettings &settings) {
    auto attempt = wss::WssSession::begin(settings);
    require(attempt.has_value() && (*attempt)->stage() == wss::WssStage::RESOLVE, "attempt-admission");

    return std::move(*attempt);
}

/** Progress both devices until the named observable condition or external deadline.
 * @tparam Done Local predicate type.
 * @param peer Loopback server driven by the fixture.
 * @param client The WSS attempt driven by its owning thread.
 * @param done Observable condition.
 * @param label Static test failure message.
 */
template <typename Done> void drive(Peer &peer, wss::WssSession &client, Done done, const char *label) {
    const auto deadline = Clock::now() + DRIVER_LIMIT;

    while (!done() && Clock::now() < deadline) {
        peer.poll();
        (void)client.runOneFor(POLL_SLICE);
    }

    require(done(), label);
}

/** Verify a failure after exact completion retirement.
 * @param client Attempt that should have reached terminal failure.
 * @param phase Originating operation.
 * @param reason Expected failure reason.
 * @return Exact native diagnostic for further assertions.
 */
const wss::WssDiagnostic &failure(const wss::WssSession &client, wss::WssStage phase, wss::WssError reason) {
    require(client.stage() == wss::WssStage::FAILED && client.diagnostic() && client.diagnostic()->stage == phase &&
                client.diagnostic()->reason == reason,
            "terminal-failure-receipt");

    return *client.diagnostic();
}

/** Assert ready-event admission after verified TLS and fragmented Beast upgrade.
 * @param fixtures Local TLS fixture folder.
 * @param ca Explicit admitted CA fixture.
 * @param version Selected TLS 1.2 or 1.3 server version.
 * @param route DNS or numeric loopback route, independent of the reference name.
 */
void ready(const std::filesystem::path &fixtures, std::string_view ca, int version, std::string_view route) {
    Peer peer{fixtures, PeerMode::VALID, "server.pem", version};
    auto settings = options(peer, ca);
    settings.routeHost = route;
    auto client = open(settings);

    drive(
        peer, *client,
        [&] {
            return (client->stage() == wss::WssStage::READY && peer.ready()) || client->diagnostic();
        },
        "verified-upgrade-deadline");
    require(client->stage() == wss::WssStage::READY && !client->diagnostic() &&
                client->readMessageLimit() == MESSAGE_LIMIT,
            "ready-and-message-cap");

    const auto authority = "localhost:" + std::to_string(peer.port());
    require(peer.request()[http::field::host] == authority && peer.request().target() == TARGET &&
                peer.sni() == "localhost" && peer.fragments() > 1,
            "fragmented-upgrade-route-and-identity");
    client->requestStop();
    (void)failure(*client, wss::WssStage::READY, wss::WssError::CANCELLED);
}

/** Verify refusal before a WebSocket request with a precise certificate diagnostic.
 * @param fixtures Local TLS fixture folder.
 * @param ca Client trust fixture.
 * @param identity DNS name for SNI and SAN verification.
 * @param leaf Local synthetic server certificate.
 * @param verification Exact native verification result.
 */
void rejectTls(const std::filesystem::path &fixtures, std::string_view ca, std::string_view identity, const char *leaf,
               long verification) {
    Peer peer{fixtures, PeerMode::VALID, leaf};
    auto settings = options(peer, ca);
    settings.identity = identity;
    auto client = open(settings);

    drive(
        peer, *client,
        [&] {
            return client->stage() == wss::WssStage::FAILED;
        },
        "verify-refusal");
    const auto &diagnostic = failure(*client, wss::WssStage::TLS, wss::WssError::TLS_VERIFICATION);
    require(diagnostic.verifyError == verification && !peer.upgradeReceived(), "verify-exact-and-no-upgrade");
}

/** Verify a refused upgrade never becomes ready.
 * @param fixtures Local TLS fixture folder.
 * @param ca Explicit trust for the peer credentials.
 * @param mode HTTP failure scenario.
 * @param expected Exact Beast HTTP or WebSocket error.
 * @param status Complete rejected HTTP status, or zero when parsing has failed.
 */
void rejectUpgrade(const std::filesystem::path &fixtures, std::string_view ca, PeerMode mode, Error expected,
                   unsigned status = 0) {
    Peer peer{fixtures, mode};
    auto client = open(options(peer, ca));

    drive(
        peer, *client,
        [&] {
            return client->stage() == wss::WssStage::FAILED;
        },
        "upgrade-refusal");
    const auto &diagnostic = failure(*client, wss::WssStage::UPGRADE, wss::WssError::NATIVE);
    require(peer.upgradeReceived() && diagnostic.native == expected && diagnostic.httpStatus == status,
            "upgrade-exact-failure");
}

/** Cut off one admitted operation, require its late completion to retire, then destroy the attempt.
 * @param fixtures Local TLS fixture folder.
 * @param ca Client trust.
 * @param phase Phase where the owner initiates cutoff.
 * @param mode Appropriate fixture response.
 */
void cancelIn(const std::filesystem::path &fixtures, std::string_view ca, wss::WssStage phase, PeerMode mode) {
    Peer peer{fixtures, mode};
    auto client = open(options(peer, ca));

    const auto pending = [&] {
        if (client->stage() != phase) {
            return false;
        }

        return (phase != wss::WssStage::TLS || peer.clientHelloQueued()) &&
               (phase != wss::WssStage::UPGRADE || peer.upgradeReceived());
    };

    drive(peer, *client, pending, "at-requested-cancel-stage");
    client->requestStop();
    require(client->stage() == wss::WssStage::STOPPING && client->diagnostic() && client->diagnostic()->stage == phase,
            "stop-diagnostic-before-retirement");
    drive(
        peer, *client,
        [&] {
            return client->stage() == wss::WssStage::FAILED;
        },
        "stop-retired");
    (void)failure(*client, phase, wss::WssError::CANCELLED);
}

/** Verify a stalled upgrade reaches its own deadline after a request reached the peer.
 * @param fixtures Local TLS fixture folder.
 * @param ca Client trust.
 */
void timeOutUpgrade(const std::filesystem::path &fixtures, std::string_view ca) {
    Peer peer{fixtures, PeerMode::HOLD_UPGRADE};
    auto settings = options(peer, ca);
    settings.upgradeTimeout = 250ms;
    auto client = open(settings);

    drive(
        peer, *client,
        [&] {
            return client->stage() == wss::WssStage::FAILED;
        },
        "upgrade-timeout-retired");
    require(peer.upgradeReceived(), "upgrade-timeout-after-request");
    (void)failure(*client, wss::WssStage::UPGRADE, wss::WssError::TIMEOUT);
}

/** Destroy the client while the upgrade's native read still borrows its backing.
 * @param fixtures Local TLS fixture folder.
 * @param ca Client trust.
 */
void destroyPendingUpgrade(const std::filesystem::path &fixtures, std::string_view ca) {
    Peer peer{fixtures, PeerMode::HOLD_UPGRADE};
    auto client = open(options(peer, ca));
    drive(
        peer, *client,
        [&] {
            return client->stage() == wss::WssStage::UPGRADE && peer.upgradeReceived();
        },
        "destructor-has-admitted-upgrade");
    client.reset();
}

/** Check configuration and trust gates before starting an asynchronous operation.
 * @param fixtures Local TLS fixture folder.
 * @param ca Ordinary admitted anchor.
 */
void rejectConfigurations(const std::filesystem::path &fixtures, const std::string &ca) {
    Peer peer{fixtures, PeerMode::VALID};
    const auto check = [&](const wss::WssSettings &settings, wss::WssError reason) {
        const auto result = wss::WssSession::begin(settings);
        require(!result && result.error().stage == wss::WssStage::CONFIGURATION && result.error().reason == reason,
                "configuration-refusal");
    };

    auto settings = options(peer, ca);
    settings.identity = "*.localhost";
    check(settings, wss::WssError::INVALID_CONFIGURATION);
    settings = options(peer, ca);
    settings.target = "/v2\r\nInjected: x";
    check(settings, wss::WssError::INVALID_CONFIGURATION);
    settings = options(peer, ca);
    settings.maxMessageBytes = 0;
    check(settings, wss::WssError::INVALID_CONFIGURATION);
    settings = options(peer, ca);
    settings.resolveTimeout = 0ms;
    check(settings, wss::WssError::INVALID_CONFIGURATION);

    const auto trailing = ca + "unframed";
    auto excessAnchors = ca;

    for (std::size_t index = 0; index < 256; ++index) {
        excessAnchors += ca;
    }

    const std::string excessBytes(512 * 1024 + 1, ' ');
    const auto disallowedKey = readFixture(fixtures / "ca-rsa5120.pem");
    const auto notCa = readFixture(fixtures / "server.pem");

    for (const auto *trust : {&trailing, &std::as_const(excessAnchors), &excessBytes, &disallowedKey, &notCa}) {
        settings = options(peer, *trust);
        check(settings, wss::WssError::TRUST);
    }
}

} // namespace

/** Run the explicit trust, upgrade, route, cancellation and timeout scenario matrix.
 * @param argc Number of arguments, including the program name.
 * @param argv Expected explicit TLS fixture folder as the sole argument.
 * @return Zero on a complete pass, or one with the static diagnostic.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 2, "fixture-argument");
        require(OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) == 1, "fixture-openSSL-init");
        const std::filesystem::path fixtures{argv[1]};
        const auto ca = readFixture(fixtures / "ca.pem");
        const auto otherCa = readFixture(fixtures / "other-ca.pem");
        const auto mixedCa = ca + readFixture(fixtures / "ca-p256.pem") + readFixture(fixtures / "ca-p384.pem");

        rejectConfigurations(fixtures, ca);
        ready(fixtures, ca, TLS1_2_VERSION, "localhost");
        ready(fixtures, mixedCa, TLS1_3_VERSION, "127.0.0.1");
        rejectTls(fixtures, ca, "wrong-common-name.invalid", "server.pem", X509_V_ERR_HOSTNAME_MISMATCH);
        rejectTls(fixtures, ca, "localhost", "server-cn-only.pem", X509_V_ERR_HOSTNAME_MISMATCH);
        rejectTls(fixtures, ca, "localhost", "server-partial-wildcard.pem", X509_V_ERR_HOSTNAME_MISMATCH);
        rejectTls(fixtures, otherCa, "localhost", "server.pem", X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
        rejectTls(fixtures, ca, "localhost", "expired.pem", X509_V_ERR_CERT_HAS_EXPIRED);
        rejectUpgrade(fixtures, ca, PeerMode::INVALID, beast::websocket::error::bad_sec_accept);
        rejectUpgrade(fixtures, ca, PeerMode::REJECTED, beast::websocket::error::upgrade_declined, 403);
        rejectUpgrade(fixtures, ca, PeerMode::LARGE_HEADER, http::error::header_limit);
        rejectUpgrade(fixtures, ca, PeerMode::LARGE_BODY, http::error::body_limit);
        cancelIn(fixtures, ca, wss::WssStage::RESOLVE, PeerMode::VALID);
        cancelIn(fixtures, ca, wss::WssStage::CONNECT, PeerMode::VALID);
        cancelIn(fixtures, ca, wss::WssStage::TLS, PeerMode::HOLD_TLS);
        cancelIn(fixtures, ca, wss::WssStage::UPGRADE, PeerMode::HOLD_UPGRADE);
        timeOutUpgrade(fixtures, ca);
        destroyPendingUpgrade(fixtures, ca);
        std::cout
            << "Verified WSS: bounded trust, TLS 1.2/1.3, fragmented upgrade, HTTP limits, four cutoffs and deadline\n";

        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';

        return 1;
    }
}
