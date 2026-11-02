// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_host_preflight.hpp"
#include "tls_digest_catalog.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/provider.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#ifdef _WIN32
#    include <windows.h>
#endif

#if OPENSSL_VERSION_MAJOR != 3 || OPENSSL_VERSION_MINOR != 5 || OPENSSL_VERSION_PATCH != 9
#    error This executable requires the exact qualified OpenSSL 3.5.9 image.
#endif
#if !defined(OPENSSL_NO_AUTOLOAD_CONFIG) || !defined(OPENSSL_NO_EC2M) || !defined(OPENSSL_NO_ML_KEM) ||                \
    !defined(OPENSSL_NO_FIPS_JITTER) || !defined(OPENSSL_NO_JITTER)
#    error This executable requires the unchanged qualified configuration.
#endif
#if defined(OPENSSL_NO_BLAKE2) || defined(OPENSSL_NO_SM3) || defined(OPENSSL_NO_MD5) || defined(OPENSSL_NO_RMD160)
#    error The checked expected catalogue requires all 27 qualified default DIGEST declarations.
#endif

namespace org::ttldtor::qdcxx::connect::closed_tls {
namespace {
constexpr std::uint32_t PREFLIGHT_REVISION = 1; ///< Exact application metric/catalogue identity.
constexpr bool WORK_MODEL_QUALIFIED =
    true;                       ///< Exact closed source binding; final image/native premises remain required.
constexpr int PRIME_REASON = 1; ///< Fixed nonzero ERR readiness marker; no formatted input/data.
constexpr char DEFAULT_PROVIDER[] = "default"; ///< Selected builtin provider's exact fixed name.
constexpr char QUERY[] = "provider=default";   ///< Existing source-qualified query spelling, not user input.
constexpr std::size_t DIGEST_COUNT = 27;       ///< Expected declarations in both locked compiled profiles.
constexpr std::size_t ALIAS_COUNT = 71;        ///< Every expected alias, including OIDs and the NULL digest.
static_assert(DIGEST_CATALOG.size() == DIGEST_COUNT);
static_assert(sizeof(void *) == 8, "The selected source/native profile requires x64 pointers.");

/** The fixed operation vector includes full transitive bodies described in spec/cli/TLS-HOST-PROFILE.md.
 * Each DIGEST_FETCH contains the complete D1-D7 cold-context/provider/export/probe/MD/cache graph;
 * 130 cipher rows/four capability calls/two NULL probes are internal bounds, not new top-level axes.
 * @return Eight-axis finite whole-sequence requirement; no public TLS phase is substituted.
 */
constexpr PreflightVector requiredWork() noexcept {
    return {{{1, 1, DIGEST_COUNT, DIGEST_COUNT, ALIAS_COUNT, DIGEST_COUNT, 2 * DIGEST_COUNT + ALIAS_COUNT, 1}}};
}

/** Refuse an override before application-controlled SDK entry, without copying/parsing its value or mutating it.
 * Windows queries the same process environment as cpuid.c, preserving the caller's last-error slot.
 * Linux uses the qualified native CRT getter and inspects only its returned pointer, never the value.
 * @return True only for the required absent-override launch environment; native uncertainty refuses.
 */
bool defaultCpuEnvironment() noexcept {
#ifdef _WIN32
    const auto previousError = GetLastError();
    SetLastError(ERROR_SUCCESS);
    const auto required = GetEnvironmentVariableW(L"OPENSSL_ia32cap", nullptr, 0);
    const auto error = GetLastError();
    SetLastError(previousError);

    return required == 0 && error == ERROR_ENVVAR_NOT_FOUND;
#else

    return std::getenv("OPENSSL_ia32cap") == nullptr;
#endif
}

/** Preserve bounded static evidence using only a code already sampled at its own boundary.
 * @param error Typed category.
 * @param stage Static source checkpoint.
 * @param declaration One-based fixed row or zero.
 * @param alias One-based fixed alias or zero.
 * @param code Already observed SDK number; no new getter is entered here.
 * @return Detached static/numeric failure.
 */
PreflightDiagnostic diagnostic(PreflightError error, std::string_view stage, std::size_t declaration = 0,
                               std::size_t alias = 0, unsigned long code = 0) noexcept {
    return {error, stage, declaration, alias, static_cast<std::uint64_t>(code)};
}

/** Establish ERR ownership first; even provider/version/runtime inspection must not precede it.
 * @return Success only after a committed state supports the fixed marker; failure is terminal.
 */
std::expected<void, PreflightDiagnostic> primeErrorState() noexcept {
    ERR_clear_error();
    ERR_new();
    ERR_set_debug(nullptr, 0, nullptr);
    ERR_set_error(ERR_LIB_USER, PRIME_REASON, nullptr);
    const auto marker = ERR_peek_error();

    if (ERR_GET_LIB(marker) != ERR_LIB_USER || ERR_GET_REASON(marker) != PRIME_REASON)
        return std::unexpected(diagnostic(PreflightError::ERROR_STATE, "preflight.error-state", 0, 0, marker));

    ERR_clear_error();

    return {};
}

/** Run the exact compiled catalogue after full precharge and successful ERR-state proof.
 * @return Success only after all 27 methods and all 71 actual hash memberships were checked.
 */
std::expected<void, PreflightDiagnostic> checkCatalog() noexcept {
    for (std::size_t row = 0; row < DIGEST_CATALOG.size(); ++row) {
        const auto &expected = DIGEST_CATALOG[row];
        std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)> method(EVP_MD_fetch(nullptr, expected.canonical, QUERY),
                                                               EVP_MD_free);
        const auto fetchError = ERR_peek_error();

        if (!method || fetchError != 0)
            return std::unexpected(diagnostic(PreflightError::FETCH, "preflight.digest", row + 1, 0, fetchError));

        const auto *provider = EVP_MD_get0_provider(method.get());
        const auto *name = provider ? OSSL_PROVIDER_get0_name(provider) : nullptr;

        if (!name || std::strncmp(name, DEFAULT_PROVIDER, sizeof(DEFAULT_PROVIDER)) != 0)
            return std::unexpected(diagnostic(PreflightError::PROVIDER, "preflight.provider", row + 1));

        for (std::size_t alias = 0; alias < expected.aliases.size(); ++alias) {
            const auto matches = EVP_MD_is_a(method.get(), expected.aliases[alias]);
            const auto aliasError = ERR_peek_error();

            if (matches != 1 || aliasError != 0)
                return std::unexpected(
                    diagnostic(PreflightError::ALIAS, "preflight.alias", row + 1, alias + 1, aliasError));
        }

        method.reset();
        const auto cleanupError = ERR_peek_error();

        if (cleanupError != 0)
            return std::unexpected(
                diagnostic(PreflightError::CLEANUP, "preflight.method-release", row + 1, 0, cleanupError));
    }

    return {};
}
} // namespace

std::expected<PreflightRequirements, PreflightDiagnostic>
preflightRequirements(transport::TlsResourceLimits limits) noexcept {
    auto requirements = transport::tlsRequirements(limits, closedHostLimits());

    if (!requirements)
        return std::unexpected(diagnostic(PreflightError::RESOURCE_SHAPE, "preflight.resource-shape"));

    return PreflightRequirements{*requirements, PREFLIGHT_REVISION, requiredWork(), WORK_MODEL_QUALIFIED};
}

bool PreflightOnce::consume() noexcept {
    if (entered_)
        return false;

    entered_ = true;

    return true;
}

std::expected<CheckedClosedTlsHost, PreflightDiagnostic> checkClosedTlsHost(PreflightOnce &once,
                                                                            transport::TlsResourceLimits limits,
                                                                            PreflightCapacity &capacity,
                                                                            PreflightWork &work) noexcept {
    if (!once.consume())
        return std::unexpected(diagnostic(PreflightError::ALREADY_ENTERED, "preflight.already-entered"));

    const auto requirements = preflightRequirements(limits);

    if (!requirements)
        return std::unexpected(requirements.error());

    if (work.tlsShape != requirements->tls.shape || work.preflightRevision != requirements->preflightRevision)
        return std::unexpected(diagnostic(PreflightError::SHAPE_MISMATCH, "preflight.work-shape"));

    if (capacity.sharedBytes < requirements->tls.sdk.sharedBytes)
        return std::unexpected(diagnostic(PreflightError::RESOURCE_LIMIT, "preflight.shared-capacity"));

    for (std::size_t unit = 0; unit < requirements->work.units.size(); ++unit)
        if (work.remaining.units[unit] < requirements->work.units[unit])
            return std::unexpected(diagnostic(PreflightError::WORK_LIMIT, "preflight.work-capacity"));

    if (!requirements->workQualified)
        return std::unexpected(diagnostic(PreflightError::UNQUALIFIED_WORK_MODEL, "preflight.work-proof-pending"));

    // All checks precede all debit and SDK entry; failed admitted work/storage is never refunded.
    capacity.sharedBytes -= requirements->tls.sdk.sharedBytes;

    for (std::size_t unit = 0; unit < requirements->work.units.size(); ++unit)
        work.remaining.units[unit] -= requirements->work.units[unit];

    if (!defaultCpuEnvironment())
        return std::unexpected(diagnostic(PreflightError::ENVIRONMENT, "preflight.cpu-environment"));

    if (const auto primed = primeErrorState(); !primed)
        return std::unexpected(primed.error());

    if (const auto checked = checkCatalog(); !checked)
        return std::unexpected(checked.error());

    return CheckedClosedTlsHost{closedHostLimits(), requirements->tls.sdk.sharedBytes};
}

} // namespace org::ttldtor::qdcxx::connect::closed_tls
