// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_tls_input.hpp"

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS

#    include "recovery_file_input.hpp"

#    include <algorithm>
#    include <chrono>
#    include <climits>
#    include <cstddef>
#    include <cstdint>
#    include <memory>
#    include <span>
#    include <string>
#    include <utility>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::size_t TRUST_INPUT_BYTES = 512 * 1024;   ///< Existing qualified PEM input cap, before parsing.
constexpr std::size_t TRUST_ANCHORS = 256;              ///< All ordinary certificate BEGIN markers, before decoding.
constexpr std::size_t REFERENCE_BYTES = 254;            ///< Existing EndpointTls identity cap, not a DNS lookup limit.
constexpr std::size_t TOKEN_INPUT_BYTES = 4096;         ///< Complete exact ASCII token, including the scheme prefix.
constexpr std::uint64_t PEER_CERTIFICATE_BYTES = 65536; ///< Qualified whole Certificate message cap.
constexpr std::uint64_t PEER_CERTIFICATE_REQUEST_BYTES = 65536; ///< Qualified CertificateRequest message cap.
constexpr std::string_view BASIC_PREFIX = "Basic ";             ///< Exact case and one ASCII space, never trimmed.
constexpr std::size_t BASE64_QUARTET_BYTES = 4;                 ///< Complete standard base64 group size.
constexpr unsigned BASE64_TWO_PADDING_MASK = 0x0f; ///< Low four bits of the last sextet with two '=' bytes.
constexpr unsigned BASE64_ONE_PADDING_MASK = 0x03; ///< Low two bits of the last sextet with one '=' byte.

/** Check explicit duration representation without observing a clock.
 * @param value Finite parsed milliseconds.
 * @return True for a positive value supported by the existing endpoint deadline domain.
 */
bool validTimeout(std::chrono::milliseconds value) noexcept {
    return value.count() > 0 && value.count() <= INT_MAX &&
           value <= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::duration::max());
}

/** Count ordinary certificate labels only; do not duplicate the SDK's PEM/base64/ASN.1 decoder.
 * @param pem Complete bounded source bytes.
 * @return All exact ordinary BEGIN labels in 1..256 with matching END-label count, or static refusal.
 */
std::expected<std::size_t, std::string_view> countCertificateMarkers(std::string_view pem) noexcept {
    constexpr std::string_view BEGIN_LABEL = "-----BEGIN ";
    constexpr std::string_view END_LABEL = "-----END ";
    constexpr std::string_view BEGIN_CERTIFICATE = "-----BEGIN CERTIFICATE-----";
    constexpr std::string_view END_CERTIFICATE = "-----END CERTIFICATE-----";
    std::size_t begins = 0;
    std::size_t ends = 0;

    for (std::size_t offset = 0; (offset = pem.find(BEGIN_LABEL, offset)) != std::string_view::npos;) {
        if (!pem.substr(offset).starts_with(BEGIN_CERTIFICATE))
            return std::unexpected("recovery TLS trust requires ordinary certificate labels");

        if (++begins > TRUST_ANCHORS)
            return std::unexpected("recovery TLS trust exceeds the certificate marker ceiling");

        offset += BEGIN_CERTIFICATE.size();
    }

    for (std::size_t offset = 0; (offset = pem.find(END_LABEL, offset)) != std::string_view::npos;) {
        if (!pem.substr(offset).starts_with(END_CERTIFICATE))
            return std::unexpected("recovery TLS trust requires ordinary certificate labels");

        if (++ends > TRUST_ANCHORS)
            return std::unexpected("recovery TLS trust exceeds the certificate marker ceiling");

        offset += END_CERTIFICATE.size();
    }

    if (begins == 0 || begins != ends)
        return std::unexpected("recovery TLS trust certificate markers are incomplete");

    return begins;
}

/** Inspect one standard base64 alphabet byte without decoding credentials.
 * @param byte ASCII source byte.
 * @return Its sextet in 0..63, or -1 for every other byte, including '='.
 */
int base64Value(unsigned char byte) noexcept {
    if (byte >= 'A' && byte <= 'Z')
        return byte - 'A';

    if (byte >= 'a' && byte <= 'z')
        return byte - 'a' + 26;

    if (byte >= '0' && byte <= '9')
        return byte - '0' + 52;

    if (byte == '+')
        return 62;

    if (byte == '/')
        return 63;

    return -1;
}

/** Validate exact ASCII Basic framing and complete canonical base64 padding, without decoding.
 * @param token Complete source bytes; no whitespace, newline or prefix normalization is performed.
 * @return True for the selected lexical token profile; not a claim about credential validity.
 */
bool validBasicToken(std::string_view token) noexcept {
    if (token.size() > TOKEN_INPUT_BYTES || !token.starts_with(BASIC_PREFIX))
        return false;

    const auto payload = token.substr(BASIC_PREFIX.size());

    if (payload.empty() || payload.size() % BASE64_QUARTET_BYTES != 0)
        return false;

    std::size_t padding = 0;

    if (payload.back() == '=') {
        padding = 1;

        if (payload[payload.size() - 2] == '=')
            padding = 2;
    }

    const auto dataBytes = payload.size() - padding;

    for (std::size_t index = 0; index < dataBytes; ++index) {
        if (base64Value(static_cast<unsigned char>(payload[index])) < 0)
            return false;
    }

    const auto last = static_cast<unsigned>(base64Value(static_cast<unsigned char>(payload[dataBytes - 1])));

    if (padding == 2 && (last & BASE64_TWO_PADDING_MASK) != 0)
        return false;

    if (padding == 1 && (last & BASE64_ONE_PADDING_MASK) != 0)
        return false;

    return true;
}

/** Application-owned immutable secret; no path, configuration or runtime is retained.
 * Only the existing verified adapter invokes this provider. It performs no I/O, allocation,
 * callback or progress call. Ordinary string destruction makes no secure-erasure promise.
 */
class FileCredential final : public client::CredentialProvider {
    std::u16string token_; ///< Exact validated ASCII bytes widened once, at most 4096 UTF-16 units.

    public:
    /** Adopt the already bounded complete token without copying it.
     * @param token Exact validated UTF-16 token; ownership transfers into this provider.
     */
    explicit FileCredential(std::u16string token) noexcept : token_(std::move(token)) {
    }

    /** Copy synchronously only when the verified adapter requests a token.
     * @param destination Borrowed adapter buffer, never retained; no writes on insufficient space.
     * @return Exact copied units, or static REJECTED if the destination cannot hold the whole token.
     */
    std::expected<std::size_t, client::CredentialError> basicToken(std::span<char16_t> destination) override {
        if (destination.size() < token_.size())
            return std::unexpected(client::CredentialError::REJECTED);

        std::copy(token_.begin(), token_.end(), destination.begin());

        return token_.size();
    }
};
} // namespace

std::expected<PreparedTlsInput, std::string_view> prepareRecoveryTlsInput(const RecoveryConfiguration &input) {
    if (input.security != client::EndpointSecurity::TLS || !input.tlsTrustPem || !input.tlsReference)
        return std::unexpected("recovery TLS requires explicit trust and reference input");

    if (input.tlsReference->empty() || input.tlsReference->size() > REFERENCE_BYTES ||
        !std::ranges::all_of(*input.tlsReference, [](unsigned char byte) {
            return byte >= 0x21 && byte <= 0x7e;
        }))
        return std::unexpected("recovery TLS reference input is outside the application profile");

    if (!validTimeout(input.tlsHandshake) || !validTimeout(input.tlsClose) ||
        input.authBasicTokenFile.has_value() != input.authTimeout.has_value() ||
        (input.authTimeout && !validTimeout(*input.authTimeout)))
        return std::unexpected("recovery TLS deadlines or credential selection are invalid");

    try {
        auto trust = readRecoveryFile(*input.tlsTrustPem, TRUST_INPUT_BYTES);

        if (!trust)
            return std::unexpected(trust.error());

        const auto anchors = countCertificateMarkers(*trust);

        if (!anchors)
            return std::unexpected(anchors.error());

        PreparedTlsInput result;
        result.endpoint.referenceIdentity = *input.tlsReference;
        result.endpoint.trustPem = std::move(*trust);
        result.endpoint.handshakeTimeout = input.tlsHandshake;
        result.endpoint.closeTimeout = input.tlsClose;
        result.limits = {result.endpoint.trustPem.size(), *anchors, PEER_CERTIFICATE_BYTES,
                         PEER_CERTIFICATE_REQUEST_BYTES, result.endpoint.referenceIdentity.size()};

        if (input.authBasicTokenFile) {
            auto token = readRecoveryFile(*input.authBasicTokenFile, TOKEN_INPUT_BYTES);

            if (!token)
                return std::unexpected(token.error());

            if (!validBasicToken(*token))
                return std::unexpected("recovery TLS credential input requires a complete exact ASCII Basic token");

            std::u16string ownedToken;
            ownedToken.reserve(token->size());

            for (const unsigned char byte : *token)
                ownedToken.push_back(static_cast<char16_t>(byte));

            result.endpoint.tokenUnits = ownedToken.size();
            result.endpoint.authenticationTimeout = input.authTimeout;
            result.endpoint.credentials = std::make_shared<FileCredential>(std::move(ownedToken));
        }

        return result;
    } catch (...) {
        return std::unexpected("recovery TLS input preparation failed");
    }
}

} // namespace org::ttldtor::qdcxx::connect

#endif
