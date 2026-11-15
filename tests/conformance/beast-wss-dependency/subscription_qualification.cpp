// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "subscription.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace kraken = org::ttldtor::qdcxx::kraken;
namespace ssl = asio::ssl;
namespace ws = beast::websocket;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using Clock = std::chrono::steady_clock;
using Phase = kraken::SubscriptionPhase;
using Reason = kraken::SubscriptionError;
using namespace std::chrono_literals;

constexpr auto DRIVER_LIMIT = 5s;
constexpr auto SLICE = 2ms;
constexpr auto DELIVERY_OPPORTUNITY = 35ms;
constexpr auto SUBSCRIBE_PREFIX =
    R"({"method":"subscribe","params":{"channel":"ticker","symbol":["BTC/USD"],"event_trigger":"bbo","snapshot":true},"req_id":)";

/** Report the exact scenario boundary on failure.
 * @param valid Required observation.
 * @param label Static description of the failing assertion.
 */
void require(bool valid, const char *label) {
    if (!valid) {
        throw std::runtime_error(label);
    }
}

/** One numeric loopback listener supporting a sequence of independently accepted verified sessions. */
class Peer final {
    using Socket = ws::stream<ssl::stream<beast::tcp_stream>, false>;

    asio::io_context io_;
    Tcp::acceptor acceptor_{io_, Tcp::endpoint{asio::ip::address_v4::loopback(), 0}};
    ssl::context tls_{ssl::context::tls_server};
    std::optional<Socket> socket_;
    beast::flat_buffer request_{1024};
    std::string sending_;
    std::optional<Error> received_;
    std::optional<Error> sent_;

    /** @return Native TCP connection backing the current peer attempt. */
    Tcp::socket &tcp() {
        return beast::get_lowest_layer(*socket_).socket();
    }

    public:
    /** Prepare explicit synthetic TLS credentials without opening a provider connection.
     * @param certificates Directory containing the existing loopback fixtures.
     */
    explicit Peer(const std::filesystem::path &certificates) {
        tls_.use_certificate_chain_file((certificates / "server.pem").string());
        tls_.use_private_key_file((certificates / "server.key").string(), ssl::context::pem);
    }

    /** Close our listener and connection before retiring every fixture callback. */
    ~Peer() {
        Error ignored;
        acceptor_.close(ignored);

        if (socket_) {
            tcp().close(ignored);
        }

        io_.restart();
        (void)io_.run();
    }

    /** @return Local numeric port allocated by the fixture. */
    std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    /** Arm a verified accept, upgrade, and one bounded read of the subscription request. */
    void listen() {
        socket_.emplace(io_, tls_);
        request_.consume(request_.size());
        received_.reset();
        acceptor_.async_accept(tcp(), [this](Error error) {
            if (error) {
                received_ = error;

                return;
            }

            socket_->next_layer().async_handshake(ssl::stream_base::server, [this](Error error) {
                if (error) {
                    received_ = error;

                    return;
                }

                socket_->async_accept([this](Error error) {
                    if (error) {
                        received_ = error;

                        return;
                    }

                    socket_->async_read(request_, [this](Error error, std::size_t) {
                        received_ = error;
                    });
                });
            });
        });
    }

    /** Run ready callbacks while leaving time control to the test driver. */
    void poll() {
        io_.restart();
        (void)io_.poll();
    }

    /** @return True once the peer completed its one request read. */
    bool hasRequest() const {
        return received_.has_value();
    }

    /** @return Borrowed complete request with framing removed. */
    std::string_view request() const {
        require(received_ && !*received_, "peer request read");
        const auto bytes = request_.data();

        return {static_cast<const char *>(bytes.data()), bytes.size()};
    }

    /** Send owned text using the actual Beast/WebSocket/TLS path.
     * @param text One complete ticker message that the fixture retains to write completion.
     */
    void send(std::string text) {
        sending_ = std::move(text);
        sent_.reset();
        socket_->text(true);
        socket_->async_write(asio::buffer(sending_), [this](Error error, std::size_t) {
            sent_ = error;
        });
    }

    /** @return True once the fixture completed its write successfully. */
    bool sent() const {
        if (sent_) {
            require(!*sent_, "peer data write");
        }

        return sent_.has_value();
    }

    /** End the connection after its complete writes, leaving the listener available for reconnect. */
    void disconnect() {
        tcp().close();
        socket_.reset();
    }
};

/** Cooperatively drive the independently owned client and peer to a bounded fixture predicate.
 * @tparam Check Repeated predicate, which may retain an admitted observation.
 * @param source Source under qualification.
 * @param peer Local provider.
 * @param check Success predicate.
 * @param label Error recorded if the test deadline is reached.
 */
template <class Check> void until(kraken::KrakenSubscription &source, Peer &peer, Check check, const char *label) {
    const auto deadline = Clock::now() + DRIVER_LIMIT;

    while (!check() && Clock::now() < deadline) {
        peer.poll();
        source.runOneFor(SLICE);
    }

    require(check(), label);
}

/** Allow scheduled transport reads without taking ownership of any admitted result.
 * @param source Source under qualification.
 * @param peer Local provider.
 */
void pump(kraken::KrakenSubscription &source, Peer &peer) {
    const auto deadline = Clock::now() + DELIVERY_OPPORTUNITY;

    while (Clock::now() < deadline) {
        peer.poll();
        source.runOneFor(SLICE);
    }
}

/** @param peer Local provider route.
 * @param trust Explicit trusted synthetic CA.
 * @return Numeric-route, certificate-verified configuration with short retries.
 */
kraken::SubscriptionSettings settings(const Peer &peer, const std::string &trust) {
    kraken::SubscriptionSettings value;
    value.routeHost = "127.0.0.1";
    value.identity = "localhost";
    value.trustPem = trust;
    value.port = peer.port();
    value.stageTimeout = 2s;
    value.snapshotTimeout = 1s;
    value.quoteFreshness = 2s;
    value.initialBackoff = 40ms;
    value.maximumBackoff = 60ms;

    return value;
}

/** Check that the real client wrote the fixed public subscription with a fresh request ID.
 * @param source Source under qualification.
 * @param peer Awaiting local provider.
 * @param id Expected monotonically increasing attempt correlation.
 */
void waitForRequest(kraken::KrakenSubscription &source, Peer &peer, unsigned id) {
    until(
        source, peer,
        [&] {
            return peer.hasRequest();
        },
        "subscribe received");
    require(peer.request() == SUBSCRIBE_PREFIX + std::to_string(id) + '}', "exact fixed ticker subscription");
}

/** @param id Server echo of the client request ID.
 * @return Complete successful response in the current public ticker dialect.
 */
std::string ack(unsigned id) {
    return R"({"method":"subscribe","success":true,"result":{"channel":"ticker","symbol":"BTC/USD"},"req_id":)" +
           std::to_string(id) + '}';
}

/** @param kind Snapshot or update.
 * @param timestamp Distinct synthetic provider timestamp identifying the delivered row.
 * @return Complete exact-decimal BTC/USD quote message.
 */
std::string quote(std::string_view kind, std::string_view timestamp) {
    return R"({"channel":"ticker","type":")" + std::string{kind} +
           R"(","data":[{"symbol":"BTC/USD","bid":50000.25,"ask":50001.75,"bid_qty":1.25,"ask_qty":2.5,"timestamp":")" +
           std::string{timestamp} + R"("}]})";
}

/** Finish the peer-side write while driving client-side operations.
 * @param source Source under qualification.
 * @param peer Local provider.
 * @param message Complete application message.
 */
void send(kraken::KrakenSubscription &source, Peer &peer, std::string message) {
    peer.send(std::move(message));
    until(
        source, peer,
        [&] {
            return peer.sent();
        },
        "provider write completed");
}

/** @param source Source under qualification.
 * @param peer Local provider.
 * @param target Required application phase.
 */
void waitForPhase(kraken::KrakenSubscription &source, Peer &peer, Phase target) {
    until(
        source, peer,
        [&] {
            return source.phase() == target;
        },
        "source phase reached");
}

/** @param source Driven source from which to take one observation.
 * @param peer Local provider.
 * @param generation Expected active attempt.
 * @param stamp Exact expected timestamp.
 * @param initial Whether the admitted payload is the initial snapshot.
 */
void take(kraken::KrakenSubscription &source, Peer &peer, std::uint64_t generation, std::string_view stamp,
          bool initial) {
    std::optional<kraken::SourceObservation> row;
    until(
        source, peer,
        [&] {
            if (!row) {
                row = source.takeObservation();
            }

            return row.has_value();
        },
        "quote delivered");
    require(
        row->generation == generation && row->initialSnapshot == initial && row->observation.timestamp == stamp &&
            row->observation.quote.symbol == u"BTC/USD" && row->observation.quote.bidPrice.toString() == "50000.25" &&
            row->observation.quote.askPrice.toString() == "50001.75" &&
            row->observation.quote.bidSize.toString() == "1.25" && row->observation.quote.askSize.toString() == "2.5",
        "current-generation exact observation");
}

/** Prove both initial event orders, flush stale data on reconnect and cancel a pending receive.
 * @param certificates Existing verified loopback TLS fixtures.
 * @param trust Explicit trusted synthetic CA.
 */
void recover(const std::filesystem::path &certificates, const std::string &trust) {
    Peer peer{certificates};
    auto config = settings(peer, trust);
    config.consecutiveFailureLimit = 2;
    kraken::KrakenSubscription source{config};
    peer.listen();
    waitForRequest(source, peer, 1);
    send(source, peer, ack(1));
    send(source, peer, quote("update", "premature"));
    pump(source, peer);
    require(source.phase() == Phase::AWAITING_SNAPSHOT && !source.takeObservation(), "ACK alone offers no data");
    send(source, peer, quote("snapshot", "first"));
    waitForPhase(source, peer, Phase::ACTIVE);
    take(source, peer, 1, "first", true);
    send(source, peer, quote("update", "new delta"));
    take(source, peer, 1, "new delta", false);
    peer.disconnect();
    waitForPhase(source, peer, Phase::BACKOFF);
    require(source.diagnostic() && source.diagnostic()->generation == 1 && source.diagnostic()->transport &&
                !source.takeObservation(),
            "native failure retains diagnostic and clears current delivery");

    peer.listen();
    waitForRequest(source, peer, 2);
    send(source, peer, quote("snapshot", "replacement"));
    send(source, peer, quote("update", "before ACK"));
    send(source, peer, quote("snapshot", "duplicate"));
    pump(source, peer);
    require(source.phase() == Phase::AWAITING_SNAPSHOT && !source.takeObservation(), "snapshot alone offers no data");
    send(source, peer, ack(2));
    waitForPhase(source, peer, Phase::ACTIVE);
    take(source, peer, 2, "replacement", true);
    require(!source.diagnostic() && !source.takeObservation(), "retry starts with exactly one clean snapshot");
    source.runOneFor(0ms);
    source.requestStop();
    require(source.phase() == Phase::STOPPING && !source.takeObservation(), "stop cuts off pending read");
    waitForPhase(source, peer, Phase::STOPPED);
}

/** Prove correlated refusal, bounded retries and timeout despite valid control traffic.
 * @param certificates Existing verified loopback TLS fixtures.
 * @param trust Explicit trusted synthetic CA.
 */
void failures(const std::filesystem::path &certificates, const std::string &trust) {
    Peer peer{certificates};
    auto config = settings(peer, trust);
    config.snapshotTimeout = 250ms;
    config.consecutiveFailureLimit = 3;
    kraken::KrakenSubscription source{config};
    peer.listen();
    waitForRequest(source, peer, 1);
    send(source, peer, quote("snapshot", "must not escape"));
    send(source, peer, ack(77));
    waitForPhase(source, peer, Phase::BACKOFF);
    require(source.diagnostic() && source.diagnostic()->reason == Reason::REQUEST_ID_MISMATCH &&
                !source.takeObservation(),
            "wrong request correlation drops buffered snapshot");
    auto early = Clock::now();
    source.runOneFor(0ms);
    require(source.phase() == Phase::BACKOFF, "initial retry delay");
    waitForPhase(source, peer, Phase::CONNECTING);
    require(Clock::now() - early >= 20ms, "first retry not immediate");

    peer.disconnect();
    peer.listen();
    waitForRequest(source, peer, 2);
    send(source, peer, R"({"method":"subscribe","success":false,"error":"fixture refusal","req_id":2})");
    waitForPhase(source, peer, Phase::BACKOFF);
    require(source.diagnostic() && source.diagnostic()->reason == Reason::REQUEST_REJECTED && !source.takeObservation(),
            "correlated rejection ends attempt");
    early = Clock::now();
    waitForPhase(source, peer, Phase::CONNECTING);
    require(Clock::now() - early >= 40ms, "second retry observes increased capped delay");

    peer.disconnect();
    peer.listen();
    waitForRequest(source, peer, 3);
    send(source, peer, ack(3));
    send(source, peer, R"({"channel":"status","data":[{"system":"online"}]})");
    send(source, peer, R"({"channel":"heartbeat"})");
    waitForPhase(source, peer, Phase::FAILED);
    require(source.diagnostic() && source.diagnostic()->generation == 3 &&
                source.diagnostic()->reason == Reason::SNAPSHOT_TIMEOUT && !source.takeObservation(),
            "snapshot timeout keeps its reason when retry budget is exhausted");
}

/** Prove that control traffic does not refresh a successfully admitted quote.
 * @param certificates Existing verified loopback TLS fixtures.
 * @param trust Explicit trusted synthetic CA.
 */
void freshness(const std::filesystem::path &certificates, const std::string &trust) {
    Peer peer{certificates};
    auto config = settings(peer, trust);
    config.quoteFreshness = 250ms;
    kraken::KrakenSubscription source{config};
    peer.listen();
    waitForRequest(source, peer, 1);
    send(source, peer, ack(1));
    send(source, peer, quote("snapshot", "freshness"));
    waitForPhase(source, peer, Phase::ACTIVE);
    take(source, peer, 1, "freshness", true);
    send(source, peer, R"({"channel":"heartbeat"})");
    send(source, peer, R"({"channel":"status","data":[{"system":"online"}]})");
    waitForPhase(source, peer, Phase::BACKOFF);
    require(source.diagnostic() && source.diagnostic()->reason == Reason::QUOTE_STALE && !source.takeObservation(),
            "data freshness expires after control traffic");
    source.requestStop();
    source.runOneFor(0ms);
    require(source.phase() == Phase::STOPPED, "stop during backoff prevents a new attempt");
}

/** Prove that an admitted but unconsumed snapshot cannot escape its freshness interval.
 * @param certificates Existing verified loopback TLS fixtures.
 * @param trust Explicit trusted synthetic CA.
 */
void unreadSnapshot(const std::filesystem::path &certificates, const std::string &trust) {
    Peer peer{certificates};
    auto config = settings(peer, trust);
    config.quoteFreshness = 200ms;
    kraken::KrakenSubscription source{config};
    peer.listen();
    waitForRequest(source, peer, 1);
    send(source, peer, ack(1));
    send(source, peer, quote("snapshot", "unconsumed"));
    waitForPhase(source, peer, Phase::ACTIVE);
    std::this_thread::sleep_for(config.quoteFreshness);
    require(!source.takeObservation() && source.diagnostic() && source.diagnostic()->reason == Reason::QUOTE_STALE,
            "unconsumed initial row is invalidated at freshness cutoff");
}

} // namespace

/** Run the bounded, credential-free BTC/USD subscription tests.
 * @param argc Argument count, including executable.
 * @param argv One argument selecting the local directory of synthetic TLS certificates.
 * @return Zero after all source contracts hold, otherwise a failing fixture result.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    try {
        const std::filesystem::path certificates = argv[1];
        std::ifstream ca{certificates / "ca.pem", std::ios::binary};
        const std::string trust{std::istreambuf_iterator<char>{ca}, std::istreambuf_iterator<char>{}};
        require(!trust.empty(), "explicit fixture trust");
        recover(certificates, trust);
        failures(certificates, trust);
        freshness(certificates, trust);
        unreadSnapshot(certificates, trust);
        std::cout << "Kraken subscription recovery, correlation and freshness passed\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
