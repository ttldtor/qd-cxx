// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tls_resources.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>

namespace org::ttldtor::qdcxx::transport {
namespace {
using Count = std::uint64_t;                       ///< Owned nonnegative count/byte arithmetic domain.
constexpr Count MAX_TRUST_BYTES = 524288;          ///< Whole admitted PEM input ceiling.
constexpr Count MAX_ANCHORS = 256;                 ///< Maximum certificate blocks counted before decoding.
constexpr Count CERTIFICATE_BODY_BYTES = 65536;    ///< Fixed initial Certificate/CertificateRequest cap.
constexpr Count MAX_IDENTITY_BYTES = 255;          ///< Maximum immutable reference identity length.
constexpr Count PORTABLE_LOCK_BYTES = 56;          ///< Maximum qualified native SDK lock extent.
constexpr Count PRIVATE_METADATA_BYTES = 39308258; ///< Retained private method/cache/property union.
constexpr Count PRIVATE_CURRENT_BYTES = 1554392;   ///< Private old/candidate/preparation union.
constexpr Count AMBIENT_PREFIX_BYTES = 2480 + PORTABLE_LOCK_BYTES; ///< Stranded primary/seed/lock graph.
constexpr Count PRIVATE_PREFIX_BYTES = PRIVATE_METADATA_BYTES + PRIVATE_CURRENT_BYTES +
                                       AMBIENT_PREFIX_BYTES; ///< One persistent failed private preparation graph.
constexpr Count DECODER_ALTERNATIVES = 912 + 912 * 912;      ///< Two-level initial builtin decoder reader graph.
constexpr Count WIRE_WINDOW_BYTES = 17408;                   ///< One fixed transport or BIO window.
constexpr Count RECORD_CARRY_BYTES =
    177020 + 16716 + 2 * WIRE_WINDOW_BYTES + WIRE_WINDOW_BYTES; ///< Whole owned input carry plus one newly fed window.

/** Sticky checked arithmetic: intermediate failure rejects the entire result, never a smaller allowance. */
class Arithmetic {
    bool valid_ = true; ///< False after the first unrepresentable intermediate.

    public:
    /** Add nonnegative finite terms.
     * @param terms Independently charged counts or byte extents.
     * @return Exact sum, or an unusable zero after marking failure.
     */
    Count sum(std::initializer_list<Count> terms) noexcept {
        Count result = 0;

        for (const Count term : terms) {
            if (term > std::numeric_limits<Count>::max() - result) {
                valid_ = false;

                return 0;
            }

            result += term;
        }

        return result;
    }

    /** Multiply nonnegative finite factors.
     * @param left First count or byte extent.
     * @param right Second count or byte extent.
     * @return Exact product, or an unusable zero after marking failure.
     */
    Count product(Count left, Count right) noexcept {
        if (right != 0 && left > std::numeric_limits<Count>::max() / right) {
            valid_ = false;

            return 0;
        }

        return left * right;
    }

    /** Check all earlier arithmetic.
     * @return True only when every intermediate was representable.
     */
    bool valid() const noexcept {
        return valid_;
    }
};

/** Bound one pointer stack and its current backing.
 * @param arithmetic Sticky checked calculator.
 * @param count Finite pointer count.
 * @return Requested stack/header bytes.
 */
Count stackBytes(Arithmetic &arithmetic, Count count) noexcept {
    return arithmetic.sum({64, arithmetic.product(16, count)});
}

/** Bound both retained/candidate hash bucket extents.
 * @param arithmetic Sticky checked calculator.
 * @param count Finite hash node count.
 * @return Bucket count before pointer-width multiplication.
 */
Count buckets(Arithmetic &arithmetic, Count count) noexcept {
    return arithmetic.sum({arithmetic.product(4, count), 16});
}

/** Bound hash header, nodes and buckets.
 * @param arithmetic Sticky checked calculator.
 * @param count Finite hash node count.
 * @return Requested hash bytes.
 */
Count hashBytes(Arithmetic &arithmetic, Count count) noexcept {
    return arithmetic.sum({104, arithmetic.product(24, count), arithmetic.product(8, buckets(arithmetic, count))});
}

/** Bound generic encoded capacity including terminator/framing growth.
 * @param arithmetic Sticky checked calculator.
 * @param length Admitted source bytes.
 * @return Four-byte-group encoded capacity.
 */
Count encodedBytes(Arithmetic &arithmetic, Count length) noexcept {
    return arithmetic.product(4, arithmetic.sum({length, 3}) / 3);
}

/** Bound retained/current Name encoding, entries and nested RDN sorting owners.
 * @param arithmetic Sticky checked calculator.
 * @param length Maximum admitted trust/peer certificate bytes.
 * @return Complete requested Name peak bytes, excluding native sort scratch.
 */
Count nameBytes(Arithmetic &arithmetic, Count length) noexcept {
    const Count entries = length / 6 + 1;
    const Count rdns = length / 2 + 1;
    const Count retained = arithmetic.sum({72, stackBytes(arithmetic, entries), arithmetic.product(96, entries),
                                           arithmetic.product(12, length), encodedBytes(arithmetic, length)});

    return arithmetic.sum(
        {retained, stackBytes(arithmetic, rdns), arithmetic.product(64, rdns), arithmetic.product(16, entries),
         stackBytes(arithmetic, entries), arithmetic.product(64, entries), arithmetic.product(16, entries),
         arithmetic.product(89, entries), arithmetic.product(5, length), arithmetic.product(24, entries),
         arithmetic.product(9, length), arithmetic.product(2, encodedBytes(arithmetic, length + 1)), 24});
}

/** Bound initial decoder/current key owners for one input kind.
 * @param arithmetic Sticky checked calculator.
 * @param length Immutable admitted source byte ceiling.
 * @param keyCurrent Qualified trust or peer initial key-current envelope.
 * @return Requested bytes including qualified variable Montgomery scratch.
 */
Count decoderBytes(Arithmetic &arithmetic, Count length, Count keyCurrent) noexcept {
    const Count generic = arithmetic.sum({145, arithmetic.product(2, length), arithmetic.product(176, length / 2)});

    return arithmetic.sum(
        {768, arithmetic.product(3, encodedBytes(arithmetic, arithmetic.sum({arithmetic.product(2, length), 16384}))),
         length, std::max(generic, keyCurrent), 5616});
}

/** Calculate private owners and two nested portable native-sort requests.
 * All lengths are validated small input ceilings; host arithmetic is handled separately.
 * @param arithmetic Sticky checked calculator.
 * @param limits Immutable admitted input ceilings.
 * @return Conservative private requested backing, including four wire/BIO windows once.
 */
Count instanceBytes(Arithmetic &arithmetic, const TlsResourceLimits &limits) noexcept {
    const Count peerCount = limits.certificateBytes / 37 + 1;
    const Count certificateCount = limits.anchors + peerCount;
    const Count skeletonCount = certificateCount + 1;
    const Count requestCount = limits.certificateRequestBytes / 4 + 1;
    const Count totalInput = limits.trustBytes + limits.certificateBytes;
    const Count largest = std::max(limits.trustBytes, limits.certificateBytes);
    const Count retainedTrees =
        arithmetic.sum({arithmetic.product(802, skeletonCount), arithmetic.product(3, totalInput),
                        arithmetic.product(176, totalInput / 2), arithmetic.product(280, skeletonCount),
                        arithmetic.product(32, totalInput), arithmetic.product(140, requestCount),
                        arithmetic.product(32, limits.certificateRequestBytes), arithmetic.product(192, totalInput)});
    const Count retainedKeys =
        arithmetic.sum({arithmetic.product(19596, certificateCount), arithmetic.product(8, totalInput)});
    const Count retainedMontgomery = arithmetic.product(3224, certificateCount);
    const Count verifyPointers =
        arithmetic.sum({stackBytes(arithmetic, peerCount), stackBytes(arithmetic, limits.anchors),
                        arithmetic.product(2, stackBytes(arithmetic, 6))});
    const Count containers =
        arithmetic.sum({9330, 464, 34268, 8640, stackBytes(arithmetic, limits.anchors), 33360, verifyPointers, 124207,
                        arithmetic.product(4, limits.identityBytes + 1), stackBytes(arithmetic, 1)});
    const Count wireAndRecords = 69632 + 354072 + 240 + 2121528 + 368 + 8360 + 63281 + 67500 + 216;
    const Count currentTrees =
        arithmetic.sum({5244680,
                        std::max(decoderBytes(arithmetic, limits.trustBytes, 34481016),
                                 decoderBytes(arithmetic, limits.certificateBytes, 20412616)),
                        10560, nameBytes(arithmetic, largest), arithmetic.product(192, largest),
                        arithmetic.product(2, encodedBytes(arithmetic, largest + 1)),
                        arithmetic.product(2, stackBytes(arithmetic, largest / 2)), 24,
                        arithmetic.product(8, (largest + 7) / 8), arithmetic.product(4, largest), 82,
                        arithmetic.product(2, largest), 3, 21710, 272, 1160, 393716, largest, 376});
    const Count signatureShell = arithmetic.sum({3324, arithmetic.product(3, largest)});
    const Count signatureDer =
        arithmetic.sum({336, arithmetic.product(16, (largest + 7) / 8),
                        arithmetic.product(2, encodedBytes(arithmetic, arithmetic.product(2, largest + 16)))});
    const Count certificateCrypto = arithmetic.sum({signatureShell, signatureDer, 600, 45293259, 21710, 41576});
    const Count crypto = std::max(Count{63407559}, certificateCrypto);
    const Count sortCount = largest / 2 + 1;
    const Count directSort = arithmetic.product(sortCount, 24);
    // Check both qualified native size expressions, including the unused wide-element alternative.
    const Count indirectSort = arithmetic.sum({arithmetic.product(16, sortCount), 24});
    (void)indirectSort;
    const Count sortScratch = directSort <= 1024 ? 0 : arithmetic.product(2, directSort);

    return arithmetic.sum({PRIVATE_METADATA_BYTES, PRIVATE_CURRENT_BYTES, retainedTrees, retainedKeys,
                           retainedMontgomery, containers, wireAndRecords, currentTrees, crypto, sortScratch});
}

/** Calculate host-scoped current/history/ERR and stranded-prefix requested backing.
 * @param arithmetic Sticky checked calculator.
 * @param host Truthful immutable host maxima, never detected or modified here.
 * @return Shared requested bytes, reserved once at host scope.
 */
Count sharedBytes(Arithmetic &arithmetic, const TlsHostLimits &host) noexcept {
    const Count nameMap = arithmetic.sum({33048, arithmetic.product(152, host.namemapGroups),
                                          arithmetic.product(104, std::min(host.namemapGroups, Count{2048})), 392});
    const Count maps = arithmetic.sum(
        {4 * (80 + 2 * PORTABLE_LOCK_BYTES), 1080 * 628,
         arithmetic.product(
             628, arithmetic.sum({192, arithmetic.product(host.queryCount, arithmetic.sum({72, host.queryBytes})),
                                  arithmetic.product(8, buckets(arithmetic, host.queryCount))}))});
    const Count atoms = arithmetic.sum(
        {48, PORTABLE_LOCK_BYTES, hashBytes(arithmetic, host.propertyNames), hashBytes(arithmetic, host.propertyValues),
         stackBytes(arithmetic, host.propertyNames), stackBytes(arithmetic, host.propertyValues),
         arithmetic.product(24, arithmetic.sum({host.propertyNames, host.propertyValues})), host.propertyStringBytes});
    const Count ambientRng =
        arithmetic.sum({2592, 69, arithmetic.product(arithmetic.sum({3, host.ambientGenerators}), PORTABLE_LOCK_BYTES),
                        arithmetic.product(2424, host.ambientGenerators)});
    const Count ambient = arithmetic.sum({1710, nameMap, maps, 120696, 87292, 14710, atoms, ambientRng});
    const Count atomCandidate = arithmetic.sum(
        {24, host.propertyStringMaxBytes, 24, stackBytes(arithmetic, host.propertyNames),
         stackBytes(arithmetic, host.propertyValues), arithmetic.product(8, buckets(arithmetic, host.propertyNames)),
         arithmetic.product(8, buckets(arithmetic, host.propertyValues))});
    const Count ambientCurrent = arithmetic.sum({40,
                                                 24,
                                                 stackBytes(arithmetic, 628),
                                                 hashBytes(arithmetic, host.queryCount),
                                                 1024,
                                                 296,
                                                 75424,
                                                 48,
                                                 host.queryBytes,
                                                 arithmetic.product(8, buckets(arithmetic, host.queryCount)),
                                                 104,
                                                 PORTABLE_LOCK_BYTES,
                                                 stackBytes(arithmetic, host.namemapGroups),
                                                 82,
                                                 328,
                                                 arithmetic.product(8, buckets(arithmetic, 57)),
                                                 24,
                                                 atomCandidate,
                                                 36976,
                                                 AMBIENT_PREFIX_BYTES});
    const Count threads = arithmetic.sum({16, PORTABLE_LOCK_BYTES, stackBytes(arithmetic, host.participatingThreads),
                                          arithmetic.product(8, host.participatingThreads),
                                          arithmetic.product(32, host.globalThreadHandlers),
                                          stackBytes(arithmetic, host.participatingThreads), 8});
    const Count global = arithmetic.sum(
        {35168, 24, arithmetic.product(8, buckets(arithmetic, 436)), hashBytes(arithmetic, host.errorTableEntries),
         arithmetic.product(8, buckets(arithmetic, host.errorTableEntries)), 24, 8 * PORTABLE_LOCK_BYTES, 256, 16});
    // ERR uses portable maxima independently: Windows debug strings are longer than Linux's.
    const Count errors = arithmetic.sum(
        {arithmetic.product(host.errorThreads, 904), host.errorCapacityBytes,
         arithmetic.product(16384, host.errorThreads), host.errorDebugBytes,
         arithmetic.product(32 * 278, host.errorThreads),
         arithmetic.product(host.concurrentEntries, std::max(host.errorSlotCapacityBytes, Count{1024}))});

    return arithmetic.sum({ambient, arithmetic.product(host.concurrentEntries, ambientCurrent), threads, global, errors,
                           arithmetic.product(host.failedPrivatePrefixes, PRIVATE_PREFIX_BYTES),
                           arithmetic.product(host.failedAmbientPrefixes, AMBIENT_PREFIX_BYTES)});
}

/** Fill separately charged finite RNG axes for a phase.
 * @param arithmetic Sticky checked calculator.
 * @param vector Owned destination phase; non-RNG coordinates remain unchanged.
 * @param events Leaf chunks/generator preparation opportunities.
 */
void setRng(Arithmetic &arithmetic, TlsWorkVector &vector, Count events) noexcept {
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_EVENT)] = events;
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_GENERATE)] = arithmetic.product(3, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_INSTANTIATE)] = arithmetic.product(3, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_RESEED)] = arithmetic.product(3, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_POOL)] = arithmetic.product(6, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::RNG_AES_BLOCKS)] = arithmetic.product(33213, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::NATIVE_ENTROPY_POOL)] = arithmetic.product(6, events);
    vector.units[static_cast<std::size_t>(TlsWorkUnit::GETRANDOM_ATTEMPT_BOUND)] = arithmetic.product(221202, events);
}

/** Check the input domain before any formula or SDK action.
 * @param limits Immutable endpoint ceilings.
 * @param host Immutable same-image host maxima.
 * @return True when all explicit source-profile representation domains hold.
 */
bool validLimits(const TlsResourceLimits &limits, const TlsHostLimits &host) noexcept {
    constexpr Count SDK_INDEX_MAX = static_cast<Count>(std::numeric_limits<int>::max());

    return limits.trustBytes >= 1 && limits.trustBytes <= MAX_TRUST_BYTES && limits.anchors >= 1 &&
           limits.anchors <= MAX_ANCHORS && limits.certificateBytes == CERTIFICATE_BODY_BYTES &&
           limits.certificateRequestBytes == CERTIFICATE_BODY_BYTES && limits.identityBytes >= 1 &&
           limits.identityBytes <= MAX_IDENTITY_BYTES && host.namemapGroups <= SDK_INDEX_MAX &&
           host.propertyNames <= SDK_INDEX_MAX && host.propertyValues <= SDK_INDEX_MAX &&
           host.participatingThreads <= SDK_INDEX_MAX && host.errorSlotCapacityBytes <= SDK_INDEX_MAX &&
           host.concurrentEntries >= 1 && host.errorThreads >= 1 && host.errorThreads <= host.participatingThreads &&
           host.errorTableEntries >= 1773 && host.queryCount >= 2 && host.queryBytes >= 16 && host.propertyNames >= 6 &&
           host.propertyValues >= 22;
}
} // namespace

std::expected<TlsRequirements, TlsResourceDiagnostic> tlsRequirements(TlsResourceLimits limits,
                                                                      TlsHostLimits host) noexcept {
    if (!validLimits(limits, host)) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::INVALID_CONFIGURATION, "tls.resource.limits"});
    }

    Arithmetic arithmetic;
    const Count maxGenerators = arithmetic.product(2, host.ambientThreadPairs);
    // The unit identity includes transitive metadata visits even where outer phase counts stay one.
    (void)arithmetic.sum({host.namemapGroups, host.propertyNames, host.propertyValues, host.participatingThreads,
                          host.globalThreadHandlers, host.ownerThreadHandlers});

    if (!arithmetic.valid()) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::ARITHMETIC_OVERFLOW, "tls.resource.host"});
    }

    if (host.ambientGenerators > maxGenerators) {
        return std::unexpected(
            TlsResourceDiagnostic{TlsResourceError::INVALID_CONFIGURATION, "tls.resource.generators"});
    }

    TlsRequirements result;
    result.shape = {TLS_RESOURCE_PROFILE_REVISION, limits, host};
    const Count instance = instanceBytes(arithmetic, limits);
    const Count shared = sharedBytes(arithmetic, host);
    const Count total = arithmetic.sum({instance, shared});

    if (!arithmetic.valid() || total > std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::ARITHMETIC_OVERFLOW, "tls.resource.bytes"});
    }

    result.sdk = {static_cast<std::size_t>(instance), static_cast<std::size_t>(shared), static_cast<std::size_t>(total),
                  static_cast<std::size_t>(PRIVATE_PREFIX_BYTES), static_cast<std::size_t>(AMBIENT_PREFIX_BYTES)};
    auto &factory = result.factory.units;
    factory[static_cast<std::size_t>(TlsWorkUnit::PREPARE)] = 1;
    factory[static_cast<std::size_t>(TlsWorkUnit::TRUST_READ)] = limits.anchors + 1;
    factory[static_cast<std::size_t>(TlsWorkUnit::CERTIFICATE_TREE)] = limits.anchors;
    factory[static_cast<std::size_t>(TlsWorkUnit::DECODER_CACHE)] = limits.anchors;
    factory[static_cast<std::size_t>(TlsWorkUnit::FAMILY_ATTEMPT)] =
        arithmetic.product(limits.anchors, DECODER_ALTERNATIVES);
    factory[static_cast<std::size_t>(TlsWorkUnit::KEY_GUARD)] = limits.anchors;
    factory[static_cast<std::size_t>(TlsWorkUnit::ANCHOR_STORE)] = limits.anchors;
    factory[static_cast<std::size_t>(TlsWorkUnit::DIRECT_INPUT_BYTES)] = limits.trustBytes;
    factory[static_cast<std::size_t>(TlsWorkUnit::DIRECT_TEXT_BYTES)] = limits.identityBytes;
    factory[static_cast<std::size_t>(TlsWorkUnit::DIRECT_METADATA_VISITS)] = 25;
    setRng(
        arithmetic, result.factory,
        arithmetic.sum({arithmetic.product(120, factory[static_cast<std::size_t>(TlsWorkUnit::FAMILY_ATTEMPT)]), 9}));
    const Count peerCount = limits.certificateBytes / 37 + 1;
    auto &initial = result.initialHandshake.units;
    initial[static_cast<std::size_t>(TlsWorkUnit::CERTIFICATE_TREE)] = peerCount;
    initial[static_cast<std::size_t>(TlsWorkUnit::DECODER_CACHE)] = peerCount;
    initial[static_cast<std::size_t>(TlsWorkUnit::FAMILY_ATTEMPT)] =
        arithmetic.product(peerCount, DECODER_ALTERNATIVES);
    initial[static_cast<std::size_t>(TlsWorkUnit::KEY_GUARD)] = peerCount + 1;
    initial[static_cast<std::size_t>(TlsWorkUnit::CHAIN_VERIFY)] = 1;
    initial[static_cast<std::size_t>(TlsWorkUnit::INITIAL_FLIGHT)] = 1;
    setRng(arithmetic, result.initialHandshake,
           arithmetic.sum(
               {arithmetic.product(120, initial[static_cast<std::size_t>(TlsWorkUnit::FAMILY_ATTEMPT)]), 2405}));
    result.entry.units[static_cast<std::size_t>(TlsWorkUnit::ENGINE_ENTRY)] = 1;
    result.entry.units[static_cast<std::size_t>(TlsWorkUnit::RECORD_CONTROL)] =
        2 + RECORD_CARRY_BYTES / 4 + RECORD_CARRY_BYTES / 5;
    result.entry.units[static_cast<std::size_t>(TlsWorkUnit::DIRECT_INPUT_BYTES)] = WIRE_WINDOW_BYTES;
    result.entry.units[static_cast<std::size_t>(TlsWorkUnit::DIRECT_METADATA_VISITS)] = 2;
    result.close = result.entry;
    result.close.units[static_cast<std::size_t>(TlsWorkUnit::CLOSE_CONTROL)] = 1;
    result.retirement.units[static_cast<std::size_t>(TlsWorkUnit::ABORT_CONTROL)] = 1;
    result.retirement.units[static_cast<std::size_t>(TlsWorkUnit::RETIRE_GRAPH)] = 1;
    const auto factoryAdmission = addTlsWork(result.factory, result.retirement);
    const auto initialStart = addTlsWork(result.initialHandshake, result.entry);

    if (!arithmetic.valid() || !factoryAdmission || !initialStart) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::ARITHMETIC_OVERFLOW, "tls.resource.work"});
    }

    result.factoryAdmission = *factoryAdmission;
    result.initialStart = *initialStart;

    return result;
}

std::expected<void, TlsResourceDiagnostic> admitTlsWork(TlsWork &work, const TlsWorkShape &shape,
                                                        const TlsWorkVector &need) noexcept {
    if (work.shape != shape || shape.profileRevision != TLS_RESOURCE_PROFILE_REVISION) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::SHAPE_MISMATCH, "tls.work.shape"});
    }

    // Snapshot before mutation also permits need to alias work.remaining without partial-debit reads.
    const TlsWorkVector required = need;

    for (std::size_t index = 0; index < TLS_WORK_UNIT_COUNT; ++index) {
        if (work.remaining.units[index] < required.units[index]) {
            return std::unexpected(TlsResourceDiagnostic{TlsResourceError::WORK_LIMIT, "tls.work.limit"});
        }
    }

    for (std::size_t index = 0; index < TLS_WORK_UNIT_COUNT; ++index) {
        work.remaining.units[index] -= required.units[index];
    }

    return {};
}

std::expected<TlsWorkVector, TlsResourceDiagnostic> addTlsWork(const TlsWorkVector &left,
                                                               const TlsWorkVector &right) noexcept {
    TlsWorkVector result;
    Arithmetic arithmetic;

    for (std::size_t index = 0; index < TLS_WORK_UNIT_COUNT; ++index) {
        result.units[index] = arithmetic.sum({left.units[index], right.units[index]});
    }

    if (!arithmetic.valid()) {
        return std::unexpected(TlsResourceDiagnostic{TlsResourceError::ARITHMETIC_OVERFLOW, "tls.work.add"});
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::transport
