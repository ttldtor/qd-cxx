// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_engine.hpp"

#include <asio/ip/address.hpp>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <limits>
#include <new>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

/** Construct detached engine evidence without inspecting/formatting peer or SDK text.
 * @param error Typed category.
 * @param stage Static checkpoint.
 * @param sdk Numeric SDK evidence already sampled after immediate classification.
 * @return Detached static/numeric diagnostic.
 */
TlsDiagnostic engineFailure(TlsError error, std::string_view stage, std::uint64_t sdk = 0) noexcept {
    return {error, stage, sdk};
}

/** Publish the private-codec failure marker through the SDK-independent common core.
 * @return Static common transport marker; the stable engine retains the precise TLS cause.
 */
TcpDiagnostic commonFailure() noexcept {
    return {TcpError::PROTOCOL_FAILURE, "tls-engine"};
}

} // namespace

TlsEngine::~TlsEngine() {
    SSL_free(ssl_);
    BIO_free(peer_);
}

void TlsEngine::fail(TlsDiagnostic failure) noexcept {
    if (failure_.error == TlsError::NONE) {
        failure_ = failure;
    }
}

bool TlsEngine::refresh() noexcept {
    pendingWire_ = BIO_ctrl_pending(peer_);

    if (pendingWire_ > TLS_RECORD_CAPACITY) {
        fail(engineFailure(TlsError::RESOURCE_LIMIT, "tls-bio-bounds"));

        return false;
    }

    peerClosed_ = (SSL_get_shutdown(ssl_) & SSL_RECEIVED_SHUTDOWN) != 0;
    bufferedRead_ = SSL_has_pending(ssl_) != 0 || BIO_ctrl_pending(SSL_get_rbio(ssl_)) != 0;

    return true;
}

bool TlsEngine::drainCiphertext() noexcept {
    if (!ciphertext().empty() || pendingWire_ == 0) {
        return false;
    }

    const int bytes = BIO_read(peer_, cipher_.data(), static_cast<int>(cipher_.size()));

    if (bytes <= 0 || static_cast<std::size_t>(bytes) > pendingWire_) {
        fail(engineFailure(TlsError::TRANSPORT_FAILURE, "tls-bio-drain", ERR_peek_last_error()));

        return false;
    }

    cipherBegin_ = 0;
    cipherEnd_ = static_cast<std::size_t>(bytes);
    pendingWire_ -= cipherEnd_;

    return true;
}

void TlsEngine::classify(int classification, std::string_view stage) noexcept {
    wantRead_ = classification == SSL_ERROR_WANT_READ;
    wantWrite_ = classification == SSL_ERROR_WANT_WRITE;

    if (wantRead_ || wantWrite_) {
        return;
    }

    if (classification == SSL_ERROR_ZERO_RETURN && verified_) {
        peerClosed_ = true;
        closeRequested_ = true;
        wantRead_ = false;

        if (pendingWrite_ != nullptr) {
            fail(engineFailure(TlsError::CLOSE_FAILURE, "tls-close-during-write"));
        }

        return;
    }

    const auto sdk = ERR_peek_last_error();
    const auto verifyCode = SSL_get_verify_result(ssl_);

    if (verifyCode == X509_V_ERR_HOSTNAME_MISMATCH || verifyCode == X509_V_ERR_IP_ADDRESS_MISMATCH) {
        auto cause = engineFailure(TlsError::IDENTITY_FAILURE, "tls-reference-identity", sdk);
        cause.verifyCode = static_cast<int>(verifyCode);
        fail(cause);

        return;
    }

    if (const auto guard = policy_->verificationFailure(); guard.error != TlsError::NONE) {
        fail(guard);

        return;
    }

    auto cause = engineFailure(verifyCode != X509_V_OK ? TlsError::TRUST_FAILURE
                               : !verified_            ? TlsError::HANDSHAKE_FAILURE
                               : closeRequested_       ? TlsError::CLOSE_FAILURE
                                                       : TlsError::TRANSPORT_FAILURE,
                               stage, sdk);
    cause.verifyCode = static_cast<int>(verifyCode);
    fail(cause);
}

std::expected<void, TlsDiagnostic> TlsEngine::configureIdentity(const client::EndpointTls &settings) noexcept {
    asio::error_code addressError;
    (void)asio::ip::make_address(settings.referenceIdentity, addressError);
    auto *parameters = SSL_get0_param(ssl_);

    if (parameters == nullptr) {
        return std::unexpected(engineFailure(TlsError::TLS_POLICY_FAILURE, "tls-identity-parameters"));
    }

    if (!addressError) {
        if (X509_VERIFY_PARAM_set1_ip_asc(parameters, settings.referenceIdentity.c_str()) != 1) {
            return std::unexpected(engineFailure(TlsError::TLS_POLICY_FAILURE, "tls-ip-san", ERR_peek_last_error()));
        }
    } else {
        constexpr unsigned int FLAGS = X509_CHECK_FLAG_NEVER_CHECK_SUBJECT | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS;
        X509_VERIFY_PARAM_set_hostflags(parameters, FLAGS);

        if (X509_VERIFY_PARAM_get_hostflags(parameters) != FLAGS ||
            SSL_set1_host(ssl_, settings.referenceIdentity.c_str()) != 1 ||
            SSL_set_tlsext_host_name(ssl_, settings.referenceIdentity.c_str()) != 1) {
            return std::unexpected(
                engineFailure(TlsError::TLS_POLICY_FAILURE, "tls-dns-san-sni", ERR_peek_last_error()));
        }
    }

    return {};
}

std::expected<std::unique_ptr<TlsEngine>, TlsDiagnostic> TlsEngine::create(const client::EndpointTls &settings,
                                                                           const TlsResourceLimits &limits) {
    if (settings.referenceIdentity.empty() || settings.referenceIdentity.size() > limits.identityBytes ||
        settings.referenceIdentity.size() > 254) {
        return std::unexpected(engineFailure(TlsError::INVALID_CONFIGURATION, "tls-reference-envelope"));
    }

    auto policy = TlsPolicy::create(settings, limits);

    if (!policy) {
        return std::unexpected(policy.error());
    }

    try {
        auto engine = std::unique_ptr<TlsEngine>(new TlsEngine);
        engine->policy_ = std::move(*policy);
        engine->ssl_ = SSL_new(engine->policy_->sslContext());

        if (engine->ssl_ == nullptr) {
            return std::unexpected(engineFailure(TlsError::ALLOCATION_FAILURE, "tls-ssl-owner", ERR_peek_last_error()));
        }

        BIO *internal = nullptr;
        BIO *external = nullptr;

        if (BIO_new_bio_pair(&internal, TLS_RECORD_CAPACITY, &external, TLS_RECORD_CAPACITY) != 1) {
            BIO_free(internal);
            BIO_free(external);

            return std::unexpected(engineFailure(TlsError::ALLOCATION_FAILURE, "tls-bio-pair", ERR_peek_last_error()));
        }

        SSL_set_bio(engine->ssl_, internal, internal); // SSL owns the single shared rbio/wbio endpoint reference.
        engine->peer_ = external;
        SSL_set_connect_state(engine->ssl_);
        constexpr long FORBIDDEN_WRITE_MODES = SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
        SSL_clear_mode(engine->ssl_, FORBIDDEN_WRITE_MODES);

        if ((SSL_get_mode(engine->ssl_) & FORBIDDEN_WRITE_MODES) != 0) {
            return std::unexpected(engineFailure(TlsError::TLS_POLICY_FAILURE, "tls-write-retry-mode"));
        }

        if (auto identity = engine->configureIdentity(settings); !identity) {
            return std::unexpected(identity.error());
        }

        return engine;
    } catch (const std::bad_alloc &) {
        return std::unexpected(engineFailure(TlsError::ALLOCATION_FAILURE, "tls-engine-storage"));
    }
}

void TlsEngine::fundInitial() noexcept {
    initialFunded_ = true;
}

void TlsEngine::fundEntry(bool close) noexcept {
    entryFunded_ = true;
    closeFunded_ = close;
}

void TlsEngine::endEntry() noexcept {
    entryFunded_ = false;
    closeFunded_ = false;
}

std::span<const std::byte> TlsEngine::ciphertext() const noexcept {
    return {cipher_.data() + cipherBegin_, cipherEnd_ - cipherBegin_};
}

bool TlsEngine::consumeCiphertext(std::size_t bytes) noexcept {
    if (bytes > cipherEnd_ - cipherBegin_) {
        return false;
    }

    cipherBegin_ += bytes;

    if (cipherBegin_ == cipherEnd_) {
        cipherBegin_ = cipherEnd_ = 0;
    }

    return true;
}

std::span<const std::byte> TlsEngine::plaintext() const noexcept {
    return {plain_.data() + plainBegin_, plainEnd_ - plainBegin_};
}

void TlsEngine::consumePlaintext(std::size_t bytes) noexcept {
    if (bytes > plainEnd_ - plainBegin_) {
        fail(engineFailure(TlsError::RESOURCE_LIMIT, "tls-plaintext-count"));

        return;
    }

    plainBegin_ += bytes;

    if (plainBegin_ == plainEnd_) {
        plainBegin_ = plainEnd_ = 0;
    }
}

bool TlsEngine::needsStep(bool hasInput, bool hasOutput) const noexcept {
    if (aborted_ || failure_.error != TlsError::NONE || !ciphertext().empty() || !plaintext().empty() || finished()) {
        return false;
    }

    if (pendingWire_ != 0 || hasInput || !started_) {
        return true;
    }

    if (wantRead_) {
        return verified_ && hasOutput && pendingWrite_ == nullptr && !closeRequested_;
    }

    return wantWrite_ || bufferedRead_ || (closeRequested_ && !shutdownComplete_) || (verified_ && hasOutput);
}

ByteEngineStep TlsEngine::advance(std::span<const std::byte> input, std::span<const std::byte> output) noexcept {
    ByteEngineStep step;

    if (aborted_ || !initialFunded_ || !entryFunded_ || failure_.error != TlsError::NONE) {
        step.failure = commonFailure();

        return step;
    }

    entryFunded_ = false;

    if (const auto environment = policy_->checkEnvironment(); !environment) {
        fail(environment.error());
        step.failure = commonFailure();

        return step;
    }

    if (pendingWire_ != 0) {
        step.progressed = drainCiphertext();

        if (failure_.error != TlsError::NONE) {
            step.failure = commonFailure();
        }

        return step;
    }

    if (!input.empty()) {
        const auto bytes = (std::min)(input.size(), TLS_RECORD_CAPACITY);
        const int written = BIO_write(peer_, input.data(), static_cast<int>(bytes));

        if (written > 0) {
            step.inputConsumed = static_cast<std::size_t>(written);
            step.progressed = true;
            wantRead_ = false;
        } else if (BIO_should_retry(peer_) == 0) {
            fail(engineFailure(TlsError::TRANSPORT_FAILURE, "tls-bio-input", ERR_peek_last_error()));
        }
    }

    if (failure_.error == TlsError::NONE && !verified_) {
        started_ = true;
        ERR_clear_error();
        const int result = SSL_do_handshake(ssl_);
        const int error = result == 1 ? SSL_ERROR_NONE : SSL_get_error(ssl_, result);
        step.progressed = true;

        if (result == 1) {
            const auto verification = SSL_get_verify_result(ssl_);
            const int version = SSL_version(ssl_);

            if (verification != X509_V_OK || SSL_get0_peer_certificate(ssl_) == nullptr ||
                SSL_session_reused(ssl_) != 0 || (version != TLS1_2_VERSION && version != TLS1_3_VERSION)) {
                auto cause = engineFailure(TlsError::TRUST_FAILURE, "tls-handshake-verification");
                cause.verifyCode = static_cast<int>(verification);
                fail(cause);
            } else {
                verified_ = true;
                wantRead_ = wantWrite_ = false;
            }
        } else {
            classify(error, "tls-handshake");
        }
    } else if (failure_.error == TlsError::NONE && closeRequested_) {
        if (!closeFunded_) {
            fail(engineFailure(TlsError::WORK_LIMIT, "tls-close-permit"));
        } else {
            ERR_clear_error();
            const int result = SSL_shutdown(ssl_);
            // Zero is normal incomplete closure: do NOT call SSL_get_error for zero or one.
            const int error = result < 0 ? SSL_get_error(ssl_, result) : SSL_ERROR_NONE;
            step.progressed = true;

            if (result == 1) {
                shutdownComplete_ = true;
                wantRead_ = wantWrite_ = false;
            } else if (result == 0) {
                wantRead_ = wantWrite_ = false; // One later prepaid attempt may classify WANT_READ.
            } else {
                classify(error, "tls-shutdown");
            }
        }
    } else if (failure_.error == TlsError::NONE && !output.empty()) {
        if (pendingWrite_ == nullptr) {
            pendingWrite_ = output.data();
            pendingWriteSize_ = (std::min)(output.size(), TLS_PLAINTEXT_CAPACITY);
        }

        if (output.data() != pendingWrite_ || output.size() < pendingWriteSize_ ||
            acceptedPlain_ > std::numeric_limits<std::uint64_t>::max() - pendingWriteSize_) {
            fail(engineFailure(TlsError::RESOURCE_LIMIT, "tls-write-arguments"));
        } else {
            ERR_clear_error();
            const int result = SSL_write(ssl_, pendingWrite_, static_cast<int>(pendingWriteSize_));
            const int error = result > 0 ? SSL_ERROR_NONE : SSL_get_error(ssl_, result);
            step.progressed = true;

            if (result > 0) {
                step.outputConsumed = static_cast<std::size_t>(result);
                acceptedPlain_ += step.outputConsumed;
                pendingWrite_ = nullptr;
                pendingWriteSize_ = 0;
                wantRead_ = wantWrite_ = false;
            } else {
                classify(error, "tls-record-write");
            }
        }
    } else if (failure_.error == TlsError::NONE) {
        ERR_clear_error();
        const int result = SSL_read(ssl_, plain_.data(), static_cast<int>(plain_.size()));
        const int error = result > 0 ? SSL_ERROR_NONE : SSL_get_error(ssl_, result);
        step.progressed = true;

        if (result > 0) {
            plainBegin_ = 0;
            plainEnd_ = static_cast<std::size_t>(result);
            wantRead_ = wantWrite_ = false;
        } else {
            classify(error, "tls-record-read");
        }
    }

    if (failure_.error == TlsError::NONE && refresh()) {
        (void)drainCiphertext();
    }

    if (failure_.error != TlsError::NONE) {
        step.failure = commonFailure();
    }

    return step;
}

bool TlsEngine::verified() const noexcept {
    return verified_;
}

bool TlsEngine::finished() const noexcept {
    // SSL_has_pending may retain a consumed alert record after SSL_shutdown returned one.
    // It is conservative input-data evidence, not a condition for completed TLS/output closure.
    return shutdownComplete_ && pendingWire_ == 0 && ciphertext().empty() && plaintext().empty();
}

std::optional<TcpDiagnostic> TlsEngine::nativeEnd() noexcept {
    if (!peerClosed_) {
        fail(engineFailure(verified_ ? TlsError::TRANSPORT_FAILURE : TlsError::HANDSHAKE_FAILURE, "tls-truncated-eof"));

        return commonFailure();
    }

    return std::nullopt;
}

void TlsEngine::abort() noexcept {
    aborted_ = true;
}

void TlsEngine::requestTlsClose() noexcept {
    closeRequested_ = true;
    wantRead_ = wantWrite_ = false;
}

bool TlsEngine::closing() const noexcept {
    return closeRequested_;
}

bool TlsEngine::pendingWrite() const noexcept {
    return pendingWrite_ != nullptr;
}

bool TlsEngine::peerClosed() const noexcept {
    return peerClosed_;
}

bool TlsEngine::dataPending() const noexcept {
    return bufferedRead_ || !finished();
}

std::size_t TlsEngine::pendingCiphertext() const noexcept {
    return pendingWire_ + cipherEnd_ - cipherBegin_;
}

std::uint64_t TlsEngine::acceptedPlaintext() const noexcept {
    return acceptedPlain_;
}

TlsDiagnostic TlsEngine::diagnostic() const noexcept {
    return failure_;
}

} // namespace org::ttldtor::qdcxx::transport::detail
