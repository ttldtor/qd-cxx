// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tls_runtime.hpp"

#ifndef OPENSSL_SUPPRESS_DEPRECATED
#    define OPENSSL_SUPPRESS_DEPRECATED
#endif
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <array>
#include <expected>
#include <memory>
#include <string_view>

namespace org::ttldtor::qdcxx::transport::detail {

inline constexpr char TLS_PROPERTY_QUERY[] = "provider=default"; ///< Fixed private method/property profile.
inline constexpr std::size_t TLS_RECORD_CAPACITY = 17408;        ///< One qualified finite record/BIO-side window.
inline constexpr std::size_t TLS_PLAINTEXT_CAPACITY = 16384;     ///< Maximum admitted application record fragment.

/** Private stable checked policy/store owner; never exposed or mutated through caller aliases.
 * Create only after F+R admission and destroy using the copied disposal reserve. SSL objects must
 * release before this owner. Every operation and release stays on the creating serialized thread;
 * the same-SDK host profile/ambient identity remains valid through final dependent native release.
 */
class TlsPolicy {
    OSSL_LIB_CTX *context_ = nullptr;   ///< Explicit private library context; never the global default.
    OSSL_PROVIDER *provider_ = nullptr; ///< Explicit builtin default-provider activation, owned until retirement.
    SSL_CTX *sslContext_ = nullptr;     ///< Private TLS client policy and guarded explicit trust store.
    std::array<EVP_RAND_CTX *, 2> ambientRng_{}; ///< Borrowed stable host primary/private generators.
    std::array<EVP_RAND_CTX *, 3> privateRng_{}; ///< Borrowed generators owned by context_.
    std::size_t peerCertificateLimit_ = 0;       ///< Complete untrusted-list ceiling derived from immutable C.
    TlsDiagnostic verificationFailure_{};        ///< Static policy preflight evidence; never peer/provider strings.

    /** Construct empty stable ownership before any SDK allocation. */
    TlsPolicy() = default;
    /** Prepare the exact decoder-first private provider and checked READY generators.
     * @return Empty on success, otherwise static policy/allocation rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> prepare();
    /** Configure the checked exact TLS policy and internal verification adapter.
     * @return Empty on success, otherwise static setup rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> configure();
    /** Decode exactly the already preframed anchor count into the explicit guarded store.
     * @param pem Immutable ordinary CERTIFICATE-only PEM input, retained for this synchronous call.
     * @param count Exact preparse anchor cardinality; no extra certificate decode is attempted.
     * @return Empty on success, otherwise detached trust/policy evidence.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> loadTrust(std::string_view pem, std::size_t count);
    /** Inspect a cached key/type/raw parameters and both certificate signature identifiers.
     * @param certificate Borrowed immutable certificate held by this store or SSL verification context.
     * @return True only for the complete admitted family/parameter/signature policy.
     */
    [[nodiscard]] bool guardCertificate(X509 *certificate) noexcept;
    /** Guard leaf plus every untrusted element before one ordinary verification on the same context.
     * @param context SDK-prepared verification context, borrowed for this callback only.
     * @param argument Stable TlsPolicy owner retained by its SSL/operation lease.
     * @return One for successful ordinary verification, otherwise zero with its actual context error preserved.
     */
    [[nodiscard]] static int verify(X509_STORE_CTX *context, void *argument) noexcept;

    public:
    /** Retire private SSL/store/provider/context owners, without process/global/thread cleanup. */
    ~TlsPolicy();
    /** Copying a private SDK owner is unavailable.
     * @param other Unused source.
     */
    TlsPolicy(const TlsPolicy &other) = delete;
    /** Copying a private SDK owner is unavailable.
     * @param other Unused source.
     * @return No result; copying is unavailable.
     */
    TlsPolicy &operator=(const TlsPolicy &other) = delete;
    /** Preframe all trust/count input before SDK parsing, then prepare one stable private policy.
     * @param settings Owned-snapshot TLS values, borrowed unchanged during preparation.
     * @param limits Checked immutable source/input ceilings.
     * @return Stable prepared owner or detached failure, including partial-prefix cleanup.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TlsPolicy>, TlsDiagnostic>
    create(const client::EndpointTls &settings, const TlsResourceLimits &limits);
    /** Recheck enforceable allocator/heap/legacy-RAND and cached READY/configuration premises.
     * Host registries, actual external maxima and complete ambient identity remain persistent
     * embedding preconditions; this readback does not isolate or enumerate process state.
     * @return Empty on success, otherwise static profile evidence; no global setting is changed.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> checkEnvironment() const noexcept;
    /** Borrow the stable prepared SSL context for one dependent client SSL object.
     * @return Nonnull owned context, valid until this policy retires.
     */
    [[nodiscard]] SSL_CTX *sslContext() const noexcept;
    /** Inspect preflight failure observed by the internal app verification callback.
     * @return Detached static/numeric evidence, NONE when no guard failure occurred.
     */
    [[nodiscard]] TlsDiagnostic verificationFailure() const noexcept;
};

/** Validate immutable trust envelope/count without decoding or SDK initialization.
 * @param pem Explicit ordinary CERTIFICATE-only bytes; no AUX/encrypted/unsupported headers.
 * @param limits Checked byte/anchor ceilings.
 * @return Exact anchor cardinality or static trust/configuration rejection.
 */
[[nodiscard]] std::expected<std::size_t, TlsDiagnostic> preparseTlsTrust(std::string_view pem,
                                                                         const TlsResourceLimits &limits) noexcept;

} // namespace org::ttldtor::qdcxx::transport::detail
