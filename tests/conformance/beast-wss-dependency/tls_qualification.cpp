// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "allocation_receipt.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using TlsStream = ssl::stream<beast::tcp_stream>;
using WebSocket = beast::websocket::stream<TlsStream>;
using namespace std::chrono_literals;
namespace receipt = org::ttldtor::qdcxx::test::beast_wss;

constexpr std::size_t WIRE_WINDOW = 17 * 1024;
constexpr std::size_t PLAIN_WINDOW = 4 * 1024;
constexpr auto OPERATION_LIMIT = 3s;
constexpr int SMALL_NATIVE_BUFFER = 1024;
constexpr int WRITE_ATTEMPTS = 256;
constexpr auto WRITE_POLL = 5ms;
constexpr char PROPERTY_QUERY[] = "provider=default";

/** Stop the qualification on a failed invariant.
 * @param ok Condition that must hold.
 * @param stage Static stage name, without fixture or credential content.
 */
void require(bool ok, const char *stage) {
    if (!ok) {
        throw std::runtime_error(stage);
    }
}

/** Track one asynchronous completion without borrowing fixture internals. */
struct Completion {
    std::optional<Error> error; ///< Set once the native/composed completion is delivered.
    std::size_t bytes = 0;      ///< Byte count reported by a data operation.
};

/** Private OpenSSL context, provider and owning Asio adapter.
 * The Asio context is destroyed before its provider and library context.
 */
class TlsContext final {
    std::unique_ptr<OSSL_LIB_CTX, decltype(&OSSL_LIB_CTX_free)> library_{OSSL_LIB_CTX_new(), OSSL_LIB_CTX_free};
    std::unique_ptr<OSSL_PROVIDER, decltype(&OSSL_PROVIDER_unload)> provider_{nullptr, OSSL_PROVIDER_unload};
    std::optional<ssl::context> context_;

    public:
    /** Prepare a private test TLS context for one explicit protocol version.
     * @param role Local role. The fixture uses one client and one synthetic server.
     * @param version TLS1_2_VERSION or TLS1_3_VERSION.
     */
    TlsContext(ssl::stream_base::handshake_type role, int version) {
        require(library_ != nullptr, "private-libctx");
        provider_.reset(OSSL_PROVIDER_load(library_.get(), "default"));
        require(provider_ != nullptr, "explicit-default-provider");
        require(RAND_set_DRBG_type(library_.get(), "CTR-DRBG", PROPERTY_QUERY, "AES-256-CTR", nullptr) == 1 &&
                    RAND_set_seed_source_type(library_.get(), "SEED-SRC", PROPERTY_QUERY) == 1,
                "explicit-rng");

        const auto *method = role == ssl::stream_base::client ? TLS_client_method() : TLS_server_method();
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> native{SSL_CTX_new_ex(library_.get(), PROPERTY_QUERY, method),
                                                                 SSL_CTX_free};
        require(native != nullptr, "native-context");

        constexpr auto OPTIONS = SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_TICKET |
                                 SSL_OP_NO_TX_CERTIFICATE_COMPRESSION | SSL_OP_NO_RX_CERTIFICATE_COMPRESSION;
        require(SSL_CTX_set_min_proto_version(native.get(), version) == 1 &&
                    SSL_CTX_set_max_proto_version(native.get(), version) == 1 &&
                    SSL_CTX_set_cipher_list(native.get(), "ECDHE-RSA-AES128-GCM-SHA256") == 1 &&
                    SSL_CTX_set_ciphersuites(native.get(), "TLS_AES_128_GCM_SHA256") == 1 &&
                    SSL_CTX_set1_groups_list(native.get(), "X25519:P-256") == 1 &&
                    SSL_CTX_set1_sigalgs_list(native.get(), "rsa_pss_rsae_sha256:rsa_pkcs1_sha256") == 1 &&
                    SSL_CTX_set_max_early_data(native.get(), 0) == 1 &&
                    SSL_CTX_set_recv_max_early_data(native.get(), 0) == 1 &&
                    SSL_CTX_set_num_tickets(native.get(), 0) == 1 &&
                    (SSL_CTX_set_options(native.get(), OPTIONS) & OPTIONS) == OPTIONS,
                "native-context-policy");
        SSL_CTX_set_post_handshake_auth(native.get(), 0);
        SSL_CTX_set_session_cache_mode(native.get(), SSL_SESS_CACHE_OFF);
        require(SSL_CTX_get_verify_callback(native.get()) == nullptr && SSL_CTX_get_app_data(native.get()) == nullptr &&
                    SSL_CTX_get_default_passwd_cb_userdata(native.get()) == nullptr,
                "native-context-app-data");
        context_.emplace(native.get());
        (void)native.release();
    }

    /** Borrow the adopted Asio context for stream construction.
     * @return Context owned by this object.
     */
    ssl::context &asioContext() noexcept {
        return *context_;
    }

    /** Borrow the native context for explicit TLS policy setup.
     * @return Context handle valid for the lifetime of this owner.
     */
    SSL_CTX *nativeContext() noexcept {
        return context_->native_handle();
    }
};

/** Test owner with fixed plaintext backing and a private-context lease. */
struct Session {
    std::shared_ptr<TlsContext> context;          ///< Outlives the WebSocket and its SSL state.
    WebSocket websocket;                          ///< Only the underlying TLS stream is used here.
    std::array<char, PLAIN_WINDOW> readBuffer{};  ///< Fixed receiver backing until completion.
    std::array<char, PLAIN_WINDOW> writeBuffer{}; ///< Fixed sender backing until completion.

    /** Construct a stable stream with fixed encrypted backing.
     * @param executor Serialized probe execution context.
     * @param owner Shared private context, retained until this stream is destroyed.
     */
    Session(asio::io_context &executor, std::shared_ptr<TlsContext> owner)
        : context(std::move(owner)), websocket(executor, context->asioContext(), WIRE_WINDOW, WIRE_WINDOW) {
        auto *native = websocket.next_layer().native_handle();
        require((SSL_get_mode(native) & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER) != 0, "asio-default-write-mode");
        require((SSL_clear_mode(native, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE) &
                 (SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE)) == 0,
                "fixed-write-mode");
        auto *bio = SSL_get_wbio(native);
        require(bio != nullptr && bio == SSL_get_rbio(native) && BIO_get_write_guarantee(bio) == WIRE_WINDOW,
                "bio-output-window");
        require(SSL_get_SSL_CTX(native) == context->nativeContext() && SSL_get_app_data(native) == nullptr,
                "stream-context-binding");
    }
};

/** Drive armed probe I/O for a bounded wall-clock interval.
 * @tparam Done Boolean predicate over the observed completions.
 * @param io Executor used by both local peers.
 * @param done Completion predicate evaluated after the poll.
 * @param stage Static failure diagnostic.
 */
template <typename Done> void finish(asio::io_context &io, Done done, const char *stage) {
    io.restart();
    (void)io.run_for(OPERATION_LIMIT);
    require(done(), stage);
}

/** Close the lowest layer and leave actual completion to the executor.
 * @param session Leased stream with possible pending I/O.
 */
void abort(Session &session) noexcept {
    Error ignored;
    auto &socket = beast::get_lowest_layer(session.websocket).socket();
    socket.cancel(ignored);
    socket.close(ignored);
}

/** Run one isolated loopback case with explicit fixture trust and identity. */
class Fixture final {
    asio::io_context io_;
    Tcp::acceptor acceptor_{io_, Tcp::endpoint{asio::ip::address_v4::loopback(), 0}};

    public:
    std::shared_ptr<Session> client; ///< Public fixture reference for cancellation tests.
    std::shared_ptr<Session> peer;   ///< Local synthetic server on the same executor.

    /** Configure test credentials and create a connected loopback pair.
     * @param files Explicit fixture directory.
     * @param version Exact protocol version for both sides.
     * @param ca Filename of the selected synthetic CA.
     * @param certificate Filename of the synthetic server certificate.
     * @param hostname DNS reference identity, distinct from the loopback route.
     */
    Fixture(const std::filesystem::path &files, int version, const char *ca, const char *certificate,
            const char *hostname) {
        auto clientContext = std::make_shared<TlsContext>(ssl::stream_base::client, version);
        auto peerContext = std::make_shared<TlsContext>(ssl::stream_base::server, version);

        auto *clientNative = clientContext->nativeContext();
        SSL_CTX_set_verify(clientNative, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_verify_depth(clientNative, 4);
        SSL_CTX_set_max_cert_list(clientNative, 64 * 1024);
        require(SSL_CTX_load_verify_locations(clientNative, (files / ca).string().c_str(), nullptr) == 1,
                "explicit-synthetic-ca");

        auto *peerNative = peerContext->nativeContext();
        require(SSL_CTX_use_certificate_chain_file(peerNative, (files / certificate).string().c_str()) == 1 &&
                    SSL_CTX_use_PrivateKey_file(peerNative, (files / "server.key").string().c_str(),
                                                SSL_FILETYPE_PEM) == 1 &&
                    SSL_CTX_check_private_key(peerNative) == 1,
                "synthetic-peer-credentials");

        client = std::make_shared<Session>(io_, std::move(clientContext));
        peer = std::make_shared<Session>(io_, std::move(peerContext));

        auto *identity = client->websocket.next_layer().native_handle();
        auto *parameters = SSL_get0_param(identity);
        constexpr auto FIXTURE_DAY = std::chrono::sys_days{std::chrono::year{2027} / 1 / 1};
        X509_VERIFY_PARAM_set_time(parameters, std::chrono::system_clock::to_time_t(FIXTURE_DAY));
        SSL_set_hostflags(identity, X509_CHECK_FLAG_NEVER_CHECK_SUBJECT | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        require(SSL_set1_host(identity, hostname) == 1 && SSL_set_tlsext_host_name(identity, hostname) == 1,
                "explicit-dns-identity");

        auto accepted = std::make_shared<Completion>();
        auto connected = std::make_shared<Completion>();
        acceptor_.async_accept(beast::get_lowest_layer(peer->websocket).socket(), [held = peer, accepted](Error error) {
            accepted->error = error;
        });
        beast::get_lowest_layer(client->websocket)
            .socket()
            .async_connect(acceptor_.local_endpoint(), [held = client, connected](Error error) {
                connected->error = error;
            });
        finish(
            io_,
            [&] {
                return accepted->error && connected->error;
            },
            "loopback-connect-deadline");
        require(!*accepted->error && !*connected->error, "loopback-connect");
        acceptor_.close();
    }

    /** Run both handshakes and require the exact client outcome.
     * @param expected OpenSSL verification status for this fixture case.
     * @param version Expected verified protocol version in successful cases.
     */
    void handshake(long expected, int version) {
        auto handshake = [](const std::shared_ptr<Session> &session, ssl::stream_base::handshake_type role) {
            auto result = std::make_shared<Completion>();
            session->websocket.next_layer().async_handshake(role, [session, result](Error error) {
                result->error = error;
            });

            return result;
        };

        const auto clientResult = handshake(client, ssl::stream_base::client);
        const auto peerResult = handshake(peer, ssl::stream_base::server);
        finish(
            io_,
            [&] {
                return clientResult->error && peerResult->error;
            },
            "handshake-deadline");

        auto *native = client->websocket.next_layer().native_handle();
        const auto verify = SSL_get_verify_result(native);

        if (verify != expected) {
            std::cerr << "expected_verify=" << expected << " actual_verify=" << verify << '\n';
        }

        require(verify == expected, "verify-result");

        if (expected == X509_V_OK) {
            require(!*clientResult->error && !*peerResult->error && SSL_version(native) == version,
                    "verified-handshake");
            const auto *seenName =
                SSL_get_servername(peer->websocket.next_layer().native_handle(), TLSEXT_NAMETYPE_host_name);
            require(seenName != nullptr && std::string_view{seenName} == "localhost", "explicit-dns-sni");
        } else {
            require(static_cast<bool>(*clientResult->error), "handshake-does-not-open-gate");
        }
    }

    /** Arm one bounded read with backing tied to its operation lease.
     * @param session Leased destination stream.
     * @return Completion receipt.
     */
    static std::shared_ptr<Completion> startRead(const std::shared_ptr<Session> &session) {
        auto result = std::make_shared<Completion>();
        session->websocket.next_layer().async_read_some(asio::buffer(session->readBuffer),
                                                        [session, result](Error error, std::size_t bytes) {
                                                            result->bytes = bytes;
                                                            result->error = error;
                                                        });

        return result;
    }

    /** Arm one bounded write using the same contiguous backing through completion.
     * @param session Leased source stream.
     * @return Completion receipt.
     */
    static std::shared_ptr<Completion> startWrite(const std::shared_ptr<Session> &session) {
        auto result = std::make_shared<Completion>();
        session->websocket.next_layer().async_write_some(asio::buffer(session->writeBuffer),
                                                         [session, result](Error error, std::size_t bytes) {
                                                             result->bytes = bytes;
                                                             result->error = error;
                                                         });

        return result;
    }

    /** Transfer the fixed write window and check the peer's exact received bytes. */
    void exchange() {
        client->writeBuffer.fill('q');
        auto sent = startWrite(client);
        auto received = startRead(peer);
        finish(
            io_,
            [&] {
                return sent->error && received->error;
            },
            "exchange-deadline");
        require(!*sent->error && !*received->error && sent->bytes == PLAIN_WINDOW && received->bytes == PLAIN_WINDOW &&
                    client->writeBuffer == peer->readBuffer,
                "exact-bounded-transfer");
    }

    /** Verify two-sided TLS close-notify completion after quiescing data operations. */
    void shutdown() {
        auto shutdown = [](const std::shared_ptr<Session> &session) {
            auto result = std::make_shared<Completion>();
            session->websocket.next_layer().async_shutdown([session, result](Error error) {
                result->error = error;
            });

            return result;
        };
        const auto clientResult = shutdown(client);
        const auto peerResult = shutdown(peer);
        finish(
            io_,
            [&] {
                return clientResult->error && peerResult->error;
            },
            "shutdown-deadline");
        require(!*clientResult->error && !*peerResult->error, "two-sided-close-notify");
    }

    /** Arm a read with a silent peer, abort it and check actual lease retirement. */
    void cancelRead() {
        const auto pending = startRead(client);
        io_.restart();
        (void)io_.poll();
        cancel(pending);
    }

    /** Stop peer reads and fill the local socket until a write is genuinely pending. */
    void cancelWrite() {
        beast::get_lowest_layer(client->websocket)
            .socket()
            .set_option(asio::socket_base::send_buffer_size{SMALL_NATIVE_BUFFER});
        beast::get_lowest_layer(peer->websocket)
            .socket()
            .set_option(asio::socket_base::receive_buffer_size{SMALL_NATIVE_BUFFER});
        client->writeBuffer.fill('q');

        for (int attempt = 1; attempt <= WRITE_ATTEMPTS; ++attempt) {
            const auto pending = startWrite(client);
            io_.restart();
            (void)io_.run_for(WRITE_POLL);

            if (!pending->error) {
                std::cout << "pending_write_attempt=" << attempt << '\n';
                cancel(pending);

                return;
            }

            require(!*pending->error && pending->bytes == PLAIN_WINDOW, "prior-write-completion");
        }

        throw std::runtime_error("could-not-observe-pending-write");
    }

    private:
    /** Require an outstanding completion and then release the public owner before draining native I/O.
     * @param pending Receipt owned by the active handler and this caller.
     */
    void cancel(const std::shared_ptr<Completion> &pending) {
        require(!pending->error, "operation-still-pending");
        const std::weak_ptr<Session> owner = client;
        const std::weak_ptr<TlsContext> context = client->context;
        abort(*client);
        client.reset();
        require(!owner.expired() && !context.expired() && !pending->error, "cutoff-before-native-completion");
        finish(
            io_,
            [&] {
                return pending->error.has_value();
            },
            "cancel-deadline");
        require(*pending->error == asio::error::operation_aborted && owner.expired() && context.expired(),
                "cancellation-and-owner-retirement");
    }
};

} // namespace

/** Exercise the candidate SSL stream on numeric loopback with committed synthetic inputs.
 * @param argc Argument count.
 * @param argv Borrowed argument vector containing the fixture directory and optional allocator mode.
 * @return Zero on qualification success and nonzero on a failed observation.
 */
int main(int argc, char **argv) {
    const bool measure = argc == 3 && std::string_view{argv[2]} == "--measure-allocator";

    if (argc != 2 && !measure) {
        return 2;
    }

    try {
        if (measure) {
            require(receipt::enableAllocationReceipt(), "allocator-hooks-before-sdk-use");
        }

        const std::filesystem::path fixtures{argv[1]};

        for (const int version : {TLS1_2_VERSION, TLS1_3_VERSION}) {
            Fixture test(fixtures, version, "ca.pem", "server.pem", "localhost");
            test.handshake(X509_V_OK, version);
            test.exchange();
            test.shutdown();
            std::cout << (version == TLS1_2_VERSION ? "TLS1.2" : "TLS1.3") << " handshake transfer shutdown PASS\n";
        }

        {
            Fixture test(fixtures, TLS1_3_VERSION, "other-ca.pem", "server.pem", "localhost");
            test.handshake(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY, TLS1_3_VERSION);
            std::cout << "untrusted issuer refused\n";
        }
        {
            Fixture test(fixtures, TLS1_3_VERSION, "ca.pem", "server.pem", "wrong-common-name.invalid");
            test.handshake(X509_V_ERR_HOSTNAME_MISMATCH, TLS1_3_VERSION);
            std::cout << "wrong hostname and CN fallback refused\n";
        }
        {
            Fixture test(fixtures, TLS1_3_VERSION, "ca.pem", "expired.pem", "localhost");
            test.handshake(X509_V_ERR_CERT_HAS_EXPIRED, TLS1_3_VERSION);
            std::cout << "expired certificate refused\n";
        }
        {
            Fixture test(fixtures, TLS1_3_VERSION, "ca.pem", "server.pem", "localhost");
            test.handshake(X509_V_OK, TLS1_3_VERSION);
            test.cancelRead();
            std::cout << "pending read canceled and owner retired\n";
        }
        {
            Fixture test(fixtures, TLS1_3_VERSION, "ca.pem", "server.pem", "localhost");
            test.handshake(X509_V_OK, TLS1_3_VERSION);
            test.cancelWrite();
            std::cout << "pending write canceled and backing retired\n";
        }

        if (measure) {
            const auto observation = receipt::getAllocationReceipt();
            require(observation.requestedPeak > observation.requestedAtObservation &&
                        observation.successfulAllocations != 0,
                    "allocator-observation");
            std::cout << "openssl_requested_peak=" << observation.requestedPeak
                      << " openssl_requested_at_observation=" << observation.requestedAtObservation
                      << " openssl_allocations=" << observation.successfulAllocations << '\n';
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
