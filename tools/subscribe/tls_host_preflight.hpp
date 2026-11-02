// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tls_resources.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

namespace org::ttldtor::qdcxx::connect::closed_tls {

/** Fixed algorithm opportunities; transitive bodies are defined in spec/cli/TLS-HOST-PROFILE.md.
 * These application units are not generic SDK calls, CPU/time units or public TlsWorkUnit aliases.
 * In particular no PREPARE/ENGINE_ENTRY credit is relabelled as arbitrary preflight permission.
 * DIGEST_FETCH includes cold context/partial cleanup and default-provider cipher export: at most
 * 130 rows, four capability callbacks and two source-guarded NULL assembly probes per complete unit.
 */
enum class PreflightUnit : std::size_t {
    ENVIRONMENT_GATE,  ///< One qualified native presence query, without reading/parsing an override value.
    ERROR_PRIME,       ///< One fixed five-getter cold ERR priming/init/string-load graph.
    DIGEST_FETCH,      ///< Full cold context/provider/MD path: 27 MD rows/71 aliases and 130 cipher-export rows.
    PROVIDER_IDENTITY, ///< One borrowed provider/name check with at most eight compared bytes.
    ALIAS_MEMBERSHIP,  ///< One actual nonzero-method-ID namemap hash lookup; no alias-list inference.
    METHOD_RELEASE,    ///< One selected EVP_MD reference retirement, including its bounded leaf cleanup.
    ERROR_OBSERVE,     ///< One post-marker ERR peek with already committed owner state.
    ERROR_CLEAR,       ///< One post-marker clear retaining the already funded ERR state/buffers.
    COUNT              ///< Coordinate count, never a credit.
};

/** Fixed preflight vector; no automatic replenishment or refund. */
struct PreflightVector {
    std::array<std::uint64_t, static_cast<std::size_t>(PreflightUnit::COUNT)> units{}; ///< Exact enum order.
};

/** Immutable qualification binding for the separate application preflight counters. */
struct PreflightWork {
    transport::TlsWorkShape tlsShape{};  ///< Same complete source/host/input shape used by pure TLS requirements.
    std::uint32_t preflightRevision = 0; ///< Must match this exact expected catalogue and source proof.
    PreflightVector remaining{};         ///< Caller credits, atomically debited before the first SDK entry.
};

/** Explicit available host-scope requested storage, independent of work credits. */
struct PreflightCapacity {
    std::size_t sharedBytes = 0; ///< Remaining whole-host logical requested-payload reservation; excludes RSS.
};

/** Static refusal categories; no SDK/user-supplied strings escape. */
enum class PreflightError {
    RESOURCE_SHAPE,         ///< Public pure TLS requirements refused the immutable input/domain.
    SHAPE_MISMATCH,         ///< Credits describe another complete source/host/catalogue shape.
    RESOURCE_LIMIT,         ///< Whole preflight host reservation cannot fit before SDK entry.
    WORK_LIMIT,             ///< A fixed preflight opportunity is unfunded before SDK entry.
    UNQUALIFIED_WORK_MODEL, ///< The compiled source binding does not enable the exact transitive work model.
    ALREADY_ENTERED,        ///< This application-owned single-use preflight scope was already consumed.
    ENVIRONMENT,            ///< An inherited CPU override prevents the default-image entry qualification.
    ERROR_STATE,            ///< Fixed first-entry marker could not prove usable ERR state.
    FETCH,                  ///< An expected compiled declaration could not be fetched without an SDK error.
    PROVIDER,               ///< Fetched method is not backed by the selected default provider.
    ALIAS,                  ///< At least one expected alias lacks actual hash membership for its method.
    CLEANUP                 ///< An SDK error remained after the owned method reference was released.
};

/** Detached bounded evidence, never a file/path/token/challenge or arbitrary exception string. */
struct PreflightDiagnostic {
    PreflightError error;        ///< Typed static failure.
    std::string_view stage;      ///< Static checkpoint in this source.
    std::size_t declaration = 0; ///< One-based expected-catalogue row, zero outside its loop.
    std::size_t alias = 0;       ///< One-based alias position, zero outside alias checking.
    std::uint64_t sdkCode = 0;   ///< Already observed numeric error; no error-string formatting is performed.
};

/** Pure resource description for this closed executable shape. */
struct PreflightRequirements {
    transport::TlsRequirements tls;  ///< Existing public calculation, including full twenty-field host declaration.
    std::uint32_t preflightRevision; ///< Exact application metric/catalogue revision.
    PreflightVector work;            ///< Whole finite preflight sequence, including failure cleanup, funded at outset.
    bool workQualified;              ///< Whether this exact compiled source binding enables the work model.
};

/** Build the closed twenty-field declaration without executing any SDK operation.
 * Values require fresh process, one exact SDK image/thread, no other users/mutation and completed private retirement.
 * The J6297/H9 derivation and retained/current/failure ownership are in spec/cli/TLS-HOST-PROFILE.md.
 * @return Full conditional host declaration; this operation does not detect external host truth.
 */
[[nodiscard]] constexpr transport::TlsHostLimits closedHostLimits() noexcept {
    return {.namemapGroups = 6297,
            .queryCount = 2,
            .queryBytes = 16,
            .propertyNames = 6,
            .propertyValues = 22,
            .propertyStringBytes = 28000,
            .propertyStringMaxBytes = 1000,
            .participatingThreads = 1,
            .globalThreadHandlers = 9,
            .ownerThreadHandlers = 9,
            .ambientThreadPairs = 1,
            .ambientGenerators = 2,
            .concurrentEntries = 1,
            .errorThreads = 1,
            .errorCapacityBytes = 0,
            .errorSlotCapacityBytes = 0,
            .errorDebugBytes = 0,
            .failedPrivatePrefixes = 3,
            .failedAmbientPrefixes = 4,
            .errorTableEntries = 1904};
}

/** Calculate explicit storage/work before the first SDK entry.
 * @param limits Actual immutable later TLS input ceilings; no trust bytes are decoded here.
 * @return Public checked resource result plus application fixed-work requirements, or a static pure refusal.
 */
[[nodiscard]] std::expected<PreflightRequirements, PreflightDiagnostic>
preflightRequirements(transport::TlsResourceLimits limits) noexcept;

/** Single application-owned fresh-process entry scope; refusal consumes it even before SDK entry.
 * No second instance/run is permitted by the closed integration contract. This is not a detector of foreign users.
 * All operations and destruction are serialized on the application's one SDK-owning thread.
 */
class PreflightOnce {
    bool entered_ = false; ///< Prevent retry/enlarged allowances after a refusal in this scope.

    public:
    /** Create an inert scope without any SDK call, heap owner or callback. */
    PreflightOnce() noexcept = default;
    PreflightOnce(const PreflightOnce &) = delete;
    PreflightOnce &operator=(const PreflightOnce &) = delete;
    PreflightOnce(PreflightOnce &&) = delete;
    PreflightOnce &operator=(PreflightOnce &&) = delete;
    /** Permanently consume the one allowed invocation without entering an SDK.
     * @return True exactly once for this application-owned scope.
     */
    [[nodiscard]] bool consume() noexcept;
};

/** Detached successful catalogue receipt; owns no method, provider, context or callback.
 * It establishes checked alias membership under the closed lifetime premises, not automatic detection of foreign SDK
 * users.
 */
struct CheckedClosedTlsHost {
    transport::TlsHostLimits limits; ///< Complete conditional declaration for all later same-image operations.
    std::size_t reservedSharedBytes; ///< Persistent host reservation stays charged through process SDK retirement.
};

/** Check every expected compiled MD/alias before any cold trust/peer decode or recovery construction.
 * The exact compiled source binding and qualified static image must satisfy the documented native premises.
 * No callbacks, native connection, file/credential access, provider/config mutation or fallback is introduced.
 * All requested storage and work are checked then atomically debited before the native presence gate
 * and first application-controlled SDK entry. Qualified CRT/static-image startup precedes this boundary.
 * After admission, failure consumes the full vector/reservation; no refund/retry or second run is permitted.
 * Method references remain owned through all provider/alias checks and retire exactly once on every return.
 * @param once Fresh-process application scope, permanently consumed on every invocation/refusal.
 * @param limits Actual immutable later TLS ceilings, checked purely before SDK entry.
 * @param capacity Whole-host remaining requested-payload allowance, not an allocator/RSS measurement.
 * @param work Separate preflight work with the exact copied immutable shape.
 * @return Detached checked membership receipt, or static bounded failure; failure must prevent TlsRecovery entry.
 */
[[nodiscard]] std::expected<CheckedClosedTlsHost, PreflightDiagnostic>
checkClosedTlsHost(PreflightOnce &once, transport::TlsResourceLimits limits, PreflightCapacity &capacity,
                   PreflightWork &work) noexcept;

} // namespace org::ttldtor::qdcxx::connect::closed_tls
