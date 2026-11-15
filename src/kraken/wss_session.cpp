// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "wss_session.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <openssl/core_names.h>
#include <openssl/pem.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <array>
#include <new>
#include <string>
#include <utility>

namespace org::ttldtor::qdcxx::kraken {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using Clock = std::chrono::steady_clock;
using WebSocket = beast::websocket::stream<ssl::stream<beast::tcp_stream>, false>;

constexpr std::size_t WIRE_WINDOW = 17 * 1024;
constexpr std::size_t WRITE_WINDOW = 4 * 1024;
constexpr std::size_t MAX_PEM_BYTES = 512 * 1024;
constexpr std::size_t MAX_ANCHORS = 256;
constexpr int MAX_CHAIN_ENTRIES = 6;
constexpr int MAX_CHAIN_BYTES = 64 * 1024;
constexpr int MAX_CERTIFICATE_BYTES = 16 * 1024;
constexpr auto MAX_STAGE_TIME = std::chrono::minutes{5};
constexpr std::size_t MAX_TARGET_BYTES = 1024;
constexpr std::size_t MAX_MESSAGE_BYTES = 256 * 1024;
constexpr std::size_t MAX_ENDPOINTS = 8;
constexpr char PROPERTY_QUERY[] = "provider=default";
constexpr std::string_view PEM_BEGIN = "-----BEGIN CERTIFICATE-----";
constexpr std::string_view PEM_END = "-----END CERTIFICATE-----";

/** Inspect one ASCII hostname byte.
 * @param byte A candidate label byte.
 * @return True for an ASCII letter or digit.
 */
bool asciiAlnum(char byte) noexcept {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9');
}

/** Validate the configured DNS SAN identity.
 * @param name Borrowed candidate hostname.
 * @return True for one bounded DNS name without a trailing dot or wildcard.
 */
bool dnsIdentity(std::string_view name) noexcept {
    if (name.empty() || name.size() > 253 || name.back() == '.') {
        return false;
    }

    if (std::ranges::all_of(name, [](char byte) {
            return (byte >= '0' && byte <= '9') || byte == '.';
        })) {
        return false;
    }

    while (!name.empty()) {
        const auto dot = name.find('.');
        const auto label = name.substr(0, dot);

        if (label.empty() || label.size() > 63 || !asciiAlnum(label.front()) || !asciiAlnum(label.back()) ||
            !std::ranges::all_of(label, [](char byte) {
                return asciiAlnum(byte) || byte == '-';
            })) {
            return false;
        }

        if (dot == std::string_view::npos) {
            return true;
        }

        name.remove_prefix(dot + 1);
    }

    return false;
}

/** Check settings before copying or creating a native owner.
 * @param settings Borrowed explicit connection inputs.
 * @return True if all finite option limits and syntax checks pass.
 */
bool settingsValid(const WssSettings &settings) noexcept {
    const auto routeByte = [](char byte) {
        return asciiAlnum(byte) || byte == '-' || byte == '.' || byte == ':' || byte == '%';
    };
    const auto targetByte = [](char byte) {
        return byte >= '!' && byte <= '~' && byte != '#';
    };
    const auto deadlineValid = [](std::chrono::milliseconds limit) {
        return limit.count() > 0 && limit <= MAX_STAGE_TIME;
    };

    return !settings.routeHost.empty() && settings.routeHost.size() <= 253 &&
           std::ranges::all_of(settings.routeHost, routeByte) && dnsIdentity(settings.identity) &&
           !settings.target.empty() && settings.target.size() <= MAX_TARGET_BYTES && settings.target.front() == '/' &&
           std::ranges::all_of(settings.target, targetByte) && settings.port != 0 && settings.maxMessageBytes > 0 &&
           settings.maxMessageBytes <= MAX_MESSAGE_BYTES && deadlineValid(settings.resolveTimeout) &&
           deadlineValid(settings.connectTimeout) && deadlineValid(settings.tlsTimeout) &&
           deadlineValid(settings.upgradeTimeout) && deadlineValid(settings.receiveTimeout) &&
           deadlineValid(settings.sendTimeout) && deadlineValid(settings.closeTimeout);
}

/** Scan the complete trust text and bound the number of decodes before entering OpenSSL.
 * @param pem Explicit ordinary CERTIFICATE blocks.
 * @param blocks Caller-owned fixed storage for the accepted block slices.
 * @return Number of admitted slices, or zero for invalid/empty input.
 */
std::size_t scanTrust(std::string_view pem, std::array<std::string_view, MAX_ANCHORS> &blocks) noexcept {
    if (pem.empty() || pem.size() > MAX_PEM_BYTES) {
        return 0;
    }

    std::size_t count = 0;

    while (!pem.empty()) {
        const auto first = pem.find_first_not_of(" \t\r\n");

        if (first == std::string_view::npos) {
            return count;
        }

        pem.remove_prefix(first);

        if (count == blocks.size() || !pem.starts_with(PEM_BEGIN)) {
            return 0;
        }

        const auto data = pem.substr(PEM_BEGIN.size());
        const auto end = data.find(PEM_END);

        if (end == std::string_view::npos || end == 0 || (data.front() != '\n' && !data.starts_with("\r\n")) ||
            data[end - 1] != '\n' || !std::ranges::all_of(data.substr(0, end), [](char byte) {
                return asciiAlnum(byte) || byte == '+' || byte == '/' || byte == '=' || byte == '\n' || byte == '\r';
            })) {
            return 0;
        }

        const auto length = PEM_BEGIN.size() + end + PEM_END.size();
        const auto tail = pem.substr(length);

        if (!tail.empty() && tail.front() != '\n' && !tail.starts_with("\r\n")) {
            return 0;
        }

        const auto lineEnding = tail.starts_with("\r\n") ? 2U : (!tail.empty() ? 1U : 0U);
        blocks[count++] = pem.substr(0, length + lineEnding);
        pem.remove_prefix(length + lineEnding);
    }

    return count;
}

/** Check the supported public-key and serialized certificate limits.
 * @param cert Borrowed decoded certificate.
 * @param library The private provider context used for the public-key check.
 * @return True if the envelope and public key match the profile.
 */
bool guardCertificate(X509 *cert, OSSL_LIB_CTX *library) noexcept {
    if (cert == nullptr) {
        return false;
    }

    const auto encoded = i2d_X509(cert, nullptr);

    if (encoded <= 0 || encoded > MAX_CERTIFICATE_BYTES) {
        return false;
    }

    auto *key = X509_get0_pubkey(cert);

    if (key == nullptr) {
        return false;
    }

    if (EVP_PKEY_is_a(key, "RSA") == 1) {
        const auto bits = EVP_PKEY_get_bits(key);
        BIGNUM *exponent = nullptr;
        const auto read = EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &exponent);
        const std::unique_ptr<BIGNUM, decltype(&BN_free)> owned{exponent, BN_free};

        if (bits < 2048 || bits > 4096 || read != 1 || exponent == nullptr || BN_is_word(exponent, 65537) != 1) {
            return false;
        }
    } else if (EVP_PKEY_is_a(key, "EC") == 1) {
        std::array<char, 24> group{};
        int explicitParameters = 0;

        if (EVP_PKEY_get_group_name(key, group.data(), group.size(), nullptr) != 1 ||
            EVP_PKEY_get_int_param(key, OSSL_PKEY_PARAM_EC_DECODED_FROM_EXPLICIT_PARAMS, &explicitParameters) != 1 ||
            explicitParameters != 0 ||
            (std::string_view{group.data()} != "prime256v1" && std::string_view{group.data()} != "secp384r1")) {
            return false;
        }
    } else {
        return false;
    }

    const std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> check{
        EVP_PKEY_CTX_new_from_pkey(library, key, PROPERTY_QUERY), EVP_PKEY_CTX_free};

    return check && EVP_PKEY_public_check(check.get()) == 1;
}

/** Apply envelope limits before invoking the default OpenSSL verification exactly once.
 * @param context Native verification state; flags, depth, trust and identity stay in force.
 * @param argument Private provider context, owned by the same attempt.
 * @return Native verified status, or zero if an envelope check fails.
 */
int verifyChain(X509_STORE_CTX *context, void *argument) noexcept {
    auto *library = static_cast<OSSL_LIB_CTX *>(argument);
    const auto *untrusted = X509_STORE_CTX_get0_untrusted(context);
    const int count = untrusted != nullptr ? sk_X509_num(untrusted) : 0;
    auto *leaf = X509_STORE_CTX_get0_cert(context);

    if (count < 0 || count > MAX_CHAIN_ENTRIES || !guardCertificate(leaf, library)) {
        X509_STORE_CTX_set_error(context, X509_V_ERR_APPLICATION_VERIFICATION);

        return 0;
    }

    for (int index = 0; index < count; ++index) {
        auto *certificate = sk_X509_value(untrusted, index);

        if (certificate != leaf && !guardCertificate(certificate, library)) {
            X509_STORE_CTX_set_error(context, X509_V_ERR_APPLICATION_VERIFICATION);

            return 0;
        }
    }

    return X509_verify_cert(context);
}

/** Refuse a password request without invoking system or application prompts.
 * @return Zero bytes for every request.
 */
int refusePassword(char *, int, int, void *) noexcept {
    return 0;
}

/** Dedicated OpenSSL library context and its adopted Asio SSL context. */
class TlsContext final {
    std::unique_ptr<OSSL_LIB_CTX, decltype(&OSSL_LIB_CTX_free)> library_{nullptr, OSSL_LIB_CTX_free};
    std::unique_ptr<OSSL_PROVIDER, decltype(&OSSL_PROVIDER_unload)> provider_{nullptr, OSSL_PROVIDER_unload};
    std::optional<ssl::context> context_;

    /** Decode only already admitted anchor slices into the private store.
     * @param native Prepared native SSL context.
     * @param blocks Complete prechecked slice collection.
     * @param count Number of valid slices.
     * @return True if every admitted anchor is accepted.
     */
    bool loadTrust(SSL_CTX *native, const std::array<std::string_view, MAX_ANCHORS> &blocks, std::size_t count) {
        auto *store = SSL_CTX_get_cert_store(native);

        if (store == nullptr) {
            return false;
        }

        for (std::size_t index = 0; index < count; ++index) {
            const auto block = blocks[index];
            const std::unique_ptr<BIO, decltype(&BIO_free)> input{
                BIO_new_mem_buf(block.data(), static_cast<int>(block.size())), BIO_free};
            auto *candidate = X509_new_ex(library_.get(), PROPERTY_QUERY);

            if (!input || candidate == nullptr) {
                X509_free(candidate);

                return false;
            }

            auto *decoded = PEM_read_bio_X509(input.get(), &candidate, refusePassword, nullptr);
            const std::unique_ptr<X509, decltype(&X509_free)> owned{candidate, X509_free};

            if (decoded == nullptr || !guardCertificate(candidate, library_.get()) || X509_check_ca(candidate) <= 0 ||
                X509_STORE_add_cert(store, candidate) != 1) {
                return false;
            }
        }

        return true;
    }

    public:
    /** Construct the selected isolated context and load the complete explicit trust snapshot.
     * @param pem Bounded borrowed trust PEM consumed synchronously.
     * @return No value on success, or the policy/trust failure classification.
     */
    std::optional<WssError> initialize(std::string_view pem) {
        std::array<std::string_view, MAX_ANCHORS> blocks{};
        const auto count = scanTrust(pem, blocks);

        if (count == 0) {
            return WssError::TRUST;
        }

        if (OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) != 1) {
            return WssError::TLS_POLICY;
        }

        library_.reset(OSSL_LIB_CTX_new());

        if (!library_) {
            return WssError::TLS_POLICY;
        }

        provider_.reset(OSSL_PROVIDER_load(library_.get(), "default"));

        if (!provider_ || RAND_set_DRBG_type(library_.get(), "CTR-DRBG", PROPERTY_QUERY, "AES-256-CTR", nullptr) != 1 ||
            RAND_set_seed_source_type(library_.get(), "SEED-SRC", PROPERTY_QUERY) != 1) {
            return WssError::TLS_POLICY;
        }

        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> native{
            SSL_CTX_new_ex(library_.get(), PROPERTY_QUERY, TLS_client_method()), SSL_CTX_free};
        constexpr auto DISABLED_FEATURES = SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_TICKET |
                                           SSL_OP_NO_TX_CERTIFICATE_COMPRESSION | SSL_OP_NO_RX_CERTIFICATE_COMPRESSION;

        if (!native || SSL_CTX_set_min_proto_version(native.get(), TLS1_2_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(native.get(), TLS1_3_VERSION) != 1 ||
            SSL_CTX_set_cipher_list(native.get(), "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256") != 1 ||
            SSL_CTX_set_ciphersuites(native.get(), "TLS_AES_128_GCM_SHA256") != 1 ||
            SSL_CTX_set1_groups_list(native.get(), "X25519:P-256") != 1 ||
            SSL_CTX_set1_sigalgs_list(native.get(), "ecdsa_secp256r1_sha256:ecdsa_secp384r1_sha384:"
                                                    "rsa_pss_rsae_sha256:rsa_pkcs1_sha256:rsa_pkcs1_sha384") != 1 ||
            SSL_CTX_set_max_early_data(native.get(), 0) != 1 || SSL_CTX_set_recv_max_early_data(native.get(), 0) != 1 ||
            SSL_CTX_set_num_tickets(native.get(), 0) != 1 ||
            (SSL_CTX_set_options(native.get(), DISABLED_FEATURES) & DISABLED_FEATURES) != DISABLED_FEATURES) {
            return WssError::TLS_POLICY;
        }

        SSL_CTX_set_post_handshake_auth(native.get(), 0);
        SSL_CTX_set_session_cache_mode(native.get(), SSL_SESS_CACHE_OFF);
        SSL_CTX_set_verify(native.get(), SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_verify_depth(native.get(), 4);
        SSL_CTX_set_max_cert_list(native.get(), MAX_CHAIN_BYTES);
        SSL_CTX_set_security_level(native.get(), 2);
        SSL_CTX_set_cert_verify_callback(native.get(), verifyChain, library_.get());

        if (!loadTrust(native.get(), blocks, count)) {
            return WssError::TRUST;
        }

        if (SSL_CTX_get_verify_callback(native.get()) != nullptr || SSL_CTX_get_app_data(native.get()) != nullptr ||
            SSL_CTX_get_default_passwd_cb_userdata(native.get()) != nullptr) {
            return WssError::TLS_POLICY;
        }

        context_.emplace(native.get());
        (void)native.release();

        return std::nullopt;
    }

    /** Borrow the adopted context before constructing its only stream.
     * @return Context owned until after the stream retires.
     */
    ssl::context &nativeContext() noexcept {
        return *context_;
    }
};

} // namespace

/** Stable state and operation backing. Application callbacks never run on this executor. */
class WssSession::Impl final {
    asio::io_context io_;
    std::optional<Tcp::resolver> resolver_{std::in_place, io_};
    std::optional<TlsContext> context_;
    std::optional<WebSocket> websocket_;
    std::optional<beast::websocket::response_type> response_;
    std::optional<beast::flat_buffer> input_;
    std::optional<WssMessage> message_;
    std::optional<std::string> output_;
    std::array<Tcp::endpoint, MAX_ENDPOINTS> endpoints_{};
    Tcp::resolver::results_type resolved_;
    std::string route_;
    std::string service_;
    std::string authority_;
    std::string target_;
    std::chrono::milliseconds connectTimeout_;
    std::chrono::milliseconds tlsTimeout_;
    std::chrono::milliseconds upgradeTimeout_;
    std::chrono::milliseconds receiveTimeout_;
    std::chrono::milliseconds sendTimeout_;
    std::chrono::milliseconds closeTimeout_;
    std::size_t maxMessageBytes_;
    Clock::time_point deadline_{};
    WssStage stage_ = WssStage::CONFIGURATION;
    bool pending_ = false;
    Error result_{};
    std::optional<WssDiagnostic> diagnostic_;

    /** @return True after a terminal diagnostic and native backing retirement. */
    bool terminal() const noexcept {
        return stage_ == WssStage::FAILED || stage_ == WssStage::CLOSED;
    }

    /** Enter a subsequent active phase before initiating its sole operation.
     * @param stage New active phase.
     * @param timeout Admitted relative deadline.
     */
    void enter(WssStage stage, std::chrono::milliseconds timeout) noexcept {
        stage_ = stage;
        deadline_ = Clock::now() + timeout;
    }

    /** Retire the operation after recording its result on the owner thread.
     * @param error Native error, equal to zero for ordinary success.
     */
    void complete(Error error) noexcept {
        result_ = error;
        pending_ = false;
    }

    /** Release native and operation ownership after the top-level handler retires. */
    void retireTerminal() noexcept {
        response_.reset();
        input_.reset();
        message_.reset();
        output_.reset();
        websocket_.reset();
        context_.reset();
        stage_ = diagnostic_->reason == WssError::GRACEFUL_CLOSE ? WssStage::CLOSED : WssStage::FAILED;
    }

    /** Install a terminal receipt and cut off further native I/O.
     * @param receipt Bounded failure information identifying the active phase.
     */
    void cutoff(WssDiagnostic receipt) noexcept {
        diagnostic_ = receipt;

        // Destroy the resolver to invalidate its operation without allocating a replacement cancel token.
        resolver_.reset();

        if (websocket_) {
            Error ignored;
            beast::get_lowest_layer(*websocket_).socket().close(ignored);
        }

        if (pending_) {
            stage_ = WssStage::STOPPING;
        } else {
            retireTerminal();
        }
    }

    /** Admit the peer close receipt after Beast completes the WebSocket and transport handshake. */
    void finishClose() noexcept {
        WssDiagnostic receipt{stage_, WssError::GRACEFUL_CLOSE, result_};
        receipt.closeCode = static_cast<std::uint16_t>(websocket_->reason().code);
        cutoff(receipt);
    }

    /** Stage one complete valid text message or drop an empty text message. */
    void finishReceive() {
        if (input_->size() > maxMessageBytes_) {
            cutoff({stage_, WssError::MESSAGE_LIMIT});

            return;
        }

        if (!websocket_->got_text()) {
            cutoff({stage_, WssError::UNSUPPORTED_BINARY});

            return;
        }

        if (input_->size() != 0) {
            const auto bytes = input_->data();
            message_.emplace(WssMessage{std::string_view{static_cast<const char *>(bytes.data()), bytes.size()}});
        }

        input_.reset();
        stage_ = WssStage::READY;
    }

    /** Advance the establishment chain or admit a completed post-upgrade operation. */
    void advance() {
        switch (stage_) {
        case WssStage::RESOLVE: {
            resolver_.reset();

            if (resolved_.empty() || resolved_.size() > endpoints_.size()) {
                cutoff({stage_, WssError::RESOURCE});

                return;
            }

            const auto copied = std::ranges::copy(resolved_, endpoints_.begin());
            resolved_ = {};
            enter(WssStage::CONNECT, connectTimeout_);
            beast::get_lowest_layer(*websocket_)
                .async_connect(endpoints_.begin(), copied.out, [this](Error error, auto) {
                    complete(error);
                });
            pending_ = true;

            return;
        }
        case WssStage::CONNECT:
            enter(WssStage::TLS, tlsTimeout_);
            websocket_->next_layer().async_handshake(ssl::stream_base::client, [this](Error error) {
                complete(error);
            });
            pending_ = true;

            return;
        case WssStage::TLS:
            enter(WssStage::UPGRADE, upgradeTimeout_);
            response_.emplace();
            websocket_->async_handshake(*response_, authority_, target_, [this](Error error) {
                complete(error);
            });
            pending_ = true;

            return;
        case WssStage::UPGRADE:

            if (response_->count(beast::http::field::sec_websocket_extensions) != 0) {
                cutoff({stage_, WssError::UNSUPPORTED_EXTENSION});

                return;
            }

            response_.reset();
            stage_ = WssStage::READY;

            return;
        case WssStage::RECEIVING:
            finishReceive();

            return;
        case WssStage::SENDING:
            output_.reset();
            stage_ = WssStage::READY;

            return;
        case WssStage::CLOSING:
            finishClose();

            return;
        default:
            return;
        }
    }

    /** Map the completed operation error before closing the attempt. */
    void failOperation() noexcept {
        WssDiagnostic receipt{stage_, WssError::NATIVE, result_};

        if (stage_ == WssStage::TLS) {
            const auto verification = SSL_get_verify_result(websocket_->next_layer().native_handle());

            if (verification != X509_V_OK) {
                receipt.reason = WssError::TLS_VERIFICATION;
                receipt.verifyError = verification;
            }
        }

        if (stage_ == WssStage::UPGRADE && result_ == beast::websocket::error::upgrade_declined) {
            receipt.httpStatus = response_->result_int();
        }

        if (stage_ == WssStage::RECEIVING || stage_ == WssStage::SENDING || stage_ == WssStage::CLOSING) {
            namespace ws = beast::websocket;

            if (result_ == ws::error::closed) {
                finishClose();

                return;
            }

            if (result_ == ws::error::message_too_big || result_ == ws::error::buffer_overflow) {
                receipt.reason = WssError::MESSAGE_LIMIT;
            } else if (result_ == ws::condition::protocol_violation) {
                receipt.reason = WssError::PROTOCOL;
            } else if (result_ == asio::error::eof || result_ == ssl::error::stream_truncated) {
                receipt.reason = WssError::PEER_EOF;
            }
        }

        cutoff(receipt);
    }

    public:
    /** Copy only the checked settings retained through the asynchronous attempt.
     * @param settings Previously admitted borrowed settings.
     */
    explicit Impl(const WssSettings &settings)
        : route_(settings.routeHost), service_(std::to_string(settings.port)), authority_(settings.identity),
          target_(settings.target), connectTimeout_(settings.connectTimeout), tlsTimeout_(settings.tlsTimeout),
          upgradeTimeout_(settings.upgradeTimeout), receiveTimeout_(settings.receiveTimeout),
          sendTimeout_(settings.sendTimeout), closeTimeout_(settings.closeTimeout),
          maxMessageBytes_(settings.maxMessageBytes) {
        if (settings.port != 443) {
            authority_.append(":").append(service_);
        }
    }

    /** Resolve the private TLS profile and arm the resolver on the stable owner.
     * @param settings Option/trust views valid for this call.
     * @return No diagnostic on successful synchronous admission.
     */
    std::optional<WssError> start(const WssSettings &settings) {
        context_.emplace();

        if (const auto failure = context_->initialize(settings.trustPem)) {
            return failure;
        }

        websocket_.emplace(io_, context_->nativeContext(), WIRE_WINDOW, WIRE_WINDOW);
        auto *native = websocket_->next_layer().native_handle();
        auto *bio = SSL_get_wbio(native);
        constexpr auto WRITE_MODES = SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE;
        const std::string identity{settings.identity};

        SSL_set_hostflags(native, X509_CHECK_FLAG_NEVER_CHECK_SUBJECT | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

        if (bio == nullptr || bio != SSL_get_rbio(native) || BIO_get_write_guarantee(bio) != WIRE_WINDOW ||
            SSL_get_SSL_CTX(native) != context_->nativeContext().native_handle() ||
            SSL_get_app_data(native) != nullptr || (SSL_clear_mode(native, WRITE_MODES) & WRITE_MODES) != 0 ||
            SSL_set1_host(native, identity.c_str()) != 1 || SSL_set_tlsext_host_name(native, identity.c_str()) != 1) {
            return WssError::TLS_POLICY;
        }

        websocket_->read_message_max(maxMessageBytes_);
        websocket_->write_buffer_bytes(WRITE_WINDOW);
        websocket_->auto_fragment(true);
        enter(WssStage::RESOLVE, settings.resolveTimeout);
        resolver_->async_resolve(route_, service_, Tcp::resolver::numeric_service,
                                 [this](Error error, Tcp::resolver::results_type endpoints) {
                                     if (!diagnostic_) {
                                         resolved_ = std::move(endpoints);
                                     }

                                     complete(error);
                                 });
        pending_ = true;

        return std::nullopt;
    }

    /** Cut off any open state, then retire handlers that still borrow this owner. */
    ~Impl() {
        stop();

        // The worker may still be inside the platform resolver after cancellation.
        io_.restart();
        (void)io_.run();
    }

    /** Run one native handler, then select completion, cutoff or the next active phase.
     * @param wait Maximum wait requested by the owner.
     * @return Stage after progress.
     */
    WssStage run(std::chrono::milliseconds wait) {
        if (stage_ == WssStage::READY || terminal()) {
            return stage_;
        }

        const auto now = Clock::now();
        constexpr auto MAX_WAIT = std::chrono::duration_cast<std::chrono::milliseconds>(MAX_STAGE_TIME);
        auto budget =
            std::chrono::duration_cast<Clock::duration>(std::clamp(wait, std::chrono::milliseconds::zero(), MAX_WAIT));

        if (stage_ != WssStage::STOPPING) {
            if (now >= deadline_) {
                cutoff({stage_, WssError::TIMEOUT});

                return stage_;
            }

            budget = std::min(budget, deadline_ - now);
        }

        io_.restart();

        if (budget > Clock::duration::zero()) {
            (void)io_.run_one_for(budget);
        } else {
            (void)io_.poll_one();
        }

        if (stage_ == WssStage::STOPPING) {
            if (!pending_) {
                retireTerminal();
            }
        } else if (Clock::now() >= deadline_) {
            cutoff({stage_, WssError::TIMEOUT});
        } else if (!pending_) {
            if (result_) {
                failOperation();
            } else {
                try {
                    advance();
                } catch (const std::bad_alloc &) {
                    cutoff({stage_, WssError::RESOURCE});
                } catch (const boost::system::system_error &error) {
                    cutoff({stage_, WssError::NATIVE, error.code()});
                }
            }
        }

        return stage_;
    }

    /** Arm one operation after admitting its backing and a caller-work slot.
     * @param operation Receiving, sending or closing phase selected by the entry point.
     * @param text Borrowed UTF-8 output considered only for SENDING.
     * @return Exact admission outcome.
     */
    WssAdmission startIo(WssStage operation, std::string_view text = {}) {
        if (stage_ == WssStage::RECEIVING || stage_ == WssStage::SENDING || stage_ == WssStage::CLOSING || message_) {
            return WssAdmission::BUSY;
        }

        if (stage_ != WssStage::READY) {
            return WssAdmission::NOT_READY;
        }

        if (operation == WssStage::SENDING && (text.empty() || text.size() > maxMessageBytes_ ||
                                               !beast::websocket::detail::check_utf8(text.data(), text.size()))) {
            return WssAdmission::INVALID_PAYLOAD;
        }

        try {
            switch (operation) {
            case WssStage::RECEIVING:
                enter(operation, receiveTimeout_);
                // At the exact message limit, Beast still needs a writable byte to process an empty final fragment.
                input_.emplace(maxMessageBytes_ + 1);
                websocket_->async_read(*input_, [this](Error error, std::size_t) {
                    complete(error);
                });
                break;
            case WssStage::SENDING:
                enter(operation, sendTimeout_);
                output_.emplace(text);
                websocket_->text(true);
                websocket_->async_write(asio::buffer(*output_), [this](Error error, std::size_t) {
                    complete(error);
                });
                break;
            case WssStage::CLOSING:
                enter(operation, closeTimeout_);
                websocket_->async_close(beast::websocket::close_code::normal, [this](Error error) {
                    complete(error);
                });
                break;
            default:
                return WssAdmission::NOT_READY;
            }

            pending_ = true;

            return WssAdmission::ACCEPTED;
        } catch (const std::bad_alloc &) {
            cutoff({stage_, WssError::RESOURCE});
        } catch (const boost::system::system_error &error) {
            cutoff({stage_, WssError::NATIVE, error.code()});
        }

        return WssAdmission::FAILED;
    }

    /** @return Move the held text result out and free the next caller-work slot. */
    std::optional<WssMessage> takeMessage() noexcept {
        return std::exchange(message_, std::nullopt);
    }

    /** Impose owner cutoff no later than this call. */
    void stop() noexcept {
        if (!terminal() && stage_ != WssStage::STOPPING) {
            cutoff({stage_, WssError::CANCELLED});
        }
    }

    /** @return Current owner-thread phase. */
    WssStage stage() const noexcept {
        return stage_;
    }

    /** @return Borrowed stable diagnostic receipt, empty before any cutoff. */
    const std::optional<WssDiagnostic> &diagnostic() const noexcept {
        return diagnostic_;
    }

    /** @return Positive Beast parser message limit in bytes. */
    std::size_t readMessageLimit() const noexcept {
        return websocket_ ? websocket_->read_message_max() : maxMessageBytes_;
    }
};

WssSession::WssSession(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

std::expected<std::unique_ptr<WssSession>, WssDiagnostic> WssSession::begin(const WssSettings &settings) {
    if (!settingsValid(settings)) {
        return std::unexpected(WssDiagnostic{WssStage::CONFIGURATION, WssError::INVALID_CONFIGURATION});
    }

    try {
        auto session = std::unique_ptr<WssSession>{new WssSession(std::make_unique<Impl>(settings))};

        if (const auto failure = session->impl_->start(settings)) {
            return std::unexpected(WssDiagnostic{WssStage::CONFIGURATION, *failure});
        }

        return session;
    } catch (const std::bad_alloc &) {
        return std::unexpected(WssDiagnostic{WssStage::CONFIGURATION, WssError::RESOURCE});
    } catch (const boost::system::system_error &error) {
        return std::unexpected(WssDiagnostic{WssStage::CONFIGURATION, WssError::NATIVE, error.code()});
    }
}

WssSession::~WssSession() = default;

WssMessage::WssMessage(std::string_view text) : bytes_(text) {
}

std::string_view WssMessage::text() const noexcept {
    return bytes_;
}

WssStage WssSession::runOneFor(std::chrono::milliseconds wait) {
    return impl_->run(wait);
}

WssAdmission WssSession::startReceive() {
    return impl_->startIo(WssStage::RECEIVING);
}

std::optional<WssMessage> WssSession::takeMessage() noexcept {
    return impl_->takeMessage();
}

WssAdmission WssSession::startSend(std::string_view text) {
    return impl_->startIo(WssStage::SENDING, text);
}

WssAdmission WssSession::startClose() {
    return impl_->startIo(WssStage::CLOSING);
}

void WssSession::requestStop() noexcept {
    impl_->stop();
}

WssStage WssSession::stage() const noexcept {
    return impl_->stage();
}

const std::optional<WssDiagnostic> &WssSession::diagnostic() const noexcept {
    return impl_->diagnostic();
}

std::size_t WssSession::readMessageLimit() const noexcept {
    return impl_->readMessageLimit();
}

} // namespace org::ttldtor::qdcxx::kraken
