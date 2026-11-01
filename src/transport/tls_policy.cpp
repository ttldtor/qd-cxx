// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_policy.hpp"

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/params.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <new>

#if OPENSSL_VERSION_MAJOR != 3 || OPENSSL_VERSION_MINOR != 5 || OPENSSL_VERSION_PATCH != 9
#    error The qualified TLS profile requires OpenSSL 3.5.9
#endif
#if !defined(OPENSSL_NO_AUTOLOAD_CONFIG) || !defined(OPENSSL_NO_EC2M) || !defined(OPENSSL_NO_ML_KEM)
#    error The qualified TLS profile requires no-autoload-config, no-ec2m and no-ml-kem
#endif

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

inline constexpr std::size_t MAX_DECODER_DECLARATIONS = 76; ///< Conservative all-feature qualified catalog superset.
inline constexpr int ERR_PRIME_REASON = 1;                  ///< Fixed nonzero private queue-readiness marker.
inline constexpr char TLS12_CIPHERS[] =
    "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305"; ///< Exact TLS 1.2
                                                                                                 ///< list.
inline constexpr char TLS13_CIPHERS[] =
    "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"; ///< Exact TLS 1.3 list.
inline constexpr char SIGNATURES[] =
    "rsa_pss_rsae_sha256:rsa_pss_rsae_sha384:rsa_pss_rsae_sha512:"
    "rsa_pss_pss_sha256:rsa_pss_pss_sha384:rsa_pss_pss_sha512:"
    "rsa_pkcs1_sha256:rsa_pkcs1_sha384:rsa_pkcs1_sha512:"
    "ecdsa_secp256r1_sha256:ecdsa_secp384r1_sha384";   ///< Exact eleven selected TLS signature schemes.
inline constexpr char GROUPS[] = "X25519:P-256:P-384"; ///< Exact selected ephemeral groups.

/** Make detached evidence without formatting an SDK/peer string.
 * @param error Typed category.
 * @param stage Static checkpoint.
 * @param sdk Numeric SDK code, when already sampled at the correct boundary.
 * @return Static/numeric diagnostic.
 */
TlsDiagnostic diagnostic(TlsError error, std::string_view stage, std::uint64_t sdk = 0) noexcept {
    return {error, stage, sdk};
}

/** Recognize the explicit ordinary PEM whitespace profile without locale-dependent state.
 * @param byte One input byte.
 * @return True for space, tab, CR or LF.
 */
bool whitespace(char byte) noexcept {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}

/** Recognize one base64 presentation byte; the SDK remains the certificate/base64 validator.
 * @param byte One input byte.
 * @return True for the finite base64 alphabet or padding.
 */
bool base64(char byte) noexcept {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
           byte == '+' || byte == '/' || byte == '=';
}

/** Refuse every password route without UI, stdin, environment or credential-provider fallback.
 * @param buffer Unused output; no password is written.
 * @param size Unused destination capacity.
 * @param writing Unused request direction.
 * @param argument Unused opaque argument.
 * @return Negative noninteractive failure, never a successful empty password.
 */
int refusePassword(char *buffer, int size, int writing, void *argument) noexcept {
    (void)buffer;
    (void)size;
    (void)writing;
    (void)argument;

    return -1;
}

/** Finite allocation-free decoder-first enumeration receipt; count does not prove catalog completeness. */
struct DecoderReceipt {
    std::size_t count = 0; ///< Successful callback visits, capped at the qualified catalog bound.
    bool overflow = false; ///< More callbacks than the conservative declaration bound.
};

/** Count one catalog callback without SDK calls, user code, allocation or retained SDK pointers.
 * @param decoder Borrowed unused method.
 * @param argument Stable call-local receipt.
 */
void countDecoder(OSSL_DECODER *decoder, void *argument) noexcept {
    (void)decoder;
    auto &receipt = *static_cast<DecoderReceipt *>(argument);

    if (receipt.count == MAX_DECODER_DECLARATIONS) {
        receipt.overflow = true;
    } else {
        ++receipt.count;
    }
}

/** Check actually enforceable shared-image prerequisites without changing global settings.
 * @return Empty for builtin allocator, uninitialized secure heap and unchanged legacy RAND method.
 */
std::expected<void, TlsDiagnostic> sharedProfile() noexcept {
    CRYPTO_malloc_fn allocate = nullptr;
    CRYPTO_realloc_fn reallocate = nullptr;
    CRYPTO_free_fn release = nullptr;
    CRYPTO_get_mem_functions(&allocate, &reallocate, &release);

    if (allocate != CRYPTO_malloc || reallocate != CRYPTO_realloc || release != CRYPTO_free) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "sdk-allocator"));
    }

    if (CRYPTO_secure_malloc_initialized() != 0) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "sdk-secure-heap"));
    }

    if (RAND_get_rand_method() != RAND_OpenSSL()) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "sdk-legacy-rand"));
    }

    return {};
}

/** Validate cached generator READY state, exact builtin CTR/AES256/DF profile and bounded queries.
 * No actual SEED-SRC parent object is inferred: the reviewed NULL-parent core-entropy route remains
 * charged. Stable seed-source configuration and ambient identity are persistent host obligations.
 * @param generator Borrowed generator retained by its dependent library context/thread owner.
 * @param provider Private provider when enforcing pointer identity; null for the qualified ambient image.
 * @return True for the selected READY builtin generator.
 */
bool generatorProfile(EVP_RAND_CTX *generator, const OSSL_PROVIDER *provider) noexcept {
    if (generator == nullptr || EVP_RAND_get_state(generator) != EVP_RAND_STATE_READY) {
        return false;
    }

    const auto *method = EVP_RAND_CTX_get0_rand(generator);

    if (method == nullptr || EVP_RAND_is_a(method, "CTR-DRBG") != 1) {
        return false;
    }

    const auto *actualProvider = EVP_RAND_get0_provider(method);

    if (actualProvider == nullptr || (provider != nullptr && actualProvider != provider) ||
        std::strcmp(OSSL_PROVIDER_get0_name(actualProvider), "default") != 0) {
        return false;
    }

    std::array<char, 16> cipher{};
    int derivation = 0;
    OSSL_PARAM parameters[] = {OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_CIPHER, cipher.data(), cipher.size()),
                               OSSL_PARAM_construct_int(OSSL_DRBG_PARAM_USE_DF, &derivation),
                               OSSL_PARAM_construct_end()};

    return EVP_RAND_CTX_get_params(generator, parameters) == 1 && OSSL_PARAM_modified(&parameters[0]) &&
           OSSL_PARAM_modified(&parameters[1]) && parameters[0].return_size < cipher.size() &&
           std::strcmp(cipher.data(), "AES-256-CTR") == 0 && derivation == 1;
}

/** Accept a SHA2 AlgorithmIdentifier with its finite NULL/absent parameter rule.
 * @param algorithm Borrowed identifier.
 * @return Digest bytes32/48/64, or zero for an unsupported/malformed identifier.
 */
int sha2(const X509_ALGOR *algorithm) noexcept {
    if (algorithm == nullptr) {
        return 0;
    }

    const ASN1_OBJECT *object = nullptr;
    int type = V_ASN1_UNDEF;
    X509_ALGOR_get0(&object, &type, nullptr, algorithm);

    if (type != V_ASN1_UNDEF && type != V_ASN1_NULL) {
        return 0;
    }

    switch (OBJ_obj2nid(object)) {
    case NID_sha256:
        return 32;
    case NID_sha384:
        return 48;
    case NID_sha512:
        return 64;
    default:
        return 0;
    }
}

/** Compare a standard object's canonical encoding within a fixed provided scratch buffer.
 * @param parameters Parsed PSS schema.
 * @param bytes Original complete canonical DER bytes.
 * @return True for exact canonical full consumption and reencoding.
 */
bool canonicalPss(const RSA_PSS_PARAMS *parameters, std::span<const unsigned char> bytes) noexcept {
    std::array<unsigned char, 60> scratch{};
    const int size = i2d_RSA_PSS_PARAMS(parameters, nullptr);

    if (size <= 0 || static_cast<std::size_t>(size) != bytes.size() || bytes.size() > scratch.size()) {
        return false;
    }

    auto *cursor = scratch.data();

    return i2d_RSA_PSS_PARAMS(parameters, &cursor) == size && std::equal(bytes.begin(), bytes.end(), scratch.begin());
}

/** Validate the strict finite PSS schema using the SDK parser and bounded canonical reencoding.
 * @param type Raw AlgorithmIdentifier parameter type.
 * @param value Borrowed ASN1 parameter value.
 * @param spki True for key restrictions, false for a certificate signature.
 * @return True for absent unrestricted SPKI or the explicitly admitted canonical SHA2/MGF1 policy.
 */
bool pssParameters(int type, const void *value, bool spki) noexcept {
    if (type == V_ASN1_UNDEF) {
        return spki;
    }

    if (type != V_ASN1_SEQUENCE || value == nullptr) {
        return false;
    }

    const auto *sequence = static_cast<const ASN1_STRING *>(value);
    const int length = ASN1_STRING_length(sequence);
    const int maximum = spki ? 59 : 60;

    if (length <= 0 || length > maximum) {
        return false;
    }

    const auto *bytes = ASN1_STRING_get0_data(sequence);
    const auto *cursor = bytes;
    std::unique_ptr<RSA_PSS_PARAMS, decltype(&RSA_PSS_PARAMS_free)> parameters(
        d2i_RSA_PSS_PARAMS(nullptr, &cursor, length), RSA_PSS_PARAMS_free);

    if (!parameters || cursor != bytes + length ||
        !canonicalPss(parameters.get(), {bytes, static_cast<std::size_t>(length)})) {
        return false;
    }

    const int hashBytes = sha2(parameters->hashAlgorithm);

    if (hashBytes == 0 || parameters->maskGenAlgorithm == nullptr) {
        return false;
    }

    const ASN1_OBJECT *mask = nullptr;
    int maskType = V_ASN1_UNDEF;
    const void *maskValue = nullptr;
    X509_ALGOR_get0(&mask, &maskType, &maskValue, parameters->maskGenAlgorithm);

    if (OBJ_obj2nid(mask) != NID_mgf1 || maskType != V_ASN1_SEQUENCE || maskValue == nullptr) {
        return false;
    }

    const auto *maskSequence = static_cast<const ASN1_STRING *>(maskValue);
    const int maskLength = ASN1_STRING_length(maskSequence);

    if (maskLength <= 0 || maskLength > 15) {
        return false;
    }

    const auto *maskBytes = ASN1_STRING_get0_data(maskSequence);
    cursor = maskBytes;
    std::unique_ptr<X509_ALGOR, decltype(&X509_ALGOR_free)> maskHash(d2i_X509_ALGOR(nullptr, &cursor, maskLength),
                                                                     X509_ALGOR_free);

    if (!maskHash || cursor != maskBytes + maskLength || sha2(maskHash.get()) != hashBytes) {
        return false;
    }

    std::array<unsigned char, 60> scratch{};
    const int canonicalLength = i2d_X509_ALGOR(maskHash.get(), nullptr);

    if (canonicalLength != maskLength) {
        return false;
    }

    auto *encoded = scratch.data();

    if (i2d_X509_ALGOR(maskHash.get(), &encoded) != maskLength ||
        !std::equal(maskBytes, maskBytes + maskLength, scratch.begin())) {
        return false;
    }

    std::int64_t salt = 20;
    std::int64_t trailer = 1;

    if ((parameters->saltLength != nullptr && ASN1_INTEGER_get_int64(&salt, parameters->saltLength) != 1) ||
        (parameters->trailerField != nullptr && ASN1_INTEGER_get_int64(&trailer, parameters->trailerField) != 1)) {
        return false;
    }

    return trailer == 1 && salt >= 0 && salt <= (spki ? hashBytes : 478);
}

/** Validate the finite selected certificate signature OID and parameter policy.
 * @param algorithm Borrowed outer or TBS identifier.
 * @return True for selected RSA PKCS1/PSS or ECDSA SHA2 identifiers.
 */
bool signatureAlgorithm(const X509_ALGOR *algorithm) noexcept {
    if (algorithm == nullptr) {
        return false;
    }

    const ASN1_OBJECT *object = nullptr;
    int type = V_ASN1_UNDEF;
    const void *value = nullptr;
    X509_ALGOR_get0(&object, &type, &value, algorithm);

    switch (OBJ_obj2nid(object)) {
    case NID_sha256WithRSAEncryption:
    case NID_sha384WithRSAEncryption:
    case NID_sha512WithRSAEncryption:
        return type == V_ASN1_NULL || type == V_ASN1_UNDEF;
    case NID_ecdsa_with_SHA256:
    case NID_ecdsa_with_SHA384:
        return type == V_ASN1_UNDEF;
    case NID_rsassaPss:
        return pssParameters(type, value, false);
    default:
        return false;
    }
}

/** Validate actual public RSA n/e through bounded provider output without BIGNUM export/resize.
 * @param key Borrowed already type/provider/raw-SPKI admitted key.
 * @return True for positive2048..4096-bit modulus and exponent65537 with every output modified.
 */
bool rsaKey(EVP_PKEY *key) noexcept {
    std::array<unsigned char, 512> modulus{};
    std::uint64_t exponent = 0;
    int bits = 0;
    OSSL_PARAM parameters[] = {OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_BITS, &bits),
                               OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_N, modulus.data(), modulus.size()),
                               OSSL_PARAM_construct_uint64(OSSL_PKEY_PARAM_RSA_E, &exponent),
                               OSSL_PARAM_construct_end()};

    if (EVP_PKEY_get_params(key, parameters) != 1 || !OSSL_PARAM_modified(&parameters[0]) ||
        !OSSL_PARAM_modified(&parameters[1]) || !OSSL_PARAM_modified(&parameters[2]) ||
        parameters[1].return_size > modulus.size() || parameters[2].return_size > sizeof(exponent) || bits < 2048 ||
        bits > 4096 || exponent != 65537) {
        return false;
    }

    std::size_t highest = modulus.size();

    while (highest != 0 && modulus[highest - 1] == 0) {
        --highest;
    }

    if (highest == 0) {
        return false;
    }

    unsigned char top = modulus[highest - 1];
    int topBits = 0;

    while (top != 0) {
        ++topBits;
        top >>= 1;
    }

    return static_cast<std::size_t>(bits) == (highest - 1) * 8 + static_cast<std::size_t>(topBits);
}

/** Validate the named EC provider fields without point export or key conversion.
 * @param key Borrowed already type/provider/raw-SPKI admitted key.
 * @param curve Raw exact admitted curve NID.
 * @return True for complete named P256/P384 with no decoded-explicit parameters.
 */
bool ecKey(EVP_PKEY *key, int curve) noexcept {
    std::array<char, 16> group{};
    std::array<char, 16> encoding{};
    int explicitParameters = -1;
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group.data(), group.size()),
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_EC_ENCODING, encoding.data(), encoding.size()),
        OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_EC_DECODED_FROM_EXPLICIT_PARAMS, &explicitParameters),
        OSSL_PARAM_construct_end()};

    if (EVP_PKEY_get_params(key, parameters) != 1 || !OSSL_PARAM_modified(&parameters[0]) ||
        !OSSL_PARAM_modified(&parameters[1]) || !OSSL_PARAM_modified(&parameters[2]) ||
        parameters[0].return_size >= group.size() || parameters[1].return_size >= encoding.size() ||
        explicitParameters != 0 || std::strcmp(encoding.data(), "named_curve") != 0) {
        return false;
    }

    if (curve == NID_X9_62_prime256v1) {
        return std::strcmp(group.data(), "prime256v1") == 0 || std::strcmp(group.data(), "P-256") == 0;
    }

    return curve == NID_secp384r1 &&
           (std::strcmp(group.data(), "secp384r1") == 0 || std::strcmp(group.data(), "P-384") == 0);
}

} // namespace

std::expected<std::size_t, TlsDiagnostic> preparseTlsTrust(std::string_view pem,
                                                           const TlsResourceLimits &limits) noexcept {
    if (pem.empty() || pem.size() > limits.trustBytes || limits.trustBytes > 524288 || limits.anchors == 0 ||
        limits.anchors > 256) {
        return std::unexpected(diagnostic(TlsError::INVALID_CONFIGURATION, "trust-envelope"));
    }

    enum class State {
        OUTSIDE,   ///< Only explicit whitespace or the next BEGIN delimiter is legal.
        BEGIN,     ///< Match the ordinary CERTIFICATE begin delimiter one byte at a time.
        BEGIN_EOL, ///< Require a complete begin line before any body byte.
        BODY,      ///< Match finite base64 presentation; headers are refused here before decode.
        END,       ///< Match the ordinary CERTIFICATE end delimiter one byte at a time.
        END_EOL    ///< Require EOF or whitespace after a complete end delimiter.
    };
    constexpr std::string_view BEGIN_MARKER = "-----BEGIN CERTIFICATE-----";
    constexpr std::string_view END_MARKER = "-----END CERTIFICATE-----";
    State state = State::OUTSIDE;
    std::size_t marker = 0;
    std::size_t count = 0;
    std::size_t body = 0;
    bool lineStart = true;

    for (const char byte : pem) {
        switch (state) {
        case State::OUTSIDE:

            if (whitespace(byte)) {
                break;
            }

            if (byte != BEGIN_MARKER.front()) {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-framing"));
            }

            state = State::BEGIN;
            marker = 1;
            break;
        case State::BEGIN:

            if (byte != BEGIN_MARKER[marker++]) {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-label"));
            }

            if (marker == BEGIN_MARKER.size()) {
                state = State::BEGIN_EOL;
            }

            break;
        case State::BEGIN_EOL:

            if (byte == '\r') {
                break;
            }

            if (byte != '\n') {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-begin-line"));
            }

            state = State::BODY;
            body = 0;
            lineStart = true;
            break;
        case State::BODY:

            if (byte == '-' && lineStart) {
                state = State::END;
                marker = 1;
            } else if (whitespace(byte)) {
                lineStart = byte == '\n' || lineStart;
            } else if (base64(byte)) {
                ++body;
                lineStart = false;
            } else {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-body-header"));
            }

            break;
        case State::END:

            if (byte != END_MARKER[marker++]) {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-end-label"));
            }

            if (marker == END_MARKER.size()) {
                if (body == 0 || body % 4 != 0 || ++count > limits.anchors) {
                    return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-anchor-count"));
                }

                state = State::END_EOL;
            }

            break;
        case State::END_EOL:

            if (!whitespace(byte)) {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-end-line"));
            }

            state = State::OUTSIDE;
            break;
        }
    }

    if (count == 0 || (state != State::OUTSIDE && state != State::END_EOL)) {
        return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "trust-incomplete"));
    }

    return count;
}

TlsPolicy::~TlsPolicy() {
    SSL_CTX_free(sslContext_);
    OSSL_PROVIDER_unload(provider_);
    OSSL_LIB_CTX_free(context_);
}

std::expected<void, TlsDiagnostic> TlsPolicy::prepare() {
    if (const auto profile = sharedProfile(); !profile) {
        return profile;
    }

    ERR_clear_error();
    ERR_raise(ERR_LIB_USER, ERR_PRIME_REASON);
    const auto marker = ERR_peek_error();

    if (ERR_GET_LIB(marker) != ERR_LIB_USER || ERR_GET_REASON(marker) != ERR_PRIME_REASON) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "sdk-error-state", marker));
    }

    ERR_clear_error();
    context_ = OSSL_LIB_CTX_new();

    if (context_ == nullptr) {
        return std::unexpected(diagnostic(TlsError::ALLOCATION_FAILURE, "tls-library-context", ERR_peek_last_error()));
    }

    provider_ = OSSL_PROVIDER_load(context_, "default");

    if (provider_ == nullptr || std::strcmp(OSSL_PROVIDER_get0_name(provider_), "default") != 0 ||
        RAND_set_DRBG_type(context_, "CTR-DRBG", TLS_PROPERTY_QUERY, "AES-256-CTR", nullptr) != 1) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-default-provider", ERR_peek_last_error()));
    }

    DecoderReceipt receipt;
    ERR_clear_error();
    OSSL_DECODER_do_all_provided(context_, countDecoder, &receipt);
    const auto enumerationError = ERR_peek_error();

    if (receipt.count == 0 || receipt.overflow || enumerationError != 0) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-decoder-first", enumerationError));
    }

    if (RAND_set_seed_source_type(context_, "SEED-SRC", TLS_PROPERTY_QUERY) != 1) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-seed-source", ERR_peek_last_error()));
    }

    ambientRng_ = {RAND_get0_primary(nullptr), RAND_get0_private(nullptr)};
    privateRng_ = {RAND_get0_primary(context_), RAND_get0_public(context_), RAND_get0_private(context_)};

    return checkEnvironment();
}

std::expected<void, TlsDiagnostic> TlsPolicy::checkEnvironment() const noexcept {
    if (const auto shared = sharedProfile(); !shared) {
        return shared;
    }

    const std::array<EVP_RAND_CTX *, 2> ambient = {RAND_get0_primary(nullptr), RAND_get0_private(nullptr)};

    for (std::size_t index = 0; index < ambient.size(); ++index) {
        if (ambient[index] != ambientRng_[index] || !generatorProfile(ambient[index], nullptr)) {
            return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-rng-profile", ERR_peek_last_error()));
        }
    }

    for (auto *generator : privateRng_) {
        if (!generatorProfile(generator, provider_)) {
            return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-rng-profile", ERR_peek_last_error()));
        }
    }

    return {};
}

std::expected<void, TlsDiagnostic> TlsPolicy::configure() {
    sslContext_ = SSL_CTX_new_ex(context_, TLS_PROPERTY_QUERY, TLS_client_method());

    if (sslContext_ == nullptr) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-context", ERR_peek_last_error()));
    }

    constexpr std::uint64_t REQUIRED_OPTIONS = SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_TICKET |
                                               SSL_OP_NO_TX_CERTIFICATE_COMPRESSION |
                                               SSL_OP_NO_RX_CERTIFICATE_COMPRESSION;

    if (SSL_CTX_set_min_proto_version(sslContext_, TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(sslContext_, TLS1_3_VERSION) != 1 ||
        SSL_CTX_get_min_proto_version(sslContext_) != TLS1_2_VERSION ||
        SSL_CTX_get_max_proto_version(sslContext_) != TLS1_3_VERSION ||
        SSL_CTX_set_cipher_list(sslContext_, TLS12_CIPHERS) != 1 ||
        SSL_CTX_set_ciphersuites(sslContext_, TLS13_CIPHERS) != 1 ||
        SSL_CTX_set1_groups_list(sslContext_, GROUPS) != 1 || SSL_CTX_set1_sigalgs_list(sslContext_, SIGNATURES) != 1 ||
        SSL_CTX_set_max_early_data(sslContext_, 0) != 1 || SSL_CTX_set_recv_max_early_data(sslContext_, 0) != 1 ||
        (SSL_CTX_set_options(sslContext_, REQUIRED_OPTIONS) & REQUIRED_OPTIONS) != REQUIRED_OPTIONS) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-context-policy", ERR_peek_last_error()));
    }

    SSL_CTX_set_post_handshake_auth(sslContext_, 0); // Void setter: no success result exists to invent.
    SSL_CTX_set_verify(sslContext_, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_verify_depth(sslContext_, 4);
    SSL_CTX_set_max_cert_list(sslContext_, 65536);
    SSL_CTX_set_session_cache_mode(sslContext_, SSL_SESS_CACHE_OFF);

    if (SSL_CTX_get_verify_mode(sslContext_) != SSL_VERIFY_PEER || SSL_CTX_get_verify_depth(sslContext_) != 4 ||
        SSL_CTX_get_max_cert_list(sslContext_) != 65536 ||
        SSL_CTX_get_session_cache_mode(sslContext_) != SSL_SESS_CACHE_OFF ||
        sk_SSL_CIPHER_num(SSL_CTX_get_ciphers(sslContext_)) != 9) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-context-readback"));
    }

    auto *parameters = SSL_CTX_get0_param(sslContext_);

    if (parameters == nullptr || X509_VERIFY_PARAM_set_flags(parameters, X509_V_FLAG_TRUSTED_FIRST) != 1 ||
        (X509_VERIFY_PARAM_get_flags(parameters) & X509_V_FLAG_TRUSTED_FIRST) == 0) {
        return std::unexpected(diagnostic(TlsError::TLS_POLICY_FAILURE, "tls-verify-policy"));
    }

    SSL_CTX_set_cert_verify_callback(sslContext_, verify, this);

    return {};
}

bool TlsPolicy::guardCertificate(X509 *certificate) noexcept {
    if (certificate == nullptr) {
        return false;
    }

    auto *key = X509_get0_pubkey(certificate);

    if (key == nullptr || EVP_PKEY_get0_provider(key) != provider_) {
        return false;
    }

    const bool rsa = EVP_PKEY_is_a(key, "RSA") == 1;
    const bool pss = EVP_PKEY_is_a(key, "RSA-PSS") == 1;
    const bool ec = EVP_PKEY_is_a(key, "EC") == 1;

    if (!rsa && !pss && !ec) {
        return false;
    }

    auto *publicKey = X509_get_X509_PUBKEY(certificate);
    ASN1_OBJECT *object = nullptr;
    X509_ALGOR *algorithm = nullptr;

    if (publicKey == nullptr || X509_PUBKEY_get0_param(&object, nullptr, nullptr, &algorithm, publicKey) != 1) {
        return false;
    }

    int type = V_ASN1_UNDEF;
    const void *value = nullptr;
    X509_ALGOR_get0(nullptr, &type, &value, algorithm);
    const int family = OBJ_obj2nid(object);
    bool admitted = false;

    if (rsa && family == NID_rsaEncryption && (type == V_ASN1_NULL || type == V_ASN1_UNDEF)) {
        admitted = rsaKey(key);
    } else if (pss && family == NID_rsassaPss && pssParameters(type, value, true)) {
        admitted = rsaKey(key);
    } else if (ec && family == NID_X9_62_id_ecPublicKey && type == V_ASN1_OBJECT && value != nullptr) {
        const int curve = OBJ_obj2nid(static_cast<const ASN1_OBJECT *>(value));
        admitted = (curve == NID_X9_62_prime256v1 || curve == NID_secp384r1) && ecKey(key, curve);
    }

    if (!admitted || EVP_PKEY_missing_parameters(key) != 0) {
        return false;
    }

    const X509_ALGOR *outer = nullptr;
    X509_get0_signature(nullptr, &outer, certificate);
    const auto *tbs = X509_get0_tbs_sigalg(certificate);

    return signatureAlgorithm(outer) && signatureAlgorithm(tbs) && X509_ALGOR_cmp(outer, tbs) == 0;
}

int TlsPolicy::verify(X509_STORE_CTX *context, void *argument) noexcept {
    auto &policy = *static_cast<TlsPolicy *>(argument);
    auto *leaf = X509_STORE_CTX_get0_cert(context);
    const auto *untrusted = X509_STORE_CTX_get0_untrusted(context);
    const int count = untrusted == nullptr ? 0 : sk_X509_num(untrusted);
    bool admitted =
        count >= 0 && static_cast<std::size_t>(count) <= policy.peerCertificateLimit_ && policy.guardCertificate(leaf);

    for (int index = 0; admitted && index < count; ++index) {
        admitted = policy.guardCertificate(sk_X509_value(untrusted, index));
    }

    if (!admitted) {
        policy.verificationFailure_ = diagnostic(TlsError::TRUST_FAILURE, "tls-certificate-policy");
        X509_STORE_CTX_set_error(context, X509_V_ERR_APPLICATION_VERIFICATION);

        return 0;
    }

    const int result = X509_verify_cert(context);

    return result > 0 ? 1 : 0; // Preserve this exact prepared context's ordinary verification result/error.
}

std::expected<void, TlsDiagnostic> TlsPolicy::loadTrust(std::string_view pem, std::size_t count) {
    std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())),
                                                    BIO_free);
    auto *store = SSL_CTX_get_cert_store(sslContext_);

    if (!input || store == nullptr) {
        return std::unexpected(diagnostic(TlsError::ALLOCATION_FAILURE, "tls-trust-reader", ERR_peek_last_error()));
    }

    /** Fixed automatic pointer owner; every candidate is guarded before any trust-store insertion. */
    struct Anchors {
        std::array<X509 *, 256> values{}; ///< Decoded candidates, owned until the complete set is guarded.

        ~Anchors() {
            for (auto *certificate : values) {
                X509_free(certificate);
            }
        }
    } anchors;

    for (std::size_t index = 0; index < count; ++index) {
        X509 *candidate = X509_new_ex(context_, TLS_PROPERTY_QUERY);

        if (candidate == nullptr) {
            return std::unexpected(
                diagnostic(TlsError::ALLOCATION_FAILURE, "tls-trust-certificate", ERR_peek_last_error()));
        }

        auto *decoded = PEM_read_bio_X509(input.get(), &candidate, refusePassword, nullptr);
        anchors.values[index] = candidate;

        if (decoded == nullptr || !guardCertificate(candidate)) {
            return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "tls-trust-anchor", ERR_peek_last_error()));
        }
    }

    for (std::size_t index = 0; index < count; ++index) {
        if (X509_STORE_add_cert(store, anchors.values[index]) != 1) {
            return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "tls-trust-store", ERR_peek_last_error()));
        }
    }

    std::array<char, 256> tail{};

    for (;;) {
        const int bytes = BIO_read(input.get(), tail.data(), static_cast<int>(tail.size()));

        if (bytes <= 0) {
            if (BIO_eof(input.get()) != 1) {
                return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "tls-trust-eof", ERR_peek_last_error()));
            }

            break;
        }

        if (!std::all_of(tail.begin(), tail.begin() + bytes, whitespace)) {
            return std::unexpected(diagnostic(TlsError::TRUST_FAILURE, "tls-trust-tail"));
        }
    }

    return {};
}

std::expected<std::unique_ptr<TlsPolicy>, TlsDiagnostic> TlsPolicy::create(const client::EndpointTls &settings,
                                                                           const TlsResourceLimits &limits) {
    const auto count = preparseTlsTrust(settings.trustPem, limits);

    if (!count) {
        return std::unexpected(count.error());
    }

    try {
        auto policy = std::unique_ptr<TlsPolicy>(new TlsPolicy);
        policy->peerCertificateLimit_ = static_cast<std::size_t>(limits.certificateBytes / 37 + 1);

        if (auto prepared = policy->prepare(); !prepared) {
            return std::unexpected(prepared.error());
        }

        if (auto configured = policy->configure(); !configured) {
            return std::unexpected(configured.error());
        }

        if (auto trust = policy->loadTrust(settings.trustPem, *count); !trust) {
            return std::unexpected(trust.error());
        }

        return policy;
    } catch (const std::bad_alloc &) {
        return std::unexpected(diagnostic(TlsError::ALLOCATION_FAILURE, "tls-policy-storage"));
    }
}

SSL_CTX *TlsPolicy::sslContext() const noexcept {
    return sslContext_;
}

TlsDiagnostic TlsPolicy::verificationFailure() const noexcept {
    return verificationFailure_;
}

} // namespace org::ttldtor::qdcxx::transport::detail
