// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

namespace org::ttldtor::qdcxx::transport {

/** Revision of the qualified TLS backing/work model, independent of wire protocol versions. */
inline constexpr std::uint32_t TLS_RESOURCE_PROFILE_REVISION = 1;

/** Pure TLS requirement/admission failures; no error contains caller, peer or credential text. */
enum class TlsResourceError {
    NONE,                  ///< The requested value operation succeeded.
    INVALID_CONFIGURATION, ///< Required bounds or their SDK representation domain are invalid.
    ARITHMETIC_OVERFLOW,   ///< A checked count/byte product cannot be represented.
    SHAPE_MISMATCH,        ///< Credits were qualified for different immutable bounds/profile.
    WORK_LIMIT             ///< At least one required coordinate exceeds the remaining credits.
};

/** Detached diagnostic for pure resource operations, with static library-owned stage storage. */
struct TlsResourceDiagnostic {
    TlsResourceError error = TlsResourceError::NONE; ///< Typed static rejection.
    std::string_view stage = "none";                 ///< Static label; never an input string.
};

/** Immutable qualified input ceilings; all five values must be supplied explicitly.
 * Trust/identity ceilings may conservatively exceed actual immutable input. The full PEM framing
 * scan must reject too many blocks before decoding, including an otherwise valid excess block.
 * Certificate and CertificateRequest caps are fixed at65536 by the initial TLS profile.
 */
struct TlsResourceLimits {
    std::uint64_t trustBytes = 0;              ///< Whole ordinary certificate PEM bytes, in1..524288.
    std::uint64_t anchors = 0;                 ///< All certificate blocks before decoding, in1..256.
    std::uint64_t certificateBytes = 0;        ///< Whole peer Certificate body cap; exactly65536.
    std::uint64_t certificateRequestBytes = 0; ///< Whole CertificateRequest body cap; exactly65536.
    std::uint64_t identityBytes = 0;           ///< Reference identity byte ceiling, in1..255.

    /** Compare owned numeric ceilings without borrowing external storage.
     * @param other Numeric ceilings to compare.
     * @return True when every ceiling is equal.
     */
    bool operator==(const TlsResourceLimits &other) const noexcept = default;
};

/** Persistent same-image host qualification maxima, including history/current/failure prefixes.
 * The embedding application controls all users of the qualified SDK image and keeps these bounds
 * truthful through final native/SDK retirement. The adapter validates representation/arithmetic,
 * not external truth, and does not enumerate or mutate shared SDK registries. Zero means no owner
 * of that kind, never unlimited. Shared bytes are reserved once at the declared host scope.
 * Stable ambient identity/profile, builtin methods/providers/allocator state, secure heap remains uninitialized and the
 * qualified native binding premises are additional preconditions.
 */
struct TlsHostLimits {
    std::uint64_t namemapGroups = 0;          ///< Retained ambient name/group attempt/orphan highwater.
    std::uint64_t queryCount = 0;             ///< Historical query union cardinality, including our two queries.
    std::uint64_t queryBytes = 0;             ///< Maximum stored query bytes excluding NUL, at least16.
    std::uint64_t propertyNames = 0;          ///< Ambient property-name atom highwater, including builtin names.
    std::uint64_t propertyValues = 0;         ///< Ambient property-value atom highwater, including builtin values.
    std::uint64_t propertyStringBytes = 0;    ///< Total stored atom string bytes including NUL terminators.
    std::uint64_t propertyStringMaxBytes = 0; ///< Maximum one atom string allocation payload including NUL.
    std::uint64_t participatingThreads = 0;   ///< Shared participating-thread pointer-vector highwater.
    std::uint64_t globalThreadHandlers = 0;   ///< All live shared thread-handler nodes, including candidates.
    std::uint64_t ownerThreadHandlers = 0;    ///< Handler nodes traversed on this serialized owner's thread.
    std::uint64_t ambientThreadPairs = 0;     ///< Participating dependent ambient context/thread pairs.
    std::uint64_t ambientGenerators = 0;      ///< Ambient public/private generators; at most twice the pair bound.
    std::uint64_t concurrentEntries = 0;      ///< Concurrent admitted SDK/initialization entries, at least one.
    std::uint64_t errorThreads = 0;           ///< Dependent owner threads with retained ERR state.
    std::uint64_t errorCapacityBytes = 0;     ///< Sum of permitted preexisting/external ERR data capacities.
    std::uint64_t errorSlotCapacityBytes = 0; ///< Maximum one retained ERR data capacity, at most INT_MAX.
    std::uint64_t errorDebugBytes = 0;        ///< Sum of preexisting retained ERR file/function capacities.
    std::uint64_t failedPrivatePrefixes = 0;  ///< Still-reserved possibly stranded private preparation graphs.
    std::uint64_t failedAmbientPrefixes = 0;  ///< Still-reserved possibly stranded ambient generator graphs.
    std::uint64_t errorTableEntries = 0;      ///< Shared error-string table highwater, including builtin1773.

    /** Compare all copied qualification maxima; no SDK state is inspected.
     * @param other Host maxima to compare.
     * @return True when every copied maximum is equal.
     */
    bool operator==(const TlsHostLimits &other) const noexcept = default;
};

/** Exact named transitive logical units of the qualified initial TLS profile.
 * Algorithm units include their bounded ASN.1/Name/sort/metadata/error/failure work, except separately
 * charged descendants. They are neither generic SSL call credits nor serial allocator/instruction
 * counts. All coordinates retain the exact input/profile qualification in TlsWorkShape.
 */
enum class TlsWorkUnit : std::size_t {
    PREPARE,                 ///< One bounded shared/private initialization graph, excluding RNG engines.
    TRUST_READ,              ///< One bounded ordinary PEM reader; final extra reader is nondecoding EOF only.
    CERTIFICATE_TREE,        ///< One bounded outer X509/Name tree, excluding key-cache/family descendants.
    DECODER_CACHE,           ///< One bounded public-SPKI cache/template preparation, excluding family decode.
    FAMILY_ATTEMPT,          ///< One qualified initial builtin public-key decoder attempt, excluding RNG.
    KEY_GUARD,               ///< One cached-key/raw-PSS/family/complete-parameter inspection.
    ANCHOR_STORE,            ///< One guarded anchor insertion and bounded fingerprint/Name comparisons.
    CHAIN_VERIFY,            ///< One same-context guarded depth-four default chain-verification graph.
    INITIAL_FLIGHT,          ///< One bounded initial TLS flight/crypto graph, excluding separately charged work.
    ENGINE_ENTRY,            ///< One bounded memory-BIO SDK dispatch/classification/WANT bookkeeping entry.
    RECORD_CONTROL,          ///< One bounded record or completed-control-message opportunity.
    CLOSE_CONTROL,           ///< One bounded local close_notify transition, excluding engine/record coordinates.
    ABORT_CONTROL,           ///< One reserved allocation-free request-only cutoff; never inline SDK cleanup.
    RETIRE_GRAPH,            ///< One bounded complete normal/failed SDK graph retirement after native drain.
    DIRECT_INPUT_BYTES,      ///< Direct adapter preparse/feed byte visits; nested SDK revisits are transitive.
    DIRECT_TEXT_BYTES,       ///< Direct reference-identity validation byte visits.
    DIRECT_METADATA_VISITS,  ///< Input-field checks or mandatory live-public-owner/current-attempt checks.
    RNG_EVENT,               ///< One bounded leaf chunk or generator-object preparation opportunity.
    RNG_GENERATE,            ///< One qualified builtin CTR generation operation, excluding nested unit bodies.
    RNG_INSTANTIATE,         ///< One qualified builtin CTR instantiate operation.
    RNG_RESEED,              ///< One qualified builtin CTR reseed operation.
    RNG_POOL,                ///< One source-bounded entropy pool acquisition/growth/cleanup.
    RNG_AES_BLOCKS,          ///< One qualified fixed-shape AES block request; not CPU instructions.
    NATIVE_ENTROPY_POOL,     ///< One bounded qualified native entropy-backend acquisition including source routes.
    GETRANDOM_ATTEMPT_BOUND, ///< Linux getrandom progress/failure request bound; not all native syscalls.
    COUNT                    ///< Number of coordinates; not a work unit.
};

/** Fixed coordinate count; no dynamic storage or SDK dependency is needed by the work values. */
inline constexpr std::size_t TLS_WORK_UNIT_COUNT = static_cast<std::size_t>(TlsWorkUnit::COUNT);

/** Owned logical credits/requirement vector. A zero coordinate grants no work of that kind. */
struct TlsWorkVector {
    std::array<std::uint64_t, TLS_WORK_UNIT_COUNT> units{}; ///< Exact coordinate order is TlsWorkUnit.

    /** Compare all copied credits.
     * @param other Coordinate values to compare.
     * @return True when all coordinates are equal.
     */
    bool operator==(const TlsWorkVector &other) const noexcept = default;
};

/** Owned qualification identity; it never borrows caller settings, providers or SDK objects. */
struct TlsWorkShape {
    std::uint32_t profileRevision = 0; ///< Must match the compiled qualified resource model revision.
    TlsResourceLimits limits{};        ///< Copied immutable input ceilings.
    TlsHostLimits host{};              ///< Copied persistent same-image host qualification.

    /** Compare qualification identities before atomically admitting a vector.
     * @param other Qualification identity to compare.
     * @return True for the same profile and all copied limits.
     */
    bool operator==(const TlsWorkShape &other) const noexcept = default;
};

/** Mutable caller-turn credits; SDK-entering operations never replenish/refund them automatically.
 * This value is borrowed only during a synchronous public operation. No runtime retains its address.
 * An owned disposal reserve is copied separately before SDK construction and survives this value.
 */
struct TlsWork {
    TlsWorkShape shape{};      ///< Qualification identity for every remaining credit.
    TlsWorkVector remaining{}; ///< Componentwise remaining credits for this explicit caller turn.
};

/** Conservative requested-backing allowance, excluding allocator bookkeeping/RSS/fixed native frames.
 * Embedded heap arrays, old/candidate/temporary/retired owners and variable alloca/ASM are included.
 * Four fixed wire/BIO windows are included once in instanceBytes. Shared bytes are host-scope owners,
 * not a separate isolated copy per runtime. No method initializes or changes the SDK/global profile.
 */
struct TlsSdkRequirements {
    std::size_t instanceBytes = 0;            ///< Conservative private instance/temporary/disposal envelope.
    std::size_t sharedBytes = 0;              ///< Conservative shared host/current/history/ERR envelope.
    std::size_t totalBytes = 0;               ///< Checked sum, useful only with its declared ownership scopes.
    std::size_t failedPrivatePrefixBytes = 0; ///< Possible persistent private preparation debt; not an error refund.
    std::size_t failedAmbientPrefixBytes = 0; ///< Possible persistent ambient generator debt per failed prefix.
};

/** Source-derived preparation/progress/disposal values for one immutable qualified run.
 * Factory admission is factory+retirement. First TLS entry admission is initialHandshake+entry.
 * Initial phase permits/disposal are owned values after admission; WANT never restarts/refunds them.
 * Normal SDK entry methods are nonreentrant. Reserved requestClose/public-owner cutoff remains legal
 * during a credential-provider callback and requests only, deferring SDK cleanup to safe retirement.
 */
struct TlsRequirements {
    TlsWorkShape shape{};             ///< Identity shared by every requirement vector below.
    TlsSdkRequirements sdk{};         ///< Requested backing, with shared ownership explicitly separated.
    TlsWorkVector factory{};          ///< Factory work excluding its separately owned disposal reserve.
    TlsWorkVector initialHandshake{}; ///< Once-only initial peer/flight/verification phase permit.
    TlsWorkVector initialStart{};     ///< Atomic initialHandshake+entry admission before first TLS SDK entry.
    TlsWorkVector entry{};            ///< One bounded ordinary SDK entry in the current caller turn.
    TlsWorkVector close{};            ///< One orderly TLS-close entry; shutdown zero remains pending.
    TlsWorkVector retirement{};       ///< Owned request-only abort and eventual SDK graph disposal reserve.
    TlsWorkVector factoryAdmission{}; ///< Atomic factory+retirement admission before any SDK allocation.
};

/** Compute finite checked source-profile backing and work requirements without touching any SDK.
 * Host maxima are declarations, not automatically detected state. The fixed qualified native/backend
 * premises remain the embedding application's responsibility through final retirement.
 * @param limits Explicit immutable input ceilings in their documented domains.
 * @param host Complete finite same-image host maxima, including current candidates/failure history.
 * @return Owning values, or a static invalid-domain/overflow rejection; never performs I/O/callbacks.
 */
[[nodiscard]] std::expected<TlsRequirements, TlsResourceDiagnostic> tlsRequirements(TlsResourceLimits limits,
                                                                                    TlsHostLimits host) noexcept;

/** Atomically precharge an indivisible qualified action; zero/one-short never mutates the budget.
 * Failure, WANT and internal retries do not refund an admitted vector. The expected identity/need
 * must be recomputed or retained by the runtime, rather than accepted as a caller verification claim.
 * @param work Caller-owned turn value, borrowed and modified only during this function.
 * @param shape Exact immutable qualification identity for this action.
 * @param need Complete source-derived componentwise requirement.
 * @return Success after subtracting all coordinates, or static shape/work rejection with work unchanged.
 */
[[nodiscard]] std::expected<void, TlsResourceDiagnostic> admitTlsWork(TlsWork &work, const TlsWorkShape &shape,
                                                                      const TlsWorkVector &need) noexcept;

/** Add two independently required vectors with checked coordinate arithmetic.
 * @param left First owned requirement/allowance value.
 * @param right Second owned requirement/allowance value with the same intended qualification.
 * @return Checked coordinate sum, or ARITHMETIC_OVERFLOW; no SDK or budget is changed.
 */
[[nodiscard]] std::expected<TlsWorkVector, TlsResourceDiagnostic> addTlsWork(const TlsWorkVector &left,
                                                                             const TlsWorkVector &right) noexcept;

} // namespace org::ttldtor::qdcxx::transport
