#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/core_names.h>
#include <openssl/provider.h>
#include <openssl/x509v3.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#    include <tlhelp32.h>
#endif

#if !defined(OPENSSL_NO_AUTOLOAD_CONFIG)
#    error "The selected feasibility SDK must be built with no-autoload-config"
#endif

static_assert(ASIO_VERSION == 103802, "The feasibility profile pins standalone Asio 1.38.2");
static_assert(OPENSSL_VERSION_NUMBER == 0x30500090L, "The feasibility profile pins OpenSSL 3.5.9");

namespace {
using Tcp = asio::ip::tcp;
using Error = asio::error_code;
using Clock = std::chrono::steady_clock;
constexpr auto CANCEL_DELAY = std::chrono::milliseconds(100);
constexpr auto CASE_DEADLINE = std::chrono::seconds(3);
constexpr std::size_t WRITE_BYTES = 32 * 1024 * 1024;
constexpr int SOCKET_BUFFER_BYTES = 1024;
constexpr long TLS_MAX_CERTIFICATE_LIST_BYTES = 64 * 1024;
constexpr int TLS_VERIFY_DEPTH = 4;
constexpr std::size_t TLS_RECORD_BUFFER_BYTES = 17 * 1024;
constexpr auto TLS_DISABLED_OPTIONS = SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION |
                                      SSL_OP_NO_TX_CERTIFICATE_COMPRESSION | SSL_OP_NO_RX_CERTIFICATE_COMPRESSION;

/** Count only this probe's operating-system threads for helper lifetime evidence.
 * @return Number of threads in the current process.
 */
std::size_t threadCount() {
#if defined(_WIN32)
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("thread snapshot failed");
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    std::size_t count = 0;

    if (Thread32First(snapshot, &entry)) {
        do {
            count += entry.th32OwnerProcessID == GetCurrentProcessId();
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return count;
#else
    std::size_t count = 0;

    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/task")) {
        static_cast<void>(entry);
        ++count;
    }

    return count;
#endif
}

/** Reject failed proof conditions rather than silently reducing coverage.
 * @param condition Required condition.
 * @param text Diagnostic condition name.
 */
void require(bool condition, const std::string &text) {
    if (!condition) {
        throw std::runtime_error(text);
    }
}

/** Capture a completion's stable numeric/category diagnostic.
 * @param error Completion error.
 * @return Stable category/value plus readable detail.
 */
std::string describe(const Error &error) {
    return error.category().name() + std::string(":") + std::to_string(error.value()) + ":" + error.message();
}

/** Two connected local sockets; no worker thread and no DNS lookup. */
struct Pair {
    /// Caller-owned progress host; destroyed after both sockets.
    asio::io_context io;
    /// Ephemeral loopback listener, closed after constructing the pair.
    Tcp::acceptor acceptor{io, {asio::ip::address_v4::loopback(), 0}};
    /// Test client; may be moved into a TLS stream.
    Tcp::socket client{io};
    /// Connected idle/scripted peer; may be moved into a TLS stream.
    Tcp::socket peer{io};

    /** Establish a local connection before the asynchronous case begins. */
    Pair() {
        client.connect(acceptor.local_endpoint());
        acceptor.accept(peer);
        acceptor.close();
    }
};

/** Close only an owned socket, retaining pending handler storage.
 * @param socket Owned socket to cancel and close.
 */
void closeSocket(Tcp::socket &socket) {
    Error ignored;
    socket.cancel(ignored);
    socket.close(ignored);
}

/** Demonstrate pending read/write cancellation and same-domain self-stop.
 * @param writeCase Whether to fill the send path while the peer never reads.
 */
void tcpCancel(bool writeCase) {
    Pair pair;
    pair.client.set_option(asio::socket_base::send_buffer_size(SOCKET_BUFFER_BYTES));
    pair.peer.set_option(asio::socket_base::receive_buffer_size(SOCKET_BUFFER_BYTES));
    std::vector<unsigned char> bytes(writeCase ? WRITE_BYTES : 1, 0x41);
    asio::steady_timer timer(pair.io, CANCEL_DELAY);
    asio::steady_timer deadline(pair.io, CASE_DEADLINE);
    const auto owner = std::this_thread::get_id();
    const auto start = Clock::now();
    int completions = 0;
    int timerCallbacks = 0;
    bool pendingAtCancel = false;
    bool deadlineHit = false;
    Error result;
    std::size_t transferred = 0;
    auto completion = [&](Error error, std::size_t count) {
        require(std::this_thread::get_id() == owner, "TCP completion owner affinity");
        ++completions;
        result = error;
        transferred = count;
        // This is a stop request from a completion: it never joins/destroys its host.
        closeSocket(pair.client);
        closeSocket(pair.peer);
        timer.cancel();
        deadline.cancel();
    };

    if (writeCase) {
        asio::async_write(pair.client, asio::buffer(bytes), completion);
    } else {
        pair.client.async_read_some(asio::buffer(bytes), completion);
    }

    timer.async_wait([&](Error error) {
        ++timerCallbacks;

        if (!error) {
            pendingAtCancel = completions == 0;
            closeSocket(pair.client);
        }
    });
    deadline.async_wait([&](Error error) {
        ++timerCallbacks;

        if (!error) {
            deadlineHit = true;
            closeSocket(pair.client);
            closeSocket(pair.peer);
        }
    });
    pair.io.run();
    require(!deadlineHit && pendingAtCancel && completions == 1 && timerCallbacks == 2 &&
                result == asio::error::operation_aborted,
            "pending TCP cancellation/quiescence");
    pair.io.restart();
    require(pair.io.poll() == 0, "no retained TCP delivery after run drain");
    std::cout << (writeCase ? "tcp-write" : "tcp-read") << " pending=1 peer-idle=1 callbacks=" << completions
              << " timer-callbacks=" << timerCallbacks << " result=" << describe(result) << " bytes=" << transferred
              << " threads=" << threadCount()
              << " elapsed-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count()
              << '\n';
}

/** Exercise a connect/cancel race without claiming the connection was pending. */
void connectRace() {
    asio::io_context io;
    Tcp::acceptor acceptor(io, {asio::ip::address_v4::loopback(), 0});
    Tcp::socket socket(io);
    int completions = 0;
    Error result;
    socket.async_connect(acceptor.local_endpoint(), [&](Error error) {
        ++completions;
        result = error;
    });
    closeSocket(socket);
    io.run();
    require(completions == 1, "connect completion once");
    std::cout << "tcp-connect-race callbacks=" << completions << " result=" << describe(result) << " pending-proof=0\n";
}

/** Transfer a real payload with asynchronous read/write on the caller's owner thread. */
void tcpTransfer() {
    Pair pair;
    const std::array<unsigned char, 4> sent{0x51, 0x44, 0x00, 0xff};
    std::array<unsigned char, 4> received{};
    int callbacks = 0;
    asio::steady_timer deadline(pair.io, CASE_DEADLINE);
    bool timedOut = false;
    auto complete = [&](Error error, std::size_t count) {
        require(!error && count == sent.size(), "TCP payload transfer");
        ++callbacks;

        if (callbacks == 2) {
            // A completion may request its own run's stop without joining/destroying the host.
            closeSocket(pair.client);
            closeSocket(pair.peer);
            deadline.cancel();
        }
    };
    deadline.async_wait([&](Error error) {
        if (!error) {
            timedOut = true;
            closeSocket(pair.client);
            closeSocket(pair.peer);
        }
    });
    asio::async_read(pair.peer, asio::buffer(received), complete);
    asio::async_write(pair.client, asio::buffer(sent), complete);
    pair.io.run();
    require(!timedOut && callbacks == 2 && received == sent, "TCP transfer/self-stop drain");
    std::cout << "tcp-transfer callbacks=2 bytes=4 self-stop-from-handler=1 quiescent=1\n";
}

/** Saturate only a local bounded listen queue and cancel every unfinished attempt. */
void connectPending() {
    constexpr int ATTEMPTS = 512;
    asio::io_context io;
    Tcp::acceptor acceptor(io, {asio::ip::address_v4::loopback(), 0}, false);
    acceptor.listen(1);
    std::vector<std::unique_ptr<Tcp::socket>> sockets;
    std::array<bool, ATTEMPTS> done{};
    int callbacks = 0;
    int aborted = 0;
    int succeeded = 0;
    int refused = 0;
    int otherErrors = 0;
    Error lastOther;
    int pending = 0;
    asio::steady_timer cancelTimer(io, CANCEL_DELAY);

    for (int index = 0; index < ATTEMPTS; ++index) {
        sockets.push_back(std::make_unique<Tcp::socket>(io));
        sockets.back()->async_connect(acceptor.local_endpoint(), [&, index](Error error) {
            done[index] = true;
            ++callbacks;
            aborted += error == asio::error::operation_aborted;
            succeeded += !error;
            refused += error == asio::error::connection_refused;

            if (error && error != asio::error::operation_aborted && error != asio::error::connection_refused) {
                ++otherErrors;
                lastOther = error;
            }
        });
    }

    cancelTimer.async_wait([&](Error error) {
        require(!error, "connect cancellation timer");

        for (int index = 0; index < ATTEMPTS; ++index) {
            pending += !done[index];
            closeSocket(*sockets[index]);
        }

        acceptor.close();
    });
    io.run();
    require(callbacks == ATTEMPTS, "saturated connect cancellation drain");
    require(succeeded > 0 && pending > 0 && aborted > 0, "at least one genuinely pending connect was cancelled");
    std::cout << "tcp-connect-saturated attempts=" << ATTEMPTS << " callbacks=" << callbacks
              << " pending-at-cancel=" << pending << " aborted=" << aborted << " pending-proof=" << (aborted > 0)
              << " succeeded=" << succeeded << " refused=" << refused << " other=" << otherErrors
              << " last-other=" << describe(lastOther) << '\n';
}

/** Own the algorithm context/provider longer than all SSL contexts and operations. */
class Crypto {
    std::unique_ptr<OSSL_LIB_CTX, decltype(&OSSL_LIB_CTX_free)> libctx_{nullptr, OSSL_LIB_CTX_free};
    std::unique_ptr<OSSL_PROVIDER, decltype(&OSSL_PROVIDER_unload)> provider_{nullptr, OSSL_PROVIDER_unload};

    public:
    /** Create an isolated algorithm context with the built-in default provider. */
    Crypto() {
        libctx_.reset(OSSL_LIB_CTX_new());
        require(libctx_ != nullptr, "OSSL_LIB_CTX_new");
        provider_.reset(OSSL_PROVIDER_load(libctx_.get(), "default"));
        require(provider_ != nullptr, "explicit default provider load");
        std::cout << "provider=" << OSSL_PROVIDER_get0_name(provider_.get()) << '\n';
        char *version = nullptr;
        OSSL_PARAM parameters[] = {OSSL_PARAM_construct_utf8_ptr(OSSL_PROV_PARAM_VERSION, &version, 0),
                                   OSSL_PARAM_construct_end()};
        require(OSSL_PROVIDER_get_params(provider_.get(), parameters) == 1 && version != nullptr, "provider version");
        std::cout << "provider-version=" << version << '\n';
    }

    Crypto(const Crypto &) = delete;
    Crypto &operator=(const Crypto &) = delete;

    /** Release provider after all SSL objects using its library context are gone. */
    ~Crypto() = default;

    /** Transfer an SSL_CTX to Asio, which owns and frees the native handle.
     * @param server Whether to create a server method.
     * @return Owning Asio SSL context.
     */
    std::unique_ptr<asio::ssl::context> makeContext(bool server) {
        SSL_CTX *native = SSL_CTX_new_ex(libctx_.get(), nullptr, server ? TLS_server_method() : TLS_client_method());
        require(native != nullptr, "SSL_CTX_new_ex");
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> guard(native, SSL_CTX_free);
        require(SSL_CTX_set_min_proto_version(native, TLS1_2_VERSION) == 1, "TLS minimum");
        require(SSL_CTX_set_max_proto_version(native, TLS1_3_VERSION) == 1, "TLS maximum");
        SSL_CTX_set_max_cert_list(native, TLS_MAX_CERTIFICATE_LIST_BYTES);
        SSL_CTX_set_verify_depth(native, TLS_VERIFY_DEPTH);
        SSL_CTX_set_options(native, TLS_DISABLED_OPTIONS);
        require((SSL_CTX_get_options(native) & TLS_DISABLED_OPTIONS) == TLS_DISABLED_OPTIONS, "disabled TLS options");
        require(SSL_CTX_get_max_cert_list(native) == TLS_MAX_CERTIFICATE_LIST_BYTES, "certificate-list byte limit");
        require(SSL_CTX_get_verify_depth(native) == TLS_VERIFY_DEPTH, "certificate chain depth limit");
        auto context = std::make_unique<asio::ssl::context>(native);
        guard.release();

        return context;
    }
};

/** Run an authenticated TLS handshake, rejection, cancellation, or bounded close case.
 * @param crypto Library context owner, retained throughout the case.
 * @param dir Explicit synthetic fixture directory.
 * @param name Case identifier.
 * @param expectedVerify Expected X509 verification result; zero for success/cancellation.
 */
void tlsCase(Crypto &crypto, const std::filesystem::path &dir, const std::string &name, long expectedVerify) {
    Pair pair;
    auto clientContext = crypto.makeContext(false);
    auto serverContext = crypto.makeContext(true);
    const bool stall = name == "handshake-cancel";
    const bool ip = name == "trusted-ip";
    const bool closeStall = name == "close-timeout";
    const auto ca = dir / (name == "untrusted" ? "other-ca.pem" : "ca.pem");
    clientContext->load_verify_file(ca.string());
    clientContext->set_verify_mode(asio::ssl::verify_peer);

    if (name == "trusted-tls12") {
        require(SSL_CTX_set_max_proto_version(clientContext->native_handle(), TLS1_2_VERSION) == 1, "force TLS1.2");
    }

    serverContext->use_certificate_chain_file((dir / (name == "expired" ? "expired.pem" : "server.pem")).string());
    serverContext->use_private_key_file((dir / "server.key").string(), asio::ssl::context::pem);
    asio::ssl::stream<Tcp::socket> client(std::move(pair.client), *clientContext, TLS_RECORD_BUFFER_BYTES,
                                          TLS_RECORD_BUFFER_BYTES);
    asio::ssl::stream<Tcp::socket> server(std::move(pair.peer), *serverContext, TLS_RECORD_BUFFER_BYTES,
                                          TLS_RECORD_BUFFER_BYTES);
    SSL *ssl = client.native_handle();
    X509_VERIFY_PARAM *parameters = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(parameters,
                                    X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);

    if (ip) {
        require(X509_VERIFY_PARAM_set1_ip_asc(parameters, "127.0.0.1") == 1, "IP identity");
    } else {
        const char *host = name == "wrong-host" ? "wrong.invalid" : "localhost";
        require(SSL_set1_host(ssl, host) == 1, "DNS identity");
        require(SSL_set_tlsext_host_name(ssl, host) == 1, "DNS SNI");
    }

    asio::steady_timer deadline(pair.io, CASE_DEADLINE);
    asio::steady_timer cancelTimer(pair.io, CANCEL_DELAY);
    const auto owner = std::this_thread::get_id();
    const auto start = Clock::now();
    int clientCallbacks = 0;
    int serverCallbacks = 0;
    int shutdownCallbacks = 0;
    bool pendingAtCancel = false;
    bool deadlineHit = false;
    Error result;
    Error serverResult;
    Error closeResult;
    std::string protocol;
    auto closeBoth = [&] {
        closeSocket(client.next_layer());
        closeSocket(server.next_layer());
    };
    auto finishHandshake = [&] {
        if (clientCallbacks != 1) {
            return;
        }

        if (!result && serverCallbacks != 1) {
            return;
        }

        if (!result) {
            require(!serverResult, "both TLS peers completed their handshake");
        }

        if (!result && closeStall) {
            // The peer stays open and never starts SSL_shutdown; the caller deadline aborts it.
            client.async_shutdown([&](Error shutdownError) {
                ++shutdownCallbacks;
                closeResult = shutdownError;
                closeBoth();
                deadline.cancel();
            });
            cancelTimer.async_wait([&](Error timerError) {
                if (!timerError) {
                    pendingAtCancel = shutdownCallbacks == 0;
                    closeBoth();
                }
            });
        } else {
            closeBoth();
            deadline.cancel();
            cancelTimer.cancel();
        }
    };
    deadline.async_wait([&](Error error) {
        if (!error) {
            deadlineHit = true;
            closeBoth();
            cancelTimer.cancel();
        }
    });

    if (stall) {
        // A raw accepted TCP peer neither reads nor writes TLS records.
        cancelTimer.async_wait([&](Error error) {
            if (!error) {
                pendingAtCancel = clientCallbacks == 0;
                closeBoth();
            }
        });
    } else {
        server.async_handshake(asio::ssl::stream_base::server, [&](Error error) {
            require(std::this_thread::get_id() == owner, "TLS server owner affinity");
            ++serverCallbacks;
            serverResult = error;
            finishHandshake();
        });
    }

    client.async_handshake(asio::ssl::stream_base::client, [&](Error error) {
        require(std::this_thread::get_id() == owner, "TLS client owner affinity");
        ++clientCallbacks;
        result = error;
        protocol = error ? "none" : SSL_get_version(ssl);
        finishHandshake();
    });
    pair.io.run();
    const long verify = SSL_get_verify_result(ssl);
    require(!deadlineHit && clientCallbacks == 1 && serverCallbacks == (stall ? 0 : 1), "TLS callback drain");

    if (stall) {
        require(pendingAtCancel && result == asio::error::operation_aborted, "pending handshake cancel");
    } else if (expectedVerify != X509_V_OK) {
        require(result && verify == expectedVerify, "certificate failure category");
    } else {
        require(!result && verify == X509_V_OK, "trusted TLS handshake");
    }

    if (closeStall) {
        require(pendingAtCancel && shutdownCallbacks == 1 && closeResult == asio::error::operation_aborted,
                "bounded pending TLS close cancellation");
    }

    pair.io.restart();
    require(pair.io.poll() == 0, "no delivery after TLS quiescence");
    const char *serverName = SSL_get_servername(server.native_handle(), TLSEXT_NAMETYPE_host_name);

    if (!stall && !result) {
        require(ip ? serverName == nullptr : serverName != nullptr && std::string(serverName) == "localhost",
                "DNS SNI present and IP SNI absent");
    }

    std::cout << "tls-" << name << " client-callbacks=" << clientCallbacks << " server-callbacks=" << serverCallbacks
              << " shutdown-callbacks=" << shutdownCallbacks << " pending-at-cancel=" << pendingAtCancel
              << " verify=" << verify << " result=" << describe(result) << " server=" << describe(serverResult)
              << " close=" << describe(closeResult) << " negotiated=" << protocol
              << " server-SNI=" << (serverName ? serverName : "none")
              << " elapsed-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count()
              << '\n';
}

constexpr const char *HELP = R"(qdcxx private TLS feasibility probe
Usage: qdcxx_tls_probe [--help] | --run --fixtures DIRECTORY
No arguments and --help print this text without I/O, sockets, or TLS initialization.
--run requires explicit synthetic PEM fixtures: ca.pem, other-ca.pem, server.pem,
expired.pem, server.key. No DNS, external server, OS trust store, or credentials.
The run uses ephemeral IPv4 loopback sockets, one caller-driven owner thread,
32 MiB maximum test payload, 100 ms cancellation and 3 s per-case deadlines.
TLS uses an explicit CA file, TLS 1.2..1.3 and mandatory SAN/peer verification.
Output: version/backend and per-case completion/error/quiescence evidence.
Exit: 0 success/help, 1 failed proof/I/O, 2 invalid arguments.
)";
} // namespace

/** Dispatch side-effect-free help or the explicitly requested private suite.
 * @param argc Argument count.
 * @param argv Arguments; only --run --fixtures DIRECTORY enables networking.
 * @return Zero on success/help, one on runtime failure, two on invalid arguments.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cout << HELP;
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    if (argc != 4 || std::string(argv[1]) != "--run" || std::string(argv[2]) != "--fixtures") {
        std::cerr << "invalid arguments; use --help\n";

        return 2;
    }

    try {
        const std::filesystem::path dir(argv[3]);
        require(std::filesystem::is_directory(dir), "explicit fixture directory exists");
        std::cout << "Asio=" << ASIO_VERSION << " OpenSSL=" << OpenSSL_version(OPENSSL_VERSION) << '\n';
        std::cout << "OPENSSL_NO_AUTOLOAD_CONFIG=1\n";
        std::cout << "threads-before-host=" << threadCount()
                  << " max-cert-list-bytes=" << TLS_MAX_CERTIFICATE_LIST_BYTES << " verify-depth=" << TLS_VERIFY_DEPTH
                  << '\n';
#if defined(ASIO_HAS_IOCP)
        std::cout << "backend=IOCP\n";
#elif defined(ASIO_HAS_EPOLL)
        std::cout << "backend=epoll\n";
#else
        std::cout << "backend=other\n";
#endif
#if defined(ASIO_HAS_IO_URING)
        std::cout << "io_uring=enabled\n";
#else
        std::cout << "io_uring=disabled\n";
#endif
        connectRace();
        connectPending();
        tcpTransfer();
        tcpCancel(false);
        tcpCancel(true);
        Crypto crypto;
        tlsCase(crypto, dir, "trusted-dns", X509_V_OK);
        tlsCase(crypto, dir, "trusted-ip", X509_V_OK);
        tlsCase(crypto, dir, "trusted-tls12", X509_V_OK);
        tlsCase(crypto, dir, "untrusted", X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
        tlsCase(crypto, dir, "wrong-host", X509_V_ERR_HOSTNAME_MISMATCH);
        tlsCase(crypto, dir, "expired", X509_V_ERR_CERT_HAS_EXPIRED);
        tlsCase(crypto, dir, "handshake-cancel", X509_V_OK);
        tlsCase(crypto, dir, "close-timeout", X509_V_OK);
        std::cout << "threads-after-hosts=" << threadCount() << '\n';
        std::cout << "PASS all operations drained before contexts/provider/host destruction\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';

        return 1;
    }
}
